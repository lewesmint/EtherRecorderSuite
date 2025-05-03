#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shared_memory_monitor.h"
#include "platform_shared_memory.h"
#include "platform_error.h"
#include "platform_time.h"
#include "platform_threads.h"
#include "platform_sockets.h"
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
static SharedMemoryMonitorConfig monitor_config = {
    .size = 0,                     // Auto-detect size
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

// Forward declarations
static void* shared_memory_monitor_thread(void* arg);

// Initialize a shared memory monitor configuration with defaults
PlatformErrorCode shared_memory_monitor_init_config(SharedMemoryMonitorConfig* config) {
    if (!config) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    // Start with default configuration
    *config = monitor_config;
    return PLATFORM_ERROR_SUCCESS;
}

static void* shared_memory_monitor_thread(void* arg) {
    (void)arg;
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
    
    // Load shared memory configuration
    const char* name = get_config_string(section_name, "name", config->name);
    if (name) {
        strncpy(config->name, name, sizeof(config->name) - 1);
        config->name[sizeof(config->name) - 1] = '\0';
    }
    
    config->size = (size_t)get_config_int(section_name, "size", (int)config->size);
    config->interval_ms = (unsigned int)get_config_int(section_name, "interval_ms", config->interval_ms);
    config->retry_interval_ms = (unsigned int)get_config_int(section_name, "retry_interval_ms", config->retry_interval_ms);
    config->max_retries = (unsigned int)get_config_int(section_name, "max_retries", config->max_retries);
    config->wait_indefinitely = get_config_bool(section_name, "wait_indefinitely", config->wait_indefinitely);
    config->detect_changes = get_config_bool(section_name, "detect_changes", config->detect_changes);
    config->num_threads = get_config_int(section_name, "num_threads", config->num_threads);
    
    // Access mode as string
    const char* access_str = get_config_string(section_name, "access", "read");
    if (access_str) {
        // if (strcmp_nocase(access_str, "read") == 0) {
        //     config->access = PLATFORM_SHM_READ;
        // } else if (strcmp_nocase(access_str, "write") == 0) {
        //     config->access = PLATFORM_SHM_WRITE;
        // } else if (strcmp_nocase(access_str, "readwrite") == 0) {
        //     config->access = PLATFORM_SHM_READWRITE;
        // }
    }
    
    // Load network forwarding configuration
    // char forward_key[256];
    // const char* hostname;
    
    // // Find the first block that matches this name
    // for (int i = 1; i <= 10; i++) {
    //     snprintf(forward_key, sizeof(forward_key), "block%d.name", i);
    //     const char* block_name = get_config_string("shared_memory_sync", forward_key, NULL);
        
    //     if (block_name && strcmp(block_name, config->name) == 0) {
    //         // Found matching block, read its config
    //         snprintf(forward_key, sizeof(forward_key), "block%d.hostname", i);
    //         hostname = get_config_string("shared_memory_sync", forward_key, "localhost");
    //         strncpy(config->forwarding.hostname, hostname, sizeof(config->forwarding.hostname) - 1);
    //         config->forwarding.hostname[sizeof(config->forwarding.hostname) - 1] = '\0';
            
    //         snprintf(forward_key, sizeof(forward_key), "block%d.port", i);
    //         config->forwarding.port = get_config_int("shared_memory_sync", forward_key, 5000);
            
    //         snprintf(forward_key, sizeof(forward_key), "block%d.protocol", i);
    //         const char* protocol = get_config_string("shared_memory_sync", forward_key, "udp");
    //         config->forwarding.use_tcp = (strcmp_nocase(protocol, "tcp") == 0);
            
    //         snprintf(forward_key, sizeof(forward_key), "block%d.retry_count", i);
    //         config->forwarding.retry_count = (unsigned int)get_config_int("shared_memory_sync", forward_key, 3);
            
    //         snprintf(forward_key, sizeof(forward_key), "block%d.retry_interval_ms", i);
    //         config->forwarding.retry_interval_ms = (unsigned int)get_config_int("shared_memory_sync", forward_key, 500);
            
    //         logger_log(LOG_INFO, "Loaded forwarding configuration for shared memory '%s' to %s:%d via %s", 
    //                  config->name, config->forwarding.hostname, config->forwarding.port, 
    //                  config->forwarding.use_tcp ? "TCP" : "UDP");
    //         break;
    //     }
    // }
    
    return PLATFORM_ERROR_SUCCESS;
}

struct ThreadConfig* get_shared_memory_monitor_thread(void) {
    static ThreadConfig monitor_thread = {
        .label = "SM_MONITOR",
        .func = shared_memory_monitor_thread,
        .data = &monitor_config,
        .suppressed = false
    };
    
    // Load configuration from config.ini
    shared_memory_load_config_from_ini(&monitor_config, "shared_memory");
    
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
