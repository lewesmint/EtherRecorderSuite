#include "app_thread.h"

// System includes
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Platform includes
#include "platform_threads.h"
#include "platform_string.h"
#include "platform_error.h"
#include "platform_sync.h"

// Project includes
#include "app_config.h"
#include "thread_registry.h"
#include "app_error.h"

// #include "client_manager.h"
// #include "command_interface.h"
#include "log_queue.h"
#include "logger.h"
// #include "server_manager.h"
#include "thread_registry.h"
// #include "comm_threads.h"

#include "utils.h"

extern bool g_registry_initialized;
extern volatile bool logger_ready;

typedef enum WaitResult {
    APP_WAIT_SUCCESS = PLATFORM_WAIT_SUCCESS,    // 0
    APP_WAIT_TIMEOUT = PLATFORM_WAIT_TIMEOUT,    // 1
    APP_WAIT_ERROR = PLATFORM_WAIT_ERROR         // -1
} WaitResult;

extern PlatformCondition_T logger_thread_condition;
extern PlatformMutex_T logger_thread_mutex;

static THREAD_LOCAL const char* thread_label = NULL;

void set_thread_label(const char* label) {
    thread_label = label;
}

const char* get_thread_label(void) {
    return thread_label;
}

bool app_thread_init(void) {
    if (g_registry_initialized) {
        return true;
    }
    
    ThreadRegistryError result = init_global_thread_registry();
    if (result != THREAD_REG_SUCCESS) {
        return false;
    }
    
    g_registry_initialized = true;
    return true;
}

