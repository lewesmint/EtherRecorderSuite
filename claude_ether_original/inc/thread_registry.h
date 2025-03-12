/**
 * @file thread_registry.h
 * @brief Dynamic thread registry for managing application threads.
 */
#ifndef THREAD_REGISTRY_H
#define THREAD_REGISTRY_H

#include <stdbool.h>
#include "app_thread.h"
#include "platform_mutex.h"
#include "platform_defs.h"
#include "message_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Thread states for lifecycle management
 */
typedef enum ThreadState {
    THREAD_STATE_CREATED,    ///< Thread has been created but might not be running yet
    THREAD_STATE_RUNNING,    ///< Thread is running normally
    THREAD_STATE_STOPPING,   ///< Thread has been signaled to stop
    THREAD_STATE_TERMINATED, ///< Thread has terminated
    THREAD_STATE_ERROR      ///< Thread encountered an error
} ThreadState;

/**
 * @brief Thread registry entry structure
 */
typedef struct ThreadRegistryEntry {
    AppThread_T* thread;                ///< Pointer to thread structure
    ThreadState state;                  ///< Current thread state
    ThreadHandle_T handle;              ///< Thread handle
    bool auto_cleanup;                  ///< Whether to automatically clean up thread resources
    MessageQueue_T* queue;              ///< Thread's message queue
    struct ThreadRegistryEntry* next;   ///< Pointer to next entry in the linked list
} ThreadRegistryEntry;

/**
 * @brief Thread registry structure
 */
typedef struct ThreadRegistry {
    ThreadRegistryEntry* head;          ///< Head of the registry entries linked list
    PlatformMutex_T mutex;             ///< Mutex for thread-safe operations
    uint32_t count;                     ///< Number of registered threads
} ThreadRegistry;

// Core registry functions
bool thread_registry_init(ThreadRegistry* registry);
bool thread_registry_register(ThreadRegistry* registry, AppThread_T* thread, 
                            ThreadHandle_T handle, bool auto_cleanup);
bool thread_registry_update_state(ThreadRegistry* registry, AppThread_T* thread, 
                                ThreadState state);
bool thread_registry_deregister(ThreadRegistry* registry, AppThread_T* thread);
ThreadRegistryEntry* thread_registry_find_by_label(ThreadRegistry* registry, 
                                                 const char* label);
ThreadRegistryEntry* thread_registry_find_by_handle(ThreadRegistry* registry, 
                                                  ThreadHandle_T handle);
bool thread_registry_is_registered(ThreadRegistry* registry, AppThread_T* thread);
void thread_registry_cleanup(ThreadRegistry* registry);
ThreadRegistry* get_thread_registry(void);
bool init_global_thread_registry(void);

// Message queue functions
bool thread_registry_init_queue(ThreadRegistry* registry, AppThread_T* thread, 
                              uint32_t max_size);
bool thread_registry_push_message(ThreadRegistry* registry, const char* thread_label, 
                                Message_T* message);
bool thread_registry_pop_message(ThreadRegistry* registry, const char* thread_label, 
                               Message_T* message, int32_t timeout_ms);
bool thread_registry_setup_relay(ThreadRegistry* registry, const char* source_label, 
                               const char* target_label);
uint32_t thread_registry_get_queue_size(ThreadRegistry* registry, const char* thread_label);
void thread_registry_clear_queue(ThreadRegistry* registry, const char* thread_label);

#ifdef __cplusplus
}
#endif

#endif // THREAD_REGISTRY_H
