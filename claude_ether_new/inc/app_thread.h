/**
 * @file app_thread.h
 * @brief Application thread management interface
 */
#ifndef APP_THREAD_H
#define APP_THREAD_H

#include <stdbool.h>
#include "platform_threads.h"
#include "logger.h"
#include "app_error.h"

#define MAX_THREADS 100


/**
 * @brief Function pointer types for thread lifecycle management
 */
typedef void* (*PreCreateFunc_T)(void* arg);    ///< Called before thread creation
typedef void* (*PostCreateFunc_T)(void* arg);   ///< Called after thread creation
typedef void* (*InitFunc_T)(void* arg);         ///< Thread initialization
typedef void* (*ExitFunc_T)(void* arg);         ///< Thread cleanup
typedef void* (*ThreadFunc_T)(void* arg);       ///< Main thread function

typedef enum ThreadResult {
    THREAD_SUCCESS = 0,
    THREAD_ERROR_INIT_FAILED = -1,
    THREAD_ERROR_LOGGER_TIMEOUT = -2,
    THREAD_ERROR_MUTEX_ERROR = -3,
    THREAD_ERROR_CONFIG_ERROR = -4,
    // Add other thread-specific error codes as needed
} ThreadResult;

/**
 * @brief Core thread configuration and management structure
 */
typedef struct AppThread_T {
    const char* label;                   ///< Thread identifier (e.g., "CLIENT" or "SERVER")
    ThreadFunc_T func;                   ///< Main thread function
    PlatformThreadHandle thread_id;      ///< Platform-agnostic thread identifier
    void *data;                          ///< Thread-specific context data
    PreCreateFunc_T pre_create_func;     ///< Pre-creation hook
    PostCreateFunc_T post_create_func;   ///< Post-creation hook
    InitFunc_T init_func;                ///< Initialization hook
    ExitFunc_T exit_func;                ///< Cleanup hook
    bool suppressed;                     ///< Thread suppression flag
} AppThread_T;

/**
 * @brief Thread startup configuration
 */
typedef struct {
    AppThread_T* thread;   ///< Thread to start
    bool is_essential;     ///< If true, suppression generates warning
} ThreadStartInfo;

// Thread Management Functions
bool app_thread_init(void);
void app_thread_cleanup(void);
ThreadRegistryError app_thread_create(AppThread_T* thread);
void start_threads(void);
bool wait_for_all_threads_to_complete(uint32_t timeout_ms);
void wait_for_all_other_threads_to_complete(void);

// Thread Labeling
const char* get_thread_label(void);
void set_thread_label(const char* label);

// Thread Lifecycle Stubs
void* pre_create_stub(void* arg);
void* post_create_stub(void* arg);
void* init_stub(void* arg);
void* exit_stub(void* arg);
void* init_wait_for_logger(void* arg);

// Thread State
bool shutdown_signalled(void);

#endif // APP_THREAD_H
