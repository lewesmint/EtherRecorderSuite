
#include "comm_threads.h"

#include <stdlib.h>
#include <string.h>


#include "platform_atomic.h"
#include "platform_sockets.h"    // This should provide all socket functionality
#include "platform_defs.h"       // For platform-agnostic types

#include "thread_registry.h"
#include "logger.h"
#include "app_config.h"

// External declarations
extern bool shutdown_signalled(void);
extern void sleep_ms(uint32_t milliseconds);
extern void set_thread_label(const char* label);

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
static bool is_socket_writable(SOCKET sock, int timeout_sec) {
    fd_set write_fds;
    struct timeval timeout;
    setup_select_timeout(sock, &write_fds, &timeout, timeout_sec, 0);
    
    int select_result = select((int)sock + 1, NULL, &write_fds, NULL, &timeout);
    return (select_result > 0 && FD_ISSET(sock, &write_fds));
}

/**
 * @brief Processes received data and queues it for handling
 * @param comm_args Communication arguments containing the message queue
 * @param buffer Buffer containing received data
 * @param bytes Number of bytes received
 * @return true if processing successful, false otherwise
 */
static bool process_received_data(CommArgs_T* comm_args, const uint8_t* buffer, size_t bytes) {
    Message_T message = {0};
    
    if (bytes > sizeof(message.content)) {
        logger_log(LOG_ERROR, "Received message too large for buffer (%zu > %zu)", 
                  bytes, sizeof(message.content));
        return false;
    }
    
    memcpy(message.content, buffer, bytes);
    message.header.content_size = (uint32_t)bytes;
    
    if (!message_queue_push(comm_args->queue, &message, 100)) {
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
static bool send_single_message(SOCKET sock, const Message_T* message, bool is_tcp, 
    struct sockaddr_in* client_addr, socklen_t addr_len) {
    int bytes_sent;
    if (is_tcp) {
        bytes_sent = send(sock, message->content, message->header.content_size, 0);
    } else {
        bytes_sent = sendto(sock, message->content, message->header.content_size, 0,
                            (struct sockaddr*)client_addr, addr_len);
    }

    if (bytes_sent > 0) {
    logger_log(LOG_DEBUG, "Sent %d bytes of data", bytes_sent);
    return true;
    } else {
    char error_buffer[SOCKET_ERROR_BUFFER_SIZE];
    get_socket_error_message(error_buffer, sizeof(error_buffer));
    logger_log(LOG_ERROR, "Send error: %s", error_buffer);
    return false;
    }
}


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
    strncpy(group->name, name, sizeof(group->name) - 1);
    group->name[sizeof(group->name) - 1] = '\0';
    
    // Remove message queue allocation since we'll use per-thread queues
    // from the thread registration system
    
    // Initialize base thread group
    return thread_group_init(&group->base, name);
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
    
    // No need to clean up message queue here anymore since
    // thread queues are managed by the thread registration system
    
    logger_log(LOG_DEBUG, "Communication thread group cleaned up");
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
 * @brief Validates communication arguments for a thread
 * @param thread_info Thread information structure
 * @return CommArgs_T* pointer if valid, NULL if invalid
 */
static CommArgs_T* validate_comm_args(AppThread_T* thread_info) {
    CommArgs_T* comm_args = (CommArgs_T*)thread_info->data;
    if (!comm_args || !comm_args->sock || !(*comm_args->sock) || *comm_args->sock == INVALID_SOCKET) {
        logger_log(LOG_ERROR, "Invalid communication arguments");
        return NULL;
    }
    return comm_args;
}

/**
 * @brief Handles message sending loop
 * @param sock Socket to send on
 * @param comm_args Communication arguments
 * @param comms_info Thread-specific information
 * @return true if successful, false if should exit
 */
static bool handle_send_message(SOCKET sock, CommArgs_T* comm_args, CommsThreadArgs_T* comms_info) {
    Message_T message;
    bool is_tcp = comms_info ? comms_info->is_tcp : true;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    
    // Use a reasonable timeout (e.g. 100ms) to avoid both CPU spinning and long blocking
    if (!message_queue_pop(comm_args->queue, &message, 100)) {
        return true;  // No message available, but that's OK - just continue
    }
    
    if (!is_socket_writable(sock, BLOCKING_TIMEOUT_SEC)) {
        return true;
    }
    
    if (!send_single_message(sock, &message, is_tcp, &comm_args->client_addr, addr_len)) {
        comms_thread_group_close(comm_args->comm_group);
        return false;
    }
    
    // Handle test data sending interval
    if (comms_info && comms_info->send_test_data && comms_info->send_interval_ms > 0) {
        sleep_ms(comms_info->send_interval_ms);
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
    set_thread_label(thread_info->label);
    
    CommArgs_T* comm_args = validate_comm_args(thread_info);
    if (!comm_args) {
        return NULL;
    }
    
    SOCKET sock = *comm_args->sock;
    CommsThreadArgs_T* comms_info = (CommsThreadArgs_T*)comm_args->thread_info;
    bool is_tcp = comms_info ? comms_info->is_tcp : true;
    
    logger_log(LOG_INFO, "Started send thread for %s connection", is_tcp ? "TCP" : "UDP");
    
    while (!shutdown_signalled() && !comms_thread_group_is_closed(comm_args->comm_group)) {
        if (*comm_args->sock == INVALID_SOCKET) {
            logger_log(LOG_ERROR, "Socket became invalid in send thread");
            comms_thread_group_close(comm_args->comm_group);
            break;
        }
        
        if (!handle_send_message(sock, comm_args, comms_info)) {
            break;
        }
    }
    
    logger_log(LOG_INFO, "Exiting send thread");
    return NULL;
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
                       struct sockaddr_in* client_addr, socklen_t* addr_len) {
    if (is_tcp) {
        return recv(sock, (char*)buffer, buffer_size, 0);
    } else {
        return recvfrom(sock, (char*)buffer, buffer_size, 0,
                       (struct sockaddr*)client_addr, addr_len);
    }
}

// Static helper functions that should be in this file
static bool is_socket_readable(SOCKET sock, int timeout_sec) {
    fd_set read_fds;
    struct timeval timeout;
    setup_select_timeout(sock, &read_fds, &timeout, timeout_sec, 0);
    
    int select_result = select((int)sock + 1, &read_fds, NULL, NULL, &timeout);
    return (select_result > 0 && FD_ISSET(sock, &read_fds));
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
static bool handle_received_data(SOCKET sock, CommArgs_T* comm_args, CommsThreadArgs_T* comms_info,
                               uint8_t* buffer, int* buffered_length, int* batch_bytes) {
    bool is_tcp = comms_info ? comms_info->is_tcp : true;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    
    int bytes = receive_data(sock, buffer + *buffered_length, 
                           COMMS_BUFFER_SIZE - *buffered_length,
                           is_tcp, &comm_args->client_addr, &addr_len);
    
    if (bytes > 0) {
        *buffered_length += bytes;
        *batch_bytes += bytes;
        
        log_buffered_data(buffer, buffered_length, *batch_bytes);
        
        if (!process_received_data(comm_args, buffer, bytes)) {
            comms_thread_group_close(comm_args->comm_group);
            return false;
        }
        
        *buffered_length = 0;
        *batch_bytes = 0;
    } else if (bytes == 0) {
        logger_log(LOG_INFO, "Connection closed by peer");
        comms_thread_group_close(comm_args->comm_group);
        return false;
    } else {
        char error_buffer[SOCKET_ERROR_BUFFER_SIZE];
        get_socket_error_message(error_buffer, sizeof(error_buffer));
        logger_log(LOG_ERROR, "Receive error: %s", error_buffer);
        comms_thread_group_close(comm_args->comm_group);
        return false;
    }
    
    return true;
}

/**
 * @brief Thread function for receiving data from a socket
 * @param arg Pointer to thread arguments (AppThread_T structure)
 * @return NULL
 */
void* receive_thread_function(void* arg) {
    AppThread_T* thread_info = (AppThread_T*)arg;
    set_thread_label(thread_info->label);
    
    CommArgs_T* comm_args = validate_comm_args(thread_info);
    if (!comm_args) {
        return NULL;
    }
    
    SOCKET sock = *comm_args->sock;
    CommsThreadArgs_T* comms_info = (CommsThreadArgs_T*)comm_args->thread_info;
    bool is_tcp = comms_info ? comms_info->is_tcp : true;
    
    logger_log(LOG_INFO, "Started receive thread for %s connection", is_tcp ? "TCP" : "UDP");
    
    uint8_t buffer[COMMS_BUFFER_SIZE];
    int buffered_length = 0;
    int batch_bytes = 0;
    
    while (!shutdown_signalled() && !comms_thread_group_is_closed(comm_args->comm_group)) {
        if (*comm_args->sock == INVALID_SOCKET) {
            logger_log(LOG_ERROR, "Socket became invalid in receive thread");
            comms_thread_group_close(comm_args->comm_group);
            break;
        }
        
        if (!is_socket_readable(sock, BLOCKING_TIMEOUT_SEC)) {
            continue;
        }
        
        if (!handle_received_data(sock, comm_args, comms_info, 
                                buffer, &buffered_length, &batch_bytes)) {
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
 * X -> [Server Recv] -> Queue A -> [Client Send] -> Y
 * Y -> [Client Recv] -> Queue B -> [Server Send] -> X
 * 
 * @param server_args Server connection args (connected to X)
 * @param client_args Client connection args (connected to Y)
 * @return bool true if relay setup successful
 */
bool init_relay(
    CommArgs_T* server_args,    // Connected to X
    CommArgs_T* client_args     // Connected to Y
) {
    if (!server_args || !client_args) {
        logger_log(LOG_ERROR, "Invalid arguments for relay initialization");
        return false;
    }

    // Get the thread queues from the registry
    ThreadQueue_T* server_send_queue = get_thread_queue(server_args->thread_id);
    ThreadQueue_T* client_send_queue = get_thread_queue(client_args->thread_id);
    
    if (!server_send_queue || !client_send_queue) {
        logger_log(LOG_ERROR, "Failed to get thread queues for relay");
        return false;
    }

    // Set up bidirectional relay routing using the thread queues
    server_args->queue = &server_send_queue->queue;  // Server thread's own queue
    client_args->queue = &client_send_queue->queue;  // Client thread's own queue

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

/**
 * @brief Create send and receive threads for a connection
 * 
 * @param group Pointer to the communication thread group
 * @param client_addr Pointer to the client address structure
 * @param thread_info Pointer to thread info structure
 * @return bool true on success, false on failure
 */
bool comms_thread_group_create_threads(CommsThreadGroup* group, struct sockaddr_in* client_addr, CommsThreadArgs_T* thread_info) {
    if (!group || !client_addr) {
        return false;
    }
    
    // Create separate comm args structures for send and receive threads
    CommArgs_T send_comm_args = {
        .sock = group->socket,
        .client_addr = *client_addr,
        .thread_info = thread_info,
        .comm_group = group
        // queue will be set after thread registration
    };

    CommArgs_T recv_comm_args = {
        .sock = group->socket,
        .client_addr = *client_addr,
        .thread_info = thread_info,
        .comm_group = group
        // queue will be set after thread registration
    };
    
    // Set up send thread
    AppThread_T send_thread = {
        .label = NULL,  // Set by thread_group_add
        .func = send_thread_function,
        .pre_create_func = pre_create_stub,
        .post_create_func = post_create_stub,
        .init_func = init_wait_for_logger,
        .exit_func = exit_stub,
        .suppressed = true,
        .data = &send_comm_args
    };

    // Set up receive thread
    AppThread_T receive_thread = {
        .label = NULL,  // Set by thread_group_add
        .func = receive_thread_function,
        .pre_create_func = pre_create_stub,
        .post_create_func = post_create_stub,
        .init_func = init_wait_for_logger,
        .exit_func = exit_stub,
        .suppressed = true,
        .data = &recv_comm_args
    };
    
    // Add threads to the group and get their queues
    if (!thread_group_add(&group->base, &send_thread)) {
        logger_log(LOG_ERROR, "Failed to add send thread to group");
        return false;
    }
    
    // Store the thread ID in comm args
    send_comm_args.thread_id = send_thread.thread_id;
    
    ThreadQueue_T* send_queue = register_thread(send_thread.thread_id);
    if (!send_queue) {
        thread_group_remove(&group->base, send_thread.label);
        logger_log(LOG_ERROR, "Failed to register send thread queue");
        return false;
    }
    send_comm_args.queue = &send_queue->queue;
    
    if (!thread_group_add(&group->base, &receive_thread)) {
        thread_group_remove(&group->base, send_thread.label);
        logger_log(LOG_ERROR, "Failed to add receive thread to group");
        return false;
    }

    // Store the thread ID in comm args
    recv_comm_args.thread_id = receive_thread.thread_id;

    ThreadQueue_T* recv_queue = register_thread(receive_thread.thread_id);
    if (!recv_queue) {
        thread_group_remove(&group->base, receive_thread.label);
        thread_group_remove(&group->base, send_thread.label);
        logger_log(LOG_ERROR, "Failed to register receive thread queue");
        return false;
    }
    recv_comm_args.queue = &recv_queue->queue;

    // Check if relay mode is enabled in config
    bool relay_enabled = get_config_bool("network", "enable_relay", false);
    if (relay_enabled) {
        group->is_relay_enabled = true;
        
        // Update queue routing for relay mode
        if (!init_relay(&send_comm_args, &recv_comm_args)) {
            logger_log(LOG_ERROR, "Failed to initialize relay");
            return false;
        }
    }

    return true;
}

bool comms_thread_group_is_closed(CommsThreadGroup* group) {
    if (!group || !group->connection_closed) {
        return true;
    }
    
    return platform_atomic_compare_exchange(group->connection_closed, 0, 0) != 0;
}
