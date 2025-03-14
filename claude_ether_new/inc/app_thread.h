/**
 * @file app_thread.h
 * @brief Application thread management interface
 */
#ifndef APP_THREAD_H
#define APP_THREAD_H

#include <stdbool.h>
#include "platform_threads.h"
#include "thread_registry_errors.h"
#include "thread_result_errors.h"
#include "message_types.h"

#define MAX_THREADS 100

/**
 * @brief Function pointer types for thread lifecycle management
 */
typedef void* (*PreCreateFunc_T)(void* arg);    ///< Called before thread creation
typedef void* (*PostCreateFunc_T)(void* arg);   ///< Called after thread creation
typedef void* (*InitFunc_T)(void* arg);         ///< Thread initialization
typedef void* (*ExitFunc_T)(void* arg);         ///< Thread cleanup
typedef void* (*ThreadFunc_T)(void* arg);       ///< Main thread function


struct ThreadConfig;

/**
 * @brief Message processing callback function type
 * @param thread The thread context
 * @param message The received message
 * @return ThreadResult indicating processing result
 */
typedef ThreadResult (*MessageProcessor_T)(struct ThreadConfig* thread, const Message_T* message);

/**
 * @brief Core thread configuration and management structure
 */
typedef struct ThreadConfig {
    const char* label;                   ///< Thread identifier (e.g., "CLIENT" or "SERVER")
    ThreadFunc_T func;                   ///< Main thread function
    PlatformThreadId thread_id;          ///< Platform-agnostic thread identifier
    void *data;                          ///< Thread-specific context data
    PreCreateFunc_T pre_create_func;     ///< Pre-creation hook
    PostCreateFunc_T post_create_func;   ///< Post-creation hook
    InitFunc_T init_func;                ///< Initialization hook
    ExitFunc_T exit_func;                ///< Cleanup hook
    bool suppressed;                     ///< Thread suppression flag
    
    // Queue processing related fields
    MessageProcessor_T msg_processor;     ///< Message processing callback
    uint32_t queue_process_interval_ms;  ///< How often to check queue (0 = every loop)
    uint32_t max_process_time_ms;        ///< Max time to spend processing queue (0 = no limit)
    uint32_t msg_batch_size;             ///< Max messages to process per batch (0 = no limit)
} ThreadConfig;

// Declare the template
extern const ThreadConfig ThreadConfigemplate;

/**
 * @brief Thread startup configuration
 */
typedef struct {
    ThreadConfig* thread;   ///< Thread to start
    bool is_essential;     ///< If true, suppression generates warning
} ThreadStartInfo;

// Thread Management Functions
ThreadRegistryError app_thread_init(void);
void app_thread_cleanup(void);
ThreadRegistryError app_thread_create(ThreadConfig* thread);
void start_threads(void);

// Thread Labeling
const char* get_thread_label(void);
void set_thread_label(const char* label);

// Thread Lifecycle Stubs
void* pre_create_stub(void* arg);
void* post_create_stub(void* arg);
void* init_stub(void* arg);
void* exit_stub(void* arg);

// Thread State
bool shutdown_signalled(void);

/**
 * @brief Process messages in thread's queue
 * @param thread Thread context
 * @return ThreadResult indicating processing result
 */
ThreadResult service_thread_queue(ThreadConfig* thread);

#endif // APP_THREAD_H
