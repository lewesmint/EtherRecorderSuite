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
    THREAD_STATE_FAILED,     ///< Thread encountered an error
    THREAD_STATE_UNKNOWN     ///< Thread state is unknown
} ThreadState;

typedef struct ThreadRegistryEntry {
    const ThreadConfig* thread;       // Thread configuration
    PlatformThreadHandle handle;        // Thread handle
    ThreadState state;                  // Current thread state
    bool auto_cleanup;                  // Auto cleanup flag
    MessageQueue_T* queue;              // Message queue for this thread
    struct ThreadRegistryEntry* next;   // Next entry in list
    PlatformEvent_T completion_event;   // Event signaled on thread completion
} ThreadRegistryEntry;


ThreadRegistryError init_global_thread_registry(void);

// Helper function declartion
ThreadRegistryEntry* thread_registry_find_thread(
    const char* thread_label
);

// Core registry operations
void thread_registry_cleanup(void);
ThreadRegistryError thread_registry_register(const ThreadConfig* thread, PlatformThreadHandle handle, bool auto_cleanup);
ThreadRegistryError thread_registry_update_state(const char* thread_label, ThreadState new_state);
ThreadState thread_registry_get_state(const char* thread_label);
bool thread_registry_is_registered(const ThreadConfig* thread);

// Message queue operations - renamed to remove redundancy
ThreadRegistryError init_queue(const char* thread_label);
ThreadRegistryError push_message(const char* thread_label, const Message_T* message, uint32_t timeout_ms);
ThreadRegistryError pop_message(const char* thread_label, Message_T* message, uint32_t timeout_ms);

// Helper function for queue access
MessageQueue_T* get_queue_by_label(const char* thread_label);


ThreadRegistryError thread_registry_wait_all(uint32_t timeout_ms);
ThreadRegistryError thread_registry_wait_others(void);

#endif // THREAD_REGISTRY_H
