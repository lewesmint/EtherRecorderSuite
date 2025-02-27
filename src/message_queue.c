#include "message_queue.h"
#include <stdlib.h>
#include <string.h>
#include <windows.h> // For Interlocked operations and thread functions
#include "logger.h"

#define MAX_THREADS 5

static ThreadQueue_T thread_queues[MAX_THREADS];

bool message_queue_init(MessageQueue_T* queue, uint32_t max_size) {
    if (!queue) {
        return false;
    }
    
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    queue->max_size = max_size;
    
    InitializeCriticalSection(&queue->lock);
    
    // Create events for synchronization
    queue->not_empty = CreateEvent(NULL, TRUE, FALSE, NULL);
    queue->not_full = CreateEvent(NULL, TRUE, TRUE, NULL);
    
    if (!queue->not_empty || !queue->not_full) {
        logger_log(LOG_ERROR, "Failed to create queue synchronization events");
        DeleteCriticalSection(&queue->lock);
        if (queue->not_empty) CloseHandle(queue->not_empty);
        if (queue->not_full) CloseHandle(queue->not_full);
        return false;
    }
    
    return true;
}

bool message_queue_push(MessageQueue_T* queue, const Message_T* message, DWORD timeout_ms) {
    if (!queue || !message) {
        return false;
    }
    
    // Wait for space in the queue if it's limited
    if (queue->max_size > 0) {
        EnterCriticalSection(&queue->lock);
        while (queue->size >= queue->max_size) {
            LeaveCriticalSection(&queue->lock);
            
            DWORD wait_result = WaitForSingleObject(queue->not_full, timeout_ms);
            if (wait_result != WAIT_OBJECT_0) {
                logger_log(LOG_WARN, "Timeout or error waiting for space in message queue");
                return false;
            }
            
            EnterCriticalSection(&queue->lock);
            // Check again after getting the lock
            if (queue->size < queue->max_size) {
                break;
            }
        }
        LeaveCriticalSection(&queue->lock);
    }
    
    // Create a new message
    Message_T* new_message = (Message_T*)malloc(sizeof(Message_T));
    if (!new_message) {
        logger_log(LOG_ERROR, "Failed to allocate memory for message");
        return false;
    }
    
    // Copy the message data
    memcpy(new_message, message, sizeof(Message_T));
    new_message->next = NULL;
    
    // Add to the queue with proper locking
    EnterCriticalSection(&queue->lock);
    
    if (queue->tail) {
        queue->tail->next = new_message;
        queue->tail = new_message;
    } else {
        // Empty queue
        queue->head = new_message;
        queue->tail = new_message;
    }
    
    queue->size++;
    
    // Signal that the queue is not empty
    SetEvent(queue->not_empty);
    
    // If the queue is full now, reset the not_full event
    if (queue->max_size > 0 && queue->size >= queue->max_size) {
        ResetEvent(queue->not_full);
    }
    
    LeaveCriticalSection(&queue->lock);
    return true;
}

bool message_queue_pop(MessageQueue_T* queue, Message_T* message, DWORD timeout_ms) {
    if (!queue || !message) {
        return false;
    }
    
    // Wait for a message in the queue
    EnterCriticalSection(&queue->lock);
    while (queue->head == NULL) {
        // Reset the not_empty event since we're about to wait on it
        ResetEvent(queue->not_empty);
        
        LeaveCriticalSection(&queue->lock);
        
        DWORD wait_result = WaitForSingleObject(queue->not_empty, timeout_ms);
        if (wait_result != WAIT_OBJECT_0) {
            logger_log(LOG_WARN, "Timeout or error waiting for message in queue");
            return false;
        }
        
        EnterCriticalSection(&queue->lock);
        // Check again after getting the lock
        if (queue->head != NULL) {
            break;
        }
    }
    
    // Remove the message from the queue
    Message_T* head_message = queue->head;
    queue->head = head_message->next;
    
    if (queue->head == NULL) {
        // Queue is now empty
        queue->tail = NULL;
        ResetEvent(queue->not_empty);
    }
    
    queue->size--;
    
    // Signal that the queue is not full
    if (queue->max_size > 0 && queue->size < queue->max_size) {
        SetEvent(queue->not_full);
    }
    
    LeaveCriticalSection(&queue->lock);
    
    // Copy the message data and free the queue node
    memcpy(message, head_message, sizeof(Message_T));
    free(head_message);
    
    return true;
}

bool message_queue_is_empty(MessageQueue_T* queue) {
    if (!queue) {
        return true;
    }
    
    EnterCriticalSection(&queue->lock);
    bool is_empty = (queue->head == NULL);
    LeaveCriticalSection(&queue->lock);
    
    return is_empty;
}

bool message_queue_is_full(MessageQueue_T* queue) {
    if (!queue || queue->max_size == 0) {
        return false;
    }
    
    EnterCriticalSection(&queue->lock);
    bool is_full = (queue->size >= queue->max_size);
    LeaveCriticalSection(&queue->lock);
    
    return is_full;
}

uint32_t message_queue_get_size(MessageQueue_T* queue) {
    if (!queue) {
        return 0;
    }
    
    EnterCriticalSection(&queue->lock);
    uint32_t size = queue->size;
    LeaveCriticalSection(&queue->lock);
    
    return size;
}

void message_queue_clear(MessageQueue_T* queue) {
    if (!queue) {
        return;
    }
    
    EnterCriticalSection(&queue->lock);
    
    Message_T* current = queue->head;
    while (current) {
        Message_T* next = current->next;
        free(current);
        current = next;
    }
    
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    
    // Reset synchronization events
    ResetEvent(queue->not_empty);
    SetEvent(queue->not_full);
    
    LeaveCriticalSection(&queue->lock);
}

void message_queue_destroy(MessageQueue_T* queue) {
    if (!queue) {
        return;
    }
    
    // Clear the queue first
    message_queue_clear(queue);
    
    // Clean up synchronization objects
    CloseHandle(queue->not_empty);
    CloseHandle(queue->not_full);
    DeleteCriticalSection(&queue->lock);
}

ThreadQueue_T* register_thread(HANDLE thread_handle) {
    for (int i = 0; i < MAX_THREADS; i++) {
        LONG prev_active = InterlockedCompareExchange(&thread_queues[i].active, 1, 0);
        if (prev_active == 0) {
            // Found an inactive slot
            thread_queues[i].thread_handle = thread_handle;
            message_queue_init(&thread_queues[i].queue, 0); // Unlimited queue size
            return &thread_queues[i];
        }
    }
    logger_log(LOG_ERROR, "Failed to register thread: maximum thread count reached");
    return NULL;
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