/**
 * @file shared_memory_monitor.h
 * @brief Shared memory monitoring functionality
 */
#ifndef SHARED_MEMORY_MONITOR_H
#define SHARED_MEMORY_MONITOR_H

#include "platform_shared_memory.h"
#include "platform_error.h"
#include "platform_sockets.h"
#include "app_thread.h"

// Network forwarding configuration
typedef struct {
    char hostname[128];
    uint16_t listen_port;        // Local port to listen on
    uint16_t forward_port;       // Remote port to forward to
    bool use_tcp;
    uint8_t retry_count;
    uint16_t retry_interval_ms;
} SharedMemoryForwardingConfig;

// Shared memory monitor configuration
typedef struct {
    char name[64];
    bool create;
    size_t data_size;
    PlatformSharedMemoryAccess access;
    uint32_t interval_ms;
    uint32_t retry_interval_ms;
    uint32_t max_retries;
    bool wait_indefinitely;
    int block_index;
    SharedMemoryForwardingConfig forwarding;
    
    // Add socket information
    PlatformSocketHandle socket;
    bool socket_initialized;
} SharedMemoryMonitorConfig;

// Initialize a shared memory monitor configuration with defaults
PlatformErrorCode shared_memory_monitor_init_config(SharedMemoryMonitorConfig* config);

/**
 * @brief Get the shared memory monitor thread configuration
 * 
 * @return Pointer to thread configuration
 */
struct ThreadConfig* get_shared_memory_monitor_thread(void);

/**
 * @brief Thread function for monitoring a single shared memory block
 * 
 * @param arg Pointer to BlockMonitorThreadData
 * @return Thread result
 */
void* shared_memory_monitor_thread(void* arg);

#endif // SHARED_MEMORY_MONITOR_H
