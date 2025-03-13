/**
 * @file thread_group.c
 * @brief Implementation of thread group management.
 */
#include "thread_group.h"

#include <string.h>

#include "logger.h"
#include "app_thread.h"
#include "platform_threads.h"
#include "platform_utils.h"

bool thread_group_init(ThreadGroup* group, const char* name) {
    if (!group || !name) {
        return false;
    }
    
    strncpy(group->name, name, sizeof(group->name) - 1);
    group->name[sizeof(group->name) - 1] = '\0';
    
    if (!thread_registry_init(&group->registry)) {
        logger_log(LOG_ERROR, "Failed to initialise thread registry for group '%s'", name);
        return false;
    }
    
    logger_log(LOG_DEBUG, "Thread group '%s' initialised", name);
    return true;
}

bool thread_group_add(ThreadGroup* group, AppThread_T* thread) {
    if (!group || !thread) {
        return false;
    }
    
    // Create the thread
    if (!thread->pre_create_func) thread->pre_create_func = pre_create_stub;
    if (!thread->post_create_func) thread->post_create_func = post_create_stub;
    if (!thread->init_func) thread->init_func = init_stub;
    if (!thread->exit_func) thread->exit_func = exit_stub;
    
    // Call pre-create function
    if (thread->pre_create_func) {
        thread->pre_create_func(thread);
    }
    
    // Create the thread
    if (platform_thread_create(&thread->thread_id, (ThreadFunc_T)app_thread, thread) != 0) {
        logger_log(LOG_ERROR, "Failed to create thread '%s' in group '%s'", thread->label, group->name);
        return false;
    }
    
    // Call post-create function
    if (thread->post_create_func) {
        thread->post_create_func(thread);
    }
    
    // Register the thread in the group's registry
    if (!thread_registry_register(&group->registry, thread, thread->thread_id, true)) {
        logger_log(LOG_ERROR, "Failed to register thread '%s' in group '%s'", thread->label, group->name);
        return false;
    }
    
    thread_registry_update_state(&group->registry, thread, THREAD_STATE_RUNNING);
    
    logger_log(LOG_DEBUG, "Thread '%s' added to group '%s'", thread->label, group->name);
    return true;
}

bool thread_group_remove(ThreadGroup* group, const char* thread_label) {
    if (!group || !thread_label) {
        return false;
    }
    
    platform_mutex_lock(&group->registry.mutex);
    
    // Find the thread entry with the matching label
    ThreadRegistryEntry* entry = group->registry.head;
    ThreadRegistryEntry* prev = NULL;
    
    while (entry != NULL) {
        AppThread_T* thread = entry->thread;
        
        if (thread && thread->label && strcmp(thread->label, thread_label) == 0) {
            // Found the thread, remove it from the registry
            
            // Update linked list
            if (prev == NULL) {
                group->registry.head = entry->next;
            } else {
                prev->next = entry->next;
            }
            
            // Update thread count
            group->registry.count--;
            
            // Free the registry entry
            free(entry);
            
            platform_mutex_unlock(&group->registry.mutex);
            
            logger_log(LOG_DEBUG, "Removed thread '%s' from group '%s'", 
                      thread_label, group->name);
            return true;
        }
        
        prev = entry;
        entry = entry->next;
    }
    
    platform_mutex_unlock(&group->registry.mutex);
    logger_log(LOG_WARN, "Thread '%s' not found in group '%s'", 
              thread_label, group->name);
    return false;
}

bool thread_group_terminate_all(ThreadGroup* group, uint32_t timeout_ms) {
    if (!group) {
        return false;
    }
    
    platform_mutex_lock(&group->registry.mutex);
    
    ThreadRegistryEntry* entry = group->registry.head;
    uint32_t active_count = 0;
    
    while (entry != NULL) {
        if (entry->state != THREAD_STATE_TERMINATED) {
            // Update thread state to stopping
            entry->state = THREAD_STATE_STOPPING;
            active_count++;
        }
        entry = entry->next;
    }
    
    if (active_count == 0) {
        platform_mutex_unlock(&group->registry.mutex);
        return true;
    }
    
    // Collect handles for active threads
    PlatformThread_T* threads = (PlatformThread_T*)malloc(active_count * sizeof(PlatformThread_T));
    if (!threads) {
        platform_mutex_unlock(&group->registry.mutex);
        logger_log(LOG_ERROR, "Failed to allocate memory for thread handles");
        return false;
    }
    
    uint32_t i = 0;
    entry = group->registry.head;
    while (entry != NULL && i < active_count) {
        if (entry->state == THREAD_STATE_STOPPING) {
            threads[i++] = entry->handle;
        }
        entry = entry->next;
    }
    
    platform_mutex_unlock(&group->registry.mutex);
    
    // Wait for all threads to terminate
    int result = platform_thread_wait_multiple(threads, active_count, true, timeout_ms);
    free(threads);
    
    if (result == PLATFORM_WAIT_SUCCESS) {
        // Update all thread states to terminated
        platform_mutex_lock(&group->registry.mutex);
        entry = group->registry.head;
        while (entry != NULL) {
            if (entry->state == THREAD_STATE_STOPPING) {
                entry->state = THREAD_STATE_TERMINATED;
            }
            entry = entry->next;
        }
        platform_mutex_unlock(&group->registry.mutex);
        
        logger_log(LOG_DEBUG, "All threads in group '%s' terminated", group->name);
        return true;
    } else if (result == PLATFORM_WAIT_TIMEOUT) {
        // Only log timeout warning during shutdown
        if (shutdown_signalled()) {
            logger_log(LOG_WARN, "Timeout waiting for threads in group '%s' to terminate", group->name);
        }
        return false;
    } else {
        char error_msg[256];
        platform_get_error_message(error_msg, sizeof(error_msg));
        logger_log(LOG_ERROR, "Error waiting for threads in group '%s' to terminate: %s", 
                   group->name, error_msg);
        return false;
    }
}

