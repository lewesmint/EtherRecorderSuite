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
};

// Forward declarations
static void* shared_memory_monitor_thread(void* arg);
static void* monitor_block_thread(void* arg);

// Structure to pass to individual monitor threads
typedef struct {
    SharedMemoryMonitorConfig config;
    int block_index;
} BlockMonitorThreadData;

// Array to store thread data for each block
static BlockMonitorThreadData* g_block_thread_data = NULL;
static int g_block_thread_count = 0;

// Initialize a shared memory monitor configuration with defaults
PlatformErrorCode shared_memory_monitor_init_config(SharedMemoryMonitorConfig* config) {
    if (!config) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    // // Start with default configuration
    // *config = monitor_config;
    return PLATFORM_ERROR_SUCCESS;
}

// Get all shared memory block configurations from config.ini
PlatformErrorCode shared_memory_get_all_configs(SharedMemoryMonitorConfig** configs, int* count) {
    if (!configs || !count) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    // Get the number of blocks from config
    int block_count = get_config_int("shared_memory", "block_count", 0);
    
    if (block_count <= 0) {
        *count = 0;
        *configs = NULL;
        logger_log(LOG_INFO, "No shared memory blocks configured (block_count=%d)", block_count);
        return PLATFORM_ERROR_SUCCESS;  // No blocks defined, not an error
    }
    
    // Allocate memory for the configurations
    *configs = (SharedMemoryMonitorConfig*)malloc(block_count * sizeof(SharedMemoryMonitorConfig));
    if (!*configs) {
        return PLATFORM_ERROR_OUT_OF_MEMORY;
    }
    
    // Initialize with defaults
    for (int i = 0; i < block_count; i++) {
        shared_memory_monitor_init_config(&(*configs)[i]);
    }
    
    // Load each configuration
    for (int i = 1; i <= block_count; i++) {
        char block_name[32];
        snprintf(block_name, sizeof(block_name), "block%d", i);
        
        PlatformErrorCode result = shared_memory_load_config_from_ini(&(*configs)[i-1], block_name);
        if (result != PLATFORM_ERROR_SUCCESS) {
            logger_log(LOG_WARN, "Failed to load configuration for %s", block_name);
        } else {
            logger_log(LOG_INFO, "Loaded configuration for %s: name=%s, data_size=%zu", 
                     block_name, (*configs)[i-1].name, (*configs)[i-1].data_size);
        }
    }
    
    *count = block_count;
    logger_log(LOG_INFO, "Loaded %d shared memory block configurations", block_count);
    return PLATFORM_ERROR_SUCCESS;
}

static void* shared_memory_monitor_thread(void* arg) {
    ThreadConfig* thread_config = (ThreadConfig*)arg;
    if (!thread_config) {
        logger_log(LOG_ERROR, "Invalid shared memory monitor thread configuration");
        return NULL;
    }
    
    // Load configurations
    SharedMemoryMonitorConfig* configs = NULL;
    int config_count = 0;
    
    PlatformErrorCode result = shared_memory_get_all_configs(&configs, &config_count);
    if (result != PLATFORM_ERROR_SUCCESS || config_count == 0) {
        logger_log(LOG_ERROR, "Failed to load shared memory configurations");
        return NULL;
    }
    
    logger_log(LOG_INFO, "Shared memory monitor thread starting with %d configurations", config_count);
    
    // Allocate thread data for each block
    g_block_thread_data = (BlockMonitorThreadData*)malloc(config_count * sizeof(BlockMonitorThreadData));
    if (!g_block_thread_data) {
        logger_log(LOG_ERROR, "Failed to allocate memory for block thread data");
        free(configs);
        return NULL;
    }
    g_block_thread_count = config_count;
    
    // Create a thread for each block
    for (int i = 0; i < config_count; i++) {
        // Copy configuration to thread data
        g_block_thread_data[i].config = configs[i];
        g_block_thread_data[i].block_index = i + 1;
        
        // Create thread name
        char thread_label[32];
        snprintf(thread_label, sizeof(thread_label), "SM_BLOCK_%d", i + 1);
        
        // Create thread configuration
        ThreadConfig block_thread_config = {
            .label = thread_label,
            .func = monitor_block_thread,
            .data = &g_block_thread_data[i],
            .suppressed = false
        };
        
        // Start the thread
        ThreadResult thread_result = app_thread_create(&block_thread_config);
        if (thread_result != THREAD_SUCCESS) {
            logger_log(LOG_ERROR, "Failed to create thread for block %d: %s", 
                     i + 1, g_block_thread_data[i].config.name);
        } else {
            logger_log(LOG_INFO, "Started monitor thread for block %d: %s", 
                     i + 1, g_block_thread_data[i].config.name);
        }
    }
    
    // Free the configs array as we've copied the data
    free(configs);
    
    // Wait until shutdown is signaled
    while (!shutdown_signalled()) {
        sleep_ms(1000);
    }
    
    logger_log(LOG_INFO, "Shared memory monitor thread shutting down");
    
    // Wait for all block threads to terminate
    // Note: In a real implementation, you might want to signal them to shut down
    
    return NULL;
}

