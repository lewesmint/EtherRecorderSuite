/**
 * @file thread_registry.c
 * @brief Implementation of the thread registry.
 */
#include "thread_registry.h"
#include "platform_error.h"
#include "platform_mutex.h"
#include "platform_atomic.h"
#include <stdlib.h>
#include <string.h>

#include "platform_utils.h"
#include "logger.h"

// Static global registry instance
static ThreadRegistry g_registry = {0};

// Add static variable for initialization state
bool g_registry_initialized = false;


ThreadRegistry* get_thread_registry(void) {
    return &g_registry;
}

bool init_global_thread_registry(void) {
    if (g_registry_initialized) {
        return true;
    }
    
    if (!thread_registry_init(&g_registry)) {
        return false;
    }
    
    g_registry_initialized = true;
    return true;
}

bool thread_registry_init(ThreadRegistry* registry) {
    if (!registry) {
        return false;
    }
    
    registry->head = NULL;
    registry->count = 0;
    
    if (platform_mutex_init(&registry->mutex) != 0) {
        logger_log(LOG_ERROR, "Failed to initialise thread registry mutex");
        return false;
    }
    
    return true;
}

bool thread_registry_register(ThreadRegistry* registry, AppThread_T* thread, ThreadHandle_T handle, bool auto_cleanup) {
    if (!registry || !thread || handle == NULL) {
        return false;
    }
    
    // Check if the thread is already registered
    if (thread_registry_is_registered(registry, thread)) {
        logger_log(LOG_WARN, "Thread '%s' is already registered", thread->label);
        return false;
    }
    
    // Create a new registry entry
    ThreadRegistryEntry* entry = (ThreadRegistryEntry*)malloc(sizeof(ThreadRegistryEntry));
    if (!entry) {
        logger_log(LOG_ERROR, "Failed to allocate memory for thread registry entry");
        return false;
    }
    
    // Initialise the entry
    entry->thread = thread;
    entry->state = THREAD_STATE_CREATED;
    entry->handle = handle;
    entry->auto_cleanup = auto_cleanup;
    entry->next = NULL;
    
    // Add the entry to the registry
    platform_mutex_lock(&registry->mutex);
    
    if (registry->head == NULL) {
        registry->head = entry;
    } else {
        ThreadRegistryEntry* current = registry->head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = entry;
    }
    
    registry->count++;
    
    platform_mutex_unlock(&registry->mutex);
    
    logger_log(LOG_DEBUG, "Thread '%s' registered in registry", thread->label);
    
    return true;
}

bool thread_registry_update_state(ThreadRegistry* registry, AppThread_T* thread, ThreadState state) {
    if (!registry || !thread) {
        return false;
    }
    
    bool found = false;
    
    platform_mutex_lock(&registry->mutex);
    
    ThreadRegistryEntry* entry = registry->head;
    while (entry != NULL) {
        if (entry->thread == thread) {
            entry->state = state;
            found = true;
            break;
        }
        entry = entry->next;
    }
    
    platform_mutex_unlock(&registry->mutex);
    
    if (!found) {
        logger_log(LOG_WARN, "Thread '%s' not found in registry when updating state", thread->label);
    }
    
    return found;
}

bool thread_registry_deregister(ThreadRegistry* registry, AppThread_T* thread) {
    if (!registry || !thread) {
        return false;
    }
    
    bool found = false;
    
    platform_mutex_lock(&registry->mutex);
    
    ThreadRegistryEntry* entry = registry->head;
    ThreadRegistryEntry* prev = NULL;
    
    while (entry != NULL) {
        if (entry->thread == thread) {
            // Remove entry from linked list
            if (prev == NULL) {
                registry->head = entry->next;
            } else {
                prev->next = entry->next;
            }
            
            // Clean up entry
            free(entry);
            registry->count--;
            found = true;
            break;
        }
        
        prev = entry;
        entry = entry->next;
    }
    
    platform_mutex_unlock(&registry->mutex);
    
    if (!found) {
        logger_log(LOG_WARN, "Thread '%s' not found in registry when deregistering", thread->label);
    } else {
        logger_log(LOG_DEBUG, "Thread '%s' deregistered from registry", thread->label);
    }
    
    return found;
}

