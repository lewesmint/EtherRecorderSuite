#include "comm_context.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform_error.h"
#include "platform_threads.h"  // Make sure this includes wait definitions
#include "thread_registry.h"
#include "app_config.h"
#include "logger.h"


typedef struct HexDumpConfig {
    int bytes_per_row;
    int bytes_per_col;
} HexDumpConfig;

static HexDumpConfig g_hex_dump_config = {0};

static void init_hex_dump_config(void) {
    g_hex_dump_config.bytes_per_row = get_config_int("logger", "hex_dump_bytes_per_row", 32);
    g_hex_dump_config.bytes_per_col = get_config_int("logger", "hex_dump_bytes_per_col", 4);
}

static void cleanup_threads(PlatformThreadId* thread_ids, uint32_t count) {
    if (!thread_ids || count == 0) {
        return;
    }

    PlatformWaitResult result = thread_registry_wait_list(thread_ids, count, DEFAULT_THREAD_WAIT_TIMEOUT_MS);
    if (result != PLATFORM_WAIT_SUCCESS) {
        for (size_t i = 0; i < count; i++) {
            if (thread_ids[i]) {
                logger_log(LOG_WARN, "Thread %lu failed to exit cleanly", (unsigned long)thread_ids[i]);
            }
        }
    }
}

void comm_context_cleanup_threads(CommContext* context) {
    if (!context) {
        return;
    }

    PlatformThreadId threads[2] = {0};
    uint32_t thread_count = 0;

    if (context->send_thread_id) {
        threads[thread_count++] = context->send_thread_id;
    }

    if (context->recv_thread_id) {
        threads[thread_count++] = context->recv_thread_id;
    }

    if (thread_count > 0) {
        cleanup_threads(threads, thread_count);
        context->send_thread_id = 0;
        context->recv_thread_id = 0;
    }
}

PlatformErrorCode comm_context_create_threads(ThreadConfig* send_config,
                                              ThreadConfig* receive_config) {
    if (!send_config || !receive_config) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    CommContext* send_context = (CommContext*)send_config->data;
    CommContext* recv_context = (CommContext*)receive_config->data;
    
    if (!send_context || !recv_context) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    // Initialize hex dump configuration
    init_hex_dump_config();

    // If relay is enabled, set up the foreign queue labels for receive thread
    if (recv_context->is_relay_enabled) {
        const char* client_send = "CLIENT.SEND";
        const char* server_send = "SERVER.SEND";
        
        const char* target_queue = strncmp(send_config->label, server_send, strlen(server_send)) == 0 
            ? client_send 
            : server_send;
        strncpy(recv_context->foreign_queue_label, target_queue, THREAD_LABEL_SIZE);
    }

    // Create send thread
    ThreadResult result = app_thread_create(send_config);
    if (result != THREAD_SUCCESS) {
        return PLATFORM_ERROR_THREAD_CREATE;
    }
    send_context->send_thread_id = send_config->thread_id;

    // Create receive thread
    result = app_thread_create(receive_config);
    if (result != THREAD_SUCCESS) {
        cleanup_threads(&send_context->send_thread_id, 1);
        send_context->send_thread_id = 0;
        return PLATFORM_ERROR_THREAD_CREATE;
    }
    recv_context->recv_thread_id = receive_config->thread_id;

    return PLATFORM_ERROR_SUCCESS;
}

static bool comm_context_is_closed(const CommContext* context) {
    if (!context) {
        return true;
    }
    return platform_atomic_load_bool(context->connection_closed) ;
}

static void comm_context_close(CommContext* context) {
    if (!context) {
        logger_log(LOG_ERROR, "Invalid context");
        return;
    }
    platform_atomic_store_bool(context->connection_closed, true);
}


PlatformErrorCode handle_send(CommContext* context, char* buffer, size_t buffer_size, size_t* bytes_sent) {

    if (!context || !buffer || buffer_size == 0) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    // Check if socket is writable
    PlatformErrorCode result = platform_socket_wait_writable(context->socket, context->timeout_ms);
    if (result != PLATFORM_ERROR_SUCCESS) {
        if (result == PLATFORM_ERROR_TIMEOUT) {
            return true;  // Timeout is not an error condition
        }
        // Any other error should close the connection
        comm_context_close(context);
        return false;
    }

    // Send the message
    PlatformErrorCode err = platform_socket_send(context->socket, buffer, 
                                               buffer_size,
                                               bytes_sent);
    if (err != PLATFORM_ERROR_SUCCESS) {
        comm_context_close(context);
        return err;
    }

    return PLATFORM_ERROR_SUCCESS;
}