// Individual thread for monitoring a single shared memory block
void* shared_memory_monitor_thread(void* arg) {
    BlockMonitorThreadData* thread_data = (BlockMonitorThreadData*)arg;
    if (!thread_data) {
        logger_log(LOG_ERROR, "Invalid block monitor thread data");
        return NULL;
    }
    
    SharedMemoryMonitorConfig* config = &thread_data->config;
    int block_index = thread_data->block_index;
    
    logger_log(LOG_INFO, "Block %d monitor thread starting for '%s'", 
              block_index, config->name);
    
    // TODO: Implement actual shared memory monitoring
    // 1. Open the shared memory
    PlatformSharedMemoryHandle shm_handle = NULL;
    PlatformErrorCode result = platform_shared_memory_open(
        &shm_handle,
        config->name,
        config->data_size,
        config->access,
        config->create
    );
    
    if (result != PLATFORM_ERROR_SUCCESS) {
        logger_log(LOG_ERROR, "Failed to open shared memory '%s': %d", 
                 config->name, result);
        return NULL;
    }
    
    // 2. Map it into our address space
    void* mapped_data = NULL;
    result = platform_shared_memory_map(shm_handle, &mapped_data);
    if (result != PLATFORM_ERROR_SUCCESS) {
        logger_log(LOG_ERROR, "Failed to map shared memory '%s': %d", 
                 config->name, result);
        platform_shared_memory_close(shm_handle);
        return NULL;
    }
    
    // Get actual size if auto-detect was requested
    size_t actual_size = config->data_size;
    if (actual_size == 0) {
        result = platform_shared_memory_get_size(shm_handle, &actual_size);
        if (result != PLATFORM_ERROR_SUCCESS) {
            logger_log(LOG_ERROR, "Failed to get shared memory size: %d", result);
            platform_shared_memory_unmap(shm_handle);
            platform_shared_memory_close(shm_handle);
            return NULL;
        }
        logger_log(LOG_INFO, "Auto-detected shared memory size: %zu bytes", actual_size);
    }
    
    // Allocate buffer for last data to detect changes
    void* last_data = NULL;
    if (config->detect_changes) {
        last_data = malloc(actual_size);
        if (!last_data) {
            logger_log(LOG_ERROR, "Failed to allocate memory for data comparison");
            platform_shared_memory_unmap(shm_handle);
            platform_shared_memory_close(shm_handle);
            return NULL;
        }
        
        // Initialize with zeros
        memset(last_data, 0, actual_size);
    }
    
    logger_log(LOG_INFO, "Successfully opened shared memory '%s' (%zu bytes)", 
             config->name, actual_size);
    
    // 3. Periodically check for changes
    bool first_read = true;
    
    // Main monitoring loop
    while (!shutdown_signalled()) {
        bool should_process = false;
        
        if (config->detect_changes) {
            // Check for changes in shared memory data
            if (first_read || memcmp(mapped_data, last_data, actual_size) != 0) {
                should_process = true;
                // Copy current data to our comparison buffer
                memcpy(last_data, mapped_data, actual_size);
                first_read = false;
                
                logger_log(LOG_DEBUG, "Detected changes in shared memory '%s'", config->name);
            }
        } else {
            // Always process all data
            should_process = true;
        }
        
        // 4. Forward changes to the network if needed
        if (should_process) {
            // TODO: Implement network forwarding
            logger_log(LOG_DEBUG, "Processing shared memory data for '%s'", config->name);
        }
        
        // Wait before next check
        sleep_ms(config->interval_ms);
    }
    
    logger_log(LOG_INFO, "Block %d monitor thread shutting down for '%s'", 
              block_index, config->name);
    
    // Clean up
    if (last_data) {
        free(last_data);
    }
    
    platform_shared_memory_unmap(shm_handle);
    platform_shared_memory_close(shm_handle);
    
    return NULL;
}

