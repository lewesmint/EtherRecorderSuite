#include "comm_threads.h"
#include "logger.h"
#include "platform_utils.h"
#include <string.h>

// External declarations
extern bool shutdown_signalled(void);

/**
 * @brief Set the connection closed flag atomically
 * 
 * @param flag Pointer to the connection closed flag
 */
static void mark_connection_closed(volatile LONG* flag) {
    if (flag) {
        InterlockedExchange(flag, TRUE);
    }
}

/**
 * @brief Check if connection is already marked as closed
 * 
 * @param flag Pointer to the connection closed flag
 * @return true if connection is closed, false otherwise
 */
static bool is_connection_closed(volatile LONG* flag) {
    if (!flag) {
        return false;
    }
    return InterlockedCompareExchange(flag, 0, 0) != 0;
}

/**
 * @brief Handle a socket error or disconnect
 * 
 * @param sock Socket that encountered an error
 * @param error_msg Error message to log
 * @param flag Connection closed flag to set
 */
static void handle_socket_error(SOCKET* sock, const char* error_msg, volatile LONG* flag) {
    if (!is_connection_closed(flag)) {
        char error_buffer[SOCKET_ERROR_BUFFER_SIZE];
        get_socket_error_message(error_buffer, sizeof(error_buffer));
        logger_log(LOG_ERROR, "%s: %s", error_msg, error_buffer);
        mark_connection_closed(flag);
    }
}

/**
 * @brief Thread function for receiving data from a socket
 */
void* receive_thread(void* arg) {
    AppThreadArgs_T* thread_info = (AppThreadArgs_T*)arg;
    set_thread_label(thread_info->label);
    
    CommArgs_T* comm_args = (CommArgs_T*)thread_info->data;
    if (!comm_args || !comm_args->sock || !(*comm_args->sock) || *comm_args->sock == INVALID_SOCKET) {
        logger_log(LOG_ERROR, "Invalid communication arguments in receive thread");
        return NULL;
    }
    
    SOCKET sock = *comm_args->sock;
    CommsThreadArgs_T* comms_info = (CommsThreadArgs_T*)comm_args->thread_info;
    bool is_tcp = comms_info ? comms_info->is_tcp : true;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    
    logger_log(LOG_INFO, "Started receive thread for %s connection", is_tcp ? "TCP" : "UDP");
    
    // Buffer for receiving data
    char buffer[BUFFER_SIZE];
    
    // Continue receiving until shutdown or connection closed
    while (!shutdown_signalled() && !is_connection_closed(comm_args->connection_closed)) {
        // Check if socket is still valid
        if (*comm_args->sock == INVALID_SOCKET) {
            logger_log(LOG_ERROR, "Socket became invalid in receive thread");
            mark_connection_closed(comm_args->connection_closed);
            break;
        }
        
        // Receive data with proper error handling
        int bytes_received;
        if (is_tcp) {
            bytes_received = recv(sock, buffer, sizeof(buffer), 0);
        } else {
            bytes_received = recvfrom(sock, buffer, sizeof(buffer), 0, 
                                    (struct sockaddr*)&comm_args->client_addr, &addr_len);
        }
        
        if (bytes_received > 0) {
            // Successfully received data
            logger_log(LOG_DEBUG, "Received %d bytes of data", bytes_received);
            
            // Process the received data here
            // ...
            
        } else if (bytes_received == 0) {
            // Connection closed by peer
            logger_log(LOG_INFO, "Connection closed by peer");
            mark_connection_closed(comm_args->connection_closed);
            break;
        } else {
            // Socket error
            handle_socket_error(comm_args->sock, "Error receiving data", comm_args->connection_closed);
            break;
        }
        
        // Short sleep to prevent CPU spinning in case of rapid messages
        sleep_ms(1);
    }
    
    logger_log(LOG_INFO, "Exiting receive thread");
    return NULL;
}

/**
 * @brief Thread function for sending data to a socket
 */
void* send_thread(void* arg) {
    AppThreadArgs_T* thread_info = (AppThreadArgs_T*)arg;
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
    int send_interval = (comms_info && comms_info->send_interval_ms > 0) ? 
                        comms_info->send_interval_ms : 1000;
    bool send_test_data = comms_info ? comms_info->send_test_data : false;
    
    logger_log(LOG_INFO, "Started send thread for %s connection, interval: %dms, test data: %s", 
              is_tcp ? "TCP" : "UDP", send_interval, send_test_data ? "enabled" : "disabled");
    
    // Continue sending until shutdown or connection closed
    while (!shutdown_signalled() && !is_connection_closed(comm_args->connection_closed)) {
        // Check if socket is still valid
        if (*comm_args->sock == INVALID_SOCKET) {
            logger_log(LOG_ERROR, "Socket became invalid in send thread");
            mark_connection_closed(comm_args->connection_closed);
            break;
        }
        
        // Only send if test data is enabled
        if (send_test_data) {
            // Create test data packet
            DummyPayload payload;
            int payload_size = generateRandomData(&payload);
            
            // Send data with proper error handling
            int bytes_sent;
            if (is_tcp) {
                bytes_sent = send(sock, (char*)&payload, payload_size, 0);
            } else {
                bytes_sent = sendto(sock, (char*)&payload, payload_size, 0,
                                  (struct sockaddr*)&comm_args->client_addr, addr_len);
            }
            
            if (bytes_sent > 0) {
                // Successfully sent data
                logger_log(LOG_DEBUG, "Sent %d bytes of data", bytes_sent);
            } else {
                // Socket error
                handle_socket_error(comm_args->sock, "Error sending data", comm_args->connection_closed);
                break;
            }
        }
        
        // Sleep for the configured interval
        sleep_ms(send_interval);
    }
    
    logger_log(LOG_INFO, "Exiting send thread");
    return NULL;
}