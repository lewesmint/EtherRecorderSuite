#ifndef COMM_CONTEXT_H
#define COMM_CONTEXT_H

#include <stdbool.h>
#include <stdint.h>

#include "platform_threads.h"
#include "platform_sockets.h"
#include "platform_atomic.h"
#include "thread_registry.h"
#include "app_thread.h"

// Configuration constants
#define COMM_BUFFER_SIZE 8192
#define SOCKET_ERROR_BUFFER_SIZE 256
#define DEFAULT_BLOCKING_TIMEOUT_SEC 10

typedef struct CommContext {
    PlatformSocketHandle socket;
    PlatformThreadId send_thread_id;
    PlatformThreadId recv_thread_id;
    PlatformAtomicBool connection_closed;
    bool is_relay_enabled;
    bool is_tcp;
    size_t max_message_size;
    uint32_t timeout_ms;
    char foreign_queue_label[MAX_THREAD_LABEL_LENGTH];    // Using existing constant from thread_registry.h
} CommContext;

typedef struct CommConfig {
    uint32_t max_message_size;               // Maximum message size
    uint32_t timeout_ms;                     // Operation timeout
    bool enable_relay;                       // Enable relay mode
    bool is_tcp;                            // Use TCP (true) or UDP (false)
} CommConfig;

// Function declarations
void comm_context_init(CommContext* context);
bool comm_context_is_closed(const CommContext* context);
void comm_context_close(CommContext* context);

/**
 * @brief Thread function for sending messages
 * @param arg Pointer to CommContext
 * @return NULL
 */
void* comm_send_thread(void* arg);

/**
 * @brief Thread function for receiving messages
 * @param arg Pointer to CommContext
 * @return NULL
 */
void* comm_receive_thread(void* arg);

/**
 * @brief Creates send and receive threads for a communication context
 * 
 * @param context The communication context
 * @param send_config Pointer to send thread configuration
 * @param recv_config Pointer to receive thread configuration
 * @return PlatformErrorCode PLATFORM_ERROR_SUCCESS on success, error code otherwise
 */
PlatformErrorCode comm_context_create_threads(CommContext* context, 
                                              ThreadConfig* send_config,
                                              ThreadConfig* recv_config);

/**
 * @brief Cleans up send and receive threads for a communication context
 * 
 * @param context The communication context
 */
void comm_context_cleanup_threads(CommContext* context);

#endif // COMM_CONTEXT_H