// // Set up network forwarding
// PlatformErrorCode shared_memory_set_network_forwarding(
//     SharedMemoryMonitorConfig* config,
//     const char* hostname,
//     int port,
// ) {
//     // if (!config || !hostname || port <= 0) {
//     //     return PLATFORM_ERROR_INVALID_ARGUMENT;
//     // }
    
//     // strncpy(config->forwarding.hostname, hostname, sizeof(config->forwarding.hostname) - 1);
//     // config->forwarding.hostname[sizeof(config->forwarding.hostname) - 1] = '\0';
//     // config->forwarding.port = port;
//     // config->forwarding.use_tcp = use_tcp;
    
//     return PLATFORM_ERROR_SUCCESS;
// }

// Load configuration from config.ini
PlatformErrorCode shared_memory_load_config_from_ini(
    SharedMemoryMonitorConfig* config,
    const char* section_name
) {
    if (!config || !section_name) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    // Start with defaults
    shared_memory_monitor_init_config(config);
    
    // Parse section name to extract block number if it's in format "blockN"
    int block_num = 0;
    if (sscanf(section_name, "block%d", &block_num) == 1) {
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
        
        // Get port
        snprintf(key, sizeof(key), "%sport", key_prefix);
        config->forwarding.port = get_config_int("shared_memory", key, 5000);
        
        // Get create flag
        snprintf(key, sizeof(key), "%screate", key_prefix);
        config->create = get_config_bool("shared_memory", key, true);
        
        // Get update interval
        snprintf(key, sizeof(key), "%supdate_interval_ms", key_prefix);
        config->interval_ms = (unsigned int)get_config_int("shared_memory", key, config->interval_ms);
        
        // Get retry count
        snprintf(key, sizeof(key), "%sretry_count", key_prefix);
        config->forwarding.retry_count = (unsigned int)get_config_int("shared_memory", key, 3);
        
        // Get retry interval
        snprintf(key, sizeof(key), "%sretry_interval_ms", key_prefix);
        config->forwarding.retry_interval_ms = (unsigned int)get_config_int("shared_memory", key, 500);
        
        logger_log(LOG_INFO, "Loaded configuration for shared memory block %d: %s", 
                 block_num, config->name);
    }
    
    return PLATFORM_ERROR_SUCCESS;
}

// Global array to store all shared memory configurations
static SharedMemoryMonitorConfig* g_shared_memory_configs = NULL;
static int g_shared_memory_config_count = 0;

struct ThreadConfig* get_shared_memory_monitor_thread(void) {
    static ThreadConfig monitor_thread = {
        .label = "SM_MONITOR",
        .func = shared_memory_monitor_thread,
        .data = NULL,
        .suppressed = false
    };
    
    // Check if we have any shared memory blocks configured
    int block_count = get_config_int("shared_memory", "block_count", 0);
    if (block_count <= 0) {
        logger_log(LOG_INFO, "No shared memory blocks configured, suppressing monitor thread");
        monitor_thread.suppressed = true;
    } else {
        monitor_thread.suppressed = false;
    }
    
    return &monitor_thread;
}

// // Initialize network connection
// static PlatformErrorCode initialize_network(MonitorContext* context) {
//     if (!context) {
//         return PLATFORM_ERROR_INVALID_ARGUMENT;
//     }
    
//     PlatformErrorCode result = PLATFORM_ERROR_SUCCESS;
//     NetworkForwardingConfig* fwd = &context->config.forwarding;
    
//     // Initialize socket subsystem
//     result = platform_socket_init();
//     if (result != PLATFORM_ERROR_SUCCESS) {
//         logger_log(LOG_ERROR, "Failed to initialize socket subsystem (error: %d)", result);
//         return result;
//     }

//     PlatformSocketOptions sock_opts = {
//         .blocking = true,
//         .send_timeout_ms = config->timeout_ms,
//         .recv_timeout_ms = config->timeout_ms,
//         .connect_timeout_ms = DEFAULT_CONNECTION_TIMEOUT_SECONDS * PLATFORM_MS_PER_SEC,
//         .keep_alive = true,       // Match server settings for connection health monitoring
//         .no_delay = true          // Better latency for our relay system
//         // Using OS defaults for buffer sizes
//     };
    
