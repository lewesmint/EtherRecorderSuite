#include "comm_threads.h"

#include <string.h>
#include <stdlib.h>

#include "logger.h"
#include "platform_utils.h"
#include "thread_group.h"
#include "platform_sockets.h"
#include "platform_atomic.h"

/**
 * @brief Initialise a communication thread group
 * 
 * @param group Pointer to the communication thread group
 * @param name Group name
 * @param socket Pointer to the socket
 * @param connection_closed Pointer to the connection closed flag
 * @return bool true on success, false on failure
 */
bool comms_thread_group_init(CommsThreadGroup* group, const char* name, SOCKET* socket, volatile long* connection_closed) {
    if (!group || !name || !socket || !connection_closed) {
        return false;
    }
    
    group->socket = socket;
    group->connection_closed = connection_closed;
    
    // Initialise message queues
    group->incoming_queue = (MessageQueue_T*)malloc(sizeof(MessageQueue_T));
    group->outgoing_queue = (MessageQueue_T*)malloc(sizeof(MessageQueue_T));
    
    if (!group->incoming_queue || !group->outgoing_queue) {
        free(group->incoming_queue);
        free(group->outgoing_queue);
        return false;
    }
    
    message_queue_init(group->incoming_queue, 0);  // No size limit
    message_queue_init(group->outgoing_queue, 0);  // No size limit
    
    // Initialise base thread group
    return thread_group_init(&group->base, name);
}

// bool comms_thread_group_init(CommsThreadGroup* group, const char* name, SOCKET* socket, volatile long* connection_closed) {
//     if (!group || !name || !socket || !connection_closed) {
//         return false;
//     }
    
//     group->socket = socket;
//     group->connection_closed = connection_closed;
    
//     // Initialise message queues
//     group->incoming_queue = (MessageQueue_T*)malloc(sizeof(MessageQueue_T));
//     group->outgoing_queue = (MessageQueue_T*)malloc(sizeof(MessageQueue_T));
    
//     if (!group->incoming_queue || !group->outgoing_queue) {
//         free(group->incoming_queue);
//         free(group->outgoing_queue);
//         return false;
//     }
    
//     message_queue_init(group->incoming_queue, 0);  // No size limit
//     message_queue_init(group->outgoing_queue, 0);  // No size limit
    
//     // Initialise base thread group
//     return thread_group_init(&group->base, name);
// }

/**
 * @brief Create send and receive threads for a connection
 * 
 * @param group Pointer to the communication thread group
 * @param client_addr Pointer to the client address structure
 * @param thread_info Pointer to thread info structure
 * @return bool true on success, false on failure
 */
