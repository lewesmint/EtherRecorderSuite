#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include <windows.h>

#define MESSAGE_QUEUE_SIZE 1024
#define MAX_MESSAGE_DATA_SIZE 1024

// Message structure
typedef struct {
    uint32_t type;
    uint32_t size;
    uint8_t data[MAX_MESSAGE_DATA_SIZE];
    // Add other message fields as needed
} Message_T;

// Lock-free queue structure
typedef struct {
    Message_T entries[MESSAGE_QUEUE_SIZE];
    volatile LONG head;
    volatile LONG tail;
    HANDLE not_empty_event;
    HANDLE not_full_event;
    uint32_t max_size;
} MessageQueue_T;

// Thread queue structure
typedef struct {
    HANDLE thread_handle;
    volatile LONG active;
    MessageQueue_T queue;
} ThreadQueue_T;

// Queue management functions
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