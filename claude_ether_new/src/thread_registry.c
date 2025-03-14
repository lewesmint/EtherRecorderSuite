#include "thread_registry.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "platform_error.h"
#include "platform_sync.h"

#include "message_types.h"
#include "app_error.h"
#include "logger.h"

typedef struct ThreadRegistry {
    ThreadRegistryEntry* head;          // Head of registry entries
    PlatformMutex_T mutex;              // Registry lock
    uint32_t count;                     // Number of registered threads
} ThreadRegistry;

static ThreadRegistry g_registry = {0};
bool g_registry_initialized = false;

// Validation helpers
static bool validate_thread_label(const char* label) {
    return label && strlen(label) < MAX_THREAD_LABEL_LENGTH;
}

static bool validate_state_transition(ThreadState current, ThreadState new) {
    switch (current) {
        case THREAD_STATE_CREATED:
            return new == THREAD_STATE_RUNNING || new == THREAD_STATE_FAILED;
        case THREAD_STATE_RUNNING:
            return new == THREAD_STATE_SUSPENDED || 
                   new == THREAD_STATE_STOPPING || 
                   new == THREAD_STATE_FAILED;
        case THREAD_STATE_SUSPENDED:
            return new == THREAD_STATE_RUNNING || 
                   new == THREAD_STATE_STOPPING;
        case THREAD_STATE_STOPPING:
            return new == THREAD_STATE_TERMINATED;
        case THREAD_STATE_TERMINATED:
        case THREAD_STATE_FAILED:
            return false;
        default:
            return false;
    }
}