bool thread_group_wait_all(ThreadGroup* group, uint32_t timeout_ms) {
    if (!group) {
        return false;
    }
    
    platform_mutex_lock(&group->registry.mutex);
    
    uint32_t active_count = 0;
    ThreadRegistryEntry* entry = group->registry.head;
    
    while (entry != NULL) {
        if (entry->state != THREAD_STATE_TERMINATED) {
            active_count++;
        }
        entry = entry->next;
    }
    
    if (active_count == 0) {
        platform_mutex_unlock(&group->registry.mutex);
        return true;
    }
    
    // Allocate handles array
    PlatformThread_T* threads = (PlatformThread_T*)malloc(active_count * sizeof(PlatformThread_T));
    if (!threads) {
        platform_mutex_unlock(&group->registry.mutex);
        logger_log(LOG_ERROR, "Failed to allocate memory for thread handles");
        return false;
    }
    
    // Collect handles
    uint32_t i = 0;
    entry = group->registry.head;
    while (entry != NULL && i < active_count) {
        if (entry->state != THREAD_STATE_TERMINATED) {
            threads[i++] = entry->handle;
        }
        entry = entry->next;
    }
    
    platform_mutex_unlock(&group->registry.mutex);
    
    // Wait for all threads
    int result = platform_thread_wait_multiple(threads, active_count, true, timeout_ms);
    free(threads);
    
    if (result == PLATFORM_WAIT_SUCCESS) {
        // Update thread states
        platform_mutex_lock(&group->registry.mutex);
        entry = group->registry.head;
        while (entry != NULL) {
            if (entry->state != THREAD_STATE_TERMINATED) {
                entry->state = THREAD_STATE_TERMINATED;
            }
            entry = entry->next;
        }
        platform_mutex_unlock(&group->registry.mutex);
        
        logger_log(LOG_DEBUG, "All threads in group '%s' completed", group->name);
        return true;
    } else if (result == PLATFORM_WAIT_TIMEOUT) {
        // Only log timeout warning during shutdown
        if (shutdown_signalled()) {
            logger_log(LOG_WARN, "Timeout waiting for threads in group '%s' to complete", group->name);
        }
        return false;
    } else {
        char error_msg[256];
        platform_get_error_message(error_msg, sizeof(error_msg));
        logger_log(LOG_ERROR, "Error waiting for threads in group '%s' to complete: %s", 
                   group->name, error_msg);
        return false;
    }
}

bool thread_group_is_empty(ThreadGroup* group) {
    if (!group) {
        return true;
    }
    
    bool is_empty = false;
    
    platform_mutex_lock(&group->registry.mutex);
    is_empty = (group->registry.head == NULL);
    platform_mutex_unlock(&group->registry.mutex);
    
    return is_empty;
}

uint32_t thread_group_get_active_count(ThreadGroup* group) {
    if (!group) {
        return 0;
    }
    
    uint32_t active_count = 0;
    
    platform_mutex_lock(&group->registry.mutex);
    
    ThreadRegistryEntry* entry = group->registry.head;
    while (entry != NULL) {
        if (entry->state != THREAD_STATE_TERMINATED) {
            active_count++;
        }
        entry = entry->next;
    }
    
    platform_mutex_unlock(&group->registry.mutex);
    
    return active_count;
}

void thread_group_cleanup(ThreadGroup* group) {
    if (!group) {
        return;
    }
    
    thread_registry_cleanup(&group->registry);
    
    logger_log(LOG_DEBUG, "Thread group '%s' cleaned up", group->name);
}
