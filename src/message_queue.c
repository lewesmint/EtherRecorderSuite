#include "message_queue.h"
#include <stdlib.h>
#include <string.h>
#include <windows.h> // For Interlocked operations and thread functions

#define MAX_THREADS 5

static ThreadQueue_T thread_queues[MAX_THREADS];

void message_queue_init(MessageQueue_T* queue) {
    queue->head = NULL;
    queue->tail = NULL;
}

bool message_queue_push(MessageQueue_T* queue, const Message_T* message) {
    if (!message) {
        return false;
    }

    Message_T* new_message = (Message_T*)malloc(sizeof(Message_T));
    if (!new_message) {
        return false;
    }

    memcpy(new_message, message, sizeof(Message_T));
    new_message->next = NULL;

    Message_T* old_tail;
    do {
        old_tail = queue->tail;
        if (old_tail) {
            old_tail->next = new_message;
        } else {
            queue->head = new_message;
        }
    } while (InterlockedCompareExchangePointer((void**)&queue->tail, new_message, old_tail) != old_tail);

    return true;
}

bool message_queue_pop(MessageQueue_T* queue, Message_T* message) {
    if (!message) {
        return false;
    }

    Message_T* old_head;
    do {
        old_head = queue->head;
        if (!old_head) {
            return false; // Queue is empty
        }
    } while (InterlockedCompareExchangePointer((void**)&queue->head, old_head->next, old_head) != old_head);

    memcpy(message, old_head, sizeof(Message_T));
    if (!queue->head) {
        queue->tail = NULL;
    }

    free(old_head);
    return true;
}

void message_queue_destroy(MessageQueue_T* queue) {
    Message_T* current = queue->head;
    while (current) {
        Message_T* temp = current;
        current = current->next;
        free(temp);
    }
    queue->head = NULL;
    queue->tail = NULL;
}

void register_thread(HANDLE thread_handle) {
    for (int i = 0; i < MAX_THREADS; i++) {
        if (InterlockedCompareExchange(&thread_queues[i].active, 1, 0) == 0) {
            thread_queues[i].thread_handle = thread_handle;
            message_queue_init(&thread_queues[i].queue);
            break;
        }
    }
}

void deregister_thread(HANDLE thread_handle) {
    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread_queues[i].active && thread_queues[i].thread_handle == thread_handle) {
            message_queue_destroy(&thread_queues[i].queue);
            InterlockedExchange(&thread_queues[i].active, 0);
            break;
        }
    }
}

bool is_thread_active(HANDLE thread_handle) {
    DWORD exit_code;
    if (GetExitCodeThread(thread_handle, &exit_code)) {
        return exit_code == STILL_ACTIVE;
    }
    return false;
}