bool app_thread_wait_all(uint32_t timeout_ms) {
    if (!g_registry_initialized) {
        stream_print(stderr, "Thread registry not initialized\n");
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
    PlatformThreadHandle* handles = (PlatformThreadHandle*)malloc(active_count * sizeof(PlatformThreadHandle));
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
    
    // Wait for all threads with the specified timeout
    PlatformWaitResult result = platform_wait_multiple(handles, active_count, true, timeout_ms);
    free(handles);
    
    return (result == PLATFORM_WAIT_SUCCESS);
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
    return (void*)THREAD_SUCCESS;
}

void* exit_stub(void* arg) {
    (void)arg;
    return (void*)THREAD_SUCCESS;
}

int wait_for_condition_with_timeout(void* condition, void* mutex, int timeout_ms) {
    int result = platform_cond_timedwait((PlatformCondition_T*)condition, 
                                       (PlatformMutex_T*)mutex, 
                                       (uint32_t)timeout_ms);
                                       
    if (result == PLATFORM_ERROR_SUCCESS) {
        return APP_WAIT_SUCCESS;
    } else if (result == PLATFORM_ERROR_TIMEOUT) {
        return APP_WAIT_TIMEOUT;
    } else {
        return APP_WAIT_ERROR;
    }
}

// void* init_wait_for_logger(void* arg) {
//     AppThread_T* thread_info = (AppThread_T*)arg;
//     init_thread_timestamp_system();

//     platform_mutex_lock(&logger_thread_mutex);
//     while (!logger_ready) {
//         int result = wait_for_condition_with_timeout(&logger_thread_condition, 
//                                                    &logger_thread_mutex, 
//                                                    5000);
//         if (result != APP_WAIT_SUCCESS) {
//             platform_mutex_unlock(&logger_thread_mutex);
//             return (void*)(intptr_t)result;
//         }
//     }
//     platform_mutex_unlock(&logger_thread_mutex);
    
//     set_thread_log_file_from_config(thread_info->label);
//     logger_log(LOG_INFO, "Thread %s initialised", thread_info->label);
    
//     return (void*)THREAD_SUCCESS;
// }

void* init_wait_for_logger(void* arg) {
    AppThread_T* thread_info = (AppThread_T*)arg;
    set_thread_label(thread_info->label);
    init_thread_timestamp_system();

    platform_mutex_lock(&logger_thread_mutex);
    while (!logger_ready) {
        int result = wait_for_condition_with_timeout(&logger_thread_condition, &logger_thread_mutex, 5000);
        if (result == APP_WAIT_TIMEOUT) {
            platform_mutex_unlock(&logger_thread_mutex);
            return (void*)THREAD_ERROR_LOGGER_TIMEOUT;
        } else if (result == APP_WAIT_ERROR) {
            platform_mutex_unlock(&logger_thread_mutex);
            return (void*)THREAD_ERROR_MUTEX_ERROR;
        }
    }
    platform_mutex_unlock(&logger_thread_mutex);
    
    set_thread_log_file_from_config(thread_info->label);
    // if (!set_thread_log_file_from_config(thread_info->label)) {
    //     return (void*)THREAD_ERROR_CONFIG_ERROR;
    // }
    
    logger_log(LOG_INFO, "Thread %s initialised", thread_info->label);
    logger_log(LOG_INFO, "Logger thread initialised");

    return (void*)THREAD_SUCCESS;
}

bool is_thread_suppressed(const char* suppressed_list, const char* thread_label) {
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
        if (strcmp_nocase(start, thread_label) == 0) {
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


// Define the thread structure
AppThread_T logger_thread = {
    .label = "LOGGER",
    .func = logger_thread_function,
    .data = NULL,
    .pre_create_func = pre_create_stub,
    .post_create_func = post_create_stub,
    .init_func = init_stub,        // Logger doesn't wait for itself
    .exit_func = exit_stub,
    .suppressed = false            // Logger cannot be suppressed
};

// Update start_threads function
void start_threads(void) {
    // Initialize synchronization primitives
    platform_cond_init(&logger_thread_condition);
    platform_mutex_init(&logger_thread_mutex);
    
    // Initialize thread registry
    if (!app_thread_init()) {
        stream_print(stderr, "Failed to initialize thread registry\n");
        return;
    }
    
    // Start logger thread first
    if (app_thread_create(&logger_thread) != THREAD_REG_SUCCESS) {
        stream_print(stderr, "Failed to create logger thread\n");
        return;
    }
    
    // Brief pause to ensure logger is ready
    sleep_ms(100);
    
    // Get suppressed threads from configuration
    const char* suppressed_list = get_config_string("debug", "suppress_threads", "");
    
    // Define the remaining threads to start
    ThreadStartInfo threads_to_start[] = {
        // { &logger_thread, true },      // Logger always starts first and is essential
        // { &client_thread, false },
        // { &server_thread, false },
        // { &command_interface_thread, false }
    };
    
    // Start each non-suppressed thread
    for (size_t i = 0; i < sizeof(threads_to_start) / sizeof(threads_to_start[0]); i++) {
        AppThread_T* thread = threads_to_start[i].thread;
        bool is_essential = threads_to_start[i].is_essential;
        
        if (!thread) continue;
        
        // Check if thread should be suppressed
        if (!is_essential && is_thread_suppressed(thread->label, suppressed_list)) {
            thread->suppressed = true;
            continue;
        }
        
        ThreadRegistryError result = app_thread_create(thread);
        if (result != THREAD_REG_SUCCESS) {
            logger_log(LOG_ERROR, "Failed to create thread %s (error: %d)", 
                      thread->label, result);
            if (is_essential) {
                return;
            }
        }
    }
}

bool wait_for_all_threads_to_complete(uint32_t timeout_ms) {
    if (!g_registry_initialized) {
        stream_print(stderr, "Thread registry not initialised\n");
        return false;
    }

    ThreadRegistry* registry = get_thread_registry();
    platform_mutex_lock(&registry->mutex);

    // Use stack allocation with maximum registry size
    PlatformThreadHandle handles[MAX_THREADS];
    uint32_t active_count = 0;

    // Collect active thread handles
    ThreadRegistryEntry* entry = registry->head;
    while (entry != NULL && active_count < MAX_THREADS) {
        if (entry->state != THREAD_STATE_TERMINATED) {
            handles[active_count++] = entry->handle;
        }
        entry = entry->next;
    }

    platform_mutex_unlock(&registry->mutex);

    if (active_count == 0) {
        return true;
    }

    // Wait for all threads
    PlatformWaitResult result = platform_wait_multiple(handles, active_count, true, timeout_ms);
    return (result == PLATFORM_WAIT_SUCCESS);
}

void wait_for_all_other_threads_to_complete(void) {
    if (!g_registry_initialized) {
        stream_print(stderr, "Thread registry not initialised\n");
        return;
    }

    const char* current_thread_label = get_thread_label();
    ThreadRegistry* registry = get_thread_registry();
    platform_mutex_lock(&registry->mutex);

    // Use stack allocation
    PlatformThreadHandle handles[MAX_THREADS];
    uint32_t active_count = 0;

    // Collect handles of other active threads
    ThreadRegistryEntry* entry = registry->head;
    while (entry != NULL && active_count < MAX_THREADS) {
        if (entry->state != THREAD_STATE_TERMINATED && 
            entry->thread && entry->thread->label && 
            strcmp(entry->thread->label, current_thread_label) != 0) {
            handles[active_count++] = entry->handle;
        }
        entry = entry->next;
    }

    platform_mutex_unlock(&registry->mutex);

    if (active_count > 0) {
        platform_wait_multiple(handles, active_count, true, PLATFORM_WAIT_INFINITE);
        logger_log(LOG_INFO, "Thread '%s' has seen all other threads complete", current_thread_label);
        
        // Process remaining logs
        LogEntry_T log_entry;
        while (log_queue_pop(&global_log_queue, &log_entry)) {
            log_now(&log_entry);
        }
    }
}

static void* thread_wrapper(AppThread_T* thread_args) {
    if (!thread_args) {
        return (void*)THREAD_ERROR_INIT_FAILED;
    }

    ThreadResult run_result = THREAD_SUCCESS;

    // Set thread-specific data
    set_thread_label(thread_args->label);
    
    // Update thread state to running
    thread_registry_update_state(get_thread_registry(), thread_args->label, THREAD_STATE_RUNNING);
    
    // Call init function if exists
    if (thread_args->init_func) {
        ThreadResult init_result = (ThreadResult)(uintptr_t)thread_args->init_func(thread_args);
        if (init_result != THREAD_SUCCESS) {
            logger_log(LOG_ERROR, "Thread '%s' initialization failed with result %d", 
                      thread_args->label, init_result);
            thread_registry_update_state(get_thread_registry(), 
                                      thread_args->label, 
                                      THREAD_STATE_FAILED);
            return (void*)(uintptr_t)init_result;
        }
    }
 
    // Execute the main thread function
    if (thread_args->func) {
        run_result = (ThreadResult)(uintptr_t)thread_args->func(thread_args);
        if (run_result != THREAD_SUCCESS) {
            logger_log(LOG_ERROR, "Thread '%s' run failed with result %d", 
                      thread_args->label, run_result);
        }
    }
    
    // Call exit function if exists
    if (thread_args->exit_func) {
        ThreadResult exit_result = (ThreadResult)(uintptr_t)thread_args->exit_func(thread_args);
        if (exit_result != THREAD_SUCCESS) {
            logger_log(LOG_ERROR, "Thread '%s' exit function failed with result %d",
                thread_args->label, exit_result);
        }
    }
    
    // Update thread state to terminated
    thread_registry_update_state(get_thread_registry(), thread_args->label, THREAD_STATE_TERMINATED);
    
    return (void*)(run_result);
}

ThreadRegistryError app_thread_create(AppThread_T* thread) {
    if (!g_registry_initialized) {
        stream_print(stderr, "Thread registry not initialised\n");
        return THREAD_REG_NOT_INITIALIZED;
    }
    
    if (!thread) {
        return THREAD_REG_INVALID_ARGS;
    }
    
    ThreadRegistry* registry = get_thread_registry();
    
    // Check if thread is already registered
    if (thread_registry_is_registered(registry, thread)) {
        logger_log(LOG_WARN, "Thread '%s' is already registered", thread->label);
        return THREAD_REG_DUPLICATE_THREAD;
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
    PlatformThreadHandle thread_handle = NULL;
    PlatformThreadAttributes attributes = {0}; // Initialize default attributes
    
    if (platform_thread_create(&thread->thread_id, &attributes, 
                             (PlatformThreadFunction) thread_wrapper, thread) != PLATFORM_ERROR_SUCCESS) {
        logger_log(LOG_ERROR, "Failed to create thread '%s'", thread->label);
        return THREAD_REG_CREATION_FAILED;
    }
    
    thread_handle = thread->thread_id;
    
    // Call post-create function
    if (thread->post_create_func) {
        thread->post_create_func(thread);
    }
    
    // Register the thread
    ThreadRegistryError reg_result = thread_registry_register(registry, thread, thread_handle, true);
    if (reg_result != THREAD_REG_SUCCESS) {
        logger_log(LOG_ERROR, "Failed to register thread '%s': %s", 
                  thread->label, 
                  app_error_get_message(THREAD_REGISTRY_DOMAIN, reg_result));
        return reg_result;
    }
    
    // Update thread state
    thread_registry_update_state(registry, thread->label, THREAD_STATE_RUNNING);
    
    return THREAD_REG_SUCCESS;
}

void* deprecated_app_thread(AppThread_T* thread_args) {
    if (!thread_args) {
        return NULL;
    }

    // Set thread-specific data
    set_thread_label(thread_args->label);

    // Call initialization if provided
    if (thread_args->init_func) {
        if (thread_args->init_func(thread_args) == NULL) {
            logger_log(LOG_ERROR, "Thread '%s' initialization failed", thread_args->label);
            return NULL;
        }
    }

    // Execute main thread function
    void* result = thread_args->func(thread_args);

    // Call cleanup if provided
    if (thread_args->exit_func) {
        thread_args->exit_func(thread_args);
    }

    // Signal completion via registry
    ThreadRegistry* registry = get_thread_registry();
    thread_registry_update_state(registry, thread_args->label, THREAD_STATE_TERMINATED);

    return result;
}
