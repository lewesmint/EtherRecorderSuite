
#include "client_manager.h"
#include <string.h>
#include "platform_atomic.h"
#include "platform_error.h"
#include "platform_sockets.h"
#include "platform_threads.h"
#include "platform_error.h"
#include "platform_time.h"
#include "app_config.h"
#include "comm_context.h"
#include "logger.h"
#include "thread_registry.h"
#include "utils.h"
#include "platform_string.h"


// Default configuration values
#define DEFAULT_CONNECT_RETRY_LIMIT 10
#define DEFAULT_CONNECT_BACKOFF_MAX_SECONDS 32
#define DEFAULT_CONNECTION_TIMEOUT_SECONDS 5

static PlatformErrorCode attempt_connection(const ClientConfig* config,
                                          PlatformSocketHandle* out_socket) {
    uint32_t backoff_ms = config->backoff_initial_ms;
    uint32_t retry_count = 0;
    PlatformSocketHandle sock = NULL;
    PlatformErrorCode err;

    while (!shutdown_signalled()) {
        // Create socket with options
        PlatformSocketOptions sock_opts = {
            .blocking = true,
            .send_timeout_ms = config->timeout_ms,
            .recv_timeout_ms = config->timeout_ms,
            .connect_timeout_ms = DEFAULT_CONNECTION_TIMEOUT_SECONDS * PLATFORM_MS_PER_SEC,
            .keep_alive = true,       // Match server settings for connection health monitoring
            .no_delay = true          // Better latency for our relay system
            // Using OS defaults for buffer sizes
        };
        
        err = platform_socket_create(&sock, config->is_tcp, &sock_opts);
        if (err != PLATFORM_ERROR_SUCCESS) {
            char error_buffer[256];
            platform_get_error_message_from_code(err, error_buffer, sizeof(error_buffer));
            logger_log(LOG_ERROR, "Failed to create socket: %s", error_buffer);
            return PLATFORM_ERROR_SOCKET_CREATE;
        }

        // Attempt connection
        logger_log(LOG_INFO, "Attempting to connect to %s:%d...", 
                  config->server_host, config->server_port);

        PlatformSocketAddress addr = {
            .port = config->server_port,
            .is_ipv6 = false  // Assuming IPv4 for now
        };
        strncpy(addr.host, config->server_host, sizeof(addr.host) - 1);
        addr.host[sizeof(addr.host) - 1] = '\0';
        
        err = platform_socket_connect(sock, &addr);
        if (err == PLATFORM_ERROR_SUCCESS) {
            logger_log(LOG_INFO, "Connected to server %s:%d", 
                      config->server_host, config->server_port);
            *out_socket = sock;
            return PLATFORM_ERROR_SUCCESS;
        }

        platform_socket_close(sock);
        char error_buffer[256];
        platform_get_error_message_from_code(err, error_buffer, sizeof(error_buffer));
        logger_log(LOG_ERROR, "Connection failed: %s. Will retry in %d ms...", 
                  error_buffer, backoff_ms);
        
        sleep_ms(backoff_ms);
        retry_count++;
        if (config->retry_limit > 0 && retry_count >= config->retry_limit) {
            logger_log(LOG_ERROR, "Connection attempts exceeded retry limit (%d)", config->retry_limit);
            return PLATFORM_ERROR_SOCKET_CONNECT;
        }
        if (config->retry_limit > 0) {
            logger_log(LOG_INFO, "Retry attempt %d of %d", retry_count, config->retry_limit);
        } else {
            logger_log(LOG_INFO, "Retry attempt %d (unlimited retries)", retry_count);
        }
        backoff_ms = (backoff_ms * 2 < config->backoff_max_ms) ? backoff_ms * 2 : config->backoff_max_ms;
    }

    return PLATFORM_ERROR_NOT_INITIALIZED;
}

