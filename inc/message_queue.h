#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <stdbool.h>
#include <stdint.h>
#include <windows.h> // For HANDLE

typedef struct Message {
    uint64_t id;
    char content[1024];
    struct Message* next;
} Message_T;

typedef struct {
    Message_T* head;
    Message_T* tail;
} MessageQueue_T;

typedef struct {
    HANDLE thread_handle;
    MessageQueue_T queue;
    volatile LONG active; // Use atomic operations for active status
} ThreadQueue_T;

void message_queue_init(MessageQueue_T* queue);
bool message_queue_push(MessageQueue_T* queue, const Message_T* message);
bool message_queue_pop(MessageQueue_T* queue, Message_T* message);
void message_queue_destroy(MessageQueue_T* queue);

void register_thread(HANDLE thread_handle);
void deregister_thread(HANDLE thread_handle);
bool is_thread_active(HANDLE thread_handle);

#endif // MESSAGE_QUEUE_H