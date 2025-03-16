#include "comm_context.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform_error.h"
#include "platform_threads.h"  // Make sure this includes wait definitions
#include "thread_registry.h"
#include "logger.h"

#define DEFAULT_THREAD_WAIT_TIMEOUT_MS 5000

static void cleanup_threads(PlatformThreadId* thread_ids, size_t count) {
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
    size_t thread_count = 0;

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

PlatformErrorCode comm_context_create_threads(CommContext* context,
                                            ThreadConfig* send_config,
                                            ThreadConfig* receive_config) {
    if (!context || !send_config || !receive_config) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;  // Changed from INVALID_PARAMETER
    }

    // Create send thread
    ThreadResult result = app_thread_create(send_config);
    if (result != THREAD_SUCCESS) {
        return PLATFORM_ERROR_THREAD_CREATE;
    }
    context->send_thread_id = send_config->thread_id;

    // Create receive thread
    result = app_thread_create(receive_config);
    if (result != THREAD_SUCCESS) {
        cleanup_threads(&context->send_thread_id, 1);
        context->send_thread_id = 0;
        return PLATFORM_ERROR_THREAD_CREATE;
    }
    context->recv_thread_id = receive_config->thread_id;

    return PLATFORM_ERROR_SUCCESS;
}

bool comm_context_is_closed(const CommContext* context) {
    if (!context) {
        return true;
    }
    return platform_atomic_load_bool(&context->connection_closed) ;
}

void comm_context_close(CommContext* context) {
    if (!context) {
        logger_log(LOG_ERROR, "Invalid context");
        return;
    }
    platform_atomic_store_bool(&context->connection_closed, true);
}


static PlatformErrorCode handle_send(CommContext* context, char* buffer, size_t buffer_size, size_t* bytes_sent) {

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
    
    // Configuration for row display
    const int bytes_per_row = 16;      // Bytes per row
    const int bytes_per_col = 4;       // Bytes per column (32-bit words)
    const int cols_per_row = bytes_per_row / bytes_per_col;
    const int row_capacity = bytes_per_row;
    
    // Position within the current row
    static int row_position = 0;

    // Character mapping for hex values
    const char hex_chars[] = "0123456789ABCDEF";

    logger_log(LOG_INFO, "%d bytes received: top", batch_bytes);

    while (index < length) {
        // Initialize the row with placeholder dots
        char row[256] = {0};
        for (int i = 0; i < cols_per_row; i++) {
            int base = i * (bytes_per_col * 2 + 1); // 2 chars per byte + 1 space
            for (int j = 0; j < bytes_per_col * 2; j++) {
                row[base + j] = '.';
            }
            row[base + bytes_per_col * 2] = ' ';  // Space after each column
        }
        
        // Calculate how many bytes to place in this row
        int avail = row_capacity - row_position;
        int to_place = (length - index < (size_t)avail) ? (length - index) : avail;

        // Place the bytes into the row
        for (int i = 0; i < to_place; i++) {
            int pos = row_position + i;           // Position within the row
            int col = pos / bytes_per_col;        // Column index (32-bit word)
            int offset = pos % bytes_per_col;     // Offset within the word
            
            // Each byte takes 2 hexdigits in the output
            int dest_index = col * (bytes_per_col * 2 + 1) + offset * 2;
            
            uint8_t byte = buffer[index + i];
            row[dest_index] = hex_chars[byte >> 4];       // High nibble
            row[dest_index + 1] = hex_chars[byte & 0x0F]; // Low nibble
        }

        // Update the row position
        row_position += to_place;
        if (row_position >= row_capacity) {
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

    // Try to get the foreign queue by label
    MessageQueue_T* foreign_queue = get_queue_by_label(context->foreign_queue_label);
    if (!foreign_queue) {
        return true;  // Queue not available yet, ignore
    }

    // Create message structure
    Message_T message = {0};
    if (bytes_received > sizeof(message.content)) {
        logger_log(LOG_ERROR, "Message too large for relay buffer");
        return false;
    }
    
    // Copy the data into the message structure
    memcpy(message.content, buffer, bytes_received);
    message.header.content_size = bytes_received;
    message.header.type = MSG_TYPE_RELAY;

    // Push to foreign queue for relay
    if (!message_queue_push(foreign_queue, &message, DEFAULT_THREAD_WAIT_TIMEOUT_MS)) {
        logger_log(LOG_ERROR, "Failed to relay message to foreign queue");
        return false;
    }

    return true;
}

static bool handle_receive(CommContext* context, char* buffer, size_t buffer_size) {
    if (!context || !buffer) {
        return false;
    }

    // Check if socket is readable
    PlatformErrorCode result = platform_socket_wait_readable(context->socket, context->timeout_ms);
    if (result != PLATFORM_ERROR_SUCCESS) {
        if (result == PLATFORM_ERROR_TIMEOUT) {
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
        return false;
    }

    return true;
}

static int32_t populate_buffer(char* buffer, size_t buffer_size) {
    (void)buffer;
    (void)buffer_size;
    return 0;
    // if (!buffer || buffer_size == 0) {
    //     return 0;
    // }

    // // Populate the buffer with some data
    // for (size_t i = 0; i < buffer_size; i++) {
    //     buffer[i] = (char)i;
    // }

    // return (int32_t)buffer_size;
}

void* comm_send_thread(void* arg) {
    ThreadConfig* thread_config = (ThreadConfig*)arg;
    CommContext* context = (CommContext*)thread_config->data;
    if (!context) {
        return NULL;
    }

    char buffer[2048];
    bool send_error = false;

    logger_log(LOG_INFO, "Send thread started");

    while (!comm_context_is_closed(context) && !shutdown_signalled() && !send_error) {
        size_t bytes_sent = 0;
        int32_t bytes_to_send = populate_buffer(buffer, sizeof(buffer));
        
        // Validate bytes_to_send
        if (bytes_to_send < 0 || (size_t)bytes_to_send > sizeof(buffer)) {
            logger_log(LOG_ERROR, "Invalid bytes_to_send: %d, max: %zu", bytes_to_send, sizeof(buffer));
            break;
        }

        // Send all data
        size_t total_sent = 0;
        while (total_sent < (size_t)bytes_to_send && !send_error) {
            PlatformErrorCode result = handle_send(context, 
                                                 buffer + total_sent, 
                                                 (size_t)bytes_to_send - total_sent, 
                                                 &bytes_sent);
            if (result != PLATFORM_ERROR_SUCCESS) {
                if (result == PLATFORM_ERROR_TIMEOUT) {
                    continue;  // Try again on timeout
                }
                send_error = true;
                break;
            }
            total_sent += bytes_sent;
        }

        if (!send_error) {
            sleep_ms(10);
        }
    }

    if (send_error) {
        logger_log(LOG_ERROR, "Send error occurred");
    }
    printf("Send thread Out of here\n");
    fflush(stdout);
    return NULL;
}

void* comm_receive_thread(void* arg) {
    ThreadConfig* thread_config = (ThreadConfig*)arg;
    CommContext* context = (CommContext*)thread_config->data;
    if (!context) {
        return NULL;
    }

    logger_log(LOG_INFO, "Receive thread started");

    char buffer[2048];

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
