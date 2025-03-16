#include "thread_registry.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "platform_error.h"
#include "platform_mutex.h"
#include "platform_sync.h"

#include "utils.h"
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

PlatformWaitResult thread_registry_wait_for_thread(PlatformThreadId thread_id, uint32_t timeout_ms) {
    if (!thread_id) {
        return PLATFORM_WAIT_ERROR;
    }

    return thread_registry_wait_list(&thread_id, 1, timeout_ms);
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

PlatformWaitResult thread_registry_wait_all(uint32_t timeout_ms) {
    if (!g_registry_initialized) {
        return PLATFORM_WAIT_ERROR;
    }

    platform_mutex_lock(&g_registry.mutex);
    
    // Count active threads
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
        return PLATFORM_WAIT_SUCCESS;
    }
    
    // Allocate and populate thread list
    PlatformThreadId* thread_list = malloc(active_count * sizeof(PlatformThreadId));
    if (!thread_list) {
        platform_mutex_unlock(&g_registry.mutex);
        return PLATFORM_WAIT_ERROR;
    }
    
    uint32_t i = 0;
    entry = g_registry.head;
    while (entry != NULL && i < active_count) {
        if (entry->state != THREAD_STATE_TERMINATED) {
            thread_list[i++] = entry->thread->thread_id;
        }
        entry = entry->next;
    }
    
    platform_mutex_unlock(&g_registry.mutex);
    
    PlatformWaitResult result = thread_registry_wait_list(thread_list, active_count, timeout_ms);
    free(thread_list);
    
    return result;
}

PlatformWaitResult thread_registry_wait_others(void) {
    if (!g_registry_initialized) {
        return PLATFORM_WAIT_ERROR;
    }

    PlatformThreadId current_id = platform_thread_get_id();
    platform_mutex_lock(&g_registry.mutex);
    
    // Count other active threads
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
        return PLATFORM_WAIT_SUCCESS;
    }
    
    // Allocate and populate thread list
    PlatformThreadId* thread_list = malloc(active_count * sizeof(PlatformThreadId));
    if (!thread_list) {
        platform_mutex_unlock(&g_registry.mutex);
        return PLATFORM_WAIT_ERROR;
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
    
    PlatformWaitResult result = thread_registry_wait_list(thread_list, active_count, PLATFORM_WAIT_INFINITE);
    free(thread_list);
    
    return result;
}

