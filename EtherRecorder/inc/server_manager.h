#ifndef SERVER_MANAGER_H
#define SERVER_MANAGER_H


#include "platform_error.h"
#include "platform_sockets.h"
#include "app_thread.h"

// Forward declarations
struct CommContext;
struct CommsThreadArgs;

typedef struct ServerConfig {
    uint16_t port;
    bool is_tcp;
    int backoff_max_seconds;
    uint32_t retry_limit;
    int thread_wait_timeout_ms;
    bool enable_relay;           // Added relay configuration
} ServerConfig;


// Get the server thread configuration
ThreadConfig* get_server_thread(void);


#endif // SERVER_MANAGER_H