static void log_buffered_data(const uint8_t* buffer, size_t length, int batch_bytes) {
    size_t index = 0;
    
    // Use the global configuration
    const uint32_t bytes_per_row = g_hex_dump_config.bytes_per_row;
    const uint32_t bytes_per_col = g_hex_dump_config.bytes_per_col;
    const uint32_t cols_per_row  = bytes_per_row / bytes_per_col;
   
    // Position within the current row
    static size_t row_position = 0;

    // Character mapping for hex values
    const char hex_chars[] = "0123456789ABCDEF";

    logger_log(LOG_INFO, "%d bytes received: top", batch_bytes);

    while (index < length) {
        // Initialize the row with placeholder dots
        char row[256] = {0};
        for (uint32_t i = 0; i < cols_per_row; i++) {
            uint32_t base = i * (bytes_per_col * 2 + 1); // 2 chars per byte + 1 space
            for (uint32_t j = 0; j < bytes_per_col * 2; j++) {
                row[base + j] = '.';
            }
            row[base + bytes_per_col * 2] = ' ';  // Space after each column
        }
        
        // Calculate how many bytes to place in this row
        size_t avail = (size_t)(bytes_per_row - row_position);
        size_t to_place = (length - index < avail) ? (length - index) : avail;

        // Place the bytes into the row
        for (size_t i = 0; i < to_place; i++) {
            size_t pos = row_position + i;           // Position within the row
            size_t col = pos / bytes_per_col;        // Column index (32-bit word)
            size_t offset = pos % bytes_per_col;     // Offset within the word
            
            // Each byte takes 2 hexdigits in the output
            size_t dest_index = col * (bytes_per_col * 2 + 1) + offset * 2;
            
            uint8_t byte = buffer[index + i];
            row[dest_index] = hex_chars[byte >> 4];       // High nibble
            row[dest_index + 1] = hex_chars[byte & 0x0F]; // Low nibble
        }

        // Update the row position
        row_position += to_place;
        if (row_position >= bytes_per_row) {
            row_position = 0;  // Start a fresh row next time
        }
        index += to_place;

        // Log the formatted row
        logger_log(LOG_INFO, "%s", row);
    }

    logger_log(LOG_INFO, "%d bytes received: bottom", batch_bytes);
}

static bool process_relay_data(CommContext* context, const char* buffer, size_t bytes_received) {
    if (!context->is_relay_enabled || context->foreign_queue_label[0] == '\0') {
        return true;  // Not an error, just no relay needed
    }

    MessageQueue_T* foreign_queue = get_queue_by_label(context->foreign_queue_label);
    if (!foreign_queue) {
        return true;  // Queue not available yet, ignore
    }

    const size_t max_content_size = sizeof(((Message_T*)0)->content);
    const char* current_pos = buffer;
    size_t remaining = bytes_received;

    int chunk_count = 0;
    while (remaining > 0) {
        if (chunk_count >= 1) {
            logger_log(LOG_ERROR, "Chunks in relay data %d", chunk_count);
            return false;
        }
        Message_T message = {0};
        message.header.type = MSG_TYPE_RELAY;
        message.header.content_size = (remaining > max_content_size) ? max_content_size : remaining;
        
        memcpy(message.content, current_pos, message.header.content_size);
        
        if (!message_queue_push(foreign_queue, &message, DEFAULT_THREAD_WAIT_TIMEOUT_MS)) {
            logger_log(LOG_ERROR, "Failed to relay message to foreign queue");
            return false;
        }

        current_pos += message.header.content_size;
        remaining -= message.header.content_size;
        chunk_count++;
    }

    return true;
}

static bool handle_receive(CommContext* context, char* buffer, size_t buffer_size) {
    if (!context || !buffer) {
        return false;
    }

    static int timeout_count = 0;

    // Check if socket is readable
    PlatformErrorCode result = platform_socket_wait_readable(context->socket, context->timeout_ms);
    if (result != PLATFORM_ERROR_SUCCESS) {
        if (result == PLATFORM_ERROR_TIMEOUT) {
            timeout_count++;
            if (timeout_count >= 10) {
                timeout_count = 0;
                logger_log(LOG_ERROR, "Socket read timed out 10 times in a row");
                return false;
            }
            logger_log(LOG_INFO, "Socket read timed out");
            return true;  // Timeout is not an error condition
        }
        // Any other error should close the connection
        comm_context_close(context);
        return false;
    }

    size_t bytes_received;
    PlatformErrorCode err = platform_socket_receive(context->socket,
                                                    buffer,
                                                    buffer_size,
                                                    &bytes_received);
    if (err != PLATFORM_ERROR_SUCCESS) {
        comm_context_close(context);
        return false;
    }

    // Log the received data in hex format
    log_buffered_data((const uint8_t*)buffer, bytes_received, (int)bytes_received);

    // Handle relay if enabled
    if (!process_relay_data(context, buffer, bytes_received)) {
        // a false return means an issue with the relay, not the receive
        // and it will have been reported on already
        ;
    }

    return true;
}

// static int32_t populate_buffer(char* buffer, size_t buffer_size) {
//     (void)buffer;
//     (void)buffer_size;
//     return 0;
//     // if (!buffer || buffer_size == 0) {
//     //     return 0;
//     // }

//     // // Populate the buffer with some data
//     // for (size_t i = 0; i < buffer_size; i++) {
//     //     buffer[i] = (char)i;
//     // }

