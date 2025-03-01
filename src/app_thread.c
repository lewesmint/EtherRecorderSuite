#include "app_thread.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>

#include "platform_utils.h"
#include "platform_threads.h"
#include "log_queue.h"
#include "logger.h"
#include "client_manager.h"
#include "server_manager.h"
#include "command_interface.h"
#include "app_config.h"


//////////////////
#include "thread_registry.h"

// Global thread registry
static ThreadRegistry g_thread_registry;
static bool g_registry_initialized = false;

bool app_thread_init(void) {
    if (g_registry_initialized) {
        return true;
    }
    
    if (!thread_registry_init(&g_thread_registry)) {
        fprintf(stderr, "Failed to initialize thread registry\n");
        return false;
    }
    
    g_registry_initialized = true;
    return true;
}

bool app_thread_wait_all(uint32_t timeout_ms) {
    if (!g_registry_initialized) {
        fprintf(stderr, "Thread registry not initialized\n");
        return false;
    }
    
    platform_mutex_lock(&g_thread_registry.mutex);
    
    // Count active threads and collect handles
    uint32_t active_count = 0;
    ThreadRegistryEntry* entry = g_thread_registry.head;
    while (entry != NULL) {
        if (entry->state != THREAD_STATE_TERMINATED) {
            active_count++;
        }
        entry = entry->next;
    }
    
    if (active_count == 0) {
        platform_mutex_unlock(&g_thread_registry.mutex);
        return true;
    }
    
    // Allocate handles array
    ThreadHandle_T* handles = (ThreadHandle_T*)malloc(active_count * sizeof(ThreadHandle_T));
    if (!handles) {
        platform_mutex_unlock(&g_thread_registry.mutex);
        logger_log(LOG_ERROR, "Failed to allocate memory for thread handles");
        return false;
    }
    
    // Collect handles
    uint32_t i = 0;
    entry = g_thread_registry.head;
    while (entry != NULL && i < active_count) {
        if (entry->state != THREAD_STATE_TERMINATED) {
            handles[i++] = entry->handle;
        }
        entry = entry->next;
    }
    
    platform_mutex_unlock(&g_thread_registry.mutex);
    
    // Wait for all threads
    int result = platform_thread_wait_multiple(handles, active_count, true, timeout_ms);
    free(handles);
    
    if (result == PLATFORM_WAIT_SUCCESS) {
        return true;
    } else if (result == PLATFORM_WAIT_TIMEOUT) {
        logger_log(LOG_WARN, "Timeout waiting for threads to complete");
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
    char* token = strtok_s(list_copy, ",", &saveptr);
    
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
        
        token = strtok_s(NULL, ",", &saveptr);
    }
    
    free(list_copy);
    return suppressed;
}

void app_thread_cleanup(void) {
    if (!g_registry_initialized) {
        return;
    }
    
    thread_registry_cleanup(&g_thread_registry);
    g_registry_initialized = false;
}


extern bool shutdown_signalled(void);


#define NUM_THREADS (sizeof(all_threads) / sizeof(all_threads[0]))

THREAD_LOCAL static const char *thread_label = NULL;

extern AppThread_T server_send_thread;
extern AppThread_T server_receive_thread;
extern AppThread_T client_send_thread;
extern AppThread_T client_receive_thread;

// Replace these Windows-specific declarations:
static PlatformCondition_T logger_thread_condition;
static PlatformMutex_T logger_thread_mutex;
volatile bool logger_ready = false;

typedef enum WaitResult {
    APP_WAIT_SUCCESS = 0,
    APP_WAIT_TIMEOUT = 1,
    APP_WAIT_ERROR = -1
} WaitResult;

void* app_thread_x(AppThread_T* thread_args) {
    if (thread_args->init_func) {
        if ((WaitResult)(uintptr_t)thread_args->init_func(thread_args) != APP_WAIT_SUCCESS) {
            printf("[%s] Initialisation failed, exiting thread\n", thread_args->label);
            return NULL;
        }
    }
    thread_args->func(thread_args);
    if (thread_args->exit_func)
        thread_args->exit_func(thread_args);
    
    return NULL;
}

void create_app_thread(AppThread_T *thread) {
    if (thread->pre_create_func)
        thread->pre_create_func(thread);
    platform_thread_create(&thread->thread_id, (ThreadFunc_T)app_thread_x, thread);

    if (thread->post_create_func)
        thread->post_create_func(thread);
}

void set_thread_label(const char *label) {
    thread_label = label;
}

