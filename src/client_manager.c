#include "client_manager.h"

#include <stdio.h>      // for perror, snprintf
#include <string.h>     // for memcpy, memset, strncpy, memmove, strerror
#include <time.h>       // for time_t, time, ctime

#include "common_socket.h"
#include "app_thread.h"
#include "logger.h"
#include "platform_utils.h"
#include "app_config.h"
#include "comm_threads.h"

// External declarations for stub functions
extern void* pre_create_stub(void* arg);
extern void* post_create_stub(void* arg);
extern void* init_stub(void* arg);
extern void* exit_stub(void* arg);
extern void* init_wait_for_logger(void* arg);
extern bool shutdown_signalled(void);

// Default configuration values
#define DEFAULT_RETRY_LIMIT 10
#define DEFAULT_BACKOFF_MAX_SECONDS 32
#define DEFAULT_THREAD_WAIT_TIMEOUT_MS 5000
#define DEFAULT_CONNECTION_TIMEOUT_SECONDS 5

static volatile LONG suppress_client_send_data = TRUE;

/**
 * Attempts to set up the socket connection, including retries with
 * exponential backoff. For TCP, it tries to connect with a configured timeout.
 * For UDP, no connection attempt is necessary.
 *
 * @param is_server      Flag indicating if this is a server.
 * @param is_tcp         Flag indicating if the socket is TCP.
 * @param addr           Pointer to the sockaddr_in for the server address.
 * @param client_addr    Pointer to the sockaddr_in for the client address.
 * @param hostname       The server hostname.
 * @param port           The port number.
 * @param conn_timeout   Connection timeout in seconds.
 * @return A valid socket on success, or INVALID_SOCKET on failure.
 */
static SOCKET attempt_connection(bool is_server, bool is_tcp, struct sockaddr_in* addr,
    struct sockaddr_in* client_addr, const char* hostname, int port, int conn_timeout) {
    
    int backoff = 1;  // Start with a 1-second backoff
    int backoff_max = get_config_int("network", "client.backoff_max_seconds", DEFAULT_BACKOFF_MAX_SECONDS);
    
    while (!shutdown_signalled()) {
        logger_log(LOG_DEBUG, "Client Manager attempting to connect to server %s on port %d...", hostname, port);
        SOCKET sock = setup_socket(is_server, is_tcp, addr, client_addr, hostname, port);
        if (sock == INVALID_SOCKET) {
            char error_buffer[SOCKET_ERROR_BUFFER_SIZE];
            get_socket_error_message(error_buffer, sizeof(error_buffer));
            logger_log(LOG_ERROR, "Socket setup failed: %s. Retrying in %d seconds...", error_buffer, backoff);
            
            sleep(backoff);
            backoff = (backoff < backoff_max) ? backoff * 2 : backoff_max;
            continue;
        }

        if (is_tcp) {
            if (connect_with_timeout(sock, addr, conn_timeout) == PLATFORM_SOCKET_SUCCESS) {
                logger_log(LOG_INFO, "Client Manager connected to server %s:%d", hostname, port);
                return sock;
            }
            else {
                char error_buffer[SOCKET_ERROR_BUFFER_SIZE];
                get_socket_error_message(error_buffer, sizeof(error_buffer));
                logger_log(LOG_ERROR, "Connection failed: %s. Retrying in %d seconds...", error_buffer, backoff);
                
                if (sock != INVALID_SOCKET) {
                    close_socket(&sock);
                }
                sleep(backoff);
                backoff = (backoff < backoff_max) ? backoff * 2 : backoff_max;
                continue;
            }
        }
        else {
            // For UDP clients no connection attempt is required
            logger_log(LOG_INFO, "UDP Client ready to send on port %d.", port);
            return sock;
        }
    }
    
    logger_log(LOG_INFO, "Client Manager attempt to connect exiting due to app shutdown.");
    return INVALID_SOCKET;
}

AppThread_T client_send_thread = {
    .suppressed = true,
    .label = "CLIENT.SEND",
    .func = send_thread,
    .data = NULL,
    .pre_create_func = pre_create_stub,
    .post_create_func = post_create_stub,
    .init_func = init_wait_for_logger,
    .exit_func = exit_stub
};

