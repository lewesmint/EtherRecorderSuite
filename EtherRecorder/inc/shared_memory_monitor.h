/**
 * @file shared_memory_monitor.h
 * @brief Shared memory monitoring functionality
 */
#ifndef SHARED_MEMORY_MONITOR_H
#define SHARED_MEMORY_MONITOR_H

#include <stdbool.h>
#include <stddef.h>
#include "platform_error.h"
#include "platform_shared_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration for shared memory monitor
 */
typedef struct {
    char name[256];                  ///< Name of the shared memory segment to monitor
    size_t size;                     ///< Size of the shared memory segment (0 for auto-detect)
    unsigned int interval_ms;        ///< Interval between checks in milliseconds
    unsigned int retry_interval_ms;  ///< Interval between retry attempts in milliseconds
    unsigned int max_retries;        ///< Maximum number of retry attempts (0 for infinite)
    bool wait_indefinitely;          ///< Whether to wait indefinitely for the shared memory
    void (*callback)(const void* data, size_t size); ///< Callback function for data changes
} SharedMemoryMonitorConfig;

/**
 * @brief Initialize shared memory monitor configuration with defaults
 * 
 * @param config Pointer to configuration structure to initialize
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode shared_memory_monitor_init_config(SharedMemoryMonitorConfig* config);

/**
 * @brief Forward declaration for thread configuration
 */
struct ThreadConfig;

/**
 * @brief Get the shared memory monitor thread configuration
 * 
 * @return Pointer to thread configuration
 */
struct ThreadConfig* get_shared_memory_monitor_thread(void);

#ifdef __cplusplus
}
#endif

#endif // SHARED_MEMORY_MONITOR_H