bool comms_thread_group_create_threads(CommsThreadGroup* group, struct sockaddr_in* client_addr, void* thread_info) {
    if (!group || !client_addr) {
        return false;
    }
    
    // Set up send thread
    AppThread_T* send_thread = (AppThread_T*)malloc(sizeof(AppThread_T));
    if (!send_thread) {
        logger_log(LOG_ERROR, "Failed to allocate memory for send thread");
        return false;
    }
    
    memset(send_thread, 0, sizeof(AppThread_T));
    
    char* send_thread_label = (char*)malloc(64);
    if (!send_thread_label) {
        free(send_thread);
        logger_log(LOG_ERROR, "Failed to allocate memory for send thread label");
        return false;
    }
    
    snprintf(send_thread_label, 64, "%s.SEND", group->base.name);
    
    send_thread->label = send_thread_label;
    send_thread->func = send_thread_function;
    send_thread->pre_create_func = pre_create_stub;
    send_thread->post_create_func = post_create_stub;
    send_thread->init_func = init_wait_for_logger;
    send_thread->exit_func = exit_stub;
    
    // Create comm args for send thread
    CommArgs_T* send_args = (CommArgs_T*)malloc(sizeof(CommArgs_T));
    if (!send_args) {
        free(send_thread_label);
        free(send_thread);
        logger_log(LOG_ERROR, "Failed to allocate memory for send thread args");
        return false;
    }
    
    memset(send_args, 0, sizeof(CommArgs_T));
    
    send_args->sock = group->socket;
    send_args->client_addr = *client_addr;
    send_args->thread_info = thread_info;
    send_args->comm_group = group;  // Set pointer to the comm group
    send_args->incoming_queue = group->incoming_queue;
    send_args->outgoing_queue = group->outgoing_queue;
    
    send_thread->data = send_args;
    
    // Set up receive thread
    AppThread_T* receive_thread = (AppThread_T*)malloc(sizeof(AppThread_T));
    if (!receive_thread) {
        free(send_args);
        free(send_thread_label);
        free(send_thread);
        logger_log(LOG_ERROR, "Failed to allocate memory for receive thread");
        return false;
    }
    
    memset(receive_thread, 0, sizeof(AppThread_T));
    
    char* receive_thread_label = (char*)malloc(64);
    if (!receive_thread_label) {
        free(send_args);
        free(send_thread_label);
        free(send_thread);
        free(receive_thread);
        logger_log(LOG_ERROR, "Failed to allocate memory for receive thread label");
        return false;
    }
    
    snprintf(receive_thread_label, 64, "%s.RECEIVE", group->base.name);
    
    receive_thread->label = receive_thread_label;
    receive_thread->func = receive_thread_function;
    receive_thread->pre_create_func = pre_create_stub;
    receive_thread->post_create_func = post_create_stub;
    receive_thread->init_func = init_wait_for_logger;
    receive_thread->exit_func = exit_stub;
    
    // Create comm args for receive thread
    CommArgs_T* receive_args = (CommArgs_T*)malloc(sizeof(CommArgs_T));
    if (!receive_args) {
        free(send_args);
        free(send_thread_label);
        free(send_thread);
        free(receive_thread_label);
        free(receive_thread);
        logger_log(LOG_ERROR, "Failed to allocate memory for receive thread args");
        return false;
    }
    
    memset(receive_args, 0, sizeof(CommArgs_T));
    
    receive_args->sock = group->socket;
    receive_args->client_addr = *client_addr;
    receive_args->thread_info = thread_info;
    receive_args->comm_group = group;  // Set pointer to the comm group
    receive_args->incoming_queue = group->incoming_queue;
    receive_args->outgoing_queue = group->outgoing_queue;
    
    receive_thread->data = receive_args;
    
    // Add threads to the group
    if (!thread_group_add(&group->base, send_thread)) {
        free(receive_args);
        free(receive_thread_label);
        free(receive_thread);
        free(send_args);
        free(send_thread_label);
        free(send_thread);
        logger_log(LOG_ERROR, "Failed to add send thread to group");
        return false;
    }
    
    if (!thread_group_add(&group->base, receive_thread)) {
        // Remove the already added send thread
        thread_group_remove(&group->base, send_thread->label);
        
        // Free all resources
        free(receive_args);
        free(receive_thread_label);
        free(receive_thread);
        free(send_args);
        free(send_thread_label);
        free(send_thread);
        
        logger_log(LOG_ERROR, "Failed to add receive thread to group");
        return false;
    }
    
    return true;
}


// bool comms_thread_group_create_threads(CommsThreadGroup* group, struct sockaddr_in* client_addr, void* thread_info) {
//     if (!group || !client_addr) {
//         return false;
//     }
    
//     // Set up send thread
//     AppThread_T* send_thread = (AppThread_T*)malloc(sizeof(AppThread_T));
//     if (!send_thread) {
//         logger_log(LOG_ERROR, "Failed to allocate memory for send thread");
//         return false;
//     }
    
//     memset(send_thread, 0, sizeof(AppThread_T));
    
//     char* send_thread_label = (char*)malloc(64);
//     if (!send_thread_label) {
//         free(send_thread);
//         logger_log(LOG_ERROR, "Failed to allocate memory for send thread label");
//         return false;
//     }
    
//     snprintf(send_thread_label, 64, "%s.SEND", group->base.name);
    
//     send_thread->label = send_thread_label;
//     send_thread->func = send_thread_function;
//     send_thread->pre_create_func = pre_create_stub;
//     send_thread->post_create_func = post_create_stub;
//     send_thread->init_func = init_wait_for_logger;
//     send_thread->exit_func = exit_stub;
    
//     // Create comm args for send thread
//     CommArgs_T* send_args = (CommArgs_T*)malloc(sizeof(CommArgs_T));
//     if (!send_args) {
//         free(send_thread_label);
//         free(send_thread);
//         logger_log(LOG_ERROR, "Failed to allocate memory for send thread args");
//         return false;
//     }
    
