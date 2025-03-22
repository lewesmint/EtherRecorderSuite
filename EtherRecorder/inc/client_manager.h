#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>  // For size_t

#include "app_thread.h"

typedef struct {
    char server_host[256];     // Fixed-size array instead of const char*
    uint16_t server_port;
    bool is_tcp;
    size_t max_message_size;
    uint32_t timeout_ms;
    uint32_t backoff_initial_ms;  // Initial backoff time
    uint32_t backoff_max_ms;      // Maximum backoff time
    uint32_t retry_limit;         // Changed from uint32_t to uint32_t, 0 means infinite retries
    bool enable_relay;           // Added relay configuration
} ClientConfig;

/**
 * @brief Main client thread function.
 * 
 * Responsible for:
 * - Establishing connection to server with retry/backoff
 * - Creating send/receive threads once connected
 * - Monitoring connection health
 * - Handling reconnection on connection loss
 * - Cleaning up resources on shutdown
 * 
 * @param arg Thread configuration (ThreadConfig*)
 * @return void* NULL
 */
void* clientMainThread(void* arg);

/**
 * @brief Get the client thread configuration
 * @return ThreadConfig* Pointer to the client thread configuration
 */
ThreadConfig* get_client_thread(void);

#endif // CLIENT_MANAGER_H
