#include "client_manager.h"

// 2. System includes
#include <stdio.h>
#include <string.h>
#include <time.h>

// 3. Platform includes
#include "platform_atomic.h"
#include "platform_sockets.h"
#include "platform_utils.h"
#include "platform_threads.h"

// 4. Project includes
#include "app_config.h"
#include "app_thread.h"
#include "comm_threads.h"
#include "logger.h"
#include "platform_defs.h"


// Default configuration values
#define DEFAULT_RETRY_LIMIT 10
#define DEFAULT_BACKOFF_MAX_SECONDS 32
#define DEFAULT_THREAD_WAIT_TIMEOUT_MS 5000
#define DEFAULT_CONNECTION_TIMEOUT_SECONDS 5

// static volatile long suppress_client_send_data = 1;

/**
 * Attempts to set up the socket connection, including retries with
 * exponential backoff. For TCP, it tries to connect with a configured timeout.
 * For UDP, no connection attempt is necessary.
 *
 * @param is_server      Flag indicating if this is a server.
 * @param is_tcp         Flag indicating if the socket is TCP.
 * @param addr           Pointer to the sockaddr_in for the server address.
 * @param hostname       The server hostname.
 * @param port           The port number.
 * @param conn_timeout   Connection timeout in seconds.
 * @return A valid socket on success, or INVALID_SOCKET on failure.
 */