//     memset(send_args, 0, sizeof(CommArgs_T));
    
//     send_args->sock = group->socket;
//     send_args->client_addr = *client_addr;
//     send_args->thread_info = thread_info;
//     send_args->connection_closed = group->connection_closed;
//     send_args->incoming_queue = group->incoming_queue;
//     send_args->outgoing_queue = group->outgoing_queue;
    
//     send_thread->data = send_args;
    
//     // Set up receive thread
//     AppThread_T* receive_thread = (AppThread_T*)malloc(sizeof(AppThread_T));
//     if (!receive_thread) {
//         free(send_args);
//         free(send_thread_label);
//         free(send_thread);
//         logger_log(LOG_ERROR, "Failed to allocate memory for receive thread");
//         return false;
//     }
    
//     memset(receive_thread, 0, sizeof(AppThread_T));
    
//     char* receive_thread_label = (char*)malloc(64);
//     if (!receive_thread_label) {
//         free(send_args);
//         free(send_thread_label);
//         free(send_thread);
//         free(receive_thread);
//         logger_log(LOG_ERROR, "Failed to allocate memory for receive thread label");
//         return false;
//     }
    
//     snprintf(receive_thread_label, 64, "%s.RECEIVE", group->base.name);
    
//     receive_thread->label = receive_thread_label;
//     receive_thread->func = receive_thread_function;
//     receive_thread->pre_create_func = pre_create_stub;
//     receive_thread->post_create_func = post_create_stub;
//     receive_thread->init_func = init_wait_for_logger;
//     receive_thread->exit_func = exit_stub;
    
//     // Create comm args for receive thread
//     CommArgs_T* receive_args = (CommArgs_T*)malloc(sizeof(CommArgs_T));
//     if (!receive_args) {
//         free(send_args);
//         free(send_thread_label);
//         free(send_thread);
//         free(receive_thread_label);
//         free(receive_thread);
//         logger_log(LOG_ERROR, "Failed to allocate memory for receive thread args");
//         return false;
//     }
    
//     memset(receive_args, 0, sizeof(CommArgs_T));
    
//     receive_args->sock = group->socket;
//     receive_args->client_addr = *client_addr;
//     receive_args->thread_info = thread_info;
//     receive_args->connection_closed = group->connection_closed;
//     receive_args->incoming_queue = group->incoming_queue;
//     receive_args->outgoing_queue = group->outgoing_queue;
    
//     receive_thread->data = receive_args;
    
//     // Add threads to the group
//     if (!thread_group_add(&group->base, send_thread)) {
//         free(receive_args);
//         free(receive_thread_label);
//         free(receive_thread);
//         free(send_args);
//         free(send_thread_label);
//         free(send_thread);
//         logger_log(LOG_ERROR, "Failed to add send thread to group");
//         return false;
//     }
    
//     if (!thread_group_add(&group->base, receive_thread)) {
//         // Remove the already added send thread
//         thread_group_remove(&group->base, send_thread->label);
        
//         // Free all resources
//         free(receive_args);
//         free(receive_thread_label);
//         free(receive_thread);
//         free(send_args);
//         free(send_thread_label);
//         free(send_thread);
        
//         logger_log(LOG_ERROR, "Failed to add receive thread to group");
//         return false;
//     }
    
//     return true;
// }

bool comms_thread_group_is_closed(CommsThreadGroup* group) {
    if (!group || !group->connection_closed) {
        return true;
    }
    
    return platform_atomic_compare_exchange(group->connection_closed, 0, 0) != 0;
}

void comms_thread_group_close(CommsThreadGroup* group) {
    if (!group || !group->connection_closed) {
        return;
    }
    
    platform_atomic_exchange(group->connection_closed, 1);
    logger_log(LOG_DEBUG, "Communication group '%s' marked as closed", group->base.name);
}

bool comms_thread_group_wait(CommsThreadGroup* group, uint32_t timeout_ms) {
    if (!group) {
        return false;
    }
    
    return thread_group_wait_all(&group->base, timeout_ms);
}

