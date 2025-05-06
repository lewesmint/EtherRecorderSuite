#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shared_memory_monitor.h"
#include "shared_memory_sync.h"
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
#include "shutdown_handler.h"

typedef struct {
    uint32_t offset;
    uint32_t length;
} ChangeRegion;

// Detect multiple distinct changed regions in memory
static int detect_memory_changes(
    const void* current, 
    const void* previous, 
    size_t size, 
    ChangeRegion* regions, 
    int max_regions
) {
    // Sanity check - max_regions should never exceed half the buffer size
    if (max_regions > size / 2) {
        logger_log(LOG_WARN, "Max regions (%d) exceeds half buffer size (%zu), capping at %zu", 
                  max_regions, size, size / 2);
        max_regions = (int)(size / 2);
    }

    const uint8_t* curr_bytes = (const uint8_t*)current;
    const uint8_t* prev_bytes = (const uint8_t*)previous;
    int region_count = 0;
    bool in_region = false;
    uint32_t region_start = 0;
    
    // Scan through memory byte by byte
    for (uint32_t i = 0; i < size; i++) {
        bool is_different = (curr_bytes[i] != prev_bytes[i]);
        
        if (is_different && !in_region) {
            // Start of a new change region
            in_region = true;
            region_start = i;
        } 
        else if (!is_different && in_region) {
            // End of a change region
            in_region = false;
            uint32_t region_length = i - region_start;
            
            // Record the region
            if (region_count < max_regions) {
                regions[region_count].offset = region_start;
                regions[region_count].length = region_length;
                region_count++;
            } else {
                // We've hit the maximum number of regions we can track
                logger_log(LOG_ERROR, "Too many changed regions detected (%d+), possible data corruption", 
                          max_regions);
                return -1;  // Error condition
            }
        }
    }
    
    // Handle case where the last region extends to the end of the buffer
    if (in_region) {
        uint32_t region_length = (uint32_t)(size - region_start);
        
        if (region_count < max_regions) {
            regions[region_count].offset = region_start;
            regions[region_count].length = region_length;
            region_count++;
        } else {
            logger_log(LOG_ERROR, "Too many changed regions detected (%d+), possible data corruption", 
                      max_regions);
            return -1;  // Error condition
        }
    }
    
    return region_count;
}

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
static SharedMemoryMonitorConfig default_monitor_config = {
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

// Open and map shared memory
static PlatformErrorCode open_and_map_shared_memory(
    SharedMemoryMonitorConfig* config,
    PlatformSharedMemoryHandle* handle,
    void** mapped_data
) {
    logger_log(LOG_INFO, "Attempting to open shared memory '%s'", config->name);
    
    PlatformErrorCode result = platform_shared_memory_open(
        handle,
        config->name,
        config->data_size,
        config->access,
        config->create
    );
    
    if (result != PLATFORM_ERROR_SUCCESS) {
        logger_log(LOG_WARN, "Failed to open shared memory '%s': error %d", 
                 config->name, result);
        return result;
    }
    
    // If auto-detect size was requested (data_size == 0), get the actual size
    if (config->data_size == 0) {
        size_t actual_size = 0;
        result = platform_shared_memory_get_size(*handle, &actual_size);
        if (result != PLATFORM_ERROR_SUCCESS) {
            logger_log(LOG_ERROR, "Failed to get shared memory size: error %d", result);
            platform_shared_memory_close(*handle);
            *handle = NULL;
            return result;
        }
        
        config->data_size = actual_size;
        logger_log(LOG_INFO, "Auto-detected shared memory size: %zu bytes", config->data_size);
    }
    
    // Map the shared memory
    result = platform_shared_memory_map(*handle, mapped_data);
    if (result != PLATFORM_ERROR_SUCCESS) {
        logger_log(LOG_ERROR, "Failed to map shared memory '%s': error %d", 
                 config->name, result);
        platform_shared_memory_close(*handle);
        *handle = NULL;
        return result;
    }
    
    logger_log(LOG_INFO, "Successfully opened and mapped shared memory '%s' (%zu bytes)", 
             config->name, config->data_size);
    
    return PLATFORM_ERROR_SUCCESS;
}

// Process and forward memory changes
static void process_memory_changes(
    SharedMemoryMonitorConfig* config,
    void* mapped_data,
    void* last_data,
    bool* first_read
) {
    // Check for changes in shared memory data
    if (*first_read) {
        // On first read, send entire memory block
        shared_memory_udp_sender_callback_with_offset(mapped_data, config->data_size, 0);
        logger_log(LOG_DEBUG, "Sent initial full memory state (%zu bytes)", config->data_size);
        
        // Copy current data to our comparison buffer
        memcpy(last_data, mapped_data, config->data_size);
        *first_read = false;
    } else {
        // Detect specific changed regions
        #define MAX_CHANGE_REGIONS 100
        ChangeRegion change_regions[MAX_CHANGE_REGIONS];
        int num_regions = detect_memory_changes(
            mapped_data, 
            last_data, 
            config->data_size, 
            change_regions, 
            MAX_CHANGE_REGIONS
        );
        
        if (num_regions > 0) {
            logger_log(LOG_DEBUG, "Detected %d changed regions in shared memory '%s'", 
                      num_regions, config->name);
            
            // Forward each changed region
            for (int i = 0; i < num_regions; i++) {
                uint32_t offset = change_regions[i].offset;
                uint32_t length = change_regions[i].length;
                
                // Send the changed region
                shared_memory_udp_sender_callback_with_offset(
                    (uint8_t*)mapped_data + offset, 
                    length, 
                    offset
                );
                
                logger_log(LOG_DEBUG, "Forwarded changed region: offset=%u, length=%u", 
                         offset, length);
            }
            
            // Update our comparison buffer with the new state
            memcpy(last_data, mapped_data, config->data_size);
        }
    }
}

// Main thread function for monitoring shared memory
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
            PlatformErrorCode result = open_and_map_shared_memory(config, &shm_handle, &mapped_data);
            
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
        }
        
        // At this point, we have successfully opened and mapped the shared memory
        logger_log(LOG_DEBUG, "Block %d monitor thread for '%s' is alive", 
                 block_index, config->name);
        
        // Allocate buffer for last data to detect changes if we're in read mode
        void* last_data = NULL;
        bool should_detect_changes = (config->access == PLATFORM_SHM_READ || 
                                     config->access == PLATFORM_SHM_READWRITE);

        if (should_detect_changes) {
            last_data = malloc(config->data_size);
            if (!last_data) {
                logger_log(LOG_ERROR, "Failed to allocate memory for data comparison");
                platform_shared_memory_unmap(shm_handle);
                platform_shared_memory_close(shm_handle);
                shm_handle = NULL;
                continue;
            }
            
            // Initialize with zeros
            memset(last_data, 0, config->data_size);
        }

        // Main monitoring loop
        bool first_read = true;
        PlatformMutex_T shm_mutex;
        platform_mutex_init(&shm_mutex);

        while (!shutdown_signalled()) {
            // Lock shared memory for reading
            PlatformErrorCode lock_result = platform_mutex_lock(&shm_mutex);
            if (lock_result != PLATFORM_ERROR_SUCCESS) {
                logger_log(LOG_WARN, "Failed to lock shared memory for reading: %d", lock_result);
                sleep_ms(config->interval_ms);
                continue;
            }
            
            if (should_detect_changes) {
                process_memory_changes(config, mapped_data, last_data, &first_read);
            } else {
                // For write-only mode, we don't need to send updates
                logger_log(LOG_DEBUG, "Write-only mode, not sending updates");
            }
            
            // Unlock shared memory
            platform_mutex_unlock(&shm_mutex);
            
            // Sleep for the configured interval
            sleep_ms(config->interval_ms);
        }

        // Clean up mutex
        platform_mutex_destroy(&shm_mutex);

        // Clean up
        if (last_data) {
            free(last_data);
            last_data = NULL;
        }
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
