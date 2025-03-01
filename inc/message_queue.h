#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <stdbool.h>
#include <stdint.h>
#include "platform_threads.h"  // For platform-independent thread definitions
#include "platform_mutex.h"   // For platform-independent event definitions

#ifdef __cplusplus
extern "C" {
#endif

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

#define MESSAGE_QUEUE_SIZE 16384   // Power of 2 for better performance

// Lock-free queue structure
typedef struct {
    Message_T* entries;              // Dynamically allocated array
    volatile int32_t head;           // Index of head (atomic access)
    volatile int32_t tail;           // Index of tail (atomic access)
    int32_t max_size;                // Maximum number of messages allowed (0 for full size)
    PlatformEvent_T not_empty_event; // Event for signaling queue not empty
    PlatformEvent_T not_full_event;  // Event for signaling queue not full
} MessageQueue_T;

// Thread queue structure
typedef struct {
    PlatformThread_T thread_handle;
    MessageQueue_T queue;
    volatile int32_t active;       // Use atomic operations for active status
} ThreadQueue_T;

// Queue management functions
bool message_queue_init(MessageQueue_T* queue, uint32_t max_size);
bool message_queue_push(MessageQueue_T* queue, const Message_T* message, uint32_t timeout_ms);
bool message_queue_pop(MessageQueue_T* queue, Message_T* message, uint32_t timeout_ms);
bool message_queue_is_empty(MessageQueue_T* queue);
bool message_queue_is_full(MessageQueue_T* queue);
uint32_t message_queue_get_size(MessageQueue_T* queue);
void message_queue_clear(MessageQueue_T* queue);
void message_queue_destroy(MessageQueue_T* queue);

// Thread management functions
ThreadQueue_T* register_thread(PlatformThread_T thread_handle);
void deregister_thread(PlatformThread_T thread_handle);
bool is_thread_active(PlatformThread_T thread_handle);

#ifdef __cplusplus
}
#endif

#endif // MESSAGE_QUEUE_H