void comms_thread_group_cleanup(CommsThreadGroup* group) {
    if (!group) {
        return;
    }
    
    // Clean up base thread group first
    thread_group_cleanup(&group->base);
    
    // Clean up message queues
    if (group->incoming_queue) {
        message_queue_destroy(group->incoming_queue);
        free(group->incoming_queue);
        group->incoming_queue = NULL;
    }
    
    if (group->outgoing_queue) {
        message_queue_destroy(group->outgoing_queue);
        free(group->outgoing_queue);
        group->outgoing_queue = NULL;
    }
    
    logger_log(LOG_DEBUG, "Communication thread group cleaned up");
}

/**
 * @brief Set the connection closed flag atomically
 * 
 * @param flag Pointer to the connection closed flag
 */
static void mark_connection_closed(volatile long* flag) {
    if (flag) {
        platform_atomic_exchange(flag, 1);
    }
}

/**
 * @brief Check if connection is already marked as closed
 * 
 * @param flag Pointer to the connection closed flag
 * @return true if connection is closed, false otherwise
 */
static bool is_connection_closed(volatile long* flag) {
    if (!flag) {
        return false;
    }
    return platform_atomic_compare_exchange(flag, 0, 0) != 0;
}


/**
 * @brief Formats and logs received data in organized rows
 * 
 * Each byte is displayed as two hex characters, and bytes are organized in rows
 * with a specified number of columns per row.
 * 
 * @param buffer The buffer containing the data to log
 * @param buffered_length Pointer to the current buffer length
 * @param batch_bytes The number of bytes in the current batch
 */
