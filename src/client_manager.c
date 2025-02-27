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

static bool suppress_client_send_data = true;


/**
 * Attempts to set up the socket connection, including retries with
 * exponential backoff. For TCP, it tries to connect with a 5-second timeout.
 * For UDP, no connection attempt is necessary.
 *
 * @param is_server      Flag indicating if this is a server.
 * @param is_tcp         Flag indicating if the socket is TCP.
 * @param addr           Pointer to the sockaddr_in for the server address.
 * @param client_addr    Pointer to the sockaddr_in for the client address.
 * @param hostname       The server hostname.
 * @param port           The port number.
 * @return A valid socket on success, or INVALID_SOCKET on failure.
 */
static SOCKET attempt_connection(bool is_server, bool is_tcp, struct sockaddr_in* addr,
    struct sockaddr_in* client_addr, const char* hostname, int port) {
    int backoff = 1;  // Start with a 1-second backoff.
    while (!shutdown_signalled()) {
        logger_log(LOG_DEBUG, "Client Manager Attempting to connect to server %s on port %d...", hostname, port);
        SOCKET sock = setup_socket(is_server, is_tcp, addr, client_addr, hostname, port);
        if (sock == INVALID_SOCKET) {
            logger_log(LOG_ERROR, "Socket setup failed. Retrying in %d seconds...", backoff);
            {
                char error_buffer[SOCKET_ERROR_BUFFER_SIZE];
                get_socket_error_message(error_buffer, sizeof(error_buffer));
                logger_log(LOG_ERROR, "%s", error_buffer);
            }
            logger_log(LOG_DEBUG, "Client Manager Sleeping");
            sleep(backoff);
            logger_log(LOG_DEBUG, "Client Manager Waking");

            backoff = (backoff < 32) ? backoff * 2 : 32;
            continue;
        }

        if (is_tcp) {
            if (connect_with_timeout(sock, addr, 5) == 0) {  // 5-second timeout.
                logger_log(LOG_INFO, "Client Manager connected to server.");
                return sock;
            }
            else {
                logger_log(LOG_ERROR, "Connection failed. Retrying in %d seconds...", backoff);
                {
                    char error_buffer[SOCKET_ERROR_BUFFER_SIZE];
                    get_socket_error_message(error_buffer, sizeof(error_buffer));
                    logger_log(LOG_ERROR, "%s", error_buffer);
                }
                if (sock != INVALID_SOCKET) {
                    close_socket(&sock);
                }
                sleep(backoff);
                backoff = (backoff < 32) ? backoff * 2 : 32;
                continue;
            }
        }
        else {
            // For UDP clients no connection attempt is required.
            logger_log(LOG_INFO, "UDP Client ready to send on port %d.", port);
            return sock;
        }
    }
    while (!shutdown_signalled()) {
        logger_log(LOG_INFO, "Client Manager attempt to connect exiting due to app shutdown.");
    }
    return INVALID_SOCKET;
}


typedef struct {
    SOCKET* sock;  // Pointer to shared socket
    struct sockaddr_in client_addr;
    CommsThreadArgs_T* client_info;
    volatile bool connection_closed;  // Shared flag to indicate socket closure
} ClientCommArgs_T;


AppThreadArgs_T client_send_thread_args = {
	.suppressed = true,
    .label = "CLIENT.SEND",
    .func = send_thread,
    .data = NULL,
    .pre_create_func = pre_create_stub,
    .post_create_func = post_create_stub,
    .init_func = init_wait_for_logger,
    .exit_func = exit_stub
};

AppThreadArgs_T client_receive_thread_args = {
    .suppressed = true,
    .label = "CLIENT.RECEIVE",
    .func = receive_thread,
    .data = NULL,
    .pre_create_func = pre_create_stub,
    .post_create_func = post_create_stub,
    .init_func = init_wait_for_logger,
    .exit_func = exit_stub
};

void* clientMainThread(void* arg) {
    AppThreadArgs_T* thread_info = (AppThreadArgs_T*)arg;
    set_thread_label(thread_info->label);
    CommsThreadArgs_T* client_info = (CommsThreadArgs_T*)thread_info->data;

    const char* config_server_hostname = get_config_string("network", "client.server_hostname", NULL);
    if (config_server_hostname) {
        strncpy(client_info->server_hostname, config_server_hostname, sizeof(client_info->server_hostname));
        client_info->server_hostname[sizeof(client_info->server_hostname) - 1] = '\0';
    }
    client_info->port = get_config_int("network", "client.port", client_info->port);
    client_info->send_interval_ms = get_config_int("network", "client.send_interval_ms", client_info->send_interval_ms);
    client_info->send_test_data = get_config_bool("network", "client.send_test_data", false);
    suppress_client_send_data = get_config_bool("debug", "suppress_client_send_data", false);
    logger_log(LOG_INFO, "Client Manager will attempt to connect to Server: %s, port: %d", client_info->server_hostname, client_info->port);

    int port = client_info->port;
    bool is_tcp = client_info->is_tcp;
    bool is_server = false;
    SOCKET sock = INVALID_SOCKET;

    struct sockaddr_in addr, client_addr;

    while (!shutdown_signalled()) {
        memset(&addr, 0, sizeof(addr));
        memset(&client_addr, 0, sizeof(client_addr));

        sock = attempt_connection(is_server, is_tcp, &addr, &client_addr, client_info->server_hostname, port);
        if (sock == INVALID_SOCKET) {
            logger_log(LOG_INFO, "Shutdown requested before communication started.");
            return NULL;
        }

        CommArgs_T comm_args = {
            &sock, 
            client_addr, 
            client_info 
        };
        AppThreadArgs_T send_thread_args_local = client_send_thread_args;
        AppThreadArgs_T receive_thread_args_local = client_receive_thread_args;
        
        send_thread_args_local.data = &comm_args;
        receive_thread_args_local.data = &comm_args;
        send_thread_args_local.suppressed = false;
        receive_thread_args_local.suppressed = false;
   
        create_app_thread(&send_thread_args_local);
        create_app_thread(&receive_thread_args_local);

        // Wait for send and receive threads to complete with a timeout
        while (!shutdown_signalled()) {

            logger_log(LOG_INFO, "CLIENT: Looping on waiting for send and receive threads to indicate they're done");

            DWORD send_thread_result = WaitForSingleObject(send_thread_args_local.thread_id, 5000); // 500 ms timeout
            DWORD receive_thread_result = WaitForSingleObject(receive_thread_args_local.thread_id, 5000); // 500 ms timeout

            if ((send_thread_result == WAIT_OBJECT_0 || send_thread_result == WAIT_FAILED) &&
                (receive_thread_result == WAIT_OBJECT_0 || receive_thread_result == WAIT_FAILED)) {
                break;
            }
        }

        logger_log(LOG_INFO, "Closing socket 1");
        if (sock != INVALID_SOCKET) {
            logger_log(LOG_INFO, "Closing socket 2");
            close_socket(&sock);
        }

        while (!shutdown_signalled()) {
            logger_log(LOG_INFO, "CLIENT: Shutdown is signaled detected by client");
            break;
        }

        logger_log(LOG_INFO, "CLIENT: Connection lost. Attempting to reconnect...");
    }

    logger_log(LOG_INFO, "CLIENT: Exiting client thread.");
    return NULL;
}
