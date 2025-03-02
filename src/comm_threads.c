#include "comm_threads.h"

#include <string.h>
#include <stdlib.h>

#include "logger.h"
#include "platform_utils.h"
#include "thread_group.h"
#include "platform_sockets.h"
#include "platform_atomic.h"


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
    send_args->connection_closed = group->connection_closed;
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
    receive_args->connection_closed = group->connection_closed;
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
 * @brief Thread function for receiving data from a socket
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
            size_t copy_size = (bytes_received > 0)
                ? MIN((size_t)bytes_received, sizeof(message.content))
                : 0;
            
            // Copy received data into message content
            memcpy(message.content, buffer, copy_size);
            
            // If relay is enabled and outgoing queue exists, push the message
            if (comm_args->outgoing_queue) {
                if (!message_queue_push(comm_args->outgoing_queue, &message, PLATFORM_WAIT_INFINITE)) {
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
