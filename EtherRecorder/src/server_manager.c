#include "server_manager.h"
#include "comm_context.h"

#include <string.h>

#include "platform_atomic.h"
#include "platform_error.h"
#include "platform_sockets.h"
#include "platform_threads.h"
#include "platform_time.h"
#include "platform_string.h"

#include "app_config.h"
#include "app_thread.h"
#include "comm_context.h"
#include "logger.h"
#include "thread_registry.h"
#include "file_reader.h"
#include "utils.h"


// Default configuration values
#define DEFAULT_LISTEN_RETRY_LIMIT 10
#define DEFAULT_LISTEN_BACKOFF_MAX_SECONDS 32
#define DEFAULT_THREAD_WAIT_TIMEOUT_MS 5000

void* serverListenerThread(void* arg);

ServerConfig server_thread_args = {
    .port = 4199,                           ///< Port number
    .is_tcp = true,                         ///< Protocol is TCP (else UDP)
    .backoff_max_seconds = DEFAULT_LISTEN_BACKOFF_MAX_SECONDS,
    .retry_limit = DEFAULT_LISTEN_RETRY_LIMIT,
    .thread_wait_timeout_ms = DEFAULT_THREAD_WAIT_TIMEOUT_MS
};

PlatformErrorCode server_manager_init_config(ServerConfig* config) {
    if (!config) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    // First handle protocol since it requires special processing
    const char* protocol = get_config_string("network", "server.protocol", "tcp");
    config->is_tcp = (strcmp_nocase(protocol, "UDP") != 0);

    // Load all other values from config, using existing struct values as defaults
    config->port = get_config_uint16("network", "server.server_port", config->port);
    config->backoff_max_seconds = get_config_int("network", "server.backoff_max_seconds", config->backoff_max_seconds);
    config->retry_limit = (uint32_t)get_config_int("network", "server.retry_limit", config->retry_limit);
    config->thread_wait_timeout_ms = get_config_int("network", "server.thread_wait_timeout_ms", config->thread_wait_timeout_ms);
    
    // Add relay configuration
    config->enable_relay = get_config_bool("network", "server.enable_relay", false);
    
    return PLATFORM_ERROR_SUCCESS;
}

// Get the server thread configuration
ThreadConfig* get_server_thread(void) {
    static ThreadConfig server_thread = {
        .label = "SERVER",
        .func = serverListenerThread,
        .data = &server_thread_args,
        .suppressed = false
    };

    // Initialize server configuration with defaults and config file values
    server_manager_init_config(&server_thread_args);
    
    return &server_thread;
}

static PlatformErrorCode handle_connection(CommContext* base_context) {
    // Create atomic bool on the stack - perfectly fine as it lives for the duration of the function
    PlatformAtomicBool connection_closed;
    platform_atomic_init_bool(&connection_closed, false);

    CommContext send_context = *base_context;  // Copy base settings
    CommContext recv_context = *base_context;  // Copy base settings
    
    // Set the atomic in just the duplicated contexts
    send_context.connection_closed = &connection_closed;
    recv_context.connection_closed = &connection_closed;

    // Create thread configurations
    ThreadConfig send_thread_config = {
        .label = "SERVER.SEND",
        .func = (ThreadFunc_T)comm_send_thread,
        .data = &send_context,    // Point to send context
        .suppressed = false
    };

    ThreadConfig receive_thread_config = create_thread_config(
        "SERVER.RECEIVE", 
        (ThreadFunc_T)comm_receive_thread, 
        &receive_thread_config
    );

    // Check if file sending is enabled for server
    const char* filepath = get_config_string("server", "send_file", NULL);
	ThreadConfig* file_reader = NULL;
    if (filepath) {
        file_reader = get_file_reader_thread(filepath, "SERVER.SEND");

        // Create threads using comm_context_create_threads
        PlatformErrorCode err = comm_context_create_threads(&send_thread_config,
                                                          &receive_thread_config);
        if (err != PLATFORM_ERROR_SUCCESS) {
            return err;
        }

        // Start file reader
        ThreadResult result = app_thread_create(file_reader);
        if (result != THREAD_SUCCESS) {
            logger_log(LOG_ERROR, "Failed to start file reader thread");
        }

    }

    // Create threads using comm_context_create_threads
    PlatformErrorCode errr = comm_context_create_threads(&send_thread_config,
                                                      &receive_thread_config);
    if (errr != PLATFORM_ERROR_SUCCESS) {
        return errr;
    }

    // Monitor connection - check either context since they share the same socket
    while (!shutdown_signalled() && !platform_atomic_load_bool(&connection_closed)) {
        sleep_ms(100);
    }

    // Cleanup
    comm_context_cleanup_threads(&send_context);
    comm_context_cleanup_threads(&recv_context);
    
    if (file_reader) {
        // Signal thread to stop via registry
        //file_reader->
        
        // Wait for thread to finish
        //platform_thread_join(file_reader->thread_id, NULL);
    }
    
    return PLATFORM_ERROR_SUCCESS;
}

