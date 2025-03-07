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

#include <stdbool.h>
#include <stdint.h>
#include "platform_threads.h"  // For platform-independent thread definitions
#include "platform_mutex.h"    // For platform-independent event definitions

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Message types for identifying different kinds of messages
 */
typedef enum MessageType {
    MSG_TYPE_COMMAND,    ///< Text command message
    MSG_TYPE_DATA,       ///< Binary data 
    MSG_TYPE_RELAY,      ///< Data to be relayed without modification
    MSG_TYPE_CONTROL     ///< Internal control message
} MessageType;

/**
 * @brief Message header structure containing metadata
 */
typedef struct MessageHeader {
    uint64_t id;           ///< Unique message ID
    MessageType type;      ///< Type of message
    uint32_t content_size; ///< Size of the message content
    uint32_t flags;        ///< Additional flags for message handling
} MessageHeader;

/**
 * @brief Complete message structure including content
 */
typedef struct Message {
    MessageHeader header;          ///< Message header
    char content[1024];           ///< Message content (fixed size for simplicity)
    struct Message* next;         ///< Maintained for compatibility but not used in array implementation
} Message_T;

/** Size of the message queue - power of 2 for better performance */
#define MESSAGE_QUEUE_SIZE 16384

/**
 * @brief Lock-free queue structure
 */
typedef struct {
    Message_T* entries;              ///< Dynamically allocated array
    volatile int32_t head;           ///< Index of head (atomic access)
    volatile int32_t tail;           ///< Index of tail (atomic access)
    int32_t max_size;                ///< Maximum number of messages allowed (0 for full size)
    PlatformEvent_T not_empty_event; ///< Event for signaling queue not empty
    PlatformEvent_T not_full_event;  ///< Event for signaling queue not full
} MessageQueue_T;

/**
 * @brief Thread queue structure for per-thread message queues
 */
typedef struct {
    PlatformThread_T thread_handle; ///< Thread identifier
    MessageQueue_T queue;           ///< Message queue for this thread
    volatile int32_t active;        ///< Thread active status (atomic access)
} ThreadQueue_T;

/**
 * @brief Initialize a message queue
 * @param queue Pointer to queue to initialize
 * @param max_size Maximum size (0 for MESSAGE_QUEUE_SIZE)
 * @return true if initialization successful
 */
bool message_queue_init(MessageQueue_T* queue, uint32_t max_size);

/**
 * @brief Push a message to the queue
 * @param queue Target queue
 * @param message Message to push
 * @param timeout_ms Maximum time to wait if queue full (0 for non-blocking)
 * @return true if message pushed successfully
 */
bool message_queue_push(MessageQueue_T* queue, const Message_T* message, uint32_t timeout_ms);

/**
 * @brief Pop a message from the queue
 * @param queue Source queue
 * @param message Buffer to receive message
 * @param timeout_ms Maximum time to wait if queue empty (0 for non-blocking)
 * @return true if message retrieved successfully
 */
bool message_queue_pop(MessageQueue_T* queue, Message_T* message, uint32_t timeout_ms);

/**
 * @brief Check if queue is empty
 * @param queue Queue to check
 * @return true if queue is empty
 */
bool message_queue_is_empty(MessageQueue_T* queue);

/**
 * @brief Check if queue is full
 * @param queue Queue to check
 * @return true if queue is full
 */
bool message_queue_is_full(MessageQueue_T* queue);

/**
 * @brief Get current number of messages in queue
 * @param queue Queue to check
 * @return Number of messages in queue
 */
uint32_t message_queue_get_size(MessageQueue_T* queue);

/**
 * @brief Clear all messages from queue
 * @param queue Queue to clear
 */
void message_queue_clear(MessageQueue_T* queue);

/**
 * @brief Destroy queue and free resources
 * @param queue Queue to destroy
 */
void message_queue_destroy(MessageQueue_T* queue);

/**
 * @brief Register a thread for message queueing
 * @param thread_handle Thread to register
 * @return Pointer to new ThreadQueue or NULL if failed
 */
ThreadQueue_T* register_thread(PlatformThread_T thread_handle);

/**
 * @brief Deregister a thread from message queueing
 * @param thread_handle Thread to deregister
 */
void deregister_thread(PlatformThread_T thread_handle);

/**
 * @brief Check if thread is active
 * @param thread_handle Thread to check
 * @return true if thread is active
 */
bool is_thread_active(PlatformThread_T thread_handle);

/**
 * @brief Get the thread queue for a given thread ID
 * @param thread_id Thread identifier to look up
 * @return Pointer to ThreadQueue_T or NULL if not found
 */
ThreadQueue_T* get_thread_queue(PlatformThread_T thread_id);

#ifdef __cplusplus
}
#endif

#endif // MESSAGE_QUEUE_H