static void log_buffered_data(uint8_t* buffer, int* buffered_length, int batch_bytes) {
    int total = *buffered_length;
    int index = 0;
    
    // Configuration for row display
    int bytes_per_row = 16;      // Bytes per row
    int bytes_per_col = 4;       // Bytes per column
    int cols_per_row = bytes_per_row / bytes_per_col;
    int row_capacity = bytes_per_row;
    
    // Position within the current row
    static int row_position = 0;

    // Character mapping for hex values
    const char hex_chars[] = "0123456789ABCDEF";

    logger_log(LOG_INFO, "%d bytes received: top", batch_bytes);

    // Process the data in row-sized chunks
    while (index < total) {
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
        int to_place = (total - index < avail) ? (total - index) : avail;

        // Place the bytes into the row
        for (int i = 0; i < to_place; i++) {
            int pos = row_position + i;           // Position within the row
            int col = pos / bytes_per_col;        // Column index
            int offset = pos % bytes_per_col;     // Offset within the column
            
            // Each byte takes 2 characters in the output
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

    *buffered_length = 0;  // Reset buffer length after logging
    logger_log(LOG_INFO, "%d bytes received: bottom", batch_bytes);
}

/**
 * @brief Sets up select parameters for socket operations
 * 
 * @param sock Socket
 * @param read_fds Pointer to read fd_set
 * @param timeout Pointer to timeout structure
 * @param sec Seconds for timeout
 * @param usec Microseconds for timeout
 */
static void setup_select_timeout(SOCKET sock, fd_set* read_fds, struct timeval* timeout, long sec, long usec) {
    FD_ZERO(read_fds);
    FD_SET(sock, read_fds);
    timeout->tv_sec = sec;
    timeout->tv_usec = usec;
}

/**
 * @brief Handles data reception from socket with buffering
 * 
 * @param sock Socket to receive from
 * @param buffer Buffer to store received data
 * @param buffered_length Pointer to current buffer length
 * @param batch_bytes Pointer to current batch size
 * @return true if successful, false if error or connection closed
 */
static bool handle_data_reception(SOCKET sock, char* buffer, int* buffered_length, int* batch_bytes) {
    int buffer_available = COMMS_BUFFER_SIZE - *buffered_length;
    if (buffer_available <= 0) {
        logger_log(LOG_ERROR, "Buffer full, cannot receive more data");
        return false;
    }
    
    char error_buffer[SOCKET_ERROR_BUFFER_SIZE];
    get_socket_error_message(error_buffer, sizeof(error_buffer));
    int bytes = recv(sock, buffer + *buffered_length, buffer_available, 0);

    if (bytes <= 0) {
        get_socket_error_message(error_buffer, sizeof(error_buffer));
        logger_log(LOG_ERROR, "recv error or connection closed (received %d bytes): %s", 
                  bytes, error_buffer);
        return false;
    }
    
    *buffered_length += bytes;
    *batch_bytes += bytes;
    logger_log(LOG_DEBUG, "Received %d bytes", bytes);
    
    return true;
}

/**
 * @brief Thread function for sending data to a socket
 * 
 * This function handles sending data to a socket. It runs in a dedicated
 * thread and continues until the connection is closed or an error occurs.
 * 
 * The implementation uses select() to check for send availability before
 * writing data to the socket.
 */
void* send_thread_function(void* arg) {
    AppThread_T* thread_info = (AppThread_T*)arg;
    set_thread_label(thread_info->label);
    
    CommArgs_T* comm_args = (CommArgs_T*)thread_info->data;
    if (!comm_args || !comm_args->sock || !(*comm_args->sock) || *comm_args->sock == INVALID_SOCKET) {
        logger_log(LOG_ERROR, "Invalid communication arguments in send thread");
        return NULL;
    }
    
    SOCKET sock = *comm_args->sock;
    CommsThreadArgs_T* comms_info = (CommsThreadArgs_T*)comm_args->thread_info;
    bool is_tcp = comms_info ? comms_info->is_tcp : true;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    
    // Get the comm group for status checking
    CommsThreadGroup* comm_group = comm_args->comm_group;
    if (!comm_group) {
        logger_log(LOG_ERROR, "Missing communication group in send thread");
        return NULL;
    }
    
    logger_log(LOG_INFO, "Started send thread for %s connection", is_tcp ? "TCP" : "UDP");
    
    // Continue sending until shutdown or connection closed
    while (!shutdown_signalled() && !comms_thread_group_is_closed(comm_group)) {
        // Check if socket is still valid
        if (*comm_args->sock == INVALID_SOCKET) {
            logger_log(LOG_ERROR, "Socket became invalid in send thread");
            comms_thread_group_close(comm_group);
            break;
        }
        
        // Check if we have automatic test data to send
        bool should_send_test_data = comms_info && comms_info->send_test_data && 
                                    comms_info->data && comms_info->data_size > 0;
        
        // Check if we have a message to send from the queue
        Message_T message;
        memset(&message, 0, sizeof(Message_T)); // Initialize explicitly

        bool have_message = comm_args->incoming_queue && 
                           message_queue_pop(comm_args->incoming_queue, &message, 
                                           should_send_test_data ? 0 : 100);
        
        // If no message from queue but we should send test data, prepare test data message
        if (!have_message && should_send_test_data) {
            message.header.type = MSG_TYPE_DATA;
            message.header.content_size = comms_info->data_size;
            message.header.id = platform_random(); // Unique message ID
            message.header.flags = 0;
            
            // Ensure we don't overflow the message content
            size_t copy_size = MIN(comms_info->data_size, sizeof(message.content));
            memcpy(message.content, comms_info->data, copy_size);
            
            have_message = true;
        }
        
        // If we have a message to send, use select to check if we can send
        if (have_message) {
            fd_set write_fds;
            struct timeval timeout;
            
            FD_ZERO(&write_fds);
            FD_SET(sock, &write_fds);
            timeout.tv_sec = BLOCKING_TIMEOUT_SEC;
            timeout.tv_usec = 0;
            
            // On Windows, first parameter is ignored; on POSIX it should be max fd + 1
            int nfds = (int)(sock + 1);
            int select_result = select(nfds, NULL, &write_fds, NULL, &timeout);
            
            if (select_result > 0 && FD_ISSET(sock, &write_fds)) {
                // Socket is ready for writing, send the message
                logger_log(LOG_INFO, "Sending %u bytes of data", message.header.content_size);
                
                int bytes_sent;
                if (is_tcp) {
                    bytes_sent = send(sock, message.content, message.header.content_size, 0);
                } else {
                    bytes_sent = sendto(sock, message.content, message.header.content_size, 0,
                                     (struct sockaddr*)&comm_args->client_addr, addr_len);
                }
                
                if (bytes_sent > 0) {
                    // Successfully sent data
                    logger_log(LOG_DEBUG, "Sent %d bytes of data", bytes_sent);
                    
                    // If this was test data, apply the send interval delay
                    if (should_send_test_data && comms_info->send_interval_ms > 0) {
                        sleep_ms(comms_info->send_interval_ms);
                    }
                } else {
                    // Socket error
                    char error_buffer[SOCKET_ERROR_BUFFER_SIZE];
                    get_socket_error_message(error_buffer, sizeof(error_buffer));
                    logger_log(LOG_ERROR, "Error sending data: %s", error_buffer);
                    comms_thread_group_close(comm_group);
                    break;
                }
            } else if (select_result == 0) {
                // Timeout - socket not ready for writing
                logger_log(LOG_DEBUG, "Socket not ready for writing within timeout period");
            } else {
                // Select error
                char error_buffer[SOCKET_ERROR_BUFFER_SIZE];
                get_socket_error_message(error_buffer, sizeof(error_buffer));
                logger_log(LOG_ERROR, "Select error: %s", error_buffer);
                
                if (shutdown_signalled()) {
                    break;
                }
                
                // Brief pause before retrying
                sleep_ms(100);
            }
        } else {
            // No message to send, sleep briefly to avoid CPU spinning
            sleep_ms(10);
        }
    }
    
    logger_log(LOG_INFO, "Exiting send thread");
    return NULL;
}

/**
 * @brief Thread function for receiving data from a socket
 * 
 * Incorporates row-based data logging like the original implementation.
 */
void* receive_thread_function(void* arg) {
    AppThread_T* thread_info = (AppThread_T*)arg;
    set_thread_label(thread_info->label);
    
    CommArgs_T* comm_args = (CommArgs_T*)thread_info->data;
    if (!comm_args || !comm_args->sock || !(*comm_args->sock) || *comm_args->sock == INVALID_SOCKET) {
        logger_log(LOG_ERROR, "Invalid communication arguments in receive thread");
        return NULL;
    }
    
    SOCKET sock = *comm_args->sock;
    CommsThreadArgs_T* comms_info = (CommsThreadArgs_T*)comm_args->thread_info;
    bool is_tcp = comms_info ? comms_info->is_tcp : true;
    
    // Get the comm group for status checking
    CommsThreadGroup* comm_group = comm_args->comm_group;
    if (!comm_group) {
        logger_log(LOG_ERROR, "Missing communication group in receive thread");
        return NULL;
    }
    
    logger_log(LOG_INFO, "Started receive thread for %s connection", is_tcp ? "TCP" : "UDP");
    
    // Buffer for receiving data
    char buffer[COMMS_BUFFER_SIZE];
    int buffered_length = 0;
    int batch_bytes = 0;
    
    // Continue receiving until shutdown or connection closed
    while (!shutdown_signalled() && !comms_thread_group_is_closed(comm_group)) {
        // Check if socket is still valid
        if (*comm_args->sock == INVALID_SOCKET) {
            logger_log(LOG_ERROR, "Socket became invalid in receive thread");
            comms_thread_group_close(comm_group);
            break;
        }
        
        // Use select to check for available data
        fd_set read_fds;
        struct timeval timeout;
        
        setup_select_timeout(sock, &read_fds, &timeout, BLOCKING_TIMEOUT_SEC, 0);
        
        // On Windows, first parameter is ignored; on POSIX it should be max fd + 1
        int nfds = (int)(sock + 1);
        int select_result = select(nfds, &read_fds, NULL, NULL, &timeout);
        
        if (select_result > 0 && FD_ISSET(sock, &read_fds)) {
            // Data is available, receive it
            if (!handle_data_reception(sock, buffer, &buffered_length, &batch_bytes)) {
                logger_log(LOG_ERROR, "Connection closed or error receiving data");
                comms_thread_group_close(comm_group);
                break;
            }
            
            // If we've received data, log it and create a message
            if (buffered_length > 0) {
                log_buffered_data((uint8_t*)buffer, &buffered_length, batch_bytes);
                
                // Create a message for the received data
                Message_T message;
                memset(&message, 0, sizeof(Message_T));
                
                // Populate message header
                message.header.type = MSG_TYPE_RELAY;
                message.header.content_size = batch_bytes;
                message.header.id = platform_random(); // Unique message ID
                message.header.flags = is_tcp ? 1 : 0; // Flag to indicate TCP/UDP
                
                // Ensure we don't overflow the message content
                size_t copy_size = MIN((size_t)batch_bytes, sizeof(message.content));
                
                // Copy received data into message content
                memcpy(message.content, buffer, copy_size);
                
                // If relay is enabled and outgoing queue exists, push the message
                if (comm_args->outgoing_queue) {
                    if (!message_queue_push(comm_args->outgoing_queue, &message, PLATFORM_WAIT_INFINITE)) {
                        logger_log(LOG_WARN, "Failed to push received message to queue");
                    }
                }
                
                // Reset batch counter
                batch_bytes = 0;
            }
        } else if (select_result == 0) {
            // Timeout - no data available
            logger_log(LOG_DEBUG, "No data received within timeout period");
        } else {
            // Select error
            char error_buffer[SOCKET_ERROR_BUFFER_SIZE];
            get_socket_error_message(error_buffer, sizeof(error_buffer));
            logger_log(LOG_ERROR, "Select error: %s", error_buffer);
            
            if (shutdown_signalled()) {
                break;
            }
            
            // Brief pause before retrying
            sleep_ms(100);
        }
    }
    
    logger_log(LOG_INFO, "Exiting receive thread");
    return NULL;
}

// /**
//  * @brief Thread function for receiving data from a socket
//  */
// void* receive_thread_function(void* arg) {
//     AppThread_T* thread_info = (AppThread_T*)arg;
//     set_thread_label(thread_info->label);
    
//     CommArgs_T* comm_args = (CommArgs_T*)thread_info->data;
//     if (!comm_args || !comm_args->sock || !(*comm_args->sock) || *comm_args->sock == INVALID_SOCKET) {
//         logger_log(LOG_ERROR, "Invalid communication arguments in receive thread");
//         return NULL;
//     }
    
//     SOCKET sock = *comm_args->sock;
//     CommsThreadArgs_T* comms_info = (CommsThreadArgs_T*)comm_args->thread_info;
//     bool is_tcp = comms_info ? comms_info->is_tcp : true;
//     socklen_t addr_len = sizeof(struct sockaddr_in);
    
//     logger_log(LOG_INFO, "Started receive thread for %s connection", is_tcp ? "TCP" : "UDP");
    
//     // Buffer for receiving data
//     char buffer[COMMS_BUFFER_SIZE];
    
//     // Continue receiving until shutdown or connection closed
//     while (!shutdown_signalled() && !is_connection_closed(comm_args->connection_closed)) {
//         // Check if socket is still valid
//         if (*comm_args->sock == INVALID_SOCKET) {
//             logger_log(LOG_ERROR, "Socket became invalid in receive thread");
//             mark_connection_closed(comm_args->connection_closed);
//             break;
//         }
        
//         // Receive data with proper error handling
//         int bytes_received;
//         if (is_tcp) {
//             bytes_received = recv(sock, buffer, sizeof(buffer), 0);
//         } else {
//             bytes_received = recvfrom(sock, buffer, sizeof(buffer), 0, 
//                                     (struct sockaddr*)&comm_args->client_addr, &addr_len);
//         }
        
//         if (bytes_received > 0) {
//             // Successfully received data
//             logger_log(LOG_DEBUG, "Received %d bytes of data", bytes_received);
            
//             // Create a message for the received data
//             Message_T message;
//             memset(&message, 0, sizeof(Message_T));
            
//             // Populate message header
//             message.header.type = MSG_TYPE_RELAY;
//             message.header.content_size = bytes_received;
//             message.header.id = platform_random(); // Unique message ID
//             message.header.flags = is_tcp ? 1 : 0; // Flag to indicate TCP/UDP
            
//             // Ensure we don't overflow the message content
//             size_t copy_size = (bytes_received > 0)
//                 ? MIN((size_t)bytes_received, sizeof(message.content))
//                 : 0;
            
//             // Copy received data into message content
//             memcpy(message.content, buffer, copy_size);
            
//             // If relay is enabled and outgoing queue exists, push the message
//             if (comm_args->outgoing_queue) {
//                 if (!message_queue_push(comm_args->outgoing_queue, &message, PLATFORM_WAIT_INFINITE)) {
//                     logger_log(LOG_WARN, "Failed to push received message to queue");
//                 }
//             }
//         } else if (bytes_received == 0) {
//             // Connection closed by peer
//             logger_log(LOG_INFO, "Connection closed by peer");
//             mark_connection_closed(comm_args->connection_closed);
//             break;
//         } else {
//             // Socket error
//             char error_buffer[SOCKET_ERROR_BUFFER_SIZE];
//             get_socket_error_message(error_buffer, sizeof(error_buffer));
//             logger_log(LOG_ERROR, "Error receiving data: %s", error_buffer);
//             mark_connection_closed(comm_args->connection_closed);
//             break;
//         }
        
//         // Short sleep to prevent CPU spinning
//         sleep_ms(1);
//     }
    
//     logger_log(LOG_INFO, "Exiting receive thread");
//     return NULL;
// }

// /**
//  * @brief Thread function for sending data to a socket
//  */
// void* send_thread_function(void* arg) {
//     AppThread_T* thread_info = (AppThread_T*)arg;
//     set_thread_label(thread_info->label);
    
//     CommArgs_T* comm_args = (CommArgs_T*)thread_info->data;
//     if (!comm_args || !comm_args->sock || !(*comm_args->sock) || *comm_args->sock == INVALID_SOCKET) {
//         logger_log(LOG_ERROR, "Invalid communication arguments in send thread");
//         return NULL;
//     }
    
//     SOCKET sock = *comm_args->sock;
//     CommsThreadArgs_T* comms_info = (CommsThreadArgs_T*)comm_args->thread_info;
//     bool is_tcp = comms_info ? comms_info->is_tcp : true;
//     socklen_t addr_len = sizeof(struct sockaddr_in);
    
//     logger_log(LOG_INFO, "Started send thread for %s connection", is_tcp ? "TCP" : "UDP");
    
//     // Continue sending until shutdown or connection closed
//     while (!shutdown_signalled() && !is_connection_closed(comm_args->connection_closed)) {
//         // Check if socket is still valid
//         if (*comm_args->sock == INVALID_SOCKET) {
//             logger_log(LOG_ERROR, "Socket became invalid in send thread");
//             mark_connection_closed(comm_args->connection_closed);
//             break;
//         }
        
//         // Try to get a message from the incoming queue
//         Message_T message;
//         if (comm_args->incoming_queue && 
//             message_queue_pop(comm_args->incoming_queue, &message, 100)) {
//             // Send the message over the socket
//             int bytes_sent;
//             if (is_tcp) {
//                 bytes_sent = send(sock, message.content, message.header.content_size, 0);
//             } else {
//                 bytes_sent = sendto(sock, message.content, message.header.content_size, 0,
//                                   (struct sockaddr*)&comm_args->client_addr, addr_len);
//             }
            
//             if (bytes_sent > 0) {
//                 // Successfully sent data
//                 logger_log(LOG_DEBUG, "Sent %d bytes of relayed data", bytes_sent);
//             } else {
//                 // Socket error
//                 char error_buffer[SOCKET_ERROR_BUFFER_SIZE];
//                 get_socket_error_message(error_buffer, sizeof(error_buffer));
//                 logger_log(LOG_ERROR, "Error sending relayed data: %s", error_buffer);
//                 mark_connection_closed(comm_args->connection_closed);
//                 break;
//             }
//         }
        
//         // Small sleep to prevent tight spinning
//         sleep_ms(10);
//     }
    
//     logger_log(LOG_INFO, "Exiting send thread");
//     return NULL;
// }

/**
 * @brief Initialise relay between communication endpoints
 */
bool init_relay(
    CommArgs_T* client_args,
    CommArgs_T* server_args,
    MessageQueue_T* client_to_server_queue,
    MessageQueue_T* server_to_client_queue
) {
    if (!client_args || !server_args || 
        !client_to_server_queue || !server_to_client_queue) {
        logger_log(LOG_ERROR, "Invalid arguments for relay initialization");
        return false;
    }

    // Set up queues for client
    client_args->incoming_queue = server_to_client_queue;
    client_args->outgoing_queue = client_to_server_queue;

    // Set up queues for server
    server_args->incoming_queue = client_to_server_queue;
    server_args->outgoing_queue = server_to_client_queue;

    return true;
}

/**
 * Checks if the communication threads show signs of activity or if the connection is alive.
 * 
 * @param group Pointer to the communication thread group
 * @return true if the connection shows signs of being active/healthy, false otherwise
 */
bool comms_thread_has_activity(CommsThreadGroup* group) {
    if (!group || !group->socket || *group->socket == INVALID_SOCKET) {
        return false;
    }
    
    // Check socket health using the platform-specific function
    return is_socket_healthy(*group->socket);
    
    // Possible future enhancement: also check message activity counters
    // if you want to distinguish between idle vs failed connections
}