//     // Create socket
//     result = platform_socket_create(&context->socket_handle, false, sock_opts);
//     if (result != PLATFORM_ERROR_SUCCESS) {
//         logger_log(LOG_ERROR, "Failed to create socket (error: %d)", result);
//         return result;
//     }
    
//     // For TCP, establish connection
//     if (fwd->use_tcp) {
//         result = platform_socket_connect(context->socket_handle, 
//                                   fwd->hostname, 
//                                   fwd->port);
//         if (result != PLATFORM_ERROR_SUCCESS) {
//             logger_log(LOG_ERROR, "Failed to connect to %s:%d (error: %d)", 
//                      fwd->hostname, fwd->port, result);
//             platform_socket_close(context->socket_handle);
//             context->socket_handle = NULL;
//             return result;
//         }
        
//         context->socket_connected = true;
//     }
    
//     logger_log(LOG_INFO, "Network initialized for forwarding to %s:%d via %s", 
//              fwd->hostname, fwd->port, 
//              fwd->use_tcp ? "TCP" : "UDP");
    
//     return PLATFORM_ERROR_SUCCESS;
// }

// Clean up network resources
// static void cleanup_network(MonitorContext* context) {
//     if (!context) {
//         return;
//     }
    
//     if (context->socket_handle) {
//         platform_socket_close(context->socket_handle);
//         context->socket_handle = NULL;
//         context->socket_connected = false;
//     }
// }

/*
// Forward data over network
static PlatformErrorCode forward_to_network(MonitorContext* context, const void* data, size_t size) {
    if (!context || !context->socket_handle || !data || size == 0) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    PlatformErrorCode result = PLATFORM_ERROR_SUCCESS;
    NetworkForwardingConfig* fwd = &context->config.forwarding;
    size_t bytes_sent = 0;
    unsigned int retries = 0;
    
    // // Send data
    // do {
    //     if (fwd->use_tcp) {
    //         // For TCP, ensure connection is still active
    //         if (!context->socket_connected) {
    //             result = platform_socket_connect(context->socket_handle, 
    //                                       fwd->hostname, 
    //                                       fwd->port);
    //             if (result == PLATFORM_ERROR_SUCCESS) {
    //                 context->socket_connected = true;
    //             } else {
    //                 logger_log(LOG_ERROR, "Failed to reconnect to %s:%d (error: %d)", 
    //                          fwd->hostname, fwd->port, result);
    //                 return result;
    //             }
    //         }
            
    //         result = platform_socket_send(context->socket_handle, data, size, &bytes_sent);
    //     } else {
    //         // For UDP, just send
    //         result = platform_socket_send_to(context->socket_handle, 
    //                                  data, size, 
    //                                  fwd->hostname, fwd->port, 
    //                                  &bytes_sent);
    //     }
        
    //     if (result != PLATFORM_ERROR_SUCCESS) {
    //         logger_log(LOG_ERROR, "Failed to send data (error: %d), retry %d of %d", 
    //                  result, retries + 1, fwd->retry_count);
            
    //         // For TCP, reconnect on next attempt
    //         if (fwd->use_tcp) {
    //             context->socket_connected = false;
    //         }
            
    //         // Wait before retry
    //         sleep_ms(fwd->retry_interval_ms);
    //     } else if (bytes_sent < size) {
    //         logger_log(LOG_WARN, "Partial send: %zu of %zu bytes", bytes_sent, size);
    //     }
        
    //     retries++;
    // } while (result != PLATFORM_ERROR_SUCCESS && retries < fwd->retry_count);
    
    // if (result != PLATFORM_ERROR_SUCCESS) {
    //     logger_log(LOG_ERROR, "Failed to send data after %d retries", retries);
    // }
    
    return result;
}
*/

// // Worker thread function
// static void* worker_thread_function(void* arg) {
//     WorkerThreadState* state = (WorkerThreadState*)arg;
//     if (!state) {
//         return NULL;
//     }
    
//     MonitorContext* context = (MonitorContext*)state->context;
//     if (!context) {
//         return NULL;
//     }
    
//     logger_log(LOG_DEBUG, "Shared memory worker thread started");
    
