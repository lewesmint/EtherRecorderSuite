/**
 * @file thread_registry.c
 * @brief Implementation of the thread registry.
 */
#include "thread_registry.h"
#include "platform_utils.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>

bool thread_registry_init(ThreadRegistry* registry) {
    if (!registry) {
        return false;
    }
    
    registry->head = NULL;
    registry->count = 0;
    
    if (platform_mutex_init(&registry->mutex) != 0) {
        logger_log(LOG_ERROR, "Failed to initialize thread registry mutex");
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
    
    // Initialize the entry
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
    
    ThreadRegistryEntry* entry = registry->head;
    while (entry != NULL) {
        ThreadRegistryEntry* next = entry->next;
        
        // Close handle if auto_cleanup is enabled
        if (entry->auto_cleanup && entry->handle != NULL) {
            platform_thread_close(entry->handle);
        }
        
        free(entry);
        entry = next;
    }
    
    registry->head = NULL;
    registry->count = 0;
    
    platform_mutex_unlock(&registry->mutex);
    platform_mutex_destroy(&registry->mutex);
    
    logger_log(LOG_DEBUG, "Thread registry cleaned up");
}