/**
 * @file server_manager.c
 * @brief Implementation of server management functions
 */

// Own header
#include "server_manager.h"

// System includes
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// Project includes
// #include "common_socket.h"
#include "comm_threads.h"
#include "app_thread.h"
#include "logger.h"
#include "platform_utils.h"
#include "platform_threads.h"
#include "platform_atomic.h"
#include "app_config.h"

// Default configuration values
#define DEFAULT_LISTEN_RETRY_LIMIT 10
#define DEFAULT_THREAD_WAIT_TIMEOUT_MS 5000
#define DEFAULT_LISTEN_BACKOFF_MAX_SECONDS 32

// // Thread arguments for the server's send and receive threads
// AppThread_T server_send_thread = {
//     .suppressed = true,
//     .label = "SERVER.SEND",
//     .func = send_thread_function,
//     .data = NULL,
//     .pre_create_func = pre_create_stub,
//     .post_create_func = post_create_stub,
//     .init_func = init_wait_for_logger,
//     .exit_func = exit_stub
// };

// AppThread_T server_receive_thread = {
//     .suppressed = true,
//     .label = "SERVER.RECEIVE",
//     .func = receive_thread_function,
//     .data = NULL,
//     .pre_create_func = pre_create_stub,
//     .post_create_func = post_create_stub,
//     .init_func = init_wait_for_logger,
//     .exit_func = exit_stub
// };

/**
 * Sets up the server listening socket with retry logic
 * 
 * @param addr Pointer to socket address structure to populate
 * @param port Port number to listen on
 * @return Valid socket or INVALID_SOCKET on failure
 */
static SOCKET setup_server_socket(struct sockaddr_in* addr, uint16_t port) {
    int backoff = 1;  // Start with a 1-second backoff
    int backoff_max = get_config_int("network", "server.backoff_max_seconds", DEFAULT_LISTEN_BACKOFF_MAX_SECONDS);
    int retry_limit = get_config_int("network", "server.retry_limit", DEFAULT_LISTEN_RETRY_LIMIT);
    int retry_count = 0;
    
    while (!shutdown_signalled()) {
        logger_log(LOG_DEBUG, "Setting up server socket on port %d", port);
        
        SOCKET sock = setup_listening_server_socket(addr, port);
        if (sock != INVALID_SOCKET) {
            logger_log(LOG_INFO, "Server socket setup successfully on port %d", port);
            return sock;
        }
        
        char error_buffer[SOCKET_ERROR_BUFFER_SIZE];
        get_socket_error_message(error_buffer, sizeof(error_buffer));
        logger_log(LOG_ERROR, "Server socket setup failed: %s. Retrying in %d seconds...", 
                  error_buffer, backoff);
        
        sleep_seconds(backoff);
        backoff = (backoff < backoff_max) ? backoff * 2 : backoff_max;
        
        retry_count++;
        if (retry_limit > 0 && retry_count >= retry_limit) {
            logger_log(LOG_ERROR, "Exceeded retry limit (%d) for server socket setup", retry_limit);
            return INVALID_SOCKET;
        }
    }
    
    logger_log(LOG_INFO, "Server socket setup aborted due to shutdown signal");
    return INVALID_SOCKET;
}