//     while (!state->shutdown) {
//         // Wait for work
//         lock_mutex(&state->mutex);
//         while (!state->busy && !state->shutdown) {
//             platform_cond_wait(&state->condition, &state->mutex);
//         }
        
//         // Check if we should exit
//         if (state->shutdown) {
//             unlock_mutex(&state->mutex);
//             break;
//         }
        
//         // Process data
//         const void* data = state->data;
//         size_t size = state->data_size;
//         unlock_mutex(&state->mutex);
        
//         // Forward data over network
//         forward_to_network(context, data, size);
        
//         // Mark thread as idle
//         lock_mutex(&state->mutex);
//         state->busy = false;
//         unlock_mutex(&state->mutex);
//     }
    
//     logger_log(LOG_DEBUG, "Shared memory worker thread exiting");
//     return NULL;
// }

// // Find an available worker thread
// static WorkerThreadState* find_available_worker(MonitorContext* context) {
//     if (!context || !context->workers || context->num_workers <= 0) {
//         return NULL;
//     }
    
//     // Simple round-robin worker selection
//     for (int i = 0; i < context->num_workers; i++) {
//         int worker_idx = (context->next_worker + i) % context->num_workers;
//         WorkerThreadState* worker = &context->workers[worker_idx];
        
//         lock_mutex(&worker->mutex);
//         if (!worker->busy) {
//             worker->busy = true;
//             unlock_mutex(&worker->mutex);
//             context->next_worker = (worker_idx + 1) % context->num_workers;
//             return worker;
//         }
//         unlock_mutex(&worker->mutex);
//     }
    
//     // No available worker found
//     return NULL;
// }

// // Process data with the worker thread pool
// static void process_data(MonitorContext* context, const void* data, size_t size) {
//     if (!context || !data || size == 0) {
//         return;
//     }
    
//     // If no worker threads, process directly
//     if (context->num_workers <= 0 || !context->workers) {
//         forward_to_network(context, data, size);
//         return;
//     }
    
//     // Find an available worker
//     WorkerThreadState* worker = find_available_worker(context);
//     if (!worker) {
//         // All workers busy, process directly
//         forward_to_network(context, data, size);
//         return;
//     }
    
//     // Assign data to worker
//     lock_mutex(&worker->mutex);
//     worker->data = data;
//     worker->data_size = size;
//     platform_cond_signal(&worker->condition);
//     unlock_mutex(&worker->mutex);
// }

// // Create and initialize worker threads
// static PlatformErrorCode create_worker_threads(MonitorContext* context) {
//     if (!context) {
//         return PLATFORM_ERROR_INVALID_ARGUMENT;
//     }
    
//     int num_threads = context->config.num_threads;
    
//     // Auto-detect number of threads if not specified
//     if (num_threads <= 0) {
//         num_threads = platform_get_num_processors();
//         if (num_threads <= 0) {
//             num_threads = 2;  // Default to 2 threads if detection fails
//         }
//     }
    
//     context->num_workers = num_threads;
//     context->next_worker = 0;
    
//     // Allocate worker states
//     context->workers = calloc(num_threads, sizeof(WorkerThreadState));
//     if (!context->workers) {
//         return PLATFORM_ERROR_OUT_OF_MEMORY;
//     }
    
//     // Initialize workers
//     for (int i = 0; i < num_threads; i++) {
//         WorkerThreadState* worker = &context->workers[i];
//         worker->shutdown = false;
//         worker->busy = false;
//         worker->context = context;
        
//         if (init_mutex(&worker->mutex) != 0) {
//             logger_log(LOG_ERROR, "Failed to initialize worker mutex");
//             return PLATFORM_ERROR_MUTEX_INIT;
//         }
        
//         if (platform_cond_init(&worker->condition) != PLATFORM_ERROR_SUCCESS) {
//             logger_log(LOG_ERROR, "Failed to initialize worker condition");
//             platform_mutex_destroy(&worker->mutex);
//             return PLATFORM_ERROR_CONDITION_INIT;
//         }
        
//         // Start worker thread
//         PlatformThreadAttributes attrs = {0};
//         if (platform_thread_create(&worker->thread_id, &attrs, 
//                                worker_thread_function, worker) != PLATFORM_ERROR_SUCCESS) {
//             logger_log(LOG_ERROR, "Failed to create worker thread");
//             platform_cond_destroy(&worker->condition);
//             platform_mutex_destroy(&worker->mutex);
//             return PLATFORM_ERROR_THREAD_CREATE;
//         }
//     }
    
