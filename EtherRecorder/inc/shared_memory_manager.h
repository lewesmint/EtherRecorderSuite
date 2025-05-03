/**
 * @file shared_memory_manager.h
 * @brief Manager for multiple shared memory monitors
 */
#ifndef SHARED_MEMORY_MANAGER_H
#define SHARED_MEMORY_MANAGER_H

#include "shared_memory_monitor.h"
#include "thread_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the shared memory manager
 * 
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode shared_memory_manager_init(void);

/**
 * @brief Shutdown the shared memory manager
 */
void shared_memory_manager_shutdown(void);

/**
 * @brief Get the shared memory manager thread configuration
 * 
 * @return Pointer to thread configuration
 */
struct ThreadConfig* get_shared_memory_manager_thread(void);

#ifdef __cplusplus
}
#endif

#endif // SHARED_MEMORY_MANAGER_H