//     // return (int32_t)buffer_size;
// }

// void* comm_send_thread(void* arg) {
//     ThreadConfig* thread_config = (ThreadConfig*)arg;
//     CommContext* context = (CommContext*)thread_config->data;
//     if (!context) {
//         return NULL;
//     }

//     char buffer[2048];
//     bool send_error = false;

//     logger_log(LOG_INFO, "Send thread started");

//     while (!comm_context_is_closed(context) && !shutdown_signalled() && !send_error) {
//         size_t bytes_sent = 0;
//         int32_t bytes_to_send = populate_buffer(buffer, sizeof(buffer));
        
//         // Validate bytes_to_send
//         if (bytes_to_send < 0 || (size_t)bytes_to_send > sizeof(buffer)) {
//             logger_log(LOG_ERROR, "Invalid bytes_to_send: %d, max: %zu", bytes_to_send, sizeof(buffer));
//             break;
//         }

//         // Send all data
//         size_t total_sent = 0;
//         while (total_sent < (size_t)bytes_to_send && !send_error) {
//             PlatformErrorCode result = handle_send(context, 
//                                                  buffer + total_sent, 
//                                                  (size_t)bytes_to_send - total_sent, 
//                                                  &bytes_sent);
//             if (result != PLATFORM_ERROR_SUCCESS) {
//                 if (result == PLATFORM_ERROR_TIMEOUT) {
//                     continue;  // Try again on timeout
//                 }
//                 send_error = true;
//                 break;
//             }
//             total_sent += bytes_sent;
//         }

//         if (!send_error) {
//             sleep_ms(10);
//         }
//     }

//     if (send_error) {
//         logger_log(LOG_ERROR, "Send error occurred");
//     }
//     printf("Send thread Out of here\n");
//     fflush(stdout);
//     return NULL;
// }

void* comm_receive_thread(void* arg) {
    ThreadConfig* thread_config = (ThreadConfig*)arg;
    CommContext* context = (CommContext*)thread_config->data;
    if (!context) {
        return NULL;
    }

    logger_log(LOG_INFO, "Receive thread started");

    char buffer[4096];

    while (!comm_context_is_closed(context) && !shutdown_signalled()) {
        if (!handle_receive(context, buffer, sizeof(buffer))) {
            break;  
        }
    }

    logger_log(LOG_INFO, "Receive thread exiting");
    printf("Receive thread Out of here\n");
    fflush(stdout);
    return NULL;
}

// ThreadResult comms_thread_message_processor(ThreadConfig* config, Message_T* message) {
//     CommContext* context = (CommContext*)config->data;
//     if (!context || !message) {
//         return THREAD_ERROR;
//     }
    
//     // Check if this is a relay message
//     bool is_relay = (message->header.type == MSG_TYPE_RELAY);
    
//     if (!is_socket_writable(context->socket, context->timeout_ms)) {
//         return THREAD_ERROR;
//     }

//     // For relay messages, additional logging may be needed
//     if (is_relay && context->is_relay_enabled) {
//         logger_log(LOG_DEBUG, "Processing relay message from thread %s", 
//                   context->foreign_queue_label);
//     }

//     size_t bytes_sent = 0;
//     PlatformErrorCode err = platform_socket_send(
//         context->socket,
//         message->content,
//         message->header.content_size,
//         &bytes_sent
//     );

//     return (err == PLATFORM_ERROR_SUCCESS) ? THREAD_SUCCESS : THREAD_ERROR;
// }

void* comm_send_thread(void* arg) {
    ThreadConfig* thread_config = (ThreadConfig*)arg;
    CommContext* context = (CommContext*)thread_config->data;
    if (!context) {
        return NULL;
    }

    logger_log(LOG_INFO, "Send thread started");

    Message_T message;
    while (!comm_context_is_closed(context) && !shutdown_signalled()) {
        // Simple non-blocking message pop
        ThreadRegistryError queue_result = pop_message(thread_config->label, &message, 0);
        
        if (queue_result == THREAD_REG_QUEUE_EMPTY) {
            sleep_ms(10);
            continue;
        }
        
        if (queue_result != THREAD_REG_SUCCESS) {
            logger_log(LOG_ERROR, "Queue error in send thread");
            break;
        }

        // Send complete message with retry on partial sends
        size_t total_sent = 0;
        while (total_sent < message.header.content_size) {
            size_t bytes_sent = 0;
            PlatformErrorCode result = platform_socket_send(
                context->socket,
                message.content + total_sent,
                message.header.content_size - total_sent,
                &bytes_sent
            );

            if (result == PLATFORM_ERROR_SUCCESS) {
                total_sent += bytes_sent;
            }
            else if (result == PLATFORM_ERROR_TIMEOUT) {
                continue;  // Retry on timeout
            }
            else {
                logger_log(LOG_ERROR, "Send error occurred");
                comm_context_close(context);
                return NULL;
            }
        }
    }

    logger_log(LOG_INFO, "Send thread shutting down");
    return NULL;
}