void* clientMainThread(void* arg) {
    ThreadConfig* thread_config = (ThreadConfig*)arg;
    ClientConfig* config = (ClientConfig*)thread_config->data;
    
    if (!config) {
        logger_log(LOG_ERROR, "Invalid client configuration");
        return NULL;
    }

    logger_log(LOG_INFO, "Client manager starting, will connect to %s:%d",
               config->server_host, config->server_port);

    while (!shutdown_signalled()) {
        PlatformSocketHandle sock = NULL;
        
        // Attempt to establish connection
        PlatformErrorCode err = attempt_connection(config, &sock);
        if (err != PLATFORM_ERROR_SUCCESS) {
            if (shutdown_signalled()) {
                logger_log(LOG_INFO, "Shutdown requested before connection established");
                break;
            }
            continue;
        }

        // Initialize communication context
        CommContext context = {
            .socket = sock,
            .is_relay_enabled = config->enable_relay,
            .is_tcp = config->is_tcp,
            .max_message_size = config->max_message_size,
            .timeout_ms = config->timeout_ms
        };

        // Create thread configurations
        ThreadConfig send_thread_config = {
            .label = "CLIENT.SEND",
            .func = (ThreadFunc_T)comm_send_thread,
            .data = &context,
            .suppressed = false
        };

        ThreadConfig receive_thread_config = {
            .label = "CLIENT.RECEIVE",
            .func = (ThreadFunc_T)comm_receive_thread,
            .data = &context,
            .suppressed = false
        };

        // Create threads using comm_context_create_threads
        err = comm_context_create_threads(&context,
                                          &send_thread_config,
                                          &receive_thread_config);
        if (err != PLATFORM_ERROR_SUCCESS) {
            platform_socket_close(sock);
            return NULL;
        }

        // Monitor connection
        while (!shutdown_signalled() && !comm_context_is_closed(&context)) {
            sleep_ms(100);
        }

        // Clean up
        comm_context_cleanup_threads(&context);
        platform_socket_close(sock);

        if (shutdown_signalled()) {
            break;
        }

        logger_log(LOG_INFO, "Connection lost, attempting to reconnect...");
        sleep_ms(200);
    }

    logger_log(LOG_INFO, "Client manager shutting down");
    return NULL;
}

static PlatformErrorCode client_manager_init_config(ClientConfig* config) {
    if (!config) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    const char* protocol = get_config_string("network", "server.protocol", "tcp");
    config->is_tcp = (strcmp_nocase(protocol, "UDP") != 0);
    config->server_port = 4200;
    config->max_message_size = 1024;
    config->timeout_ms = 1000;
    config->backoff_initial_ms = 1000;    // 1 second
    config->backoff_max_ms = 30000;       // 30 seconds
    config->retry_limit = 0;              // infinite retries
    
    // Add relay configuration
    config->enable_relay = get_config_bool("network", "client.enable_relay", false);

    // Override with configuration
    config->server_port = get_config_uint16("network", "client.server_port", config->server_port);
    config->backoff_initial_ms = get_config_int("network", "client.backoff_initial_ms", config->backoff_initial_ms);
    config->backoff_max_ms = get_config_int("network", "client.backoff_max_ms", config->backoff_max_ms);
    config->retry_limit = (uint32_t)get_config_int("network", "client.retry_limit", config->retry_limit);
    
    const char* config_hostname = get_config_string("network", "client.server_hostname", config->server_host);
    if (config_hostname) {
        strncpy(config->server_host, config_hostname, sizeof(config->server_host) - 1);
        config->server_host[sizeof(config->server_host) - 1] = '\0';
    }

    return PLATFORM_ERROR_SUCCESS;
}

// Default client configuration
static ClientConfig client_config = {
    .server_host = "localhost",  // Will be copied during initialization
    .server_port = 4200,
    .is_tcp = true,
    .max_message_size = 1024,
    .timeout_ms = 1000,
    .backoff_initial_ms = 1000,
    .backoff_max_ms = 30000,
    .retry_limit = 0
};

// Get the client thread configuration
ThreadConfig* get_client_thread(void) {
    static ThreadConfig client_thread = {
        .label = "CLIENT",
        .func = clientMainThread,
        .data = &client_config,
        .suppressed = false
    };

    // Initialize client configuration with defaults and config file values
    client_manager_init_config(&client_config);
    
    return &client_thread;
}
