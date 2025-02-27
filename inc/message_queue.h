#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <stdbool.h>
#include <stdint.h>
#include <windows.h> // For HANDLE

// Message types for identifying different kinds of messages
typedef enum MessageType {
    MSG_TYPE_COMMAND,    // Text command message
    MSG_TYPE_DATA,       // Binary data 
    MSG_TYPE_RELAY,      // Data to be relayed without modification
    MSG_TYPE_CONTROL     // Internal control message
} MessageType;

// Message header structure
typedef struct MessageHeader {
    uint64_t id;           // Unique message ID
    MessageType type;      // Type of message
    uint32_t content_size; // Size of the message content
    uint32_t flags;        // Additional flags for message handling
} MessageHeader;

// Complete message structure
typedef struct Message {
    MessageHeader header;          // Message header
    char content[1024];            // Message content (fixed size for simplicity)
    struct Message* next;          // Pointer to next message in queue
} Message_T;

// Message queue structure
typedef struct {
    Message_T* head;       // Head of the queue
    Message_T* tail;       // Tail of the queue
    CRITICAL_SECTION lock; // Lock for thread safety
    uint32_t size;         // Current number of messages in queue
    uint32_t max_size;     // Maximum number of messages before blocking
    HANDLE not_empty;      // Event for signaling queue not empty
    HANDLE not_full;       // Event for signaling queue not full
} MessageQueue_T;

// Thread queue structure
typedef struct {
    HANDLE thread_handle;
    MessageQueue_T queue;
    volatile LONG active; // Use atomic operations for active status
} ThreadQueue_T;

/**
 * @brief Initialize a message queue
 * 
 * @param queue Pointer to the queue to initialize
 * @param max_size Maximum number of messages in the queue (0 for unlimited)
 * @return true if initialization successful, false otherwise
 */
bool message_queue_init(MessageQueue_T* queue, uint32_t max_size);

/**
 * @brief Push a message to the queue
 * 
 * This function copies the message and adds it to the queue.
 * If the queue is full and has a size limit, it will block until space is available.
 * 
 * @param queue Pointer to the queue
 * @param message Message to push
 * @param timeout_ms Maximum time to wait if queue is full (INFINITE for no timeout)
 * @return true if successful, false if timeout or error
 */
bool message_queue_push(MessageQueue_T* queue, const Message_T* message, DWORD timeout_ms);

/**
 * @brief Pop a message from the queue
 * 
 * This function removes a message from the queue and copies it to the provided buffer.
 * If the queue is empty, it will block until a message is available.
 * 
 * @param queue Pointer to the queue
 * @param message Buffer to receive the message
 * @param timeout_ms Maximum time to wait if queue is empty (INFINITE for no timeout)
 * @return true if successful, false if timeout or error
 */
bool message_queue_pop(MessageQueue_T* queue, Message_T* message, DWORD timeout_ms);

/**
 * @brief Check if the queue is empty
 * 
 * @param queue Pointer to the queue
 * @return true if empty, false otherwise
 */
bool message_queue_is_empty(MessageQueue_T* queue);

/**
 * @brief Check if the queue is full
 * 
 * @param queue Pointer to the queue
 * @return true if full, false otherwise
 */
bool message_queue_is_full(MessageQueue_T* queue);

/**
 * @brief Get the current size of the queue
 * 
 * @param queue Pointer to the queue
 * @return Current number of messages in the queue
 */
uint32_t message_queue_get_size(MessageQueue_T* queue);

/**
 * @brief Clear all messages from the queue
 * 
 * @param queue Pointer to the queue
 */
void message_queue_clear(MessageQueue_T* queue);

/**
 * @brief Destroy the queue and free all resources
 * 
 * @param queue Pointer to the queue
 */
void message_queue_destroy(MessageQueue_T* queue);

/**
 * @brief Register a thread with a message queue
 * 
 * @param thread_handle Thread handle
 * @return Pointer to the thread queue, or NULL if registration failed
 */
ThreadQueue_T* register_thread(HANDLE thread_handle);

/**
 * @brief Deregister a thread
 * 
 * @param thread_handle Thread handle
 */
void deregister_thread(HANDLE thread_handle);

/**
 * @brief Check if a thread is active
 * 
 * @param thread_handle Thread handle
 * @return true if active, false otherwise
 */
bool is_thread_active(HANDLE thread_handle);

#endif // MESSAGE_QUEUE_H