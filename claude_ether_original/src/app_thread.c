#include "app_thread.h"

// 2. System includes
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 3. Platform includes
#include "platform_threads.h"
#include "platform_utils.h"

// 4. Project includes
#include "app_config.h"
#include "client_manager.h"
#include "command_interface.h"
#include "log_queue.h"
#include "logger.h"
#include "server_manager.h"
#include "thread_registry.h"
#include "comm_threads.h"


extern bool g_registry_initialized;

extern bool shutdown_signalled(void);

extern volatile bool logger_ready;

static THREAD_LOCAL const char *thread_label = NULL;

extern AppThread_T server_send_thread;
extern AppThread_T server_receive_thread;
extern AppThread_T client_send_thread;
extern AppThread_T client_receive_thread;

// Replace these Windows-specific declarations:
static PlatformCondition_T logger_thread_condition;
static PlatformMutex_T logger_thread_mutex;

typedef enum WaitResult {
    APP_WAIT_SUCCESS = 0,
    APP_WAIT_TIMEOUT = 1,
    APP_WAIT_ERROR = -1
} WaitResult;

bool app_thread_init(void) {
    if (g_registry_initialized) {
        return true;
    }
    
    if (!init_global_thread_registry()) {
        return false;
    }
    
    g_registry_initialized = true;
    return true;
}

bool app_thread_wait_all(uint32_t timeout_ms) {
    if (!g_registry_initialized) {
        stream_print(stderr, "Thread registry not initialised\n");
        return false;
    }
    
    ThreadRegistry* registry = get_thread_registry();
    platform_mutex_lock(&registry->mutex);
    
    // Count active threads and collect handles
    uint32_t active_count = 0;
    ThreadRegistryEntry* entry = registry->head;
    while (entry != NULL) {
        if (entry->state != THREAD_STATE_TERMINATED) {
            active_count++;
        }
        entry = entry->next;
    }
    
    if (active_count == 0) {
        platform_mutex_unlock(&registry->mutex);
        return true;
    }
    
    // Allocate handles array
    ThreadHandle_T* handles = (ThreadHandle_T*)malloc(active_count * sizeof(ThreadHandle_T));
    if (!handles) {
        platform_mutex_unlock(&registry->mutex);
        logger_log(LOG_ERROR, "Failed to allocate memory for thread handles");
        return false;
    }
    
    // Collect handles
    uint32_t i = 0;
    entry = registry->head;
    while (entry != NULL && i < active_count) {
        if (entry->state != THREAD_STATE_TERMINATED) {
            handles[i++] = entry->handle;
        }
        entry = entry->next;
    }
    
    platform_mutex_unlock(&registry->mutex);
    
    // Wait for all threads
    int result = platform_thread_wait_multiple(handles, active_count, true, timeout_ms);
    free(handles);
    
    if (result == PLATFORM_WAIT_SUCCESS) {
        return true;
    } else if (result == PLATFORM_WAIT_TIMEOUT) {
        // Only log the timeout warning during shutdown, not during normal operation
        if (shutdown_signalled()) {
            logger_log(LOG_WARN, "Timeout waiting for threads to complete");
        }
        return false;
    } else {
        char error_buf[256];
        platform_get_error_message(error_buf, sizeof(error_buf));
        logger_log(LOG_ERROR, "Error waiting for threads to complete: %s", error_buf);
        return false;
    }
}

bool app_thread_is_suppressed(const char* suppressed_list, const char* thread_label) {
    if (!suppressed_list || !thread_label) {
        return false;
    }
    
    // Create a copy of the suppressed list for tokenization
    char* list_copy = strdup(suppressed_list);
    if (!list_copy) {
        return false;
    }
    
    bool suppressed = false;
    char* saveptr = NULL;
    char* token = platform_strtok(list_copy, ",", &saveptr);
    
    while (token != NULL) {
        // Trim leading and trailing whitespace
        char* start = token;
        while (*start && isspace(*start)) start++;
        
        char* end = start + strlen(start) - 1;
        while (end > start && isspace(*end)) *end-- = '\0';
        
        // Check if the token matches the thread label
        if (str_cmp_nocase(start, thread_label) == 0) {
            suppressed = true;
            break;
        }
        
        token = platform_strtok(NULL, ",", &saveptr);
    }
    
    free(list_copy);
    logger_log(LOG_DEBUG, "Thread '%s' is %s", thread_label, suppressed ? "suppressed" : "not suppressed");
    return suppressed;
}

void app_thread_cleanup(void) {
    if (!g_registry_initialized) {
        return;
    }
    
    ThreadRegistry* registry = get_thread_registry();
    thread_registry_cleanup(registry);
    g_registry_initialized = false;
}

