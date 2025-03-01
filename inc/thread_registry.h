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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Thread states for lifecycle management
 */
typedef enum ThreadState {
    THREAD_STATE_CREATED,    // Thread has been created but might not be running yet
    THREAD_STATE_RUNNING,    // Thread is running normally
    THREAD_STATE_STOPPING,   // Thread has been signaled to stop
    THREAD_STATE_TERMINATED, // Thread has terminated
    THREAD_STATE_ERROR       // Thread encountered an error
} ThreadState;

/**
 * @brief Thread registry entry structure
 */
typedef struct ThreadRegistryEntry {
    AppThread_T* thread;                // Pointer to thread structure
    ThreadState state;                  // Current thread state
    ThreadHandle_T handle;              // Changed from HANDLE to ThreadHandle_T
    bool auto_cleanup;                  // Whether to automatically clean up thread resources
    struct ThreadRegistryEntry* next;   // Pointer to next entry in the linked list
} ThreadRegistryEntry;

/**
 * @brief Thread registry structure
 */
typedef struct ThreadRegistry {
    ThreadRegistryEntry* head;          // Head of the registry entries linked list
    PlatformMutex_T mutex;              // Mutex for thread-safe operations
    uint32_t count;                     // Number of registered threads
} ThreadRegistry;

/**
 * @brief Initialize a thread registry
 * 
 * @param registry Pointer to the registry to initialize
 * @return bool true on success, false on failure
 */
bool thread_registry_init(ThreadRegistry* registry);

/**
 * @brief Register a thread in the registry
 * 
 * @param registry Pointer to the registry
 * @param thread Pointer to the thread structure
 * @param handle Thread handle
 * @param auto_cleanup Whether to automatically clean up thread resources
 * @return bool true on success, false on failure
 */
bool thread_registry_register(ThreadRegistry* registry, AppThread_T* thread, 
                            ThreadHandle_T handle, bool auto_cleanup);

/**
 * @brief Update a thread's state in the registry
 * 
 * @param registry Pointer to the registry
 * @param thread Pointer to the thread structure
 * @param state New thread state
 * @return bool true on success, false on failure
 */
bool thread_registry_update_state(ThreadRegistry* registry, AppThread_T* thread, ThreadState state);

/**
 * @brief Deregister a thread from the registry
 * 
 * @param registry Pointer to the registry
 * @param thread Pointer to the thread structure
 * @return bool true on success, false on failure
 */
bool thread_registry_deregister(ThreadRegistry* registry, AppThread_T* thread);

/**
 * @brief Find a thread in the registry by label
 * 
 * @param registry Pointer to the registry
 * @param label Thread label to find
 * @return ThreadRegistryEntry* Pointer to the registry entry, or NULL if not found
 */
ThreadRegistryEntry* thread_registry_find_by_label(ThreadRegistry* registry, const char* label);

/**
 * @brief Find a thread in the registry by handle
 * 
 * @param registry Pointer to the registry
 * @param handle Thread handle to find
 * @return ThreadRegistryEntry* Pointer to the registry entry, or NULL if not found
 */
ThreadRegistryEntry* thread_registry_find_by_handle(ThreadRegistry* registry, ThreadHandle_T handle);

/**
 * @brief Check if a thread is registered
 * 
 * @param registry Pointer to the registry
 * @param thread Pointer to the thread structure
 * @return bool true if the thread is registered, false otherwise
 */
bool thread_registry_is_registered(ThreadRegistry* registry, AppThread_T* thread);

/**
 * @brief Clean up the thread registry
 * 
 * @param registry Pointer to the registry
 */
void thread_registry_cleanup(ThreadRegistry* registry);

#ifdef __cplusplus
}
#endif

#endif // THREAD_REGISTRY_H