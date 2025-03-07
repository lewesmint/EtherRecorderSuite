/**
 * @file platform_wait.h
 * @brief Platform-agnostic wait functions and constants
 */
#ifndef PLATFORM_WAIT_H
#define PLATFORM_WAIT_H

#include <stdint.h>
#include <stdbool.h>
#include "platform_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Translates platform-specific wait result to platform-agnostic wait result
 * 
 * @param result Platform-specific wait result
 * @param object_count Number of objects being waited on (for Windows)
 * @return int Platform-agnostic wait result (PLATFORM_WAIT_SUCCESS, PLATFORM_WAIT_TIMEOUT, or PLATFORM_WAIT_ERROR)
 */
int platform_translate_wait_result(int result, uint32_t object_count);

/**
 * @brief Waits for a single object with timeout
 * 
 * @param handle Object handle to wait for
 * @param timeout_ms Timeout in milliseconds (PLATFORM_WAIT_INFINITE for infinite)
 * @return int Platform-agnostic wait result (PLATFORM_WAIT_SUCCESS, PLATFORM_WAIT_TIMEOUT, or PLATFORM_WAIT_ERROR)
 */
int platform_wait_single(PlatformHandle_T handle, uint32_t timeout_ms);

/**
 * @brief Waits for multiple objects with timeout
 * 
 * @param handles Array of object handles to wait for
 * @param count Number of handles in the array
 * @param wait_all If true, wait for all objects; if false, wait for any object
 * @param timeout_ms Timeout in milliseconds (PLATFORM_WAIT_INFINITE for infinite)
 * @return int Platform-agnostic wait result (PLATFORM_WAIT_SUCCESS, PLATFORM_WAIT_TIMEOUT, or PLATFORM_WAIT_ERROR)
 */
int platform_wait_multiple(PlatformHandle_T* handles, uint32_t count, bool wait_all, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_WAIT_H