const char* get_thread_label() {
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
        fprintf(stderr, "Thread registry not initialized\n");
        return false;
    }
    
    if (!thread) {
        return false;
    }
    
    // Check if the thread is already registered
    if (thread_registry_is_registered(&g_thread_registry, thread)) {
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
    if (platform_thread_create(&thread->thread_id, (ThreadFunc_T)app_thread_x, thread) != 0) {
        logger_log(LOG_ERROR, "Failed to create thread '%s'", thread->label);
        return false;
    }
    
    thread_handle = thread->thread_id;
    
    // Call post-create function
    if (thread->post_create_func) {
        thread->post_create_func(thread);
    }
    
    // Register the thread
    if (!thread_registry_register(&g_thread_registry, thread, thread_handle, true)) {
        logger_log(LOG_ERROR, "Failed to register thread '%s'", thread->label);
        return false;
    }
    
    // Update thread state
    thread_registry_update_state(&g_thread_registry, thread, THREAD_STATE_RUNNING);
    
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

static char test_send_data [1000];

static CommsThreadArgs_T client_thread_args = {
    .server_hostname = "127.0.0.2",
    .send_test_data = false,
    .data = &test_send_data,
    .data_size = sizeof(test_send_data),
    .send_interval_ms = 2000,
    .port = 4200,
    .is_tcp = true
};

static CommsThreadArgs_T server_thread_args = {
    .server_hostname = "0.0.0.0",
    .send_test_data = false,
    .data = &test_send_data,
    .data_size = sizeof(test_send_data),
    .send_interval_ms = 2000,
    .port = 4150,
    .is_tcp = true
};


AppThread_T client_thread = {
    .label = "CLIENT",
    .func = clientMainThread,
    .data = &client_thread_args,
    .pre_create_func = pre_create_stub,
    .post_create_func = post_create_stub,
    .init_func = init_wait_for_logger,
    .exit_func = exit_stub
};

AppThread_T server_thread = {
    .label = "SERVER",
    .func = serverListenerThread,
    .data = &server_thread_args,
    .pre_create_func = pre_create_stub,
    .post_create_func = post_create_stub,
    .init_func = init_wait_for_logger,
    .exit_func = exit_stub
};


AppThread_T command_interface_thread = {
    .label = "COMMAND_INTERFACE",
    .func = command_interface_thread_function,
    .data = NULL,
    .pre_create_func = pre_create_stub,
    .post_create_func = post_create_stub,
    .init_func = init_wait_for_logger,
    .exit_func = exit_stub
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

static AppThread_T* all_threads[] = {
    &client_thread,
    // &server_thread,
    &server_receive_thread,
    &server_send_thread,
    &client_receive_thread,
    &client_send_thread,
    &command_interface_thread,
    &logger_thread
};

void check_for_suppression(void) {
    const char* suppressed_list = get_config_string("debug", "suppress_threads", "");

    char suppressed_list_copy[CONFIG_MAX_VALUE_LENGTH];
    strncpy(suppressed_list_copy, suppressed_list, CONFIG_MAX_VALUE_LENGTH - 1);
    suppressed_list_copy[CONFIG_MAX_VALUE_LENGTH - 1] = '\0';

    char* context = NULL;
    char* token = strtok_s(suppressed_list_copy, ",", &context);

    while (token != NULL) {
        for (int i = 0; i < NUM_THREADS; i++) {
            if (str_cmp_nocase(all_threads[i]->label, token) == 0) {
                all_threads[i]->suppressed = true;
            }
        }
        token = strtok_s(NULL, ",", &context);
    }
}

// Update start_threads:
void start_threads(void) {
    platform_cond_init(&logger_thread_condition);
    platform_mutex_init(&logger_thread_mutex);
    
    // Initialize thread registry
    if (!app_thread_init()) {
        fprintf(stderr, "Failed to initialize thread registry\n");
        return;
    }
    
    // Get suppressed threads from configuration
    const char* suppressed_list = get_config_string("debug", "suppress_threads", "");
    
    // Create logger thread first (always required)
    if (!app_thread_is_suppressed(suppressed_list, "LOGGER")) {
        app_thread_create(&logger_thread);
    }
    
    // Create other threads if not suppressed
    if (!app_thread_is_suppressed(suppressed_list, "CLIENT")) {
        app_thread_create(&client_thread);
    }
    
    if (!app_thread_is_suppressed(suppressed_list, "SERVER")) {
        app_thread_create(&server_thread);
    }
    
    if (!app_thread_is_suppressed(suppressed_list, "COMMAND_INTERFACE")) {
        app_thread_create(&command_interface_thread);
    }
}

bool wait_for_all_threads_to_complete(int time_ms) {
    return app_thread_wait_all(time_ms);
}

// Update wait_for_all_other_threads_to_complete:
void wait_for_all_other_threads_to_complete(void) {
    // Get the current thread label
    const char* current_thread_label = get_thread_label();
    
    // Lock the registry mutex
    platform_mutex_lock(&g_thread_registry.mutex);
    
    // Count active threads and collect handles
    DWORD active_count = 0;
    ThreadRegistryEntry* entry = g_thread_registry.head;
    while (entry != NULL) {
        if (entry->state != THREAD_STATE_TERMINATED && 
            entry->thread && entry->thread->label && 
            strcmp(entry->thread->label, current_thread_label) != 0) {
            active_count++;
        }
        entry = entry->next;
    }
    
    if (active_count == 0) {
        platform_mutex_unlock(&g_thread_registry.mutex);
        return;
    }
    
    // Allocate handles array
    ThreadHandle_T* handles = (ThreadHandle_T*)malloc(active_count * sizeof(ThreadHandle_T));
    if (!handles) {
        platform_mutex_unlock(&g_thread_registry.mutex);
        logger_log(LOG_ERROR, "Failed to allocate memory for thread handles");
        return;
    }
    
    // Collect handles
    DWORD i = 0;
    entry = g_thread_registry.head;
    while (entry != NULL && i < active_count) {
        if (entry->state != THREAD_STATE_TERMINATED && 
            entry->thread && entry->thread->label && 
            strcmp(entry->thread->label, current_thread_label) != 0) {
            handles[i++] = entry->handle;
        }
        entry = entry->next;
    }
    
    platform_mutex_unlock(&g_thread_registry.mutex);
    
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