void* serverListenerThread(void* arg) {
    ThreadConfig* thread_config = (ThreadConfig*)arg;
    ServerConfig* config = (ServerConfig*)thread_config->data;
    if (!config) {
        logger_log(LOG_ERROR, "Invalid server configuration");
        return NULL;
    }

    logger_log(LOG_INFO, "Server Manager starting. Config port: %d, protocol: %s", 
        config->port , config->is_tcp ? "TCP" : "UDP");
    
    while (!shutdown_signalled()) {
        // Create platform-agnostic socket address
        PlatformSocketAddress server_addr = {
            .port = config->port,
            .is_ipv6 = false
        };
        strcpy(server_addr.host, "0.0.0.0");  // Listen on all interfaces
        
        // Set up the listening socket with options
        PlatformSocketOptions listener_opts = {
            .blocking = true,
            .send_timeout_ms = config->thread_wait_timeout_ms,
            .recv_timeout_ms = config->thread_wait_timeout_ms,
            .reuse_address = true,    // Allow quick server restart
            .keep_alive = true,       // Detect dead connections
            .no_delay = true,         // Better latency for our relay system
            // Removed buffer size settings to use OS defaults
        };

        // Create socket with options
        PlatformSocketHandle listener = NULL;
        PlatformErrorCode err = platform_socket_create(&listener, config->is_tcp, &listener_opts);
        if (err != PLATFORM_ERROR_SUCCESS) {
            char error_buffer[256];
            platform_get_error_message_from_code(err, error_buffer, sizeof(error_buffer));
            logger_log(LOG_ERROR, "Failed to create server socket: %s", error_buffer);
            sleep_seconds(1);  // Add a small delay before retrying
            continue;  // Continue the outer loop to retry socket creation
        }

        // Track retry attempts and backoff
        uint32_t retry_count = 0;
        uint32_t backoff_ms = 1000;  // Start with 1 second

        while (!shutdown_signalled()) {
            err = platform_socket_bind(listener, &server_addr);
            if (err == PLATFORM_ERROR_SUCCESS) {
                break;
            }

            char error_buffer[256];
            platform_get_error_message_from_code(err, error_buffer, sizeof(error_buffer));
            logger_log(LOG_ERROR, "Failed to bind server socket: %s. Retrying in %d ms...", 
                      error_buffer, backoff_ms);

            platform_socket_close(listener);
            sleep_ms(backoff_ms);

            // Update backoff with exponential increase
            backoff_ms = (backoff_ms * 2 < (uint32_t)config->backoff_max_seconds * 1000) ? 
                         backoff_ms * 2 : config->backoff_max_seconds * 1000;

            retry_count++;
            if (config->retry_limit > 0 && retry_count >= config->retry_limit) {
                logger_log(LOG_ERROR, "Exceeded retry limit (%d) for socket bind", 
                          config->retry_limit);
                return NULL;
            }

            // Create new socket for next attempt
            err = platform_socket_create(&listener, config->is_tcp, &listener_opts);
            if (err != PLATFORM_ERROR_SUCCESS) {
                continue;
            }
        }

        if (config->is_tcp) {
            err = platform_socket_listen(listener, 5);  // backlog of 5
            if (err != PLATFORM_ERROR_SUCCESS) {
                char error_buffer[256];
                platform_get_error_message_from_code(err, error_buffer, sizeof(error_buffer));
                logger_log(LOG_ERROR, "Failed to listen on server socket: %s", error_buffer);
                platform_socket_close(listener);
                continue;
            }
        }
        
        logger_log(LOG_INFO, "Server is listening on port %d", config->port);
        
        // Accept client connections
        while (!shutdown_signalled()) {
            PlatformSocketHandle client = NULL;
            PlatformSocketAddress client_addr = {0};
            
            err = platform_socket_accept(listener, &client, &client_addr);
            if (err != PLATFORM_ERROR_SUCCESS) {
                if (shutdown_signalled()) break;
                
                sleep_seconds(1);
                continue;
            }
            
            logger_log(LOG_INFO, "Client connected from %s:%d", 
                      client_addr.host, client_addr.port);

            CommContext context = {
                .socket = client,
                .is_relay_enabled = config->enable_relay,
                .is_tcp = config->is_tcp,
                .max_message_size = 1024,
                .timeout_ms = 1000
            };

            // Handle connection from client
            err = handle_connection(&context);
            if (err != PLATFORM_ERROR_SUCCESS) {
                comm_context_cleanup_threads(&context);  // Use the proper cleanup function
                platform_socket_close(client);
            }
        }
        
        // Cleanup
        logger_log(LOG_INFO, "Closing listener socket");
        platform_socket_close(listener);
        
        if (!shutdown_signalled()) {
            logger_log(LOG_INFO, "Will listen for new connections");
            sleep_ms(1000);
        }
    }
    
    logger_log(LOG_INFO, "SERVER: Exiting server thread.");
    return NULL;
}
