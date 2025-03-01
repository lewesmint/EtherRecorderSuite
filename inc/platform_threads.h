/**
* @file platform_threads.h
* @brief Platform - specific thread functions.
*
*This file provides a platform - independent interface for creating and managing threads.
* The implementation of these functions is platform - specific and should be implemented in the corresponding platform_threads.c file.
*
*/
#ifndef PLATFORM_THREADS_H
#define PLATFORM_THREADS_H

#include <stdbool.h>
#include "platform_defs.h"

#ifdef _WIN32
#include <windows.h>
typedef HANDLE PlatformThread_T;
#define THREAD_LOCAL __declspec(thread)
#else
#include <pthread.h>
typedef pthread_t PlatformThread_T;
#define THREAD_LOCAL __thread
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Template for thread functions.
 * @param arg The argument passed to the thread function.
 * @return The return value of the thread function.
 */
typedef void* (*ThreadFunc_T)(void* arg);

/**
 * @brief Creates a new thread.
 *
 * @param thread Pointer to store the created thread handle.
 * @param func Function to be executed in the new thread.
 * @param arg Argument to pass to the thread function.
 * @return 0 on success, non-zero on failure.
 */
int platform_thread_create(PlatformThread_T *thread, ThreadFunc_T func, void *arg);

/**
 * @brief Waits for a thread to complete.
 *
 * @param thread The thread handle to wait for.
 * @param retval Pointer to store the return value of the thread.
 * @return 0 on success, non-zero on failure.
 */
int platform_thread_join(PlatformThread_T thread, void **retval);

/**
 * @brief Closes a thread handle.
 *
 * @param thread Pointer to the thread handle to close.
 * @return 0 on success, non-zero on failure.
 */
int platform_thread_close(PlatformThread_T *thread);

/**
 * @brief Waits for a single thread with timeout.
 *
 * @param thread Thread handle to wait for.
 * @param timeout_ms Timeout in milliseconds (PLATFORM_WAIT_INFINITE for no timeout).
 * @return PLATFORM_WAIT_SUCCESS, PLATFORM_WAIT_TIMEOUT, or PLATFORM_WAIT_ERROR.
 */
int platform_thread_wait_single(PlatformThread_T thread, uint32_t timeout_ms);

/**
 * @brief Compare two thread handles for equality
 *
 * @param thread1 First thread handle
 * @param thread2 Second thread handle
 * @return bool true if handles refer to the same thread
 */
bool platform_thread_equal(PlatformThread_T thread1, PlatformThread_T thread2);

/**
 * @brief Check if a thread is still active
 *
 * @param thread Thread handle to check
 * @return bool true if thread is active, false otherwise
 */
bool platform_thread_is_active(PlatformThread_T thread);

/**
 * @brief Wait for multiple threads to complete
 *
 * @param threads Array of thread handles
 * @param count Number of threads in the array
 * @param wait_all If true, wait for all threads; if false, wait for any thread
 * @param timeout_ms Maximum time to wait in milliseconds
 * @return int PLATFORM_WAIT_SUCCESS, PLATFORM_WAIT_TIMEOUT, or PLATFORM_WAIT_ERROR
 */
int platform_thread_wait_multiple(PlatformThread_T* threads, uint32_t count, bool wait_all, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // PLATFORM_THREADS_H
