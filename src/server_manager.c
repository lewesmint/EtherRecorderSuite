#include "server_manager.h"

#include <stdio.h>
#include <string.h>
#include <ws2tcpip.h>  // For inet_ntop

#include "common_socket.h"
#include "app_thread.h"
#include "logger.h"
#include "platform_utils.h"
#include "app_config.h"

// External declarations for stub functions
extern void* pre_create_stub(void* arg);
extern void* post_create_stub(void* arg);
extern void* init_stub(void* arg);
extern void* exit_stub(void* arg);
extern void* init_wait_for_logger(void* arg);
extern bool shutdown_signalled(void);

// Default configuration values
#define DEFAULT_LISTEN_RETRY_LIMIT 10
#define DEFAULT_THREAD_WAIT_TIMEOUT_MS 5000
#define DEFAULT_LISTEN_BACKOFF_MAX_SECONDS 32

// Thread arguments for the server's send and receive threads
AppThreadArgs_T server_send_thread_args = {
    .suppressed = true,
    .label = "SERVER.SEND",
    .func = send_thread,
    .data = NULL,
    .pre_create_func = pre_create_stub,
    .post_create_func = post_create_stub,
    .init_func = init_wait_for_logger,
    .exit_func = exit_stub
};

AppThreadArgs_T server_receive_thread_args = {
    .suppressed = true,
    .label = "SERVER.RECEIVE",
    .func = receive_thread,
    .data = NULL,
    .pre_create_func = pre_create_stub,
    .post_create_func = post_create_stub,
    .init_func = init_wait_for_logger,
    .exit_func = exit_stub
};

/**
 * Helper function to clean up thread handles
 */
static void cleanup_thread_handles(HANDLE send_handle, HANDLE receive_handle) {
    if (send_handle != NULL) {
        CloseHandle(send_handle);
    }
    
    if (receive_handle != NULL) {
        CloseHandle(receive_handle);
    }
}

/**
 * Waits for threads to complete with timeout
 * 
 * @param send_thread_id Send thread handle
 * @param receive_thread_id Receive thread handle
 * @param timeout_ms Timeout in milliseconds
 * @param connection_closed Pointer to connection closed flag
 * @return true if threads completed, false if timeout
 */
static bool wait_for_communication_threads(
    HANDLE send_thread_id, 
    HANDLE receive_thread_id, 
    int timeout_ms,
    volatile LONG* connection_closed) {
    
    HANDLE thread_handles[2];
    DWORD handle_count = 0;
    
    if (send_thread_id != NULL) {
        thread_handles[handle_count++] = send_thread_id;
    }
    
    if (receive_thread_id != NULL) {
        thread_handles[handle_count++] = receive_thread_id;
    }
    
    if (handle_count == 0) {
        return true; // No threads to wait for
    }

    // Check if threads already completed
    if (handle_count == 1) {
        DWORD result = WaitForSingleObject(thread_handles[0], 0);
        if (result == WAIT_OBJECT_0) {
            return true;
        }
    } else if (handle_count == 2) {
        // Check if both threads already completed
        DWORD result1 = WaitForSingleObject(thread_handles[0], 0);
        DWORD result2 = WaitForSingleObject(thread_handles[1], 0);
        if (result1 == WAIT_OBJECT_0 && result2 == WAIT_OBJECT_0) {
            return true;
        }
    }
    
    // Wait for all threads with timeout
    DWORD result = WaitForMultipleObjects(handle_count, thread_handles, TRUE, timeout_ms);
    
    if (result == WAIT_OBJECT_0) {
        logger_log(LOG_DEBUG, "All communication threads completed normally");
        return true;
    } else if (result == WAIT_TIMEOUT) {
        logger_log(LOG_WARN, "Timeout waiting for communication threads");
        
        // Check if connection was marked as closed by the threads
        if (InterlockedCompareExchange(connection_closed, 0, 0) != 0) {
            logger_log(LOG_INFO, "Connection was closed by a thread, proceeding with cleanup");
            return true;
        }
        
        return false;
    } else {
        logger_log(LOG_ERROR, "Error waiting for communication threads: %lu", GetLastError());
        return true; // Return true to allow cleanup
    }
}

/**
 * Sets up the server listening socket with retry logic
 * 
 * @param addr Pointer to socket address structure to populate
 * @param port Port number to listen on
 * @return SOCKET Valid socket or INVALID_SOCKET on failure
 */
static SOCKET setup_server_socket(struct sockaddr_in* addr, int port) {
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
        
        sleep(backoff);
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
    AppThreadArgs_T* thread_info = (AppThreadArgs_T*)arg;
    CommsThreadArgs_T* server_info = (CommsThreadArgs_T*)thread_info->data;
    set_thread_label(thread_info->label);
    
    // Load configuration
    int port = get_config_int("network", "server.port", server_info->port);
    bool is_tcp = get_config_bool("network", "server.is_tcp", server_info->is_tcp);
    int thread_wait_timeout = get_config_int("network", "server.thread_wait_timeout_ms", DEFAULT_THREAD_WAIT_TIMEOUT_MS);
    
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
        int client_len = sizeof(client_addr);
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
        volatile LONG connection_closed_flag = FALSE;
        
        // Set up the communication data
        CommArgs_T comm_args = {
            .sock = &client_sock,
            .client_addr = client_addr,
            .thread_info = server_info,
            .connection_closed = &connection_closed_flag
        };
        
        // Create local copies of thread arguments
        AppThreadArgs_T send_thread_args_local = server_send_thread_args;
        AppThreadArgs_T receive_thread_args_local = server_receive_thread_args;
        
        // Set up thread data
        send_thread_args_local.data = &comm_args;
        receive_thread_args_local.data = &comm_args;
        send_thread_args_local.suppressed = false;
        receive_thread_args_local.suppressed = false;
        
        // Create the communication threads
        create_app_thread(&send_thread_args_local);
        create_app_thread(&receive_thread_args_local);
        
        // Store thread handles for cleanup
        HANDLE send_thread_handle = send_thread_args_local.thread_id;
        HANDLE receive_thread_handle = receive_thread_args_local.thread_id;
        
        // Wait for the threads to complete or for a shutdown signal
        int retry_count = 0;
        while (!shutdown_signalled()) {
            logger_log(LOG_DEBUG, "SERVER: Waiting for send and receive threads");
            
            bool threads_completed = wait_for_communication_threads(
                send_thread_handle, 
                receive_thread_handle, 
                thread_wait_timeout,
                &connection_closed_flag
            );
            
            if (threads_completed) {
                break;
            }
            
            // Prevent infinite waiting if threads are stuck
            retry_count++;
            int retry_limit = get_config_int("network", "server.thread_retry_limit", DEFAULT_LISTEN_RETRY_LIMIT);
            if (retry_limit > 0 && retry_count >= retry_limit) {
                logger_log(LOG_ERROR, "Exceeded retry limit (%d) waiting for threads, forcing cleanup", retry_limit);
                break;
            }
        }
        
        // Clean up thread handles
        cleanup_thread_handles(send_thread_handle, receive_thread_handle);
        
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