ThreadRegistryEntry* thread_registry_find_by_label(ThreadRegistry* registry, const char* label) {
    if (!registry || !label) {
        return NULL;
    }
    
    ThreadRegistryEntry* result = NULL;
    
    platform_mutex_lock(&registry->mutex);
    
    ThreadRegistryEntry* entry = registry->head;
    while (entry != NULL) {
        if (entry->thread && entry->thread->label && strcmp(entry->thread->label, label) == 0) {
            result = entry;
            break;
        }
        entry = entry->next;
    }
    
    platform_mutex_unlock(&registry->mutex);
    
    return result;
}

ThreadRegistryEntry* thread_registry_find_by_handle(ThreadRegistry* registry, ThreadHandle_T handle) {
    if (!registry || handle == NULL) {
        return NULL;
    }
    
    ThreadRegistryEntry* result = NULL;
    
    platform_mutex_lock(&registry->mutex);
    
    ThreadRegistryEntry* entry = registry->head;
    while (entry != NULL) {
        if (entry->handle == handle) {
            result = entry;
            break;
        }
        entry = entry->next;
    }
    
    platform_mutex_unlock(&registry->mutex);
    
    return result;
}

bool thread_registry_is_registered(ThreadRegistry* registry, AppThread_T* thread) {
    if (!registry || !thread) {
        return false;
    }
    
    bool result = false;
    
    platform_mutex_lock(&registry->mutex);
    
    ThreadRegistryEntry* entry = registry->head;
    while (entry != NULL) {
        if (entry->thread == thread) {
            result = true;
            break;
        }
        entry = entry->next;
    }
    
    platform_mutex_unlock(&registry->mutex);
    
    return result;
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

        // Clean up message queue if it exists
        if (current->queue) {
            platform_event_destroy(&current->queue->not_empty_event);
            platform_event_destroy(&current->queue->not_full_event);
            free(current->queue->entries);
            free(current->queue);
        }

        // Clean up thread if auto_cleanup is enabled
        if (current->auto_cleanup && current->thread) {
            // Call thread's exit function if it exists
            if (current->thread->exit_func) {
                current->thread->exit_func(current->thread);
            }
            free(current->thread);
        }

        free(current);
        current = next;
    }

    registry->head = NULL;
    registry->count = 0;

    platform_mutex_unlock(&registry->mutex);
    platform_mutex_destroy(&registry->mutex);
}

bool thread_registry_init_queue(ThreadRegistry* registry, AppThread_T* thread, uint32_t max_size) {
    if (!registry || !thread) {
        return false;
    }

    platform_mutex_lock(&registry->mutex);
    
    ThreadRegistryEntry* entry = thread_registry_find_by_label(registry, thread->label);
    if (!entry) {
        platform_mutex_unlock(&registry->mutex);
        return false;
    }

    // Allocate and initialize queue if not already present
    if (!entry->queue) {
        entry->queue = (MessageQueue_T*)malloc(sizeof(MessageQueue_T));
        if (!entry->queue) {
            platform_mutex_unlock(&registry->mutex);
            return false;
        }

        entry->queue->entries = (Message_T*)malloc(sizeof(Message_T) * max_size);
        if (!entry->queue->entries) {
            free(entry->queue);
            entry->queue = NULL;
            platform_mutex_unlock(&registry->mutex);
            return false;
        }

        entry->queue->head = 0;
        entry->queue->tail = 0;
        entry->queue->max_size = max_size;
        
        // if (!platform_event_init(&entry->queue->not_empty_event) ||
        //     !platform_event_init(&entry->queue->not_full_event)) {
        //     free(entry->queue->entries);
        //     free(entry->queue);
        //     entry->queue = NULL;
        //     platform_mutex_unlock(&registry->mutex);
        //     return false;
        // }
    }

    platform_mutex_unlock(&registry->mutex);
    return true;
}