//     logger_log(LOG_INFO, "Created %d worker threads for shared memory processing", num_threads);
//     return PLATFORM_ERROR_SUCCESS;
// }

// // Clean up worker threads
// static void cleanup_worker_threads(MonitorContext* context) {
//     if (!context || !context->workers) {
//         return;
//     }
    
//     // Signal all workers to shut down
//     for (int i = 0; i < context->num_workers; i++) {
//         WorkerThreadState* worker = &context->workers[i];
//         lock_mutex(&worker->mutex);
//         worker->shutdown = true;
//         platform_cond_signal(&worker->condition);
//         unlock_mutex(&worker->mutex);
//     }
    
//     // Wait for all workers to finish
//     for (int i = 0; i < context->num_workers; i++) {
//         WorkerThreadState* worker = &context->workers[i];
//         platform_thread_join(worker->thread_id, NULL);
//         platform_cond_destroy(&worker->condition);
//         platform_mutex_destroy(&worker->mutex);
//     }
    
//     // Free worker array
//     free(context->workers);
//     context->workers = NULL;
//     context->num_workers = 0;
// }

// // Main shared memory monitoring function
// static PlatformErrorCode monitor_shared_memory(const SharedMemoryMonitorConfig* config) {
//     if (!config) {
//         return PLATFORM_ERROR_INVALID_ARGUMENT;
//     }
    
//     logger_log(LOG_INFO, "Starting shared memory monitor for '%s'", config->name);
    
//     // Initialize monitor context
//     MonitorContext context = {0};
//     memcpy(&context.config, config, sizeof(SharedMemoryMonitorConfig));
    
//     // Initialize mutex
//     if (init_mutex(&context.mutex) != 0) {
//         logger_log(LOG_ERROR, "Failed to initialize monitor mutex");
//         return PLATFORM_ERROR_MUTEX_INIT;
//     }
    
//     // Initialize network connection
//     PlatformErrorCode result = initialize_network(&context);
//     if (result != PLATFORM_ERROR_SUCCESS) {
//         platform_mutex_destroy(&context.mutex);
//         return result;
//     }
    
//     // Create worker threads
//     result = create_worker_threads(&context);
//     if (result != PLATFORM_ERROR_SUCCESS) {
//         cleanup_network(&context);
//         platform_mutex_destroy(&context.mutex);
//         return result;
//     }
    
//     unsigned int retries = 0;
//     bool first_read = true;
    
//     // Try to open shared memory
//     while (!shutdown_signalled()) {
//         result = platform_shared_memory_open(
//             &context.handle, 
//             config->name, 
//             config->size,
//             config->access, 
//             false  // Open existing
//         );
        
//         if (result == PLATFORM_ERROR_SUCCESS) {
//             logger_log(LOG_INFO, "Successfully opened shared memory '%s'", config->name);
//             break;
//         }
        
//         // Handle shared memory not found
//         if (result == PLATFORM_ERROR_NOT_FOUND) {
//             logger_log(LOG_WARN, "Shared memory '%s' not found, will retry", config->name);
//         } else {
//             logger_log(LOG_ERROR, "Failed to open shared memory '%s' (error: %d), will retry", 
//                      config->name, result);
//         }
        
//         // Check retry conditions
//         retries++;
//         if (!config->wait_indefinitely && config->max_retries > 0 && 
//             retries >= config->max_retries) {
//             logger_log(LOG_ERROR, "Maximum retries (%d) exceeded for shared memory '%s'", 
//                      config->max_retries, config->name);
            
//             cleanup_worker_threads(&context);
//             cleanup_network(&context);
//             platform_mutex_destroy(&context.mutex);
//             return result;
//         }
        
//         // Wait before retrying
//         sleep_ms(config->retry_interval_ms);
        
//         // Check if we should exit
//         if (shutdown_signalled()) {
//             cleanup_worker_threads(&context);
//             cleanup_network(&context);
//             platform_mutex_destroy(&context.mutex);
//             return PLATFORM_ERROR_TIMEOUT;
//         }
//     }
    
