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

/**
 * @brief Network forwarding configuration for shared memory
 */
typedef struct {
    char hostname[256];             ///< Hostname/IP of destination EtherRecorder instance
    int port;                       ///< UDP port for communication
} NetworkForwardingConfig;

/**
 * @brief Configuration for shared memory monitor
 */
typedef struct {
    // Configuration parameters
    int block_index;
    char name[256];                      ///< Name of the shared memory segment to monitor
    bool create;                         ///< Whether to create the shared memory if it doesn't exist
    size_t data_size;                    ///< Size of the shared memory segment (0 for auto-detect)
    PlatformSharedMemoryAccess access;   ///< Access mode for the shared memory
    unsigned int interval_ms;            ///< Interval between checks in milliseconds
    unsigned int retry_count;            ///< Current retry attempt count
    unsigned int retry_interval_ms;      ///< Interval between retry attempts in milliseconds
    unsigned int max_retries;            ///< Maximum number of retry attempts (0 for infinite)
    bool wait_indefinitely;              ///< Whether to wait indefinitely for the shared memory
    bool detect_changes;                 ///< Whether to detect changes or forward all data
    int num_threads;                     ///< Number of worker threads for processing (0 = auto)
    NetworkForwardingConfig forwarding;  ///< Network forwarding configuration
} SharedMemoryMonitorConfig;

/**
 * @brief Initialize shared memory monitor configuration with defaults
 * 
 * @param config Pointer to configuration structure to initialize
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode shared_memory_monitor_init_config(SharedMemoryMonitorConfig* config);

/**
 * @brief Set up network forwarding to another EtherRecorder instance
 * 
 * @param config Pointer to monitor configuration
 * @param hostname Hostname or IP address of the target EtherRecorder instance
 * @param port Port number for communication
 * @param use_tcp Whether to use TCP (true) or UDP (false)
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode shared_memory_set_network_forwarding(
    SharedMemoryMonitorConfig* config,
    const char* hostname,
    int port
);

/**
 * @brief Load shared memory monitoring configuration from config.ini
 * 
 * @param config Pointer to monitor configuration to populate
 * @param section_name Name of the configuration section to load from
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode shared_memory_load_config_from_ini(
    SharedMemoryMonitorConfig* config
);

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

/**
 * @brief Thread function for monitoring a single shared memory block
 * 
 * @param arg Pointer to BlockMonitorThreadData
 * @return Thread result
 */
void* shared_memory_monitor_thread(void* arg);

#endif // SHARED_MEMORY_MONITOR_H
