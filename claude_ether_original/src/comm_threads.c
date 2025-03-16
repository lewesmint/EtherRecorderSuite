
#include "comm_threads.h"

#include <stdlib.h>
#include <string.h>

#include "platform_atomic.h"
#include "platform_sockets.h"
#include "platform_defs.h"

#include "thread_registry.h"
#include "message_queue.h"
#include "logger.h"
#include "app_config.h"

// External declarations
extern bool shutdown_signalled(void);
extern void sleep_ms(uint32_t milliseconds);
extern void set_thread_label(const char* label);
extern MessageQueue_T* message_queue_create_for_thread(ThreadHandle_T thread);

/**
 * @brief Sets up timeout and fd_set structures for select() call
 * 
 * @param sock Socket to monitor
 * @param fds Pointer to fd_set structure to initialize
 * @param timeout Pointer to timeval structure to initialize
 * @param sec Seconds for timeout
 * @param usec Microseconds for timeout
 */
static void setup_select_timeout(SOCKET sock, fd_set* fds, struct timeval* timeout, long sec, long usec) {
    FD_ZERO(fds);
    FD_SET(sock, fds);
    timeout->tv_sec = sec;
    timeout->tv_usec = usec;
}

/**
 * @brief Checks if socket is ready for writing
 * @param sock The socket to check
 * @param timeout_sec Timeout in seconds
 * @return true if socket is writable, false otherwise
 */
static bool is_socket_writable(SOCKET* sock, int timeout_sec) {
    fd_set writefds;
    struct timeval tv;
    FD_ZERO(&writefds);
    FD_SET(*sock, &writefds);
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;
    return select((int)(*sock + 1), NULL, &writefds, NULL, &tv) > 0;
}

/**
 * @brief Processes received data and queues it for handling
 * @param comm_args Communication arguments containing the message queue
 * @param buffer Buffer containing received data
 * @param bytes Number of bytes received
 * @return true if processing successful, false otherwise
 */
static bool process_received_data(CommContext* context, const uint8_t* buffer, size_t bytes) {
    if (!context->is_relay_enabled || !context->foreign_queue) {
        logger_log(LOG_ERROR, "Relay not enabled or queue not initialized");
        return false;
    }
    

    if (!buffer || bytes == 0) {
        logger_log(LOG_ERROR, "Invalid buffer or no data received");
        return false;
    }

    Message_T message = {0};

    if (bytes > sizeof(message.content)) {
        logger_log(LOG_ERROR, "Received message too large for buffer (%zu > %zu)", 
                  bytes, sizeof(message.content));
        return false;
    }

    memcpy(message.content, buffer, bytes);
    message.header.content_size = (uint32_t)bytes;
    message.header.type = MSG_TYPE_RELAY;

    if (!message_queue_push(context->foreign_queue, &message, 100)) {
        logger_log(LOG_ERROR, "Failed to queue received message (queue full or timeout)");
        return false;
    }
    
    return true;
}

/**
 * @brief Handles sending a single message over the network
 * @param sock The socket to send on
 * @param message The message to send
 * @param is_tcp Whether this is a TCP connection
 * @param client_addr Client address for UDP
 * @param addr_len Length of client address
 * @return true if send successful, false otherwise
 */
