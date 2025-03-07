/**
 * @file app_thread.h
 * @brief Thread management system for application-wide thread control
 *
 * Provides a unified interface for thread creation, management, and lifecycle control.
 * Supports thread labeling, custom initialization/cleanup, and suppression capabilities.
 */

#ifndef APP_THREAD_H
#define APP_THREAD_H

// System includes
#include <stdbool.h>

// Platform includes
#include "platform_threads.h"

// Project includes
#include "client_manager.h"
#include "logger.h"

#define MAX_THREADS 100

/**
 * @brief Function pointer types for thread lifecycle management
 */
typedef void* (*PreCreateFunc_T)(void* arg);    ///< Called before thread creation
typedef void* (*PostCreateFunc_T)(void* arg);   ///< Called after thread creation
typedef void* (*InitFunc_T)(void* arg);         ///< Thread initialization
typedef void* (*ExitFunc_T)(void* arg);         ///< Thread cleanup
typedef void* (*ThreadFunc_T)(void* arg);       ///< Main thread function

/**
 * @brief Core thread configuration and management structure
 */
typedef struct AppThread_T {
    const char* label;                   ///< Thread identifier (e.g., "CLIENT" or "SERVER")
    ThreadFunc_T func;                   ///< Main thread function
    ThreadHandle_T thread_id;            ///< Platform-agnostic thread identifier
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
bool app_thread_create(AppThread_T* thread);
void start_threads(void);
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

void* app_thread(AppThread_T* thread_args);

#endif // APP_THREAD_H
