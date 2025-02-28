#include "app_thread.h"

#include <stdbool.h>
#include <windows.h>
#include <stdio.h>
#include <stdint.h>

#include "platform_utils.h"
#include "platform_threads.h"
#include "log_queue.h"
#include "logger.h"
#include "client_manager.h"
#include "server_manager.h"
#include "command_interface.h"
#include "app_config.h"

extern bool shutdown_signalled(void);


#define NUM_THREADS (sizeof(all_threads) / sizeof(all_threads[0]))

THREAD_LOCAL static const char *thread_label = NULL;

extern AppThread_T server_send_thread;
extern AppThread_T server_receive_thread;
// extern AppThread_T client_send_thread;
// extern AppThread_T client_receive_thread;

static CONDITION_VARIABLE logger_thread_condition;
static CRITICAL_SECTION logger_thread_mutex_in_app_thread;
volatile bool logger_ready = false;

typedef enum WaitResult {
    APP_WAIT_SUCCESS = 0,
    APP_WAIT_TIMEOUT = 1,
    APP_WAIT_ERROR = -1
} WaitResult;

void* app_thread(AppThread_T* thread_args) {
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
    platform_thread_create(&thread->thread_id, (ThreadFunc_T)app_thread, thread);

    if (thread->post_create_func)
        thread->post_create_func(thread);
}

void set_thread_label(const char *label) {
    thread_label = label;
}

const char* get_thread_label() {
    return thread_label;
}

void wait_for_all_other_threads_to_complete(void);

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

int wait_for_condition_with_timeout(void* condition, void* mutex, int timeout_ms) {
#ifdef _WIN32
    if (!SleepConditionVariableCS((PCONDITION_VARIABLE)condition, (PCRITICAL_SECTION)mutex, timeout_ms)) {
        DWORD error = GetLastError();
        if (error == ERROR_TIMEOUT) {
            return APP_WAIT_TIMEOUT;
        } else {
            char* errorMsg = NULL;
            FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPSTR)&errorMsg, 0, NULL);
            LocalFree(errorMsg);
            return APP_WAIT_ERROR;
        }
    }
    return APP_WAIT_SUCCESS;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }

    int rc = pthread_cond_timedwait((pthread_cond_t*)condition, (pthread_mutex_t*)mutex, &ts);
    if (rc == ETIMEDOUT) {
        printf("Timeout occurred while waiting for condition variable\n");
        return WAIT_TIMEOUT;
    }
    if (rc != 0) {
        printf("Error occurred while waiting for condition variable: %s\n", strerror(rc));
        return WAIT_ERROR;
    }
    return WAIT_SUCCESS;
#endif
}

void* init_wait_for_logger(void* arg) {
    AppThread_T* thread_info = (AppThread_T*)arg;
    set_thread_label(thread_info->label);
    init_thread_timestamp_system();

    EnterCriticalSection(&logger_thread_mutex_in_app_thread);
    while (!logger_ready) {
        int result = wait_for_condition_with_timeout(&logger_thread_condition, &logger_thread_mutex_in_app_thread, 5000);
        if (result == APP_WAIT_TIMEOUT) {
            LeaveCriticalSection(&logger_thread_mutex_in_app_thread);
            return (void*)APP_WAIT_TIMEOUT;
        } else if (result == APP_WAIT_ERROR) {
            LeaveCriticalSection(&logger_thread_mutex_in_app_thread);
            return (void*)APP_WAIT_ERROR;
        }
    }
    LeaveCriticalSection(&logger_thread_mutex_in_app_thread);
    set_thread_log_file_from_config(thread_info->label);
    logger_log(LOG_INFO, "Thread %s initialised", thread_info->label);
    logger_log(LOG_INFO, "Logger thread initialised");

    return (void*)APP_WAIT_SUCCESS;
}

void* logger_thread_function(void* arg) {
    AppThread_T* thread_info = (AppThread_T*)arg;
    set_thread_label(thread_info->label);
    logger_log(LOG_INFO, "Logger thread started");

    EnterCriticalSection(&logger_thread_mutex_in_app_thread);
    logger_ready = true;
    WakeAllConditionVariable(&logger_thread_condition);
    LeaveCriticalSection(&logger_thread_mutex_in_app_thread);

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
    .port = 4100,
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
    &server_thread,
//    &server_receive_thread
//    &server_send_thread,
//    &client_receive_thread,
//    &client_send_thread,
    &command_interface_thread,
    &logger_thread
};

bool wait_for_all_threads_to_complete(int time_ms)
{
    // uint32_t num_threads = sizeof(all_threads) / sizeof(all_threads[0]);

    HANDLE threadHandles[NUM_THREADS];
    memset(threadHandles, 0, sizeof(threadHandles));

    DWORD j = 0;
    // build an array of thread handles from structure, careful of suppressed threads
    for (size_t i = 0; i < NUM_THREADS; i++) {
        if (all_threads[i]->suppressed) {
            continue;
        }
        threadHandles[j] = all_threads[i]->thread_id;
        j++;
    }

    if (j == 0) {
        fprintf(stderr, "No threads to wait for\n");
        return true;
    }

    DWORD waitResult = WaitForMultipleObjects(j, threadHandles, TRUE, time_ms);

    if (waitResult == WAIT_FAILED) {
        DWORD err = GetLastError();
        logger_log(LOG_ERROR, "WaitForMultipleObjects failed: %lu\n", err);
        // handle error appropriately
    }
    else if (waitResult == WAIT_TIMEOUT) {
        return false;
    } else {
        printf("all threads have completed\n");

    }
    // Close the thread handles, done with them.
    for (size_t i = 0; i < j; i++) {
        if (threadHandles[i] != NULL) {
            CloseHandle(threadHandles[i]);
        }
    }
    return true;
}


void wait_for_all_other_threads_to_complete(void)
{
    // int num_threads = sizeof(all_threads) / sizeof(all_threads[0]);

    HANDLE threadHandles[NUM_THREADS];
    memset(threadHandles, 0, sizeof(threadHandles));

    DWORD j = 0;

    for (size_t i = 0; i < NUM_THREADS; i++) {
        if (all_threads[i]->suppressed ||
            str_cmp_nocase(all_threads[i]->label, get_thread_label())) {
            continue;
        }
        threadHandles[j++] = all_threads[i]->thread_id;
    }

    DWORD waitResult = WaitForMultipleObjects(j, threadHandles, TRUE, INFINITE);

    if (waitResult == WAIT_FAILED) {
        DWORD err = GetLastError();
        logger_log(LOG_ERROR, "WaitForMultipleObjects failed: %lu\n", err);
    }
    else {
        logger_log(LOG_INFO, "Logger has seen all other threads complete");
    }

    LogEntry_T entry;
    while (log_queue_pop(&global_log_queue, &entry)) {
        if (*entry.thread_label == '\0')
            printf("Logger thread processing log from: NULL\n");
        log_now(&entry);
    }
}

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

void start_threads(void) {
    InitializeConditionVariable(&logger_thread_condition);
    InitializeCriticalSection(&logger_thread_mutex_in_app_thread);

    check_for_suppression();

    for (int i = 0; i < NUM_THREADS; i++) {
        if (!all_threads[i]->suppressed) {
            create_app_thread(all_threads[i]);
        }
    }
}