bool thread_registry_push_message(ThreadRegistry* registry, const char* thread_label, Message_T* message) {
    if (!registry || !thread_label || !message) {
        return false;
    }

    platform_mutex_lock(&registry->mutex);
    
    ThreadRegistryEntry* entry = thread_registry_find_by_label(registry, thread_label);
    if (!entry || !entry->queue) {
        platform_mutex_unlock(&registry->mutex);
        return false;
    }

    MessageQueue_T* queue = entry->queue;
    int32_t next_tail = (queue->tail + 1) % queue->max_size;

    // Check if queue is full
    if (next_tail == queue->head) {
        platform_mutex_unlock(&registry->mutex);
        return false;
    }

    // // Copy message to queue
    // queue->entries[queue->tail] = *message;
    // platform_atomic_store_i32(&queue->tail, next_tail);

    // // Signal that queue is not empty
    // platform_event_signal(&queue->not_empty_event);

    platform_mutex_unlock(&registry->mutex);
    return true;
}

bool thread_registry_pop_message(ThreadRegistry* registry, const char* thread_label, 
                               Message_T* message, int32_t timeout_ms) {
    if (!registry || !thread_label || !message) {
        return false;
    }

    platform_mutex_lock(&registry->mutex);
    
    ThreadRegistryEntry* entry = thread_registry_find_by_label(registry, thread_label);
    if (!entry || !entry->queue) {
        platform_mutex_unlock(&registry->mutex);
        return false;
    }

    MessageQueue_T* queue = entry->queue;

    // Check if queue is empty
    if (queue->head == queue->tail) {
        if (timeout_ms == 0) {
            platform_mutex_unlock(&registry->mutex);
            return false;
        }
        
        // Wait for message
        platform_mutex_unlock(&registry->mutex);
        if (!platform_event_wait(&queue->not_empty_event, timeout_ms)) {
            return false;
        }
        platform_mutex_lock(&registry->mutex);
    }

    // // Get message from queue
    // *message = queue->entries[queue->head];
    // platform_atomic_store_i32(&queue->head, (queue->head + 1) % queue->max_size);

    // // Signal that queue is not full
    // platform_event_signal(&queue->not_full_event);

    platform_mutex_unlock(&registry->mutex);
    return true;
}

bool thread_registry_setup_relay(ThreadRegistry* registry, const char* source_label, 
                               const char* target_label) {
    if (!registry || !source_label || !target_label) {
        return false;
    }

    platform_mutex_lock(&registry->mutex);
    
    ThreadRegistryEntry* source = thread_registry_find_by_label(registry, source_label);
    ThreadRegistryEntry* target = thread_registry_find_by_label(registry, target_label);

    if (!source || !target || !source->queue || !target->queue) {
        platform_mutex_unlock(&registry->mutex);
        return false;
    }

    // Setup relay connection
    // Implementation depends on how you want to handle message relay
    // This is a basic example that could be enhanced based on requirements

    platform_mutex_unlock(&registry->mutex);
    return true;
}

uint32_t thread_registry_get_queue_size(ThreadRegistry* registry, const char* thread_label) {
    if (!registry || !thread_label) {
        return 0;
    }

    platform_mutex_lock(&registry->mutex);
    
    ThreadRegistryEntry* entry = thread_registry_find_by_label(registry, thread_label);
    if (!entry || !entry->queue) {
        platform_mutex_unlock(&registry->mutex);
        return 0;
    }

    MessageQueue_T* queue = entry->queue;
    uint32_t size;
    
    if (queue->tail >= queue->head) {
        size = queue->tail - queue->head;
    } else {
        size = queue->max_size - (queue->head - queue->tail);
    }

    platform_mutex_unlock(&registry->mutex);
    return size;
}

void thread_registry_clear_queue(ThreadRegistry* registry, const char* thread_label) {
    if (!registry || !thread_label) {
        return;
    }

    platform_mutex_lock(&registry->mutex);
    
    ThreadRegistryEntry* entry = thread_registry_find_by_label(registry, thread_label);
    if (!entry || !entry->queue) {
        platform_mutex_unlock(&registry->mutex);
        return;
    }

    MessageQueue_T* queue = entry->queue;
    queue->head = queue->tail = 0;
    // platform_event_signal(&queue->not_full_event);

    platform_mutex_unlock(&registry->mutex);
}
