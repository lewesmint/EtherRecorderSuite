/**
 * @file message_queue.h
 * @brief Lock-free message queue implementation for inter-thread communication
 *
 * Provides a thread-safe, lock-free queue implementation using atomic operations
 * for message passing between threads. Supports blocking and non-blocking operations
 * with configurable timeouts.
 */
#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include "platform_defs.h"
#include "message_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    PlatformThread_T thread_handle;  ///< Thread identifier
    MessageQueue_T queue;            ///< Message queue for this thread
    volatile int32_t active;         ///< Thread active status (atomic access)
} ThreadQueue_T;

#ifdef __cplusplus
}
#endif


#endif // MESSAGE_QUEUE_H