ThreadRegistryEntry* thread_registry_find_thread(const char* thread_label) {
    ThreadRegistryEntry* current = g_registry.head;
    while (current) {
        if (strcmp(current->thread->label, thread_label) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

ThreadRegistryError thread_registry_register(
    const ThreadConfig* thread,
    bool auto_cleanup
) {
    if (!g_registry_initialized) {
        return THREAD_REG_NOT_INITIALIZED;
    }

    if (!thread || !thread->thread_id || !validate_thread_label(thread->label)) {
        return THREAD_REG_INVALID_ARGS;
    }

    if (platform_mutex_lock(&g_registry.mutex) != PLATFORM_ERROR_SUCCESS) {
        return THREAD_REG_LOCK_ERROR;
    }

    if (thread_registry_find_thread(thread->label)) {
        platform_mutex_unlock(&g_registry.mutex);
        return THREAD_REG_DUPLICATE_THREAD;
    }

    ThreadRegistryEntry* entry = calloc(1, sizeof(ThreadRegistryEntry));
    if (!entry) {
        platform_mutex_unlock(&g_registry.mutex);
        return THREAD_REG_CREATION_FAILED;
    }

    // Create completion event - manual reset, initially not signaled
    if (platform_event_create(&entry->completion_event, true, false) != PLATFORM_ERROR_SUCCESS) {
        free(entry);
        platform_mutex_unlock(&g_registry.mutex);
        return THREAD_REG_CREATION_FAILED;
    }

    entry->thread = thread;
    entry->state = THREAD_STATE_CREATED;
    entry->auto_cleanup = auto_cleanup;
    entry->next = g_registry.head;
    
    g_registry.head = entry;
    g_registry.count++;

    platform_mutex_unlock(&g_registry.mutex);
    
    logger_log(LOG_INFO, "Thread '%s' registered successfully", thread->label);
    return THREAD_REG_SUCCESS;
}

ThreadRegistryError thread_registry_update_state(
    const char* thread_label,
    ThreadState new_state
) {
    if (!g_registry_initialized) {
        return THREAD_REG_NOT_INITIALIZED;
    }

    if (!validate_thread_label(thread_label)) {
        return THREAD_REG_INVALID_ARGS;
    }

    if (platform_mutex_lock(&g_registry.mutex) != PLATFORM_ERROR_SUCCESS) {
        return THREAD_REG_LOCK_ERROR;
    }

    ThreadRegistryEntry* entry = thread_registry_find_thread(thread_label);
    if (!entry) {
        platform_mutex_unlock(&g_registry.mutex);
        return THREAD_REG_NOT_FOUND;
    }

    if (!validate_state_transition(entry->state, new_state)) {
        platform_mutex_unlock(&g_registry.mutex);
        return THREAD_REG_INVALID_STATE_TRANSITION;
    }

    entry->state = new_state;

    // Signal completion event when thread terminates
    if (new_state == THREAD_STATE_TERMINATED || new_state == THREAD_STATE_FAILED) {
        platform_event_set(entry->completion_event);
    }

    platform_mutex_unlock(&g_registry.mutex);
    return THREAD_REG_SUCCESS;
}

ThreadState thread_registry_get_state(const char* thread_label) {
    if (!g_registry_initialized) {
        return THREAD_STATE_UNKNOWN;
    }
    
    platform_mutex_lock(&g_registry.mutex);

    ThreadState state = THREAD_STATE_UNKNOWN;
    
    ThreadRegistryEntry* entry = g_registry.head;
    while (entry != NULL) {
        if (entry->thread && entry->thread->label && 
            strcmp(entry->thread->label, thread_label) == 0) {
            state = entry->state;
            break;
        }
        entry = entry->next;
    }
    
    platform_mutex_unlock(&g_registry.mutex);
    
    return state;
}

ThreadRegistry* get_thread_registry(void) {
    return &g_registry;
}

ThreadRegistryError init_global_thread_registry(void) {
    if (g_registry_initialized) {
        return THREAD_REG_SUCCESS;
    }

    g_registry.head = NULL;
    g_registry.count = 0;

    if (platform_mutex_init(&g_registry.mutex) != PLATFORM_ERROR_SUCCESS) {
        return THREAD_REG_LOCK_ERROR;
    }

    g_registry_initialized = true;
    return THREAD_REG_SUCCESS;
}

ThreadRegistryError thread_registry_wait_for_thread(
    const char* thread_label,
    uint32_t timeout_ms
) {
    if (!validate_thread_label(thread_label)) {
        return THREAD_REG_INVALID_ARGS;
    }

    if (platform_mutex_lock(&g_registry.mutex) != PLATFORM_ERROR_SUCCESS) {
        return THREAD_REG_LOCK_ERROR;
    }

    ThreadRegistryEntry* entry = thread_registry_find_thread(thread_label);
    if (!entry) {
        platform_mutex_unlock(&g_registry.mutex);
        return THREAD_REG_NOT_FOUND;
    }

    PlatformEvent_T completion_event = entry->completion_event;
    platform_mutex_unlock(&g_registry.mutex);

    PlatformWaitResult result = platform_event_wait(completion_event, timeout_ms);
    
    switch (result) {
        case PLATFORM_WAIT_SUCCESS:
            return THREAD_REG_SUCCESS;
        case PLATFORM_WAIT_TIMEOUT:
            return THREAD_REG_TIMEOUT;
        default:
            return THREAD_REG_WAIT_ERROR;
    }
}

void thread_registry_cleanup(void) {
    if (!g_registry_initialized) {
        return;
    }

    platform_mutex_lock(&g_registry.mutex);

    ThreadRegistryEntry* current = g_registry.head;
    ThreadRegistryEntry* next;

    while (current) {
        next = current->next;

        // Destroy the completion event
        platform_event_destroy(current->completion_event);

        // Clean up message queue if it exists
        if (current->queue) {
            platform_event_destroy(current->queue->not_empty_event);
            platform_event_destroy(current->queue->not_full_event);
            free(current->queue->entries);
            free(current->queue);
        }

        // Clean up thread if auto_cleanup is enabled
        if (current->auto_cleanup && current->thread) {
            if (current->thread->exit_func) {
                current->thread->exit_func((void*)current->thread);
            }
            // The thread should be cleaned up by its owner
        }

        free(current);
        current = next;
    }

    g_registry.head = NULL;
    g_registry.count = 0;

    platform_mutex_unlock(&g_registry.mutex);
    platform_mutex_destroy(&g_registry.mutex);

    g_registry_initialized = false;
}

bool thread_registry_is_registered(
    const ThreadConfig* thread
) {
    if (!g_registry_initialized || !thread || !validate_thread_label(thread->label)) {
        return false;
    }

    if (platform_mutex_lock(&g_registry.mutex) != PLATFORM_ERROR_SUCCESS) {
        return false;
    }

    ThreadRegistryEntry* entry = thread_registry_find_thread(thread->label);
    bool is_registered = (entry != NULL);

    platform_mutex_unlock(&g_registry.mutex);
    return is_registered;
}

ThreadRegistryError init_queue(
    const char* thread_label
) {
    if (!g_registry_initialized) {
        return THREAD_REG_NOT_INITIALIZED;
    }

    uint32_t max_size = 1024;  // Default size
    if (!validate_thread_label(thread_label) || max_size == 0) {
        return THREAD_REG_INVALID_ARGS;
    }

    if (platform_mutex_lock(&g_registry.mutex) != PLATFORM_ERROR_SUCCESS) {
        return THREAD_REG_LOCK_ERROR;
    }

    ThreadRegistryEntry* entry = thread_registry_find_thread(thread_label);
    if (!entry) {
        platform_mutex_unlock(&g_registry.mutex);
        return THREAD_REG_NOT_FOUND;
    }

    // Don't recreate if queue already exists
    if (entry->queue) {
        platform_mutex_unlock(&g_registry.mutex);
        return THREAD_REG_SUCCESS;
    }

    // Allocate queue structure
    entry->queue = (MessageQueue_T*)calloc(1, sizeof(MessageQueue_T));
    if (!entry->queue) {
        platform_mutex_unlock(&g_registry.mutex);
        return THREAD_REG_CREATION_FAILED;
    }

    // Allocate message entries
    entry->queue->entries = (Message_T*)calloc(max_size, sizeof(Message_T));
    if (!entry->queue->entries) {
        free(entry->queue);
        entry->queue = NULL;
        platform_mutex_unlock(&g_registry.mutex);
        return THREAD_REG_CREATION_FAILED;
    }

    // Initialize queue fields
    entry->queue->max_size = max_size;
    entry->queue->head = 0;
    entry->queue->tail = 0;
    entry->queue->owner_label = thread_label;  // Set the owner label

    // Create synchronization events
    PlatformErrorCode err = platform_event_create(&entry->queue->not_empty_event, false, false);
    if (err != PLATFORM_ERROR_SUCCESS) {
        free(entry->queue->entries);
        free(entry->queue);
        entry->queue = NULL;
        platform_mutex_unlock(&g_registry.mutex);
        return THREAD_REG_CREATION_FAILED;
    }

    err = platform_event_create(&entry->queue->not_full_event, false, true);
    if (err != PLATFORM_ERROR_SUCCESS) {
        platform_event_destroy(entry->queue->not_empty_event);
        free(entry->queue->entries);
        free(entry->queue);
        entry->queue = NULL;
        platform_mutex_unlock(&g_registry.mutex);
        return THREAD_REG_CREATION_FAILED;
    }

    platform_mutex_unlock(&g_registry.mutex);
    return THREAD_REG_SUCCESS;
}

ThreadRegistryError push_message(
    const char* thread_label,
    const Message_T* message,
    uint32_t timeout_ms
) {
    if (!g_registry_initialized) {
        return THREAD_REG_NOT_INITIALIZED;
    }

    if (!validate_thread_label(thread_label) || !message) {
        return THREAD_REG_INVALID_ARGS;
    }

    if (platform_mutex_lock(&g_registry.mutex) != PLATFORM_ERROR_SUCCESS) {
        return THREAD_REG_LOCK_ERROR;
    }

    ThreadRegistryEntry* entry = thread_registry_find_thread(thread_label);
    if (!entry || !entry->queue) {
        platform_mutex_unlock(&g_registry.mutex);
        return THREAD_REG_NOT_FOUND;
    }

    MessageQueue_T* queue = entry->queue;
    platform_mutex_unlock(&g_registry.mutex);

    if (!message_queue_push(queue, message, timeout_ms)) {
        return THREAD_REG_QUEUE_FULL;
    }

    return THREAD_REG_SUCCESS;
}

ThreadRegistryError pop_message(
    const char* thread_label,
    Message_T* message,
    uint32_t timeout_ms
) {
    if (!g_registry_initialized) {
        return THREAD_REG_NOT_INITIALIZED;
    }

    if (!validate_thread_label(thread_label) || !message) {
        return THREAD_REG_INVALID_ARGS;
    }

    PlatformThreadId current = platform_thread_get_id();
    
    if (platform_mutex_lock(&g_registry.mutex) != PLATFORM_ERROR_SUCCESS) {
        return THREAD_REG_LOCK_ERROR;
    }

    ThreadRegistryEntry* entry = thread_registry_find_thread(thread_label);
    if (!entry || !entry->queue) {
        platform_mutex_unlock(&g_registry.mutex);
        return THREAD_REG_NOT_FOUND;
    }

    if (entry->thread->thread_id != current) {
        platform_mutex_unlock(&g_registry.mutex);
        return THREAD_REG_UNAUTHORIZED;
    }

    MessageQueue_T* queue = entry->queue;
    platform_mutex_unlock(&g_registry.mutex);

    if (!message_queue_pop(queue, message, timeout_ms)) {
        return THREAD_REG_QUEUE_EMPTY;
    }

    return THREAD_REG_SUCCESS;
}

MessageQueue_T* get_queue_by_label(const char* thread_label) {
    if (!validate_thread_label(thread_label)) {
        return NULL;
    }


    if (platform_mutex_lock(&g_registry.mutex) != PLATFORM_ERROR_SUCCESS) {
        return NULL;
    }

    ThreadRegistryEntry* entry = thread_registry_find_thread(thread_label);
    MessageQueue_T* queue = entry ? entry->queue : NULL;

    platform_mutex_unlock(&g_registry.mutex);
    return queue;
}

ThreadRegistryError thread_registry_wait_all(uint32_t timeout_ms) {
    if (!g_registry_initialized) {
        return THREAD_REG_NOT_INITIALIZED;
    }

    platform_mutex_lock(&g_registry.mutex);
    
    // Count and collect active thread handles
    uint32_t active_count = 0;
    ThreadRegistryEntry* entry = g_registry.head;
    while (entry != NULL) {
        if (entry->state != THREAD_STATE_TERMINATED) {
            active_count++;
        }
        entry = entry->next;
    }
    
    if (active_count == 0) {
        platform_mutex_unlock(&g_registry.mutex);
        return THREAD_REG_SUCCESS;
    }
    
    // Allocate handles array
    PlatformThreadId* thread_list = malloc(active_count * sizeof(PlatformThreadId));
    if (!thread_list) {
        platform_mutex_unlock(&g_registry.mutex);
        return THREAD_REG_ALLOCATION_FAILED;
    }
    
    // Collect thread_ids
    uint32_t i = 0;
    entry = g_registry.head;
    while (entry != NULL && i < active_count) {
        if (entry->state != THREAD_STATE_TERMINATED) {
            thread_list[i++] = entry->thread->thread_id;
        }
        entry = entry->next;
    }
    
    platform_mutex_unlock(&g_registry.mutex);
    
    // Wait for all threads
    PlatformWaitResult result = platform_wait_multiple(thread_list, active_count, true, timeout_ms);
    free(thread_list);
    
    switch (result) {
        case PLATFORM_WAIT_SUCCESS:
            return THREAD_REG_SUCCESS;
        case PLATFORM_WAIT_TIMEOUT:
            return THREAD_REG_TIMEOUT;
        default:
            return THREAD_REG_WAIT_ERROR;
    }
}

ThreadRegistryError thread_registry_wait_others(void) {
    if (!g_registry_initialized) {
        return THREAD_REG_NOT_INITIALIZED;
    }

    PlatformThreadId current_id = platform_thread_get_id();

    platform_mutex_lock(&g_registry.mutex);
    
    uint32_t active_count = 0;
    ThreadRegistryEntry* entry = g_registry.head;
    while (entry != NULL) {
        if (entry->state != THREAD_STATE_TERMINATED && 
            entry->thread->thread_id != current_id) {
            active_count++;
        }
        entry = entry->next;
    }
    
    if (active_count == 0) {
        platform_mutex_unlock(&g_registry.mutex);
        return THREAD_REG_SUCCESS;
    }
    
    PlatformThreadId* thread_list = malloc(active_count * sizeof(PlatformThreadId));
    if (!thread_list) {
        platform_mutex_unlock(&g_registry.mutex);
        return THREAD_REG_ALLOCATION_FAILED;
    }
    
    uint32_t i = 0;
    entry = g_registry.head;
    while (entry != NULL && i < active_count) {
        if (entry->state != THREAD_STATE_TERMINATED && 
            entry->thread->thread_id != current_id) {
            thread_list[i++] = entry->thread->thread_id;
        }
        entry = entry->next;
    }
    
    platform_mutex_unlock(&g_registry.mutex);
    
    PlatformWaitResult result = platform_wait_multiple(thread_list, active_count, true, PLATFORM_WAIT_INFINITE);
    free(thread_list);
    
    return (result == PLATFORM_WAIT_SUCCESS) ? THREAD_REG_SUCCESS : THREAD_REG_WAIT_ERROR;
}
