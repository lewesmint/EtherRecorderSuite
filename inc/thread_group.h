/**
 * @file thread_group.h
 * @brief Thread group management for related threads.
 */
#ifndef THREAD_GROUP_H
#define THREAD_GROUP_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#include "thread_registry.h"
#include "app_thread.h"
#include "platform_threads.h" // Include platform-independent thread definitions


/**
 * @brief Thread group structure for managing related threads
 */
typedef struct ThreadGroup {
    char name[64];                  // Group name
    ThreadRegistry registry;        // Thread registry for this group
} ThreadGroup;

/**
 * @brief Initialize a thread group
 * 
 * @param group Pointer to the thread group to initialize
 * @param name Name of the thread group
 * @return bool true on success, false on failure
 */
bool thread_group_init(ThreadGroup* group, const char* name);

/**
 * @brief Add a thread to a thread group
 * 
 * @param group Pointer to the thread group
 * @param thread Pointer to the thread structure
 * @return bool true on success, false on failure
 */
bool thread_group_add(ThreadGroup* group, AppThread_T* thread);

/**
 * @brief Remove a thread from a thread group
 * 
 * @param group Pointer to the thread group
 * @param thread_label Label of the thread to remove
 * @return bool true on success, false on failure
 */
bool thread_group_remove(ThreadGroup* group, const char* thread_label);

/**
 * @brief Terminate all threads in a thread group
 * 
 * @param group Pointer to the thread group
 * @param timeout_ms Maximum time to wait for termination in milliseconds
 * @return bool true on success, false on failure
 */
bool thread_group_terminate_all(ThreadGroup* group, uint32_t timeout_ms);

/**
 * @brief Wait for all threads in a thread group to complete
 * 
 * @param group Pointer to the thread group
 * @param timeout_ms Maximum time to wait in milliseconds
 * @return bool true if all threads completed, false on timeout or error
 */
bool thread_group_wait_all(ThreadGroup* group, uint32_t timeout_ms);

/**
 * @brief Check if a thread group is empty
 * 
 * @param group Pointer to the thread group
 * @return bool true if the group is empty, false otherwise
 */
bool thread_group_is_empty(ThreadGroup* group);

/**
 * @brief Get the number of active threads in a thread group
 * 
 * @param group Pointer to the thread group
 * @return uint32_t Number of active threads
 */
uint32_t thread_group_get_active_count(ThreadGroup* group);

/**
 * @brief Clean up a thread group
 * 
 * @param group Pointer to the thread group
 */
void thread_group_cleanup(ThreadGroup* group);


#endif // THREAD_GROUP_H