static ThreadRegistryEntry* thread_registry_find_thread_by_id(PlatformThreadId thread_id) {
    ThreadRegistryEntry* current = g_registry.head;
    while (current) {
        if (current->thread->thread_id == thread_id) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

PlatformWaitResult thread_registry_wait_list(PlatformThreadId* thread_ids, uint32_t count, uint32_t timeout_ms) {
    if (!thread_ids || count == 0) {
        return PLATFORM_WAIT_ERROR;
    }

    // Keep track of which threads we need to wait for
    bool* needs_wait = calloc(count, sizeof(bool));
    if (!needs_wait) {
        return PLATFORM_WAIT_ERROR;
    }

    PlatformWaitResult final_result = PLATFORM_WAIT_SUCCESS;

    while (true) {
        platform_mutex_lock(&g_registry.mutex);
        
        // Check if any remaining threads need waiting
        bool any_active = false;
        for (uint32_t i = 0; i < count; i++) {
            if (!needs_wait[i]) {
                continue;  // Already done with this thread
            }

            ThreadRegistryEntry* entry = thread_registry_find_thread_by_id(thread_ids[i]);
            if (!entry) {
                // Thread not found - either never existed or already cleaned up
                needs_wait[i] = false;
                continue;
            }

            if (entry->state == THREAD_STATE_TERMINATED) {
                needs_wait[i] = false;
                continue;
            }

            any_active = true;
        }

        if (!any_active) {
            platform_mutex_unlock(&g_registry.mutex);
            break;  // All threads are done
        }

        platform_mutex_unlock(&g_registry.mutex);

        // Wait a short time before checking again
        uint32_t small_time = PLATFORM_DEFAULT_SLEEP_INTERVAL_MS;  // 10ms polling interval
        sleep_ms(small_time);
 
        // Check if we've exceeded our timeout
        if (timeout_ms != PLATFORM_WAIT_INFINITE) {
            timeout_ms = (timeout_ms > small_time) ? (timeout_ms - small_time) : 0;
            if (timeout_ms == 0) {
                final_result = PLATFORM_WAIT_TIMEOUT;
                break;
            }
        }
    }

    free(needs_wait);
    return final_result;
}

ThreadRegistryError thread_registry_deregister(const char* thread_label) {
    if (!g_registry_initialized) {
        return THREAD_REG_NOT_INITIALIZED;
    }

    if (!validate_thread_label(thread_label)) {
        return THREAD_REG_INVALID_ARGS;
    }

    if (platform_mutex_lock(&g_registry.mutex) != PLATFORM_ERROR_SUCCESS) {
        return THREAD_REG_LOCK_ERROR;
    }

    ThreadRegistryEntry* entry = g_registry.head;
    ThreadRegistryEntry* prev = NULL;

    while (entry) {
        if (strcmp(entry->thread->label, thread_label) == 0) {
            // Remove from linked list
            if (prev) {
                prev->next = entry->next;
            } else {
                g_registry.head = entry->next;
            }

            // Clean up the entry
            platform_event_destroy(entry->completion_event);
            
            if (entry->queue) {
                platform_event_destroy(entry->queue->not_empty_event);
                platform_event_destroy(entry->queue->not_full_event);
                free(entry->queue->entries);
                free(entry->queue);
            }

            free(entry);
            g_registry.count--;

            platform_mutex_unlock(&g_registry.mutex);
            return THREAD_REG_SUCCESS;
        }

        prev = entry;
        entry = entry->next;
    }

    platform_mutex_unlock(&g_registry.mutex);
    return THREAD_REG_NOT_FOUND;
}

static ThreadRegistryError handle_thread_failure(ThreadRegistryEntry* entry) {
    // Signal completion event
    platform_event_set(entry->completion_event);
    
    // Update state to failed
    entry->state = THREAD_STATE_FAILED;

    // If auto_cleanup is enabled, deregister the thread
    if (entry->auto_cleanup) {
        return thread_registry_deregister(entry->thread->label);
    }
    
    return THREAD_REG_SUCCESS;
}

ThreadRegistryError thread_registry_check_thread_health(const char* thread_label) {
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

    PlatformThreadStatus status;
    ThreadRegistryError result = THREAD_REG_SUCCESS;
    
    if (platform_thread_get_status(entry->thread->thread_id, &status) 
        != PLATFORM_ERROR_SUCCESS) {
        platform_mutex_unlock(&g_registry.mutex);
        return THREAD_REG_STATUS_CHECK_FAILED;
    }

    if (status == PLATFORM_THREAD_DEAD || status == PLATFORM_THREAD_TERMINATED) {
        result = handle_thread_failure(entry);
    }

    platform_mutex_unlock(&g_registry.mutex);
    return result;
}

ThreadRegistryError thread_registry_check_all_threads(void) {
    if (!g_registry_initialized) {
        return THREAD_REG_NOT_INITIALIZED;
    }

    if (platform_mutex_lock(&g_registry.mutex) != PLATFORM_ERROR_SUCCESS) {
        return THREAD_REG_LOCK_ERROR;
    }

    ThreadRegistryEntry* entry = g_registry.head;
    ThreadRegistryError result = THREAD_REG_SUCCESS;

    while (entry) {
        ThreadRegistryEntry* next = entry->next;
        
        if (entry->state == THREAD_STATE_RUNNING) {
            PlatformThreadStatus status;
            if (platform_thread_get_status(entry->thread->thread_id, &status) 
                != PLATFORM_ERROR_SUCCESS) {
                logger_log(LOG_ERROR, "Thread '%s' status check failed", 
                         entry->thread->label);
                result = THREAD_REG_STATUS_CHECK_FAILED;
                break;
            }

            if (status == PLATFORM_THREAD_DEAD || 
                status == PLATFORM_THREAD_TERMINATED) {
                logger_log(LOG_ERROR, "Thread '%s' has died unexpectedly", 
                         entry->thread->label);
                result = handle_thread_failure(entry);
                if (result != THREAD_REG_SUCCESS) {
                    break;
                }
            }
        }
        entry = next;
    }
    
    platform_mutex_unlock(&g_registry.mutex);
    return result;
}
