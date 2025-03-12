/**
 * @file thread_registry.h
 * @brief Thread registry for managing application threads
 */
#ifndef THREAD_REGISTRY_H
#define THREAD_REGISTRY_H

#include <stdbool.h>
#include <stdint.h>
#include "platform_mutex.h"

#include "message_types.h"
#include "app_thread.h"
#include "app_error.h"  // Include for ThreadRegistryError

#define MAX_THREAD_LABEL_LENGTH 64

/**
 * @brief Thread states for lifecycle management
 */
typedef enum ThreadState {
    THREAD_STATE_CREATED,    ///< Thread created but not running
    THREAD_STATE_RUNNING,    ///< Thread is running
    THREAD_STATE_SUSPENDED,  ///< Thread temporarily suspended
    THREAD_STATE_STOPPING,   ///< Thread signaled to stop
    THREAD_STATE_TERMINATED, ///< Thread has terminated
    THREAD_STATE_FAILED     ///< Thread encountered an error
} ThreadState;

typedef struct ThreadRegistryEntry {
    const AppThread_T* thread;          // Thread configuration
    PlatformThreadHandle handle;        // Thread handle
    ThreadState state;                  // Current thread state
    bool auto_cleanup;                  // Auto cleanup flag
    MessageQueue_T* queue;              // Message queue for this thread
    struct ThreadRegistryEntry* next;   // Next entry in list
    PlatformEvent_T completion_event;   // Event signaled on thread completion
} ThreadRegistryEntry;

typedef struct ThreadRegistry {
    ThreadRegistryEntry* head;           // Head of registry entries
    PlatformMutex_T mutex;              // Registry lock
    uint32_t count;                      // Number of registered threads
} ThreadRegistry;

// Helper function declaration
ThreadRegistryEntry* thread_registry_find_thread(
    ThreadRegistry* registry, 
    const char* thread_label
);

// Core registry operations
ThreadRegistryError thread_registry_init(ThreadRegistry* registry);
void thread_registry_cleanup(ThreadRegistry* registry);

ThreadRegistryError thread_registry_register(
    ThreadRegistry* registry,
    const AppThread_T* thread,
    PlatformThreadHandle handle,
    bool auto_cleanup
);

ThreadRegistryError thread_registry_update_state(
    ThreadRegistry* registry,
    const char* thread_label,
    ThreadState new_state
);

ThreadState thread_registry_get_state(
    ThreadRegistry* registry,
    const char* thread_label
);

// Global registry access
ThreadRegistry* get_thread_registry(void);
ThreadRegistryError init_global_thread_registry(void);

bool thread_registry_is_registered(
    ThreadRegistry* registry,
    const AppThread_T* thread
);

// Add message queue related functions
ThreadRegistryError thread_registry_init_queue(
    ThreadRegistry* registry,
    const char* thread_label,
    uint32_t max_size
);

ThreadRegistryError thread_registry_push_message(
    ThreadRegistry* registry,
    const char* thread_label,
    const Message_T* message,
    uint32_t timeout_ms
);

ThreadRegistryError thread_registry_pop_message(
    ThreadRegistry* registry,
    const char* thread_label,
    Message_T* message,
    uint32_t timeout_ms
);

#endif // THREAD_REGISTRY_H