//     // Get actual size if auto-detect was requested
//     context.actual_size = config->size;
//     if (config->size == 0) {
//         result = platform_shared_memory_get_size(context.handle, &context.actual_size);
//         if (result != PLATFORM_ERROR_SUCCESS) {
//             logger_log(LOG_ERROR, "Failed to get shared memory size (error: %d)", result);
//             platform_shared_memory_close(context.handle);
            
//             cleanup_worker_threads(&context);
//             cleanup_network(&context);
//             platform_mutex_destroy(&context.mutex);
//             return result;
//         }
        
//         logger_log(LOG_INFO, "Auto-detected shared memory size: %zu bytes", context.actual_size);
//     }
    
//     // Map shared memory into our address space
//     result = platform_shared_memory_map(context.handle, &context.mapped_data);
//     if (result != PLATFORM_ERROR_SUCCESS) {
//         logger_log(LOG_ERROR, "Failed to map shared memory (error: %d)", result);
//         platform_shared_memory_close(context.handle);
        
//         cleanup_worker_threads(&context);
//         cleanup_network(&context);
//         platform_mutex_destroy(&context.mutex);
//         return result;
//     }
    
//     // Allocate buffer for last data to detect changes if needed
//     if (config->detect_changes) {
//         context.last_data = malloc(context.actual_size);
//         if (!context.last_data) {
//             logger_log(LOG_ERROR, "Failed to allocate memory for data comparison");
//             platform_shared_memory_unmap(context.handle);
//             platform_shared_memory_close(context.handle);
            
//             cleanup_worker_threads(&context);
//             cleanup_network(&context);
//             platform_mutex_destroy(&context.mutex);
//             return PLATFORM_ERROR_OUT_OF_MEMORY;
//         }
        
//         // Initialize with zeros
//         memset(context.last_data, 0, context.actual_size);
//     }
    
//     logger_log(LOG_INFO, "Starting to monitor shared memory '%s' with forwarding to %s:%d", 
//              config->name, config->forwarding.hostname, config->forwarding.port);
    
//     // Main monitoring loop
//     while (!shutdown_signalled()) {
//         bool should_process = false;
        
//         if (config->detect_changes) {
//             // Check for changes in shared memory data
//             if (first_read || memcmp(context.mapped_data, context.last_data, context.actual_size) != 0) {
//                 should_process = true;
//                 // Copy current data to our comparison buffer
//                 memcpy(context.last_data, context.mapped_data, context.actual_size);
//                 first_read = false;
                
//                 logger_log(LOG_DEBUG, "Detected changes in shared memory '%s', forwarding...", config->name);
//             }
//         } else {
//             // Always process all data
//             should_process = true;
//         }
        
//         // Process data if needed
//         if (should_process) {
//             process_data(&context, context.mapped_data, context.actual_size);
//         }
        
//         // Wait before next check
//         sleep_ms(config->interval_ms);
//     }
    
//     logger_log(LOG_INFO, "Shared memory monitor shutting down");
    
//     // Clean up
//     if (context.last_data) {
//         free(context.last_data);
//     }
    
//     platform_shared_memory_unmap(context.handle);
//     platform_shared_memory_close(context.handle);
    
//     cleanup_worker_threads(&context);
//     cleanup_network(&context);
//     platform_mutex_destroy(&context.mutex);
    
//     return PLATFORM_ERROR_SUCCESS;
// }

// // Thread entry point
// static void* shared_memory_monitor_thread(void* arg) {
//     ThreadConfig* thread_config = (ThreadConfig*)arg;
//     SharedMemoryMonitorConfig* config = (SharedMemoryMonitorConfig*)thread_config->data;
    
//     if (!config) {
//         logger_log(LOG_ERROR, "Invalid shared memory monitor configuration");
//         return NULL;
//     }
    
//     logger_log(LOG_INFO, "Shared memory monitor thread starting");
//     thread_registry_update_state(thread_config->label, THREAD_STATE_RUNNING);
    
//     // Run the monitor
//     PlatformErrorCode err = monitor_shared_memory(config);
//     if (err != PLATFORM_ERROR_SUCCESS && err != PLATFORM_ERROR_TIMEOUT) {
//         logger_log(LOG_ERROR, "Shared memory monitor failed with error code %d", err);
//         thread_registry_update_state(thread_config->label, THREAD_STATE_FAILED);
//     } else {
//         thread_registry_update_state(thread_config->label, THREAD_STATE_TERMINATED);
//     }
    
//     return NULL;
// }
