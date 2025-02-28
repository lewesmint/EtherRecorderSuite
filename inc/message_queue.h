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
    struct Message* next;          // Maintained for compatibility but not used in array implementation
} Message_T;

#define MESSAGE_QUEUE_SIZE 1024    // Power of 2 for better performance

// Lock-free queue structure
typedef struct {
    Message_T entries[MESSAGE_QUEUE_SIZE]; // Pre-allocated array of messages
    volatile LONG head;            // Index of head (atomic access)
    volatile LONG tail;            // Index of tail (atomic access)
    uint32_t max_size;             // Maximum number of messages allowed (0 for full size)
    HANDLE not_empty_event;        // Event for signaling queue not empty
    HANDLE not_full_event;         // Event for signaling queue not full
} MessageQueue_T;

// Thread queue structure
typedef struct {
    HANDLE thread_handle;
    MessageQueue_T queue;
    volatile LONG active; // Use atomic operations for active status
} ThreadQueue_T;

// Queue management functions (same signatures as original)
bool message_queue_init(MessageQueue_T* queue, uint32_t max_size);
bool message_queue_push(MessageQueue_T* queue, const Message_T* message, DWORD timeout_ms);
bool message_queue_pop(MessageQueue_T* queue, Message_T* message, DWORD timeout_ms);
bool message_queue_is_empty(MessageQueue_T* queue);
bool message_queue_is_full(MessageQueue_T* queue);
uint32_t message_queue_get_size(MessageQueue_T* queue);
void message_queue_clear(MessageQueue_T* queue);
void message_queue_destroy(MessageQueue_T* queue);

// Thread management functions
ThreadQueue_T* register_thread(HANDLE thread_handle);
void deregister_thread(HANDLE thread_handle);
bool is_thread_active(HANDLE thread_handle);

#endif // MESSAGE_QUEUE_H