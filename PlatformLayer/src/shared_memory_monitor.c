#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shared_memory_monitor.h"
#include "platform_shared_memory.h"
#include "platform_thread.h"
#include "platform_time.h"
#include "thread_registry.h"
#include "app_thread.h"
#include "logger.h"
#include "config_manager.h"
#include "shutdown_manager.h"

// Default configuration
static SharedMemoryMonitorConfig monitor_config = {
    .name = "TestSharedMemory",
    .size = 0,                  // Auto-detect size
    .interval_ms = 1000,        // Check every second
    .retry_interval_ms = 2000,  // Retry every 2 seconds
    .max_retries = 5,           // 5 retries by default
    .wait_indefinitely = true,  // Wait indefinitely by default
    .callback = NULL            // No callback by default
};

// Forward declarations
static void* shared_memory_monitor_thread(void* arg);
static void default_data_change_callback(const void* data, size_t size);

PlatformErrorCode shared_memory_monitor_init_config(SharedMemoryMonitorConfig* config) {
    if (!config) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    // Start with default configuration
    *config = monitor_config;

    // Override with values from configuration file
    const char* name = get_config_string("shared_memory", "name", config->name);
    strncpy(config->name, name, sizeof(config->name) - 1);
    config->name[sizeof(config->name) - 1] = '\0';

    config->size = (size_t)get_config_int("shared_memory", "size", config->size);
    config->interval_ms = (unsigned int)get_config_int("shared_memory", "interval_ms", config->interval_ms);
    config->retry_interval_ms = (unsigned int)get_config_int("shared_memory", "retry_interval_ms", config->retry_interval_ms);
    config->max_retries = (unsigned int)get_config_int("shared_memory", "max_retries", config->max_retries);
    config->wait_indefinitely = get_config_bool("shared_memory", "wait_indefinitely", config->wait_indefinitely);

    // Set default callback if none provided
    if (!config->callback) {
        config->callback = default_data_change_callback;
    }

    return PLATFORM_ERROR_SUCCESS;
}

ThreadConfig* get_shared_memory_monitor_thread(void) {
    static ThreadConfig monitor_thread = {
        .label = "SHARED_MEMORY_MONITOR",
        .func = shared_memory_monitor_thread,
        .data = &monitor_config,
        .suppressed = false
    };

    // Initialize monitor configuration with defaults and config file values
    shared_memory_monitor_init_config(&monitor_config);
    
    return &monitor_thread;
}

static void default_data_change_callback(const void* data, size_t size) {
    if (!data || size == 0) {
        return;
    }

    // Ensure the data is null-terminated for string operations
    const char* str_data = (const char*)data;
    
    // Log the first 100 characters of the data (or less if smaller)
    size_t log_size = (size > 100) ? 100 : size;
    char buffer[101] = {0};
    memcpy(buffer, str_data, log_size);
    
    logger_log(LOG_INFO, "Shared memory data changed: '%s'%s", 
              buffer, (size > 100) ? "..." : "");
}