void* app_thread(AppThread_T* thread_args) {
    if (thread_args->init_func) {
        if ((WaitResult)(uintptr_t)thread_args->init_func(thread_args) != APP_WAIT_SUCCESS) {
            stream_print(stderr, "[%s] Initialisation failed, exiting thread\n", thread_args->label);
            return NULL;
        }
    }
    thread_args->func(thread_args);
    if (thread_args->exit_func)
        thread_args->exit_func(thread_args);
    
    return NULL;
}

void set_thread_label(const char *label) {
    thread_label = label;
}

const char* get_thread_label(void) {
    return thread_label;
}

void* pre_create_stub(void* arg) {
    (void)arg;
    return 0;
}

void* post_create_stub(void* arg) {
    (void)arg;
    return 0;
}

void* init_stub(void* arg) {
    (void)arg;
    init_thread_timestamp_system();
    return 0;
}

void* exit_stub(void* arg) {
    (void)arg;
    return 0;
}

// Replace the wait_for_condition_with_timeout function:
int wait_for_condition_with_timeout(void* condition, void* mutex, int timeout_ms) {
    // Replace Windows-specific implementation with platform-agnostic version
    int result = platform_cond_timedwait((PlatformCondition_T*)condition, 
                                         (PlatformMutex_T*)mutex, 
                                         (uint32_t)timeout_ms);
                                         
    // Map platform wait result codes to app wait result codes
    if (result == PLATFORM_WAIT_SUCCESS) {
        return APP_WAIT_SUCCESS;
    } else if (result == PLATFORM_WAIT_TIMEOUT) {
        return APP_WAIT_TIMEOUT;
    } else {
        return APP_WAIT_ERROR;
    }
}

void* init_wait_for_logger(void* arg) {
    AppThread_T* thread_info = (AppThread_T*)arg;
    set_thread_label(thread_info->label);
    init_thread_timestamp_system();

    platform_mutex_lock(&logger_thread_mutex);
    while (!logger_ready) {
        int result = wait_for_condition_with_timeout(&logger_thread_condition, &logger_thread_mutex, 5000);
        if (result == APP_WAIT_TIMEOUT) {
            platform_mutex_unlock(&logger_thread_mutex);
            return (void*)APP_WAIT_TIMEOUT;
        } else if (result == APP_WAIT_ERROR) {
            platform_mutex_unlock(&logger_thread_mutex);
            return (void*)APP_WAIT_ERROR;
        }
    }
    platform_mutex_unlock(&logger_thread_mutex);
    set_thread_log_file_from_config(thread_info->label);
    logger_log(LOG_INFO, "Thread %s initialised", thread_info->label);
    logger_log(LOG_INFO, "Logger thread initialised");

    return (void*)APP_WAIT_SUCCESS;
}

bool app_thread_create(AppThread_T* thread) {
    if (!g_registry_initialized) {
        stream_print(stderr, "Thread registry not initialised\n");
        return false;
    }
    
    if (!thread) {
        return false;
    }
    
    ThreadRegistry* registry = get_thread_registry();
    
    // Check if the thread is already registered
    if (thread_registry_is_registered(registry, thread)) {
        logger_log(LOG_WARN, "Thread '%s' is already registered", thread->label);
        return false;
    }
    
    // Handle stubbed function pointers
    if (!thread->pre_create_func) thread->pre_create_func = pre_create_stub;
    if (!thread->post_create_func) thread->post_create_func = post_create_stub;
    if (!thread->init_func) thread->init_func = init_wait_for_logger;
    if (!thread->exit_func) thread->exit_func = exit_stub;
    
    // Call pre-create function
    if (thread->pre_create_func) {
        thread->pre_create_func(thread);
    }
    
    // Create the thread
    ThreadHandle_T thread_handle = NULL;
    if (platform_thread_create(&thread->thread_id, (ThreadFunc_T)app_thread, thread) != 0) {
        logger_log(LOG_ERROR, "Failed to create thread '%s'", thread->label);
        return false;
    }
    
    thread_handle = thread->thread_id;
    
    // Call post-create function
    if (thread->post_create_func) {
        thread->post_create_func(thread);
    }
    
    // Register the thread
    if (!thread_registry_register(registry, thread, thread_handle, true)) {
        logger_log(LOG_ERROR, "Failed to register thread '%s'", thread->label);
        return false;
    }
    
    // Update thread state
    thread_registry_update_state(registry, thread, THREAD_STATE_RUNNING);
    
    return true;
}


