#include "comm_threads.h"
#include <string.h>

#include "logger.h"
#include "platform_utils.h"


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
 * @brief Thread function for receiving data from a socket
 */
void* receive_thread(void* arg) {
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
    socklen_t addr_len = sizeof(struct sockaddr_in);
    
    logger_log(LOG_INFO, "Started receive thread for %s connection", is_tcp ? "TCP" : "UDP");
    
    // Buffer for receiving data
    char buffer[COMMS_BUFFER_SIZE];
    
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
            
            // Create a message for the received data
            Message_T message;
            memset(&message, 0, sizeof(Message_T));
            
            // Populate message header
            message.header.type = MSG_TYPE_RELAY;
            message.header.content_size = bytes_received;
            message.header.id = platform_random(); // Unique message ID
            message.header.flags = is_tcp ? 1 : 0; // Flag to indicate TCP/UDP
            
            // Ensure we don't overflow the message content
            size_t copy_size = (bytes_received > sizeof(message.content)) 
                ? sizeof(message.content) 
                : bytes_received;
            
            // Copy received data into message content
            memcpy(message.content, buffer, copy_size);
            
            // If relay is enabled and outgoing queue exists, push the message
            if (comm_args->outgoing_queue) {
                if (!message_queue_push(comm_args->outgoing_queue, &message, INFINITE)) {
                    logger_log(LOG_WARN, "Failed to push received message to queue");
                }
            }
        } else if (bytes_received == 0) {
            // Connection closed by peer
            logger_log(LOG_INFO, "Connection closed by peer");
            mark_connection_closed(comm_args->connection_closed);
            break;
        } else {
            // Socket error
            char error_buffer[SOCKET_ERROR_BUFFER_SIZE];
            get_socket_error_message(error_buffer, sizeof(error_buffer));
            logger_log(LOG_ERROR, "Error receiving data: %s", error_buffer);
            mark_connection_closed(comm_args->connection_closed);
            break;
        }
        
        // Short sleep to prevent CPU spinning
        sleep_ms(1);
    }
    
    logger_log(LOG_INFO, "Exiting receive thread");
    return NULL;
}

/**
 * @brief Thread function for sending data to a socket
 */
void* send_thread(void* arg) {
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
    
    logger_log(LOG_INFO, "Started send thread for %s connection", is_tcp ? "TCP" : "UDP");
    
    // Continue sending until shutdown or connection closed
    while (!shutdown_signalled() && !is_connection_closed(comm_args->connection_closed)) {
        // Check if socket is still valid
        if (*comm_args->sock == INVALID_SOCKET) {
            logger_log(LOG_ERROR, "Socket became invalid in send thread");
            mark_connection_closed(comm_args->connection_closed);
            break;
        }
        
        // Try to get a message from the incoming queue
        Message_T message;
        if (comm_args->incoming_queue && 
            message_queue_pop(comm_args->incoming_queue, &message, 100)) {
            // Send the message over the socket
            int bytes_sent;
            if (is_tcp) {
                bytes_sent = send(sock, message.content, message.header.content_size, 0);
            } else {
                bytes_sent = sendto(sock, message.content, message.header.content_size, 0,
                                  (struct sockaddr*)&comm_args->client_addr, addr_len);
            }
            
            if (bytes_sent > 0) {
                // Successfully sent data
                logger_log(LOG_DEBUG, "Sent %d bytes of relayed data", bytes_sent);
            } else {
                // Socket error
                char error_buffer[SOCKET_ERROR_BUFFER_SIZE];
                get_socket_error_message(error_buffer, sizeof(error_buffer));
                logger_log(LOG_ERROR, "Error sending relayed data: %s", error_buffer);
                mark_connection_closed(comm_args->connection_closed);
                break;
            }
        }
        
        // Small sleep to prevent tight spinning
        sleep_ms(10);
    }
    
    logger_log(LOG_INFO, "Exiting send thread");
    return NULL;
}

/**
 * @brief Initialize relay between communication endpoints
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