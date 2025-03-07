/**
 * @file platform_threads.h
 * @brief Platform-agnostic thread management interface
 */
#ifndef PLATFORM_THREADS_H
#define PLATFORM_THREADS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "platform_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Thread priority levels
 */
typedef enum {
    PLATFORM_THREAD_PRIORITY_LOWEST = -2,
    PLATFORM_THREAD_PRIORITY_LOW = -1,
    PLATFORM_THREAD_PRIORITY_NORMAL = 0,
    PLATFORM_THREAD_PRIORITY_HIGH = 1,
    PLATFORM_THREAD_PRIORITY_HIGHEST = 2,
    PLATFORM_THREAD_PRIORITY_REALTIME = 3
} PlatformThreadPriority;

/**
 * @brief Thread attributes structure
 */
typedef struct {
    PlatformThreadPriority priority;  ///< Thread priority
    size_t stack_size;                ///< Stack size in bytes (0 for default)
    bool detached;                    ///< True if thread should be detached
} PlatformThreadAttributes;

/**
 * @brief Thread function prototype
 */
typedef void* (*PlatformThreadFunction)(void* arg);

/**
 * @brief Opaque thread handle type
 */
typedef void* PlatformThreadHandle;

/**
 * @brief Opaque thread ID type
 */
typedef uintptr_t PlatformThreadId;

/**
 * @brief Initialize the threading subsystem
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_thread_init(void);

/**
 * @brief Cleanup the threading subsystem
 */
void platform_thread_cleanup(void);

/**
 * @brief Create a new thread
 * @param[out] handle Thread handle
 * @param[in] attributes Thread attributes (NULL for defaults)
 * @param[in] function Thread function to execute
 * @param[in] arg Argument to pass to thread function
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_thread_create(
    PlatformThreadHandle* handle,
    const PlatformThreadAttributes* attributes,
    PlatformThreadFunction function,
    void* arg);

/**
 * @brief Join with a thread (wait for it to complete)
 * @param[in] handle Thread handle
 * @param[out] result Thread return value
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_thread_join(PlatformThreadHandle handle, void** result);

/**
 * @brief Detach a thread
 * @param[in] handle Thread handle
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_thread_detach(PlatformThreadHandle handle);

/**
 * @brief Get current thread ID
 * @return Current thread ID
 */
PlatformThreadId platform_thread_get_id(void);

/**
 * @brief Set thread priority
 * @param[in] handle Thread handle
 * @param[in] priority New priority level
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_thread_set_priority(
    PlatformThreadHandle handle,
    PlatformThreadPriority priority);

/**
 * @brief Get thread priority
 * @param[in] handle Thread handle
 * @param[out] priority Current priority level
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_thread_get_priority(
    PlatformThreadHandle handle,
    PlatformThreadPriority* priority);

/**
 * @brief Yield execution to another thread
 */
void platform_thread_yield(void);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_THREADS_H
