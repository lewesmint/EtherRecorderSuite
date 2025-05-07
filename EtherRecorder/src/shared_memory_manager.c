/**
 * @file shared_memory_manager.c
 * @brief Manager for multiple shared memory monitors
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "shared_memory_manager.h"
#include "shared_memory_monitor.h"
#include "platform_string.h"
#include "platform_error.h"
#include "platform_time.h"
#include "logger.h"
#include "app_config.h"
#include "app_thread.h"
#include "shutdown_handler.h"

// Shutdown the shared memory manager
void shared_memory_manager_shutdown(void) {
    logger_log(LOG_INFO, "Shutting down shared memory manager");
    
    // // Free thread data if allocated
    // if (g_block_thread_data) {
    //     free(g_block_thread_data);
    //     g_block_thread_data = NULL;
    //     g_block_thread_count = 0;
    // }
}

// Load configuration from config.ini
PlatformErrorCode shared_memory_load_config_from_ini(
    SharedMemoryMonitorConfig* config
) {
    if (!config) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    // Parse section name to extract block number
    int block_num = config->block_index;
    if (block_num <= 0) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    char key_prefix[32];
    snprintf(key_prefix, sizeof(key_prefix), "block%d.", block_num);
    
    // Load shared memory configuration using block prefix
    char key[256];
    
    // Get name
    snprintf(key, sizeof(key), "%sname", key_prefix);
    const char* name = get_config_string("shared_memory", key, config->name);
    if (name) {
        strncpy(config->name, name, sizeof(config->name) - 1);
        config->name[sizeof(config->name) - 1] = '\0';
    }
    
    // Get size
    snprintf(key, sizeof(key), "%sdata_size", key_prefix);
    config->data_size = (size_t)get_config_int("shared_memory", key, (int)config->data_size);
    
    // Get access mode
    snprintf(key, sizeof(key), "%saccess", key_prefix);
    const char* access_str = get_config_string("shared_memory", key, "read");
    if (access_str) {
        if (strcmp_nocase(access_str, "read") == 0) {
            config->access = PLATFORM_SHM_READ;
        } else if (strcmp_nocase(access_str, "write") == 0) {
            config->access = PLATFORM_SHM_WRITE;
        } else if (strcmp_nocase(access_str, "readwrite") == 0) {
            config->access = PLATFORM_SHM_READWRITE;
        }
    }
    
    // Get hostname
    snprintf(key, sizeof(key), "%shostname", key_prefix);
    const char* hostname = get_config_string("shared_memory", key, "localhost");
    strncpy(config->forwarding.hostname, hostname, sizeof(config->forwarding.hostname) - 1);
    config->forwarding.hostname[sizeof(config->forwarding.hostname) - 1] = '\0';
    
    // Get listen port
    snprintf(key, sizeof(key), "%slisten_port", key_prefix);
    config->forwarding.listen_port = (uint16_t)get_config_int("shared_memory", key, 5000);

    // Get forward port
    snprintf(key, sizeof(key), "%sforward_port", key_prefix);
    config->forwarding.forward_port = (uint16_t)get_config_int("shared_memory", key, config->forwarding.listen_port);
    
    // Get create flag
    snprintf(key, sizeof(key), "%screate", key_prefix);
    config->create = get_config_bool("shared_memory", key, true);
    
    // Get update interval
    snprintf(key, sizeof(key), "%supdate_interval_ms", key_prefix);
    config->interval_ms = (unsigned int)get_config_int("shared_memory", key, config->interval_ms);
    
    // Get retry count
    snprintf(key, sizeof(key), "%smax_retries", key_prefix);
    config->max_retries = (unsigned int)get_config_int("shared_memory", key, 3);
    
    // Get retry interval
    snprintf(key, sizeof(key), "%sretry_interval_ms", key_prefix);
    config->retry_interval_ms = (unsigned int)get_config_int("shared_memory", key, 500);
    
    logger_log(LOG_INFO, "Loaded configuration for shared memory block %d: %s", 
             block_num, config->name);
    
    return PLATFORM_ERROR_SUCCESS;
}

// Main manager thread that spawns individual monitor threads
static void* shared_memory_manager_thread(void* arg) {
    ThreadConfig* thread_config = (ThreadConfig*)arg;
    if (!thread_config) {
        logger_log(LOG_ERROR, "Invalid shared memory manager thread configuration");
        return NULL;
    }
    
    // Get the number of blocks from config
    int block_count = get_config_int("shared_memory", "block_count", 0);
    if (block_count <= 0) {
        logger_log(LOG_INFO, "No shared memory blocks configured (block_count=%d)", block_count);
        return NULL;
    }
    
    logger_log(LOG_INFO, "Shared memory manager thread starting with %d potential blocks", block_count);
    
    // Allocate maximum possible thread data
    SharedMemoryMonitorConfig* block_thread_data = (SharedMemoryMonitorConfig*)malloc(block_count * sizeof(*block_thread_data));
    if (!block_thread_data) {
        logger_log(LOG_ERROR, "Failed to allocate memory for block thread data");
        return NULL;
    }

    // Process each block
    for (int i = 1; i <= block_count; i++) {
        // Initialize config with defaults
        SharedMemoryMonitorConfig* config = &block_thread_data[i - 1];
        memset(config, 0, sizeof(*config));
        config->block_index = i;
        
        // Try to load configuration
        char block_name[32];
        snprintf(block_name, sizeof(block_name), "block%d", i);
        PlatformErrorCode result = shared_memory_load_config_from_ini(config);
        if (result != PLATFORM_ERROR_SUCCESS) {
            logger_log(LOG_WARN, "Failed to load configuration for %s", block_name);
            continue;
        }
        
        // Skip blocks with empty names
        if (config->name[0] == '\0') {
            logger_log(LOG_INFO, "Skipping block %d (no name configured)", i);
            continue;
        }
        
        // Allocate persistent memory for thread config
        ThreadConfig* block_thread_config = (ThreadConfig*)malloc(sizeof(ThreadConfig));
        if (!block_thread_config) {
            logger_log(LOG_ERROR, "Failed to allocate memory for thread config");
            continue;
        }
        
        // Allocate persistent memory for thread label
        char* thread_label = (char*)malloc(32);
        if (!thread_label) {
            logger_log(LOG_ERROR, "Failed to allocate memory for thread label");
            free(block_thread_config);
            continue;
        }
        
        // Create thread name
        snprintf(thread_label, 32, "SM_MONITOR_%d", i);
        
        // Create thread configuration
        *block_thread_config = (ThreadConfig){
            .label = thread_label,
            .func = shared_memory_monitor_thread,
            .data = config,
            .suppressed = false
        };
        
        // Start the thread
        ThreadResult thread_result = app_thread_create(block_thread_config);
        if (thread_result != THREAD_SUCCESS) {
            logger_log(LOG_ERROR, "Failed to create thread for block %d: %s", 
                     i, config->name);
            // Free resources only if thread creation failed
            free(thread_label);
            free(block_thread_config);
        } else {
            logger_log(LOG_INFO, "Started monitor thread for block %d: %s", 
                     i, config->name);
            // Don't free the resources - the thread is using them
        }
    }
    
    logger_log(LOG_INFO, "Started %d shared memory monitor threads", block_count);
    
    // Wait until shutdown is signaled
    while (!shutdown_signalled()) {
        sleep_ms(1000);
    }
    
    logger_log(LOG_INFO, "Shared memory manager thread shutting down");
    
    // Wait for all block threads to terminate
    // Note: In a real implementation, you might want to signal them to shut down
    
    return NULL;
}

// Get the shared memory manager thread configuration
struct ThreadConfig* get_shared_memory_manager_thread(void) {
    static ThreadConfig manager_thread = {
        .label = "SM_MANAGER",
        .func = shared_memory_manager_thread,
        .data = NULL,
        .suppressed = false
    };
    
    // Check if we have any shared memory blocks configured
    int block_count = get_config_int("shared_memory", "block_count", 0);
    if (block_count <= 0) {
        logger_log(LOG_INFO, "No shared memory blocks configured, suppressing manager thread");
        manager_thread.suppressed = true;
    } else {
        manager_thread.suppressed = false;
    }
    
    return &manager_thread;
}