AppThread_T client_receive_thread = {
    .suppressed = true,
    .label = "CLIENT.RECEIVE",
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

void* clientMainThread(void* arg) {
    AppThread_T* thread_info = (AppThread_T*)arg;
    set_thread_label(thread_info->label);
    CommsThreadArgs_T* client_info = (CommsThreadArgs_T*)thread_info->data;

    // Load configuration
    const char* config_server_hostname = get_config_string("network", "client.server_hostname", NULL);
    if (config_server_hostname) {
        strncpy(client_info->server_hostname, config_server_hostname, sizeof(client_info->server_hostname) - 1);
        client_info->server_hostname[sizeof(client_info->server_hostname) - 1] = '\0';
    }
    
    client_info->port = get_config_int("network", "client.port", client_info->port);
    client_info->send_interval_ms = get_config_int("network", "client.send_interval_ms", client_info->send_interval_ms);
    client_info->send_test_data = get_config_bool("network", "client.send_test_data", false);
    
    BOOL should_suppress = get_config_bool("debug", "suppress_client_send_data", TRUE);
    InterlockedExchange(&suppress_client_send_data, should_suppress ? TRUE : FALSE);
    
    int conn_timeout = get_config_int("network", "client.connection_timeout_seconds", DEFAULT_CONNECTION_TIMEOUT_SECONDS);
    int thread_wait_timeout = get_config_int("network", "client.thread_wait_timeout_ms", DEFAULT_THREAD_WAIT_TIMEOUT_MS);
    
    logger_log(LOG_INFO, "Client Manager will attempt to connect to Server: %s, port: %d", 
               client_info->server_hostname, client_info->port);

    int port = client_info->port;
    bool is_tcp = client_info->is_tcp;
    bool is_server = false;
    
    while (!shutdown_signalled()) {
        SOCKET sock = INVALID_SOCKET;
        struct sockaddr_in addr, client_addr;
        
        memset(&addr, 0, sizeof(addr));
        memset(&client_addr, 0, sizeof(client_addr));

        sock = attempt_connection(is_server, is_tcp, &addr, &client_addr, 
                                 client_info->server_hostname, port, conn_timeout);
        
        if (sock == INVALID_SOCKET) {
            logger_log(LOG_INFO, "Shutdown requested before communication started.");
            return NULL;
        }

        // Create a flag for tracking connection state
        volatile LONG connection_closed_flag = FALSE;
        
        // Create communication thread group
        CommsThreadGroup comms_group;
        if (!comms_thread_group_init(&comms_group, "CLIENT", &sock, &connection_closed_flag)) {
            logger_log(LOG_ERROR, "Failed to initialize communication thread group");
            close_socket(&sock);
            continue;
        }
        
        // Create communication threads
        if (!comms_thread_group_create_threads(&comms_group, &client_addr, client_info)) {
            logger_log(LOG_ERROR, "Failed to create communication threads");
            comms_thread_group_cleanup(&comms_group);
            close_socket(&sock);
            continue;
        }
        
        // Wait for communication threads or shutdown
        int retry_count = 0;
        while (!shutdown_signalled() && !comms_thread_group_is_closed(&comms_group)) {
            logger_log(LOG_DEBUG, "CLIENT: Waiting for send and receive threads");
            
            bool threads_completed = comms_thread_group_wait(&comms_group, thread_wait_timeout);
            if (threads_completed) {
                break;
            }
            
            // Check if we've exceeded the retry limit for stuck threads
            retry_count++;
            int retry_limit = get_config_int("network", "client.retry_limit", DEFAULT_RETRY_LIMIT);
            if (retry_limit > 0 && retry_count >= retry_limit) {
                logger_log(LOG_ERROR, "Exceeded retry limit (%d) for stuck threads, forcing reconnection", retry_limit);
                break;
            }
        }
        
        // Clean up
        comms_thread_group_cleanup(&comms_group);
        
        // Close the socket
        logger_log(LOG_INFO, "Closing client socket");
        if (sock != INVALID_SOCKET) {
            close_socket(&sock);
        }

        // If shutting down, exit the thread
        if (shutdown_signalled()) {
            logger_log(LOG_INFO, "CLIENT: Shutdown signaled, exiting client thread");
            break;
        }

        // Otherwise, attempt to reconnect
        logger_log(LOG_INFO, "CLIENT: Connection lost or reset needed. Attempting to reconnect...");
        
        // Add a small delay before reconnection attempt
        sleep_ms(1000);
    }

    logger_log(LOG_INFO, "CLIENT: Exiting client thread.");
    return NULL;
}