static bool send_single_message(SOCKET* sock, const Message_T* message, bool is_tcp,
                              struct sockaddr_in* client_addr, socklen_t addr_len) {
    int bytes_sent;
    if (is_tcp) {
        bytes_sent = send(*sock, message->content, message->header.content_size, 0);
    } else {
        bytes_sent = sendto(*sock, message->content, message->header.content_size, 0,
                          (struct sockaddr*)client_addr, addr_len);
    }

    if (bytes_sent <= 0) {
        char error_buffer[SOCKET_ERROR_BUFFER_SIZE];
        get_socket_error_message(error_buffer, sizeof(error_buffer));
        logger_log(LOG_ERROR, "Send error: %s", error_buffer);
        return false;
    }
    
    logger_log(LOG_DEBUG, "Sent %d bytes", bytes_sent);
    return true;
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
 * @brief Receives data from the socket
 * @param sock The socket to receive from
 * @param buffer Buffer to store received data
 * @param buffer_size Size of the buffer
 * @param is_tcp Whether this is a TCP connection
 * @param client_addr Client address for UDP
 * @param addr_len Length of client address
 * @return Number of bytes received, or -1 on error
 */
static int receive_data(SOCKET sock, uint8_t* buffer, size_t buffer_size, bool is_tcp,
    struct sockaddr_in* client_addr) {
    socklen_t addr_len = sizeof(struct sockaddr_in);
    if (is_tcp) {
        return recv(sock, (char*)buffer, buffer_size, 0);
    } else {
        return recvfrom(sock, (char*)buffer, buffer_size, 0,
        (struct sockaddr*)client_addr, &addr_len);
    }
}

/**
 * @brief Handles received data processing
 * @param sock Socket to receive from
 * @param comm_args Communication arguments
 * @param comms_info Thread-specific information
 * @param buffer Receive buffer
 * @param buffered_length Current buffer length
 * @param batch_bytes Batch bytes counter
 * @return true if successful, false if should exit
 */
static bool handle_received_data(SOCKET sock, CommContext* context,
                               uint8_t* buffer, int* buffered_length, int* batch_bytes) {
    // *batch_bytes = platform_socket_receive(sock, 
    //                                      buffer + *buffered_length,
    //                                      COMMS_BUFFER_SIZE - *buffered_length);
    *batch_bytes = receive_data(sock, buffer + *buffered_length, 
        COMMS_BUFFER_SIZE - *buffered_length,
        context->is_tcp, &context->client_addr);
    // *batch_bytes = recv(sock, buffer + *buffered_length, COMMS_BUFFER_SIZE - *buffered_length, 0);

    if (*batch_bytes == 0) {
        logger_log(LOG_INFO, "Connection closed by peer");
        // comm_context_close(context);
        return false;
    } else if (*batch_bytes < 0) {
        char error_buffer[SOCKET_ERROR_BUFFER_SIZE];
        get_socket_error_message(error_buffer, sizeof(error_buffer));
        logger_log(LOG_ERROR, "Receive error: %s", error_buffer);
        // comm_context_close(context);
        return false;
    }

    // MessageQueue_T* queue = context->queue;
    
    *buffered_length += *batch_bytes;
    log_buffered_data(buffer, buffered_length, *batch_bytes);
    
    if (!process_received_data(context, buffer, *batch_bytes)) {
        // comm_context_close(context);
        return false;
    }
    
    *buffered_length = 0;
    return true;
}

/**
 * @brief Validates communication context for a thread
 * @param thread_info Thread information structure
 * @return CommContext* pointer if valid, NULL if invalid
 */
static CommContext* validate_context(AppThread_T* thread_info) {
    CommContext* context = (CommContext*)thread_info->data;
    if (!context || !context->socket || !(*context->socket) || *context->socket == INVALID_SOCKET) {
        logger_log(LOG_ERROR, "Invalid communication arguments");
        return NULL;
    }
    return context;
}

/**
 * @brief Checks if socket is ready for reading
 * @param sock The socket to check
 * @param timeout_sec Timeout in seconds
 * @return true if socket is readable, false otherwise
 */
static bool is_socket_readable(SOCKET sock, int timeout_sec) {
    fd_set read_fds;
    struct timeval timeout;
    setup_select_timeout(sock, &read_fds, &timeout, timeout_sec, 0);
    
    int select_result = select((int)sock + 1, &read_fds, NULL, NULL, &timeout);
    return (select_result > 0 && FD_ISSET(sock, &read_fds));
}

void comm_context_destroy(CommContext* context) {
    if (context) {
        free(context);
    }
}

bool comm_context_is_closed(CommContext* context) {
    return context ? (context->connection_closed != 0) : true;
}

void comm_context_close(CommContext* context) {
    if (context) {
        platform_atomic_exchange(&context->connection_closed, 1);
    }
}

/**
 * @brief Thread function for receiving data from a socket
 * @param arg Pointer to thread arguments (AppThread_T structure)
 * @return NULL
 */
void* receive_thread_function(void* arg) {
    AppThread_T* thread_info = (AppThread_T*)arg;
    //set_thread_label(thread_info->label);
    
    // from the data field
    CommContext* context = validate_context(thread_info);
    if (!context) {
        return NULL;
    }

    // // get thtreds message queue
    // ThreadQueue_T* thread_queue = message_queue_find_by_thread(thread_info->thread_id);
    
    SOCKET sock = *context->socket;
    // // CommsThreadArgs_T* comms_info = (CommsThreadArgs_T*)context->???;
    // bool is_tcp = context->is_tcp : true;
    
    logger_log(LOG_INFO, "Started receive thread for %s connection", context->is_tcp ? "TCP" : "UDP");
    
    uint8_t buffer[COMMS_BUFFER_SIZE];
    int buffered_length = 0;
    int batch_bytes = 0;
    
    while (!shutdown_signalled() && !comm_context_is_closed(context)) {
        if (*context->socket == INVALID_SOCKET) {
            logger_log(LOG_ERROR, "Socket became invalid in receive thread");
            comm_context_close(context);
            break;
        }
        
        if (!is_socket_readable(sock, BLOCKING_TIMEOUT_SEC)) {
            continue;
        }
        
        if (!handle_received_data(sock, context, buffer, &buffered_length, &batch_bytes)) {
            break;
        }
    }
    
    logger_log(LOG_INFO, "Exiting receive thread");
    return NULL;
}

/**
 * @brief Initialize relay between two endpoints (X and Y)
 * 
 * Sets up queues for bidirectional message relay:
 * X -> [Server Recv] -> Client Send Queue -> [Client Send] -> Y
 * Y -> [Client Recv] -> Server Send Queue -> [Server Send] -> X
 * 
 * @param server_context Server connection context
 * @param client_context Client connection context
 * @return true if relay setup successful
 */
bool init_relay(CommContext* server_context, CommContext* client_context) {
    if (!server_context || !client_context) {
        logger_log(LOG_ERROR, "Invalid contexts for relay setup");
        return false;
    }

    // Get the existing thread queues
    MessageQueue_T* client_send_queue = message_queue_create_for_thread(client_context->send_thread);
    MessageQueue_T* server_send_queue = message_queue_create_for_thread(server_context->send_thread);

    if (!client_send_queue || !server_send_queue) {
        logger_log(LOG_ERROR, "Failed to get thread queues");
        return false;
    }

    // Connect the queues
    server_context->foreign_queue = client_send_queue;  // Server recv -> Client send
    client_context->foreign_queue = server_send_queue;  // Client recv -> Server send

    server_context->is_relay_enabled = true;
    client_context->is_relay_enabled = true;

    logger_log(LOG_INFO, "Relay initialized between server and client");
    return true;
}

// /**
//  * @brief Create send and receive threads for a connection
//  * 
//  * @param group Pointer to the communication thread group
//  * @param client_addr Pointer to the client address structure
//  * @param thread_info Pointer to thread info structure
//  * @return bool true on success, false on failure
//  */
// bool comms_thread_group_create_threads(CommsThreadGroup* group, struct sockaddr_in* client_addr, CommsThreadArgs_T* thread_info) {
//     if (!group || !client_addr) {
//         return false;
//     }
    
//     // Create separate comm args structures for send and receive threads
//     CommArgs_T send_comm_args = {
//         .sock = group->socket,
//         .client_addr = *client_addr,
//         .thread_info = thread_info,
//         .comm_group = group
//         // queue will be set after thread registration
//     };

//     CommArgs_T recv_comm_args = {
//         .sock = group->socket,
//         .client_addr = *client_addr,
//         .thread_info = thread_info,
//         .comm_group = group
//         // queue will be set after thread registration
//     };
    
//     // Set up send thread
//     AppThread_T send_thread = {
//         .label = NULL,  // Set by thread_group_add
//         .func = send_thread_function,
//         .pre_create_func = pre_create_stub,
//         .post_create_func = post_create_stub,
//         .init_func = init_wait_for_logger,
//         .exit_func = exit_stub,
//         .suppressed = true,
//         .data = &send_comm_args
//     };

//     // Set up receive thread
//     AppThread_T receive_thread = {
//         .label = NULL,  // Set by thread_group_add
//         .func = receive_thread_function,
//         .pre_create_func = pre_create_stub,
//         .post_create_func = post_create_stub,
//         .init_func = init_wait_for_logger,
//         .exit_func = exit_stub,
//         .suppressed = true,
//         .data = &recv_comm_args
//     };
    
//     PlatformThread_T send_handle, recv_handle;

//     // Create and register send thread
//     if (!platform_thread_create(&send_handle, send_thread_function, &send_comm_args)) {
//         logger_log(LOG_ERROR, "Failed to create send thread");
//         return false;
//     }

//     if (!thread_registry_register(get_thread_registry(), &send_thread, send_handle, group->group_id)) {
//         platform_thread_close(&send_handle);
//         logger_log(LOG_ERROR, "Failed to register send thread");
//         return false;
//     }

//     // Create and register receive thread
//     if (!platform_thread_create(&recv_handle, receive_thread_function, &recv_comm_args)) {
//         thread_registry_unregister(get_thread_registry(), send_thread.thread_id);
//         platform_thread_close(&send_handle);
//         logger_log(LOG_ERROR, "Failed to create receive thread");
//         return false;
//     }

//     if (!thread_registry_register(get_thread_registry(), &receive_thread, recv_handle, group->group_id)) {
//         thread_registry_unregister(get_thread_registry(), send_thread.thread_id);
//         platform_thread_close(&send_handle);
//         platform_thread_close(&recv_handle);
//         logger_log(LOG_ERROR, "Failed to register receive thread");
//         return false;
//     }

//     // Set up message queues
//     ThreadQueue_T* send_queue = thread_registry_get_queue(get_thread_registry(), send_thread.thread_id);
//     ThreadQueue_T* recv_queue = thread_registry_get_queue(get_thread_registry(), receive_thread.thread_id);
    
//     if (!send_queue || !recv_queue) {
//         thread_registry_unregister(get_thread_registry(), send_thread.thread_id);
//         thread_registry_unregister(get_thread_registry(), receive_thread.thread_id);
//         platform_thread_close(&send_handle);
//         platform_thread_close(&recv_handle);
//         logger_log(LOG_ERROR, "Failed to get thread queues");
//         return false;
//     }

//     send_comm_args.queue = &send_queue->queue;
//     recv_comm_args.queue = &recv_queue->queue;
    
//     // Store the thread IDs in comm args
//     send_comm_args.thread_id = send_thread.thread_id;
//     recv_comm_args.thread_id = receive_thread.thread_id;

//     // Check if relay mode is enabled in config
//     bool relay_enabled = get_config_bool("network", "enable_relay", false);
//     if (relay_enabled) {
//         group->is_relay_enabled = true;
        
//         // Update queue routing for relay mode
//         if (!init_relay(&send_comm_args, &recv_comm_args)) {
//             logger_log(LOG_ERROR, "Failed to initialize relay");
//             return false;
//         }
//     }

//     return true;
// }

// bool comms_thread_group_is_closed(CommsThreadGroup* group) {
//     if (!group || !group->connection_closed) {
//         return true;
//     }
    
//     return platform_atomic_compare_exchange(group->connection_closed, 0, 0) != 0;
// }

CommContext* comm_context_create(uint32_t group_id, SOCKET* socket, bool is_tcp) {
    if (!socket) {
        return NULL;
    }
    
    CommContext* context = (CommContext*)malloc(sizeof(CommContext));
    if (!context) {
        return NULL;
    }
    
    context->socket = socket;
    context->connection_closed = 0;
    context->group_id = group_id;
    context->is_tcp = is_tcp;
    context->is_relay_enabled = false;
    context->foreign_queue = NULL;  // Will be set during relay setup
    memset(&context->client_addr, 0, sizeof(context->client_addr));
    
    return context;
}

bool create_communication_threads(AppThread_T* send_thread,
    AppThread_T* receive_thread,
    CommContext* context,
    CommsThreadArgs_T* config) {
    if (!context || !config) {
        logger_log(LOG_ERROR, "Invalid parameters for thread creation");
        return false;
    }

    PlatformThread_T send_handle, recv_handle;
    if (!platform_thread_create(&send_handle, send_thread_function, send_thread)) {
        logger_log(LOG_ERROR, "Failed to create send thread");
        return false;
    }

    // Create receive thread
    if (!platform_thread_create(&recv_handle, receive_thread_function, receive_thread)) {
        platform_thread_close(&send_handle);
        logger_log(LOG_ERROR, "Failed to create receive thread");
        return false;
    }

    // Get thread registry instance
    ThreadRegistry* registry = get_thread_registry();

    // group id is the thread id of the parent thread
    // Register threads with registry
    // bool thread_registry_register(ThreadRegistry* registry, AppThread_T* thread, ThreadHandle_T handle, bool auto_cleanup) {
 
    if (!thread_registry_register(registry, send_thread, send_handle, true)) {
        platform_thread_close(&send_handle);
        platform_thread_close(&recv_handle);
        logger_log(LOG_ERROR, "Failed to register send thread");
        return false;
    }

    if (!thread_registry_register(registry, receive_thread, recv_handle, true)) {
        // thread_registry_unregister(registry, context->group_id);
        platform_thread_close(&send_handle);
        platform_thread_close(&recv_handle);

        logger_log(LOG_ERROR, "Failed to register receive thread");
        return false;
    }

    // // Set up message queues
    // ThreadQueue_T* send_queue = message_queue_create_for_thread(send_handle);
    // ThreadQueue_T* recv_queue = message_queue_create_for_thread(recv_handle);
    
    // if (!send_queue || !recv_queue) {
    //     // thread_registry_unregister(registry, context->group_id);
    //     platform_thread_close(&send_handle);
    //     platform_thread_close(&recv_handle);
    //     logger_log(LOG_ERROR, "Failed to get thread queues");
    //     return false;
    // }

    // send_args->queue = &send_queue->queue;
    // recv_args->queue = &recv_queue->queue;

    // Check if relay mode is enabled in config
    bool relay_enabled = get_config_bool("network", "enable_relay", false);
    if (relay_enabled) {
        context->is_relay_enabled = true;
        
        // this is conecting the wrong things, the connection is between the
        // server and client
        // // Update queue routing for relay mode
        // if (!init_relay(send_args, recv_args)) {
        //     // thread_registry_unregister(registry, context->group_id);
        //     platform_thread_close(&send_handle);
        //     platform_thread_close(&recv_handle);
        //     logger_log(LOG_ERROR, "Failed to initialize relay");
        //     return false;
        // }
    }

    return true;
}


/**
 * @brief Handles message sending loop
 * @param sock Socket to send on
 * @param comm_args Communication arguments
 * @param comms_info Thread-specific information
 * @return true if successful, false if should exit
 */
static bool handle_send_message(SOCKET* sock, CommContext* context) {
    Message_T message;
    // bool is_tcp = context ? context->is_tcp : true;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    
    // Use a reasonable timeout (e.g. 100ms) to avoid both CPU spinning and long blocking
    if (!message_queue_pop(context->foreign_queue, &message, 100)) {
        return true;  // No message available, but that's OK - just continue
    }
    
    if (!is_socket_writable(sock, BLOCKING_TIMEOUT_SEC)) {
        return true;
    }
    
    if (!send_single_message(sock, &message, context->is_tcp, &context->client_addr, addr_len)) {
        // comms_thread_group_close(comm_args->comm_group);
        return false;
    }
    
    // Handle test data sending interval
    if ( context->send_test_data && context->send_interval_ms > 0) {
        sleep_ms(context->send_interval_ms);
    }
    
    return true;
}

/**
 * @brief Thread function for sending data to a socket
 * @param arg Pointer to thread arguments (AppThread_T structure)
 * @return NULL
 */
void* send_thread_function(void* arg) {
    AppThread_T* thread_info = (AppThread_T*)arg;
    // set_thread_label(thread_info->label);
    // ThreadQueue_T* thread_queue = get_thread_queue(thread_info->thread_id);
    
    CommContext* context = validate_context(thread_info);
    if (!context) {
        return NULL;
    }
    
    // SOCKET sock = *context->socket;
    // CommsThreadArgs_T* comms_info = (CommsThreadArgs_T*)context->thread_info;
    // bool is_tcp = comms_info ? comms_info->is_tcp : true;
    
    logger_log(LOG_INFO, "Started send thread for %s connection", context->is_tcp ? "TCP" : "UDP");

    while (!shutdown_signalled()) {
    // while (!shutdown_signalled() && !comms_thread_group_is_closed(comm_args->comm_group)) {
        if (*context->socket == INVALID_SOCKET) {
            logger_log(LOG_ERROR, "Socket became invalid in send thread");
            // comms_thread_group_close(comm_args->comm_group);
            break;
        }
        
        if (!handle_send_message(context->socket, context)) {
            break;
        }
    }
    
    logger_log(LOG_INFO, "Exiting send thread");
    return NULL;
}

/**
 * Checks if the communication threads show signs of activity or if the connection is alive.
 * 
 * @param group Pointer to the communication thread group
 * @return true if the connection shows signs of being active/healthy, false otherwise
 */
// bool comms_thread_has_activity(CommContext* group) {
//     if (!group || !group->socket || *group->socket == INVALID_SOCKET) {
//         return false;
//     }
    
//     // Check socket health using the platform-specific function
//     return is_socket_healthy(*context->socket);
    
//     // Possible future enhancement: also check message activity counters
//     // if you want to distinguish between idle vs failed connections
// }