void* serverListenerThread(void* arg) {
    AppThread_T* thread_info = (AppThread_T*)arg;
    CommsThreadArgs_T* server_info = (CommsThreadArgs_T*)thread_info->data;
    set_thread_label(thread_info->label);
    
    // Load configuration
    uint16_t port = get_config_uint16("network", "server.port", server_info->port);
    bool is_tcp = get_config_bool("network", "server.is_tcp", server_info->is_tcp);
    // int thread_wait_timeout = get_config_int("network", "server.thread_wait_timeout_ms", DEFAULT_THREAD_WAIT_TIMEOUT_MS);
    
    logger_log(LOG_INFO, "Server Manager starting on port: %d, protocol: %s", 
               port, is_tcp ? "TCP" : "UDP");
    
    while (!shutdown_signalled()) {
        // Set up server address structure
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        
        // Set up the listening socket
        SOCKET listener_sock = setup_server_socket(&addr, port);
        if (listener_sock == INVALID_SOCKET) {
            // If we couldn't set up the socket and didn't get shutdown signal, try again
            if (!shutdown_signalled()) {
                logger_log(LOG_WARN, "Failed to set up server socket, will retry");
                sleep_seconds(5);
                continue;
            } else {
                break; // Exit if shutdown signaled
            }
        }
        
        logger_log(LOG_INFO, "Server is listening on port %d", port);
        
        // Accept a client connection
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        memset(&client_addr, 0, sizeof(client_addr));
        
        logger_log(LOG_INFO, "Waiting for client connection...");
        
        SOCKET client_sock = INVALID_SOCKET;
        while (!shutdown_signalled() && client_sock == INVALID_SOCKET) {
            client_sock = accept(listener_sock, (struct sockaddr*)&client_addr, &client_len);
            
            if (client_sock == INVALID_SOCKET) {
                if (shutdown_signalled()) {
                    break;
                }
                
                char error_buffer[SOCKET_ERROR_BUFFER_SIZE];
                get_socket_error_message(error_buffer, sizeof(error_buffer));
                logger_log(LOG_ERROR, "Accept failed: %s. Will retry.", error_buffer);
                sleep_seconds(1);
            }
        }
        
        // Check if we exited the accept loop due to shutdown
        if (shutdown_signalled()) {
            logger_log(LOG_INFO, "Server shutting down, closing listener socket");
            close_socket(&listener_sock);
            break;
        }
        
        // Connection established, log client information
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        logger_log(LOG_INFO, "Client connected from %s:%d", client_ip, ntohs(client_addr.sin_port));
        
        // Create a flag for tracking connection state
        // volatile long connection_closed_flag = 0;
        
        // Create communication thread group
        // CommsThreadGroup comms_group;
        // if (!comms_thread_group_init(&comms_group, "SERVER_CLIENT", &client_sock, &connection_closed_flag)) {
        //     logger_log(LOG_ERROR, "Failed to initialise communication thread group");
        //     close_socket(&client_sock);
        //     continue;
        // }
        
        // // Create communication threads
        // if (!comms_thread_group_create_threads(&comms_group, &client_addr, server_info)) {
        //     logger_log(LOG_ERROR, "Failed to create communication threads");
        //     comms_thread_group_cleanup(&comms_group);
        //     close_socket(&client_sock);
        //     continue;
        // }
        
        // Wait for communication threads or shutdown
        int health_check_retries = 0;
        bool monitoring_connection_health = false;

        while (!shutdown_signalled()) {
        // while (!shutdown_signalled() && !comms_thread_group_is_closed(&comms_group)) {
            logger_log(LOG_DEBUG, "SERVER: Monitoring active connection");
            
            // bool threads_completed = comms_thread_group_wait(&comms_group, thread_wait_timeout);
            // if (threads_completed) {
            //     logger_log(LOG_INFO, "Communication threads completed, ending session");
            //     break;
            // }
            
            // Only track retry counts when we're monitoring connection health
            if (monitoring_connection_health) {
                health_check_retries++;
                int retry_limit = get_config_int("network", "server.thread_retry_limit", DEFAULT_LISTEN_RETRY_LIMIT);
                if (retry_limit > 0 && health_check_retries >= retry_limit) {
                    logger_log(LOG_ERROR, "Exceeded retry limit (%d) for connection health checks, forcing cleanup", retry_limit);
                    break;
                }
            }
            
            // Check if the socket is still healthy
            if (true) {
            // if (comms_thread_has_activity(&comms_group)) {
                // Socket is healthy, reset monitoring state
                health_check_retries = 0;
                monitoring_connection_health = false;
            } else {
                // Socket appears unhealthy, start monitoring more carefully
                if (!monitoring_connection_health) {
                    logger_log(LOG_WARN, "Connection health check failed, monitoring for recovery");
                }
                monitoring_connection_health = true;
            }
        }
        
        // Clean up
        // comms_thread_group_cleanup(&comms_group);
        
        // Close the client socket
        logger_log(LOG_INFO, "Closing client socket");
        if (client_sock != INVALID_SOCKET) {
            close_socket(&client_sock);
        }
        
        // Close the listener socket (we'll create a new one for the next connection)
        logger_log(LOG_INFO, "Closing listener socket");
        close_socket(&listener_sock);
        
        if (!shutdown_signalled()) {
            logger_log(LOG_INFO, "Client disconnected. Will listen for new connections.");
            sleep_ms(1000); // Brief delay before setting up a new listening socket
        }
    }
    
    logger_log(LOG_INFO, "SERVER: Exiting server thread.");
    return NULL;
}