static PlatformErrorCode monitor_shared_memory(const SharedMemoryMonitorConfig* config) {
    if (!config) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    logger_log(LOG_INFO, "Starting shared memory monitor for '%s'", config->name);

    PlatformSharedMemoryHandle handle = NULL;
    void* data = NULL;
    void* last_data = NULL;
    size_t actual_size = config->size;
    unsigned int retries = 0;
    bool first_read = true;

    // Allocate buffer for last data if we're using a callback
    if (config->callback && config->size > 0) {
        last_data = malloc(config->size);
        if (!last_data) {
            logger_log(LOG_ERROR, "Failed to allocate memory for data comparison");
            return PLATFORM_ERROR_OUT_OF_MEMORY;
        }
        memset(last_data, 0, config->size);
    }

    while (!shutdown_signalled()) {
        // Try to open the shared memory
        PlatformErrorCode err = platform_shared_memory_open(
            &handle,
            config->name,
            actual_size,
            PLATFORM_SHM_READ,
            false  // Open existing
        );

        if (err != PLATFORM_ERROR_SUCCESS) {
            logger_log(LOG_WARNING, "Failed to open shared memory '%s'", config->name);
            
            // Check if we should retry
            if (config->wait_indefinitely || retries < config->max_retries) {
                retries++;
                logger_log(LOG_INFO, "Retrying in %d ms... (Attempt %d%s)",
                          config->retry_interval_ms, retries,
                          config->wait_indefinitely ? "" : 
                          config->max_retries > 0 ? 
                          config->max_retries > 0 ? "/" : "" : "");
                
                sleep_ms(config->retry_interval_ms);
                continue;
            } else {
                logger_log(LOG_ERROR, "Failed to open shared memory after %d attempts", retries);
                if (last_data) {
                    free(last_data);
                }
                return err;
            }
        }

        // Reset retry counter on successful open
        retries = 0;

        // Map the shared memory
        err = platform_shared_memory_map(handle, &data);
        if (err != PLATFORM_ERROR_SUCCESS) {
            logger_log(LOG_ERROR, "Failed to map shared memory");
            platform_shared_memory_close(handle);
            if (last_data) {
                free(last_data);
            }
            return err;
        }

        // Get the actual size if we're auto-detecting
        if (actual_size == 0) {
            // The size should be available after mapping
            // We need to implement a way to get the size from the handle
            // For now, we'll use a default size
            actual_size = 1024;
            logger_log(LOG_INFO, "Auto-detected shared memory size: %zu bytes", actual_size);
            
            // Reallocate last_data buffer with the correct size
            if (config->callback) {
                if (last_data) {
                    free(last_data);
                }
                last_data = malloc(actual_size);
                if (!last_data) {
                    logger_log(LOG_ERROR, "Failed to allocate memory for data comparison");
                    platform_shared_memory_unmap(handle);
                    platform_shared_memory_close(handle);
                    return PLATFORM_ERROR_OUT_OF_MEMORY;
                }
                memset(last_data, 0, actual_size);
            }
        }

        // Monitor the shared memory for changes
        logger_log(LOG_INFO, "Successfully connected to shared memory '%s'", config->name);
        
        while (!shutdown_signalled()) {
            // Lock the shared memory
            err = platform_shared_memory_lock(handle, 100);  // Short timeout
            if (err != PLATFORM_ERROR_SUCCESS) {
                if (err == PLATFORM_ERROR_TIMEOUT) {
                    // Timeout is not an error, just try again
                    sleep_ms(10);
                    continue;
                } else {
                    logger_log(LOG_ERROR, "Failed to lock shared memory");
                    break;
                }
            }

            // Check for changes if we have a callback
            if (config->callback) {
                if (first_read) {
                    // First read, just store the data
                    memcpy(last_data, data, actual_size);
                    first_read = false;
                    
                    // Call the callback with the initial data
                    config->callback(data, actual_size);
                } else if (memcmp(last_data, data, actual_size) != 0) {
                    // Data has changed
                    memcpy(last_data, data, actual_size);
                    
                    // Call the callback with the new data
                    config->callback(data, actual_size);
                }
            }

            // Unlock the shared memory
            platform_shared_memory_unlock(handle);

            // Wait for the next check
            sleep_ms(config->interval_ms);
        }

        // Clean up
        platform_shared_memory_unmap(handle);
        platform_shared_memory_close(handle);
        
        if (shutdown_signalled()) {
            break;
        }
        
        // If we get here, the shared memory was closed or became unavailable
        logger_log(LOG_WARNING, "Lost connection to shared memory '%s'", config->name);
        
        // Reset first_read flag for the next connection
        first_read = true;
        
        // Wait before trying to reconnect
        sleep_ms(config->retry_interval_ms);
    }

    if (last_data) {
        free(last_data);
    }
    
    logger_log(LOG_INFO, "Shared memory monitor shutting down");
    return PLATFORM_ERROR_SUCCESS;
}

static void* shared_memory_monitor_thread(void* arg) {
    ThreadConfig* thread_config = (ThreadConfig*)arg;
    SharedMemoryMonitorConfig* config = (SharedMemoryMonitorConfig*)thread_config->data;
    
    if (!config) {
        logger_log(LOG_ERROR, "Invalid shared memory monitor configuration");
        return NULL;
    }

    logger_log(LOG_INFO, "Shared memory monitor thread starting");
    thread_registry_update_state(thread_config->label, THREAD_STATE_RUNNING);

    // Run the monitor
    PlatformErrorCode err = monitor_shared_memory(config);
    if (err != PLATFORM_ERROR_SUCCESS) {
        logger_log(LOG_ERROR, "Shared memory monitor failed with error code %d", err);
        thread_registry_update_state(thread_config->label, THREAD_STATE_FAILED);
    } else {
        thread_registry_update_state(thread_config->label, THREAD_STATE_STOPPED);
    }

    return NULL;
}
