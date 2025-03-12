/**
 * @file platform_sync.h
 * @brief Platform-agnostic wait functions and constants
 */
#ifndef PLATFORM_SYNC_H
#define PLATFORM_SYNC_H

#include <stdint.h>
#include <stdbool.h>
#include "platform_threads.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Platform wait result codes
 */
typedef enum PlatformWaitResult {
    PLATFORM_WAIT_SUCCESS = 0,     ///< Wait operation completed successfully
    PLATFORM_WAIT_TIMEOUT = 1,     ///< Wait operation timed out
    PLATFORM_WAIT_ERROR = -1       ///< Wait operation failed
} PlatformWaitResult;


typedef enum {
    PLATFORM_SIGNAL_INT,    // SIGINT, Ctrl+C
    PLATFORM_SIGNAL_TERM    // SIGTERM
} PlatformSignalType;

typedef void (*PlatformSignalHandler)(void);

/**
 * @brief Opaque event type
 */
typedef struct platform_event* PlatformEvent_T;

/**
 * @brief Create a new event object
 * 
 * @param event Pointer to event handle that will receive the created event
 * @param manual_reset If true, the event stays signaled until reset
 * @param initial_state Initial state of the event (signaled/not signaled)
 * @return PlatformErrorCode PLATFORM_ERROR_SUCCESS on success, error code on failure
 */
PlatformErrorCode platform_event_create(PlatformEvent_T* event, bool manual_reset, bool initial_state);

/**
 * @brief Destroy an event object
 * 
 * @param event Event handle to destroy
 * @return PlatformErrorCode PLATFORM_ERROR_SUCCESS on success, error code on failure
 */
PlatformErrorCode platform_event_destroy(PlatformEvent_T event);

/**
 * @brief Signal (set) an event
 * 
 * @param event Event handle to signal
 * @return PlatformErrorCode PLATFORM_ERROR_SUCCESS on success, error code on failure
 */
PlatformErrorCode platform_event_set(PlatformEvent_T event);

/**
 * @brief Reset an event to non-signaled state
 * 
 * @param event Event handle to reset
 * @return PlatformErrorCode PLATFORM_ERROR_SUCCESS on success, error code on failure
 */
PlatformErrorCode platform_event_reset(PlatformEvent_T event);

/**
 * @brief Wait for an event to be signaled
 * 
 * @param event Event handle to wait on
 * @param timeout_ms Maximum time to wait in milliseconds
 * @return PlatformErrorCode PLATFORM_ERROR_SUCCESS on success, 
 *                          PLATFORM_ERROR_TIMEOUT on timeout,
 *                          error code on failure
 */
PlatformErrorCode platform_event_wait(PlatformEvent_T event, uint32_t timeout_ms);

/**
 * @brief Register a handler for a specific signal type
 * 
 * @param signal_type The type of signal to handle
 * @param handler The callback function to invoke
 * @return bool true on success, false on failure
 */
bool platform_signal_register_handler(PlatformSignalType signal_type, 
                                    PlatformSignalHandler handler);

/**
 * @brief Unregister a handler for a specific signal type
 * 
 * @param signal_type The type of signal to unregister
 * @return bool true on success, false on failure
 */
bool platform_signal_unregister_handler(PlatformSignalType signal_type);

/**
 * @brief Special timeout value for infinite wait
 */
#define PLATFORM_WAIT_INFINITE ((uint32_t)-1)

/**
 * @brief Waits for a single object with timeout
 * 
 * @param handle Object handle to wait for
 * @param timeout_ms Timeout in milliseconds (PLATFORM_WAIT_INFINITE for infinite)
 * @return PlatformWaitResult indicating success, timeout, or error
 */
PlatformWaitResult platform_wait_single(PlatformThreadHandle handle, uint32_t timeout_ms);

/**
 * @brief Waits for multiple objects with timeout
 * 
 * @param handles Array of object handles to wait for
 * @param count Number of handles in the array
 * @param wait_all If true, wait for all objects; if false, wait for any object
 * @param timeout_ms Timeout in milliseconds (PLATFORM_WAIT_INFINITE for infinite)
 * @return PlatformWaitResult indicating success, timeout, or error
 */
PlatformWaitResult platform_wait_multiple(PlatformThreadHandle *threads, uint32_t count, bool wait_all, uint32_t timeout_ms);
 

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_SYNC_H