// Update logger_thread_function:
void* logger_thread_function(void* arg) {
    AppThread_T* thread_info = (AppThread_T*)arg;
    set_thread_label(thread_info->label);
    logger_log(LOG_INFO, "Logger thread started");

    platform_mutex_lock(&logger_thread_mutex);
    logger_ready = true;
    platform_cond_broadcast(&logger_thread_condition);
    platform_mutex_unlock(&logger_thread_mutex);

    LogEntry_T entry;
    bool running = true;

    while (running) {
        while (log_queue_pop(&global_log_queue, &entry)) {
            if (*entry.thread_label == '\0')
                printf("Logger thread processing log from: NULL\n");
            log_now(&entry);
        }

        sleep_ms(1);

        if (shutdown_signalled()) {
            running = false;
        }
    }

    wait_for_all_other_threads_to_complete();
    
    logger_log(LOG_INFO, "Logger thread shutting down.");
    return NULL;
}


// static CommContext client_context = {
//     .socket = NULL,              // Will be set during initialization
//     .connection_closed = 0,
//     .group_id = 0,              // Will be set during initialization
//     .is_tcp = true,
//     .is_relay_enabled = false
// };

// static CommContext server_context = {
//     .socket = NULL,              // Will be set during initialization
//     .connection_closed = 0,
//     .group_id = 0,              // Will be set during initialization
//     .is_tcp = true,
//     .is_relay_enabled = false
// };

// static CommsArgs_T client_thread_args = {
//     .context = &client_context,
//     .thread_info = NULL,        // Additional info if needed
//     .queue = NULL,              // Will be set during thread creation
//     .thread_id = 0              // Will be set during thread creation
// };

// static CommsArgs_T server_thread_args = {
//     .context = &server_context,
//     .thread_info = NULL,        // Additional info if needed
//     .queue = NULL,              // Will be set during thread creation
//     .thread_id = 0              // Will be set during thread creation
// };


ClientCommsThreadArgs_T client_thread_args = {
    .server_hostname = "localhost",                  ///< Server hostname or IP address
    .comm_args.port =4199,                           ///< Port number
    .comm_args.send_test_data = true,                ///< Send test data flag
    .comm_args.send_interval_ms = 1000,              ///< Send interval
    .comm_args.data_size = 1000                      ///< Data size
};

CommsThreadArgs_T server_thread_args = {
    .port =4199,                           ///< Port number
    .send_test_data = true,                ///< Send test data flag
    .send_interval_ms = 1000,               ///< Send interval
    .data_size = 1000                      ///< Data size
};


AppThread_T client_thread = {
    .label = "CLIENT",
    .func = clientMainThread,
    .data = &client_thread_args,
    .pre_create_func = pre_create_stub,
    .post_create_func = post_create_stub,
    .init_func = init_wait_for_logger,
    .exit_func = exit_stub,
    .suppressed=false
};

AppThread_T server_thread = {
    .label = "SERVER",
    .func = serverListenerThread,
    .data = &server_thread_args,
    .pre_create_func = pre_create_stub,
    .post_create_func = post_create_stub,
    .init_func = init_wait_for_logger,
    .exit_func = exit_stub,
    .suppressed=false
};


AppThread_T command_interface_thread = {
    .label = "COMMAND_INTERFACE",
    .func = command_interface_thread_function,
    .data = NULL,
    .pre_create_func = pre_create_stub,
    .post_create_func = post_create_stub,
    .init_func = init_wait_for_logger,
    .exit_func = exit_stub,
    .suppressed=true
};

AppThread_T logger_thread = {
    .label = "LOGGER",
    .func = logger_thread_function,
    .data = NULL,
    .pre_create_func = pre_create_stub,
    .post_create_func = post_create_stub,
    .init_func = init_stub,
    .exit_func = exit_stub
};

// Update start_threads:
void start_threads(void) {
    platform_cond_init(&logger_thread_condition);
    platform_mutex_init(&logger_thread_mutex);
    
    // Initialise thread registry
    if (!app_thread_init()) {
        stream_print(stderr, "Failed to initialize thread registry\n");
        return;
    }
    
    // Get suppressed threads from configuration
    const char* suppressed_list = get_config_string("debug", "suppress_threads", "");
    
    // Define the thread launch configuration
    ThreadStartInfo threads_to_start[] = {
        { &logger_thread, true },      // Logger always starts first and is essential
        { &client_thread, false },
        { &server_thread, false },
        { &command_interface_thread, false }
        // Add other threads as needed
    };
    
    // Start each thread if not suppressed
    for (size_t i = 0; i < sizeof(threads_to_start) / sizeof(threads_to_start[0]); i++) {
        AppThread_T* thread = threads_to_start[i].thread;
        bool is_essential = threads_to_start[i].is_essential;
        
        // Check if thread is suppressed either in its structure or in config
        if (!thread->suppressed && !app_thread_is_suppressed(suppressed_list, thread->label)) {
            app_thread_create(thread);
            
            // If this is the logger thread, give it time to initialize
            if (strcmp(thread->label, "LOGGER") == 0) {
                sleep_ms(100);  // Brief pause to ensure logger is ready
            }
        } else if (is_essential) {
            stream_print(stderr, "WARNING: Essential thread '%s' is suppressed. This may cause issues.\n", 
                    thread->label);
        } else {
            logger_log(LOG_DEBUG, "Thread '%s' is suppressed - not starting", thread->label);
        }
    }
}

