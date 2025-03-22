/**
 * @file platform_mutex.h
 * @brief Platform-agnostic synchronization primitives (mutex, condition variable, event)
 */
#ifndef PLATFORM_MUTEX_H
#define PLATFORM_MUTEX_H

#include <stdint.h>
#include <stdbool.h>

#include "platform_error.h"

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
 * @brief Opaque mutex type
 */
typedef struct {
    unsigned char opaque[PLATFORM_MUTEX_STORAGE_SIZE];
} PlatformMutex_T;

/**
 * @brief Opaque condition variable type
 */
typedef struct {
    unsigned char opaque[PLATFORM_COND_STORAGE_SIZE];
} PlatformCondition_T;

/**
 * @brief Opaque event type
 */
typedef struct platform_event* PlatformEvent_T;

/* Mutex functions */

/**
 * @brief Initialize a mutex
 * @param mutex Pointer to mutex structure
 * @return PlatformErrorCode on success, error code on failure
 */
PlatformErrorCode platform_mutex_init(PlatformMutex_T* mutex);

/**
 * @brief Lock a mutex
 * @param mutex Pointer to mutex structure
 * @return PlatformErrorCode on success, error code on failure
 */
PlatformErrorCode platform_mutex_lock(PlatformMutex_T* mutex);

/**
 * @brief Unlock a mutex
 * @param mutex Pointer to mutex structure
 * @return PlatformErrorCode on success, error code on failure
 */
PlatformErrorCode platform_mutex_unlock(PlatformMutex_T* mutex);

/**
 * @brief Destroy a mutex
 * @param mutex Pointer to mutex structure
 * @return PlatformErrorCode on success, error code on failure
 */
PlatformErrorCode platform_mutex_destroy(PlatformMutex_T* mutex);

/* Condition variable functions */

/**
 * @brief Initialize a condition variable
 * @param cond Pointer to condition variable structure
 * @return PlatformErrorCode on success, error code on failure
 */
PlatformErrorCode platform_cond_init(PlatformCondition_T* cond);

/**
 * @brief Wait on a condition variable
 * @param cond Pointer to condition variable
 * @param mutex Associated mutex
 * @return PlatformErrorCode on success, error code on failure
 */
PlatformErrorCode platform_cond_wait(PlatformCondition_T* cond, PlatformMutex_T* mutex);

/**
 * @brief Wait on a condition variable with timeout
 * @param cond Pointer to condition variable
 * @param mutex Associated mutex
 * @param timeout_ms Timeout in milliseconds (PLATFORM_WAIT_INFINITE for no timeout)
 * @return PlatformErrorCode on success, PLATFORM_ERROR_TIMEOUT on timeout, error code on failure
 */
PlatformErrorCode platform_cond_timedwait(PlatformCondition_T* cond, 
                                     PlatformMutex_T* mutex,
                                     uint32_t timeout_ms);

/**
 * @brief Signal a condition variable
 * @param cond Pointer to condition variable
 * @return PlatformErrorCode on success, error code on failure
 */
PlatformErrorCode platform_cond_signal(PlatformCondition_T* cond);

/**
 * @brief Broadcast to all waiters on a condition variable
 * @param cond Pointer to condition variable
 * @return PlatformErrorCode on success, error code on failure
 */
PlatformErrorCode platform_cond_broadcast(PlatformCondition_T* cond);

/**
 * @brief Destroy a condition variable
 * @param cond Pointer to condition variable
 * @return PlatformErrorCode on success, error code on failure
 */
PlatformErrorCode platform_cond_destroy(PlatformCondition_T* cond);

/* Event functions */

/**
 * @brief Create an event object
 * @param event Pointer to event handle
 * @param manual_reset If true, event stays signaled until reset
 * @param initial_state Initial signaled state
 * @return PlatformErrorCode on success, error code on failure
 */
PlatformErrorCode platform_event_create(PlatformEvent_T* event,
                                   bool manual_reset,
                                   bool initial_state);

/**
 * @brief Destroy an event object
 * @param event Event handle
 * @return PlatformErrorCode on success, error code on failure
 */
PlatformErrorCode platform_event_destroy(PlatformEvent_T event);

/**
 * @brief Signal an event
 * @param event Event handle
 * @return PlatformErrorCode on success, error code on failure
 */
PlatformErrorCode platform_event_set(PlatformEvent_T event);

/**
 * @brief Reset an event to non-signaled state
 * @param event Event handle
 * @return PlatformErrorCode on success, error code on failure
 */
PlatformErrorCode platform_event_reset(PlatformEvent_T event);

/**
 * @brief Wait for an event to be signaled
 * @param event Event handle
 * @param timeout_ms Maximum time to wait in milliseconds
 * @return PlatformErrorCode on success, PLATFORM_ERROR_TIMEOUT on timeout, error code on failure
 */
PlatformErrorCode platform_event_wait(PlatformEvent_T event, uint32_t timeout_ms);

/**
 * @brief Initialises a mutex.
 * @param mutex Pointer to the mutex structure.
 */
int init_mutex(PlatformMutex_T *mutex);

/**
 * @brief Locks a mutex.
 * @param mutex Pointer to the mutex structure.
 */
int lock_mutex(PlatformMutex_T *mutex);

/**
 * @brief Unlocks a mutex.
 * @param mutex Pointer to the mutex structure.
 */
int unlock_mutex(PlatformMutex_T *mutex);


#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_MUTEX_H */