static SOCKET attempt_connection(bool is_server, bool is_tcp, struct sockaddr_in* addr,
                               const char* hostname, uint16_t port, int conn_timeout) {
    int backoff = 1;  // Start with a 1-second backoff
    int backoff_max = get_config_int("network", "client.backoff_max_seconds", DEFAULT_BACKOFF_MAX_SECONDS);
    
    while (!shutdown_signalled()) {
        logger_log(LOG_DEBUG, "Client Manager attempting to connect to server %s on port %d...", hostname, port);
        SOCKET sock = setup_socket(is_server, is_tcp, addr, NULL, hostname, port);
        if (sock == INVALID_SOCKET) {
            char error_buffer[SOCKET_ERROR_BUFFER_SIZE];
            get_socket_error_message(error_buffer, sizeof(error_buffer));
            logger_log(LOG_ERROR, "Socket setup failed: %s. Retrying in %d seconds...", error_buffer, backoff);
            
            sleep_seconds(backoff);
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
                sleep_seconds(backoff);
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

void* clientMainThread(void* arg) {
    AppThread_T* thread_info = (AppThread_T*)arg;
    if (!thread_info || !thread_info->data) {
        logger_log(LOG_ERROR, "Invalid thread arguments");
        return NULL;
    }
    
    set_thread_label(thread_info->label);
    
    // Cast the thread_info->data to ClientCommsThreadArgs_T
    ClientCommsThreadArgs_T* client_config = (ClientCommsThreadArgs_T*)thread_info->data;
    
    // Validate client configuration
    if (!client_config) {
        logger_log(LOG_ERROR, "Invalid client configuration");
        return NULL;
    }
    
    // Load configuration
    const char* config_server_hostname = get_config_string("network", "client.server_hostname", NULL);
    if (config_server_hostname) {
        strncpy(client_config->server_hostname, config_server_hostname, sizeof(client_config->server_hostname) - 1);
        client_config->server_hostname[sizeof(client_config->server_hostname) - 1] = '\0';
    }
    
    // Configure the common communication arguments
    client_config->comm_args.is_tcp = get_config_bool("network", "client.is_tcp", client_config->comm_args.is_tcp);
    client_config->comm_args.port = get_config_uint16("network", "client.server_port", client_config->comm_args.port);
    client_config->comm_args.send_test_data = get_config_bool("network", "client.send_test_data", client_config->comm_args.send_test_data);
    client_config->comm_args.send_interval_ms = get_config_int("network", "client.send_interval_ms", client_config->comm_args.send_interval_ms);
    client_config->comm_args.data_size = get_config_int("network", "client.data_size", client_config->comm_args.data_size);

    // Create runtime context
    CommContext context = {
        // .group_id = platform_thread_get_id(),
        .is_tcp = client_config->comm_args.is_tcp,
        .send_test_data = client_config->comm_args.send_test_data,
        .send_interval_ms = client_config->comm_args.send_interval_ms,
        .data_size = client_config->comm_args.data_size,
        .connection_closed = 0,
        .is_relay_enabled = get_config_bool("network", "enable_relay", false)
    };
    
    logger_log(LOG_INFO, "Client Manager will attempt to connect to Server: %s, port: %d", 
               client_config->server_hostname, client_config->comm_args.port);

    while (!shutdown_signalled()) {
        SOCKET sock = INVALID_SOCKET;
        
        // Setup address structures
        memset(&context.client_addr, 0, sizeof(context.client_addr));

        sock = attempt_connection(false, context.is_tcp, &context.client_addr, 
                                client_config->server_hostname, client_config->comm_args.port, 
                                DEFAULT_CONNECTION_TIMEOUT_SECONDS);
        
        if (sock == INVALID_SOCKET) {
            if (shutdown_signalled()) {
                logger_log(LOG_INFO, "Shutdown requested before communication started.");
                break;
            }
            sleep_ms(1000);
            continue;
        }

        // Update context with new socket
        context.socket = &sock;
        context.connection_closed = 0;

        // do we need two contexts?
        // a word about message queues
        
        // This is the client. 
        // 1. What we receive through TCP will be pushed onto Server's queue
        // and serviced by the server's send thread to push out to TCP.
        // 2. What the server received via TCP will be pushed onto Client's queue
        // and be serviced by the client's send thread to push out to TCP.
        
        // So the receive thread needs to be informed of the queue to push to.
        //
        // In each case if no relay has been established the recive thread
        // just logs and swallows the data. The send thread will look on its queue
        // for data to send. To establish relay, both receive threads needs to be
        // informed of the queue to push to, via a message on its msg queue.
        // so the receive threads need to send a message to say they hold the queue
        // and then get a message that that has been understod and acknowledged

        AppThread_T send_thread = {
            .label = "CLIENT.SEND",
            .func = send_thread_function,
            .data = &context,  // Pass runtime context
            .pre_create_func = pre_create_stub,
            .post_create_func = post_create_stub,
            .init_func = init_wait_for_logger,
            .exit_func = exit_stub,
            .suppressed = true
        };
        AppThread_T receive_thread = {
            .label = "CLIENT.RECEIVE",
            .func = receive_thread_function,
            .data = &context,  // Pass runtime context
            .pre_create_func = pre_create_stub,
            .post_create_func = post_create_stub,
            .init_func = init_wait_for_logger,
            .exit_func = exit_stub,
            .suppressed = true
        };

        // Create communication threads with context
        if (!create_communication_threads(&send_thread,
                                          &receive_thread,
                                          &context,
                                          &client_config->comm_args)) {
            logger_log(LOG_ERROR, "Failed to create communication threads");
            // platform_socket_close(&sock);
            close_socket(&sock);
            continue;
        }

        // Main monitoring loop
        while (!shutdown_signalled() && !context.connection_closed) {
            // check our queue
            sleep_ms(100);
        }

        // Cleanup
        cleanup_communication_threads(&context);
        // platform_socket_close(&sock);
        close_socket(&sock);
        context.socket = NULL;

        if (shutdown_signalled()) {
            break;
        }

        logger_log(LOG_INFO, "Connection lost or reset needed. Attempting to reconnect...");
        sleep_ms(1000);
    }

    return NULL;
}

void cleanup_communication_threads(CommContext* context) {
    if (!context) {
        return;
    }
    
    // For now, just mark the connection as closed
    // We'll expand this as we implement the full threading system
    context->connection_closed = 1;
}