bool wait_for_all_threads_to_complete(int time_ms) {
    (void)time_ms;
    if (!g_registry_initialized) {
        stream_print(stderr, "Thread registry not initialised\n");
        return false;
    }

    ThreadRegistry* registry = get_thread_registry();
    platform_mutex_lock(&registry->mutex);

    // Add debug logging
    logger_log(LOG_DEBUG, "Waiting for threads to complete. Registry count: %d", registry->count);

    // Count active threads and collect handles
    uint32_t active_count = 0;
    ThreadRegistryEntry* entry = registry->head;
    while (entry != NULL) {
        if (entry->state != THREAD_STATE_TERMINATED) {
            active_count++;
            logger_log(LOG_DEBUG, "Active thread found: %s", 
                      entry->thread->label ? entry->thread->label : "unnamed");
        }
        entry = entry->next;
    }

    if (active_count == 0) {
        logger_log(LOG_DEBUG, "No active threads found");
        platform_mutex_unlock(&registry->mutex);
        return true;
    }

    logger_log(LOG_DEBUG, "Waiting for %d active threads", active_count);

    // Wait for threads to complete
    bool all_complete = true;
    entry = registry->head;
    while (entry != NULL) {
        if (entry->state != THREAD_STATE_TERMINATED) {
            void* thread_result;
            logger_log(LOG_DEBUG, "Joining thread: %s", 
                      entry->thread->label ? entry->thread->label : "unnamed");
            
            int join_result = platform_thread_join(entry->handle, &thread_result);
            if (join_result != 0) {
                logger_log(LOG_ERROR, "Failed to join thread %s (error: %d)", 
                          entry->thread->label ? entry->thread->label : "unnamed",
                          join_result);
                all_complete = false;
                break;
            }
            logger_log(LOG_DEBUG, "Successfully joined thread: %s", 
                      entry->thread->label ? entry->thread->label : "unnamed");
        }
        entry = entry->next;
    }

    platform_mutex_unlock(&registry->mutex);
    logger_log(LOG_DEBUG, "Thread wait completed. All complete: %s", 
              all_complete ? "true" : "false");
    return all_complete;
}

// Update wait_for_all_other_threads_to_complete:
void wait_for_all_other_threads_to_complete(void) {
    // Get the current thread label
    const char* current_thread_label = get_thread_label();
    
    // Get thread registry instance through the getter
    ThreadRegistry* registry = get_thread_registry();
    
    // Lock the registry mutex
    platform_mutex_lock(&registry->mutex);
    
    // Count active threads and collect handles
    uint32_t active_count = 0;
    ThreadRegistryEntry* entry = registry->head;
    while (entry != NULL) {
        if (entry->state != THREAD_STATE_TERMINATED && 
            entry->thread && entry->thread->label && 
            strcmp(entry->thread->label, current_thread_label) != 0) {
            active_count++;
        }
        entry = entry->next;
    }
    
    if (active_count == 0) {
        platform_mutex_unlock(&registry->mutex);
        return;
    }
    
    // Allocate handles array
    ThreadHandle_T* handles = (ThreadHandle_T*)malloc(active_count * sizeof(ThreadHandle_T));
    if (!handles) {
        platform_mutex_unlock(&registry->mutex);
        logger_log(LOG_ERROR, "Failed to allocate memory for thread handles");
        return;
    }
    
    // Collect handles
    uint32_t i = 0;
    entry = registry->head;
    while (entry != NULL && i < active_count) {
        if (entry->state != THREAD_STATE_TERMINATED && 
            entry->thread && entry->thread->label && 
            strcmp(entry->thread->label, current_thread_label) != 0) {
            handles[i++] = entry->handle;
        }
        entry = entry->next;
    }
    
    platform_mutex_unlock(&registry->mutex);
    
    // Wait for all other threads
    platform_thread_wait_multiple(handles, active_count, true, PLATFORM_WAIT_INFINITE);
    free(handles);
    
    logger_log(LOG_INFO, "Thread '%s' has seen all other threads complete", current_thread_label);
    
    // Process any remaining log messages
    LogEntry_T log_entry;
    while (log_queue_pop(&global_log_queue, &log_entry)) {
        log_now(&log_entry);
    }
}
