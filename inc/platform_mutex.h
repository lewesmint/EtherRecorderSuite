/** 
* @file platform_mutex.h
* @brief Platform-specific mutex, condition variable, and event functions.
*/
#ifndef PLATFORM_MUTEX_H
#define PLATFORM_MUTEX_H

#include <stdint.h>
#include <stdbool.h>
#include "platform_defs.h"

#ifdef _WIN32
    #include <windows.h>
    typedef HANDLE PlatformEvent_T;
#else // !_WIN32
    #include <pthread.h>
    typedef struct {
        pthread_mutex_t mutex;
        pthread_cond_t cond;
        bool signaled;
        bool manual_reset;
    } PlatformEvent_T;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
* Define fixed storage sizes for the opaque types.
* These sizes should be large enough to hold the underlying platform-specific
* synchronisation objects. For Windows and most POSIX systems (including macOS)
* 64 bytes is often sufficient.
*/
#ifndef PLATFORM_MUTEX_STORAGE_SIZE
#define PLATFORM_MUTEX_STORAGE_SIZE 64
#endif

#ifndef PLATFORM_COND_STORAGE_SIZE
#define PLATFORM_COND_STORAGE_SIZE 64
#endif

/**
* @brief Opaque type for a mutex.
*
* Memory is reserved automatically for the underlying platform-specific data.
*/
typedef struct PlatformMutex {
    unsigned char opaque[PLATFORM_MUTEX_STORAGE_SIZE];
} PlatformMutex_T;

/**
* @brief Opaque type for a condition variable.
*
* Memory is reserved automatically for the underlying platform-specific data.
*/
typedef struct PlatformCondition {
    unsigned char opaque[PLATFORM_COND_STORAGE_SIZE];
} PlatformCondition_T;

/**
 * @brief Initialize a platform mutex
 * 
 * @param mutex Pointer to the mutex
 * @return int 0 on success, -1 on failure.
 */
int platform_mutex_init(PlatformMutex_T *mutex);

/**
 * @brief Lock a platform mutex
 * 
 * @param mutex Pointer to the mutex
 * @return int 0 on success, -1 on failure.
 */
int platform_mutex_lock(PlatformMutex_T *mutex);

/**
 * @brief Unlock a platform mutex
 * 
 * @param mutex Pointer to the mutex
 * @return int 0 on success, -1 on failure.
 */
int platform_mutex_unlock(PlatformMutex_T *mutex);

/**
 * @brief Destroy a platform mutex
 * 
 * @param mutex Pointer to the mutex
 */
int platform_mutex_destroy(PlatformMutex_T *mutex);

/**
 * @brief Initialises the given condition variable.
*
* @param cond Pointer to a PlatformCondition_T variable.
* @return int 0 on success, -1 on failure.
*/
int platform_cond_init(PlatformCondition_T *cond);

/**
* @brief Waits on the condition variable using the given mutex.
*
* @param cond Pointer to the condition variable.
* @param mutex Pointer to the mutex.
* @return int 0 on success, -1 on failure.
*/
int platform_cond_wait(PlatformCondition_T *cond, PlatformMutex_T *mutex);

/**
* @brief Signals the given condition variable.
*
* @param cond Pointer to the condition variable.
* @return int 0 on success, -1 on failure.
*/
int platform_cond_signal(PlatformCondition_T *cond);

/**
* @brief Destroys the given condition variable.
*
* @param cond Pointer to the condition variable.
* @return int 0 on success, -1 on failure.
*/
int platform_cond_destroy(PlatformCondition_T *cond);

/**
 * @brief Create a platform event object
 * 
 * @param event Pointer to the event object
 * @param manual_reset If true, the event stays signaled until reset
 * @param initial_state Initial state of the event (signaled/not signaled)
 * @return bool true on success, false on failure
 */
bool platform_event_create(PlatformEvent_T *event, bool manual_reset, bool initial_state);

/**
 * @brief Destroy a platform event object
 * 
 * @param event Pointer to the event object
 */
void platform_event_destroy(PlatformEvent_T *event);

/**
 * @brief Signal (set) an event
 * 
 * @param event Pointer to the event object
 * @return bool true on success, false on failure
 */
bool platform_event_set(PlatformEvent_T *event);

/**
 * @brief Reset an event to non-signaled state
 * 
 * @param event Pointer to the event object
 * @return bool true on success, false on failure
 */
bool platform_event_reset(PlatformEvent_T *event);

/**
 * @brief Wait for an event to be signaled
 * 
 * @param event Pointer to the event object
 * @param timeout_ms Maximum time to wait in milliseconds
 * @return int PLATFORM_WAIT_SUCCESS on success, 
 *             PLATFORM_WAIT_TIMEOUT on timeout,
 *             PLATFORM_WAIT_ERROR on error
 */
int platform_event_wait(PlatformEvent_T *event, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_MUTEX_H */