#include "thread_registry.h"
#include "message_types.h"  // Add this for Message_T and MessageQueue_T definitions
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

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

ThreadRegistryError thread_registry_init(ThreadRegistry* registry) {
    if (!registry) {
        return THREAD_REG_INVALID_ARGS;
    }

    registry->head = NULL;
    registry->count = 0;

    if (platform_mutex_init(&registry->mutex) != PLATFORM_ERROR_SUCCESS) {
        return THREAD_REG_LOCK_ERROR;
    }

    return THREAD_REG_SUCCESS;
}

ThreadRegistryEntry* thread_registry_find_thread(
    ThreadRegistry* registry, 
    const char* thread_label
) {
    ThreadRegistryEntry* current = registry->head;
    while (current) {
        if (strcmp(current->thread->label, thread_label) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

ThreadRegistryError thread_registry_register(
    ThreadRegistry* registry,
    const AppThread_T* thread,
    PlatformThreadHandle handle,
    bool auto_cleanup
) {
    if (!registry || !thread || !handle || !validate_thread_label(thread->label)) {
        return THREAD_REG_INVALID_ARGS;
    }

    if (platform_mutex_lock(&registry->mutex) != PLATFORM_ERROR_SUCCESS) {
        return THREAD_REG_LOCK_ERROR;
    }

    if (thread_registry_find_thread(registry, thread->label)) {
        platform_mutex_unlock(&registry->mutex);
        return THREAD_REG_DUPLICATE_THREAD;
    }

    ThreadRegistryEntry* entry = calloc(1, sizeof(ThreadRegistryEntry));
    if (!entry) {
        platform_mutex_unlock(&registry->mutex);
        return THREAD_REG_CREATION_FAILED;
    }

    // Create completion event - manual reset, initially not signaled
    if (platform_event_create(&entry->completion_event, true, false) != PLATFORM_ERROR_SUCCESS) {
        free(entry);
        platform_mutex_unlock(&registry->mutex);
        return THREAD_REG_CREATION_FAILED;
    }

    entry->thread = thread;
    entry->handle = handle;
    entry->state = THREAD_STATE_CREATED;
    entry->auto_cleanup = auto_cleanup;
    entry->next = registry->head;
    
    registry->head = entry;
    registry->count++;

    platform_mutex_unlock(&registry->mutex);
    
    logger_log(LOG_INFO, "Thread '%s' registered successfully", thread->label);
    return THREAD_REG_SUCCESS;
}

ThreadRegistryError thread_registry_update_state(
    ThreadRegistry* registry,
    const char* thread_label,
    ThreadState new_state
) {
    if (!registry || !validate_thread_label(thread_label)) {
        return THREAD_REG_INVALID_ARGS;
    }

    if (platform_mutex_lock(&registry->mutex) != PLATFORM_ERROR_SUCCESS) {
        return THREAD_REG_LOCK_ERROR;
    }

    ThreadRegistryEntry* entry = thread_registry_find_thread(registry, thread_label);
    if (!entry) {
        platform_mutex_unlock(&registry->mutex);
        return THREAD_REG_NOT_FOUND;
    }

    if (!validate_state_transition(entry->state, new_state)) {
        platform_mutex_unlock(&registry->mutex);
        return THREAD_REG_INVALID_STATE_TRANSITION;
    }

    entry->state = new_state;

    // Signal completion event when thread terminates
    if (new_state == THREAD_STATE_TERMINATED || new_state == THREAD_STATE_FAILED) {
        platform_event_set(entry->completion_event);
    }

    platform_mutex_unlock(&registry->mutex);
    return THREAD_REG_SUCCESS;
}

ThreadState thread_registry_get_state(
    ThreadRegistry* registry,
    const char* thread_label
) {
    if (!registry || !validate_thread_label(thread_label)) {
        return THREAD_STATE_FAILED;
    }

    if (platform_mutex_lock(&registry->mutex) != PLATFORM_ERROR_SUCCESS) {
        return THREAD_STATE_FAILED;
    }

    ThreadRegistryEntry* entry = thread_registry_find_thread(registry, thread_label);
    ThreadState state = THREAD_STATE_FAILED;

    if (entry) {
        state = entry->state;
    }

    platform_mutex_unlock(&registry->mutex);
    return state;
}

ThreadRegistry* get_thread_registry(void) {
    return &g_registry;
}

ThreadRegistryError init_global_thread_registry(void) {
    if (g_registry_initialized) {
        return THREAD_REG_SUCCESS;
    }

    ThreadRegistryError err = thread_registry_init(&g_registry);
    if (err == THREAD_REG_SUCCESS) {
        g_registry_initialized = true;
    }

    return err;
}

ThreadRegistryError thread_registry_wait_for_thread(
    ThreadRegistry* registry,
    const char* thread_label,
    uint32_t timeout_ms
) {
    if (!registry || !validate_thread_label(thread_label)) {
        return THREAD_REG_INVALID_ARGS;
    }

    if (platform_mutex_lock(&registry->mutex) != PLATFORM_ERROR_SUCCESS) {
        return THREAD_REG_LOCK_ERROR;
    }

    ThreadRegistryEntry* entry = thread_registry_find_thread(registry, thread_label);
    if (!entry) {
        platform_mutex_unlock(&registry->mutex);
        return THREAD_REG_NOT_FOUND;
    }

    // Get the event while holding the mutex
    PlatformEvent_T completion_event = entry->completion_event;
    platform_mutex_unlock(&registry->mutex);

    // Wait for the completion event
    PlatformErrorCode result = platform_event_wait(completion_event, timeout_ms);
    
    if (result == PLATFORM_ERROR_TIMEOUT) {
        return THREAD_REG_TIMEOUT;
    } else if (result != PLATFORM_ERROR_SUCCESS) {
        return THREAD_REG_WAIT_ERROR;
    }

    return THREAD_REG_SUCCESS;
}

void thread_registry_cleanup(ThreadRegistry* registry) {
    if (!registry) {
        return;
    }

    platform_mutex_lock(&registry->mutex);

    ThreadRegistryEntry* current = registry->head;
    ThreadRegistryEntry* next;

    while (current) {
        next = current->next;

        // Destroy the completion event
        platform_event_destroy(current->completion_event);

        // Clean up thread if auto_cleanup is enabled
        if (current->auto_cleanup && current->thread) {
            if (current->thread->exit_func) {
                current->thread->exit_func((void*)current->thread);  // Cast to void*
            }
            // The thread should be cleaned up by its owner
        }

        free(current);
        current = next;
    }

    registry->head = NULL;
    registry->count = 0;

    platform_mutex_unlock(&registry->mutex);
    platform_mutex_destroy(&registry->mutex);
}

bool thread_registry_is_registered(
    ThreadRegistry* registry,
    const AppThread_T* thread
) {
    if (!registry || !thread || !validate_thread_label(thread->label)) {
        return false;
    }

    if (platform_mutex_lock(&registry->mutex) != PLATFORM_ERROR_SUCCESS) {
        return false;
    }

    ThreadRegistryEntry* entry = thread_registry_find_thread(registry, thread->label);
    bool is_registered = (entry != NULL);

    platform_mutex_unlock(&registry->mutex);
    return is_registered;
}

ThreadRegistryError thread_registry_init_queue(
    ThreadRegistry* registry,
    const char* thread_label,
    uint32_t max_size
) {
    if (!registry || !validate_thread_label(thread_label) || max_size == 0) {
        return THREAD_REG_INVALID_ARGS;
    }

    if (platform_mutex_lock(&registry->mutex) != PLATFORM_ERROR_SUCCESS) {
        return THREAD_REG_LOCK_ERROR;
    }

    ThreadRegistryEntry* entry = thread_registry_find_thread(registry, thread_label);
    if (!entry) {
        platform_mutex_unlock(&registry->mutex);
        return THREAD_REG_NOT_FOUND;
    }

    // Don't recreate if queue already exists
    if (entry->queue) {
        platform_mutex_unlock(&registry->mutex);
        return THREAD_REG_SUCCESS;
    }

    // Allocate queue structure
    entry->queue = (MessageQueue_T*)calloc(1, sizeof(MessageQueue_T));
    if (!entry->queue) {
        platform_mutex_unlock(&registry->mutex);
        return THREAD_REG_CREATION_FAILED;
    }

    // Allocate message entries
    entry->queue->entries = (Message_T*)calloc(max_size, sizeof(Message_T));
    if (!entry->queue->entries) {
        free(entry->queue);
        entry->queue = NULL;
        platform_mutex_unlock(&registry->mutex);
        return THREAD_REG_CREATION_FAILED;
    }

    // Initialize queue
    entry->queue->max_size = max_size;
    entry->queue->head = 0;
    entry->queue->tail = 0;

    // Create synchronization events
    PlatformErrorCode err = platform_event_create(&entry->queue->not_empty_event, false, false);
    if (err != PLATFORM_ERROR_SUCCESS) {
        free(entry->queue->entries);
        free(entry->queue);
        entry->queue = NULL;
        platform_mutex_unlock(&registry->mutex);
        return THREAD_REG_CREATION_FAILED;
    }

    err = platform_event_create(&entry->queue->not_full_event, false, true);
    if (err != PLATFORM_ERROR_SUCCESS) {
        platform_event_destroy(entry->queue->not_empty_event);
        free(entry->queue->entries);
        free(entry->queue);
        entry->queue = NULL;
        platform_mutex_unlock(&registry->mutex);
        return THREAD_REG_CREATION_FAILED;
    }

    platform_mutex_unlock(&registry->mutex);
    return THREAD_REG_SUCCESS;
}

ThreadRegistryError thread_registry_push_message(
    ThreadRegistry* registry,
    const char* thread_label,
    const Message_T* message,
    uint32_t timeout_ms
) {
    if (!registry || !validate_thread_label(thread_label) || !message) {
        return THREAD_REG_INVALID_ARGS;
    }

    if (platform_mutex_lock(&registry->mutex) != PLATFORM_ERROR_SUCCESS) {
        return THREAD_REG_LOCK_ERROR;
    }

    ThreadRegistryEntry* entry = thread_registry_find_thread(registry, thread_label);
    if (!entry || !entry->queue) {
        platform_mutex_unlock(&registry->mutex);
        return THREAD_REG_NOT_FOUND;
    }

    MessageQueue_T* queue = entry->queue;
    platform_mutex_unlock(&registry->mutex);

    if (!message_queue_push(queue, message, timeout_ms)) {
        return THREAD_REG_QUEUE_FULL;
    }

    return THREAD_REG_SUCCESS;
}

ThreadRegistryError thread_registry_pop_message(
    ThreadRegistry* registry,
    const char* thread_label,
    Message_T* message,
    uint32_t timeout_ms
) {
    if (!registry || !validate_thread_label(thread_label) || !message) {
        return THREAD_REG_INVALID_ARGS;
    }

    if (platform_mutex_lock(&registry->mutex) != PLATFORM_ERROR_SUCCESS) {
        return THREAD_REG_LOCK_ERROR;
    }

    ThreadRegistryEntry* entry = thread_registry_find_thread(registry, thread_label);
    if (!entry || !entry->queue) {
        platform_mutex_unlock(&registry->mutex);
        return THREAD_REG_NOT_FOUND;
    }

    MessageQueue_T* queue = entry->queue;
    platform_mutex_unlock(&registry->mutex);

    if (!message_queue_pop(queue, message, timeout_ms)) {
        return THREAD_REG_QUEUE_EMPTY;
    }

    return THREAD_REG_SUCCESS;
}
