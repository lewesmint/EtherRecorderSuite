#include "app_thread.h"
#include "demo_heartbeat_thread.h"

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

typedef enum WaitResult {
    APP_WAIT_SUCCESS = PLATFORM_WAIT_SUCCESS,    // 0
    APP_WAIT_TIMEOUT = PLATFORM_WAIT_TIMEOUT,    // 1
    APP_WAIT_ERROR = PLATFORM_WAIT_ERROR         // -1
} WaitResult;

// extern PlatformCondition_T logger_thread_condition;
// extern PlatformMutex_T logger_thread_mutex;

static THREAD_LOCAL const char* thread_label = NULL;

void set_thread_label(const char* label) {
    thread_label = label;
}

const char* get_thread_label(void) {
    return thread_label;
}

ThreadRegistryError app_thread_init(void) {
    ThreadRegistryError result = init_global_thread_registry();
    if (result != THREAD_REG_SUCCESS) {
        logger_log(LOG_ERROR, "Failed to initialize thread registry: %s",
                  app_error_get_message(THREAD_REGISTRY_DOMAIN, result));
        return result;
    }

    // Create a thread structure for the main thread
    static ThreadConfig main_thread = {
        .label = "MAIN",
    };
    main_thread.thread_id = platform_thread_get_id();

    // Register the main thread with its current handle
    result = thread_registry_register(&main_thread, false);
    if (result != THREAD_REG_SUCCESS) {
        logger_log(LOG_ERROR, "Failed to register main thread: %s",
                  app_error_get_message(THREAD_REGISTRY_DOMAIN, result));
        return result;
    }

    // Initialize message queue for main thread
    result = init_queue(main_thread.label);
    if (result != THREAD_REG_SUCCESS) {
        logger_log(LOG_ERROR, "Failed to initialize main thread message queue: %s",
                  app_error_get_message(THREAD_REGISTRY_DOMAIN, result));
        return result;
    }
    
    return THREAD_REG_SUCCESS;
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

static ThreadResult wait_for_logger(ThreadConfig* thread_info) {
    // If we are the logger, don't wait for ourselves
    if (strcmp(thread_info->label, "LOGGER") == 0) {
        return THREAD_SUCCESS;
    }

    uint32_t timeout = 5000;  // 5 second timeout
    uint32_t sleep_interval = 10;  // 10ms check intervals
    uint32_t elapsed = 0;

    while (elapsed < timeout) {
        ThreadState logger_state = thread_registry_get_state("LOGGER");
        if (logger_state == THREAD_STATE_RUNNING) {
            break;
        }
        sleep_ms(sleep_interval);
        elapsed += sleep_interval;
    }

    if (elapsed >= timeout) {
        return THREAD_ERROR_LOGGER_TIMEOUT;
    }

    set_thread_log_file_from_config(thread_info->label);
    logger_log(LOG_INFO, "Thread %s initialised", thread_info->label);

    return THREAD_SUCCESS;
}

bool is_thread_suppressed(const char* suppressed_list, const char* label) {
    if (!suppressed_list || !label || !*label) {  // Check for empty string
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
        if (strcmp_nocase(start, label) == 0) {
            suppressed = true;
            break;
        }
        
        token = platform_strtok(NULL, ",", &saveptr);
    }
    
    free(list_copy);
    logger_log(LOG_DEBUG, "Thread '%s' is %s", label, suppressed ? "suppressed" : "not suppressed");
    return suppressed;
}

void app_thread_cleanup(void) {
    thread_registry_cleanup();
}


// Define the default template
const ThreadConfig ThreadConfigTemplate = {
    .label = NULL,                         // Must be set by thread
    .func = NULL,                          // Must be set by thread
    .data = NULL,                          // Optional thread data
    .thread_id = 0,                        // Set by app_thread_create
    .pre_create_func = pre_create_stub,
    .post_create_func = post_create_stub,
    .init_func = init_stub,
    .exit_func = exit_stub,
    .suppressed = false,
    
    // Queue processing defaults
    .msg_processor = NULL,                 // No message processing by default
    .queue_process_interval_ms = 0,        // Check every loop
    .max_process_time_ms = 100,           // 100ms default
    .msg_batch_size = 10                  // Process up to 10 messages per batch
};


// Helper function to create a thread from template
ThreadConfig create_thread_config(const char* label, 
                                  ThreadFunc_T func, 
                                  const ThreadConfig* template) {
    ThreadConfig new_config = template ? *template : ThreadConfigTemplate;
    new_config.label = label;
    new_config.func = func;
    return new_config;
}

void start_threads(void) {
    // Initialize thread registry
    ThreadRegistryError result = app_thread_init();
    if (result != THREAD_REG_SUCCESS) {
        stream_print(stderr, "Failed to initialize thread registry: %s\n",
                    app_error_get_message(THREAD_REGISTRY_DOMAIN, result));
        return;
    }
    
    // Get suppressed threads from configuration
    const char* suppressed_list = get_config_string("debug", "suppress_threads", "");
    
    // Define all threads to start
    ThreadStartInfo threads_to_start[] = {
        { get_demo_heartbeat_thread(), false }, // Demo thread is not essential
        { get_logger_thread(), true }         // Logger is essential
    };
    
    // Start each thread
    for (size_t i = 0; i < sizeof(threads_to_start) / sizeof(threads_to_start[0]); i++) {
        ThreadConfig* thread = threads_to_start[i].thread;
        bool is_essential = threads_to_start[i].is_essential;
        
        if (!thread) continue;
        
        // Check if thread should be suppressed (never suppress essential threads)
        if (!is_essential && is_thread_suppressed(suppressed_list, thread->label)) {
            thread->suppressed = true;
            continue;
        }
        
        result = app_thread_create(thread);
        if (result != THREAD_REG_SUCCESS) {
            logger_log(LOG_ERROR, "Failed to create thread %s (error: %d)", 
                      thread->label, result);
            if (is_essential) {
                return;
            }
        }
    }
}

static void* thread_wrapper(ThreadConfig* thread_args) {
    if (!thread_args) {
        return (void*)THREAD_ERROR_INIT_FAILED;
    }

    ThreadResult run_result = THREAD_SUCCESS;

    // Set thread-specific data
    set_thread_label(thread_args->label);
    
    // Update thread state to running
    thread_registry_update_state(thread_args->label, THREAD_STATE_RUNNING);
    
    // Initialize message queue for this thread
    ThreadRegistryError queue_result = init_queue(thread_args->label);
    if (queue_result != THREAD_REG_SUCCESS) {
        logger_log(LOG_ERROR, "Failed to initialize message queue for thread '%s'", 
                  thread_args->label);
        thread_registry_update_state(thread_args->label, 
                                     THREAD_STATE_FAILED);
        return (void*)THREAD_ERROR_INIT_FAILED;
    }

    // Wait for logger before any initialization
    ThreadResult wait_result = wait_for_logger(thread_args);
    if (wait_result != THREAD_SUCCESS) {
        thread_registry_update_state(thread_args->label, 
                                   THREAD_STATE_FAILED);
        return (void*)(uintptr_t)(wait_result);
    }
    
    // Call init function if exists
    if (thread_args->init_func) {
        ThreadResult init_result = (ThreadResult)(uintptr_t)thread_args->init_func(thread_args);
        if (init_result != THREAD_SUCCESS) {
            logger_log(LOG_ERROR, "Thread '%s' initialization failed with result %d", 
                      thread_args->label, init_result);
            thread_registry_update_state(thread_args->label, 
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
    thread_registry_update_state(thread_args->label, THREAD_STATE_TERMINATED);
    
    return (void*)(uintptr_t)(run_result);
}

ThreadRegistryError app_thread_create(ThreadConfig* thread) {
    if (!thread) {
        return THREAD_REG_INVALID_ARGS;
    }
        
    // Check if thread is already registered
    if (thread_registry_is_registered(thread)) {
        logger_log(LOG_WARN, "Thread '%s' is already registered", thread->label);
        return THREAD_REG_DUPLICATE_THREAD;
    }
    
    // Handle stubbed function pointers
    if (!thread->pre_create_func) thread->pre_create_func = pre_create_stub;
    if (!thread->post_create_func) thread->post_create_func = post_create_stub;
    if (!thread->init_func) thread->init_func = init_stub;
    if (!thread->exit_func) thread->exit_func = exit_stub;
    
    // Set default values for queue processing if not specified
    if (!thread->msg_processor) thread->msg_processor = NULL;  // No message processing by default
    if (!thread->queue_process_interval_ms) thread->queue_process_interval_ms = 0;  // Check every loop
    if (!thread->max_process_time_ms) thread->max_process_time_ms = 100;  // 100ms default
    if (!thread->msg_batch_size) thread->msg_batch_size = 10;  // Process up to 10 messages by default
    
    // Call pre-create function
    if (thread->pre_create_func) {
        thread->pre_create_func(thread);
    }
    
    // Create the thread
    PlatformThreadAttributes attributes = {0}; // Initialize default attributes
    
    if (platform_thread_create(&thread->thread_id, &attributes, 
                             (PlatformThreadFunction) thread_wrapper, thread) != PLATFORM_ERROR_SUCCESS) {
        logger_log(LOG_ERROR, "Failed to create thread '%s'", thread->label);
        return THREAD_REG_CREATION_FAILED;
    }
    
    // Call post-create function
    if (thread->post_create_func) {
        thread->post_create_func(thread);
    }
    
    // Register the thread
    ThreadRegistryError reg_result = thread_registry_register(thread, true);
    if (reg_result != THREAD_REG_SUCCESS) {
        logger_log(LOG_ERROR, "Failed to register thread '%s': %s", 
                  thread->label, 
                  app_error_get_message(THREAD_REGISTRY_DOMAIN, reg_result));
        return reg_result;
    }
    
    // Update thread state
    thread_registry_update_state(thread->label, THREAD_STATE_RUNNING);
    
    return THREAD_REG_SUCCESS;
}

/**
 * @brief Service messages in thread's queue
 * @param thread Thread context
 * @return ThreadResult indicating processing result
 */
ThreadResult service_thread_queue(ThreadConfig* thread) {
    if (!thread || !thread->msg_processor) {
        return THREAD_SUCCESS; // No processor = nothing to do
    }

    uint32_t start_time = get_time_ms();
    uint32_t messages_processed = 0;
    ThreadResult result = THREAD_SUCCESS;
    Message_T message;

    while (true) {
        // Check time limit
        if (thread->max_process_time_ms > 0) {
            uint32_t elapsed = get_time_ms() - start_time;
            if (elapsed >= thread->max_process_time_ms) {
                break;
            }
        }

        // Check message batch limit
        if (thread->msg_batch_size > 0 && messages_processed >= thread->msg_batch_size) {
            break;
        }

        // Try to get a message (non-blocking)
        ThreadRegistryError queue_result = pop_message(thread->label, &message, 0);
        
        if (queue_result == THREAD_REG_QUEUE_EMPTY) {
            break;
        }
        else if (queue_result == THREAD_REG_SUCCESS) {
            result = thread->msg_processor(thread, &message);
            messages_processed++;
            
            if (result != THREAD_SUCCESS) {
                logger_log(LOG_ERROR, "Message processing failed in thread '%s': %d", 
                          thread->label, result);
                break;
            }
        }
        else {
            logger_log(LOG_ERROR, "Queue access error in thread '%s': %s",
                      thread->label,
                      app_error_get_message(THREAD_REGISTRY_DOMAIN, queue_result));
            result = THREAD_ERROR_QUEUE_ERROR; // Use ThreadResult enum value instead of ThreadStatus
            break;
        }
    }

    return result;
}
