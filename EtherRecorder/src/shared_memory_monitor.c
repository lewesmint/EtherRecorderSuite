#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shared_memory_monitor.h"
#include "platform_shared_memory.h"
#include "platform_error.h"
#include "platform_time.h"
#include "platform_threads.h"
#include "platform_sockets.h"
#include "platform_string.h"
#include "platform_mutex.h"
#include "thread_registry.h"
#include "logger.h"
#include "app_config.h"

// // Worker thread state
// typedef struct {
//     PlatformThreadId thread_id;
//     PlatformMutex_T mutex;
//     PlatformCondition_T condition;
//     bool shutdown;
//     bool busy;
//     const void* data;
//     size_t data_size;
//     void* context;
// } WorkerThreadState;

// // Shared memory monitor context
// typedef struct {
//     SharedMemoryMonitorConfig config;
//     PlatformSharedMemoryHandle handle;
//     void* mapped_data;
//     void* last_data;
//     size_t actual_size;
    
//     // For network forwarding
//     PlatformSocketHandle socket_handle;
//     bool socket_connected;
    
//     // Worker thread pool
//     WorkerThreadState* workers;
//     int num_workers;
//     int next_worker;
//     PlatformMutex_T mutex;
// } MonitorContext;

// Default configuration
static SharedMemoryMonitorConfig defaut_monitor_config = {
    .name = "TestSharedMemory",
    .create = true,
    .data_size = 0,                 // Auto-detect size
    .access = PLATFORM_SHM_READ,   // Read-only access by default
    .interval_ms = 100,           // Check every 10th of a second
    .retry_interval_ms = 2000,     // Retry every 2 seconds
    .max_retries = 5,              // 5 retries by default
    .wait_indefinitely = true,     // Wait indefinitely by default
    // .num_threads = 2,              // Use 2 worker threads by default
    // .forwarding = {
    //     .hostname = "localhost",
    //     .port = 5000,
    //     .use_tcp = false,         // Default to UDP
    //     .retry_count = 3,
    //     .retry_interval_ms = 500
    // }
};

// Initialize a shared memory monitor configuration with defaults
PlatformErrorCode shared_memory_monitor_init_config(SharedMemoryMonitorConfig* config) {
    if (!config) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    // // Start with default configuration
    // *config = monitor_config;
    return PLATFORM_ERROR_SUCCESS;
}

// Individual thread for monitoring a single shared memory block
void* shared_memory_monitor_thread(void* arg) {
    ThreadConfig* thread_config = (ThreadConfig*)arg;
    if (!thread_config) {
        logger_log(LOG_ERROR, "Invalid thread configuration");
        return NULL;
    }
    
    SharedMemoryMonitorConfig* config = (SharedMemoryMonitorConfig*)thread_config->data;
    if (!config) {
        logger_log(LOG_ERROR, "Invalid shared memory configuration");
        return NULL;
    }
    
    int block_index = config->block_index;
    
    logger_log(LOG_INFO, "Block %d monitor thread starting for '%s'", 
              block_index, config->name);
    
    // Log the configuration details
    logger_log(LOG_INFO, "Configuration for block %d:", block_index);
    logger_log(LOG_INFO, "  Name: %s", config->name);
    logger_log(LOG_INFO, "  Size: %zu bytes", config->data_size);
    logger_log(LOG_INFO, "  Access: %d", config->access);
    logger_log(LOG_INFO, "  Create: %s", config->create ? "true" : "false");
    logger_log(LOG_INFO, "  Interval: %u ms", config->interval_ms);
    logger_log(LOG_INFO, "  Network: %s:%d", 
             config->forwarding.hostname, config->forwarding.port);
    
    // Validate configuration
    if (config->create && config->data_size == 0) {
        logger_log(LOG_ERROR, "Cannot create shared memory with zero size for block %d", block_index);
        return NULL;
    }
    
    PlatformSharedMemoryHandle shm_handle = NULL;
    void* mapped_data = NULL;
    unsigned int retry_count = 0;
    
    // Main loop - try to open shared memory and monitor it
    while (!shutdown_signalled()) {
        // If we don't have an open handle, try to open it
        if (shm_handle == NULL) {
            logger_log(LOG_INFO, "Attempting to open shared memory '%s'", config->name);
            
            PlatformErrorCode result = platform_shared_memory_open(
                &shm_handle,
                config->name,
                config->data_size,
                config->access,
                config->create
            );
            
            if (result != PLATFORM_ERROR_SUCCESS) {
                retry_count++;
                logger_log(LOG_WARN, "Failed to open shared memory '%s' (attempt %u): error %d", 
                         config->name, retry_count, result);
                
                // Wait before retrying
                sleep_ms(config->retry_interval_ms);
                continue;
            }
            
            // Reset retry count on success
            retry_count = 0;
            
            // If auto-detect size was requested (data_size == 0), get the actual size
            if (config->data_size == 0) {
                size_t actual_size = 0;
                result = platform_shared_memory_get_size(shm_handle, &actual_size);
                if (result != PLATFORM_ERROR_SUCCESS) {
                    logger_log(LOG_ERROR, "Failed to get shared memory size: error %d", result);
                    platform_shared_memory_close(shm_handle);
                    shm_handle = NULL;
                    sleep_ms(config->retry_interval_ms);
                    continue;
                }
                
                config->data_size = actual_size;
                logger_log(LOG_INFO, "Auto-detected shared memory size: %zu bytes", config->data_size);
            }
            
            // Map the shared memory
            result = platform_shared_memory_map(shm_handle, &mapped_data);
            if (result != PLATFORM_ERROR_SUCCESS) {
                logger_log(LOG_ERROR, "Failed to map shared memory '%s': error %d", 
                         config->name, result);
                platform_shared_memory_close(shm_handle);
                shm_handle = NULL;
                sleep_ms(config->retry_interval_ms);
                continue;
            }
            
            logger_log(LOG_INFO, "Successfully opened and mapped shared memory '%s' (%zu bytes)", 
                     config->name, config->data_size);
        }
        
        // At this point, we have successfully opened and mapped the shared memory
        logger_log(LOG_DEBUG, "Block %d monitor thread for '%s' is alive", 
                 block_index, config->name);
        
        // Sleep for the configured interval
        sleep_ms(config->interval_ms);
    }
    
    // Clean up resources
    if (mapped_data != NULL) {
        platform_shared_memory_unmap(shm_handle);
    }
    
    if (shm_handle != NULL) {
        platform_shared_memory_close(shm_handle);
    }
    
    logger_log(LOG_INFO, "Block %d monitor thread shutting down for '%s'", 
              block_index, config->name);
    
    return NULL;
}
