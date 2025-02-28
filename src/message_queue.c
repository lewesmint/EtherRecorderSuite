#include "message_queue.h"
#include <stdlib.h>
#include <string.h>
#include <windows.h> // For Interlocked operations and thread functions
#include "logger.h"

#define MAX_THREADS 5
#define MESSAGE_QUEUE_SIZE 1024  // Power of 2 for performance

static ThreadQueue_T thread_queues[MAX_THREADS];

bool message_queue_init(MessageQueue_T* queue, uint32_t max_size) {
    if (!queue) {
        return false;
    }
    
    // Initialize head and tail indices
    queue->head = 0;
    queue->tail = 0;
    
    // Set max_size (cannot exceed MESSAGE_QUEUE_SIZE)
    queue->max_size = (max_size > 0 && max_size < MESSAGE_QUEUE_SIZE) ? max_size : MESSAGE_QUEUE_SIZE;
    
    // Create events for optional blocking behavior
    queue->not_empty_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    queue->not_full_event = CreateEvent(NULL, TRUE, TRUE, NULL);
    
    if (!queue->not_empty_event || !queue->not_full_event) {
        logger_log(LOG_ERROR, "Failed to create queue synchronization events");
        if (queue->not_empty_event) CloseHandle(queue->not_empty_event);
        if (queue->not_full_event) CloseHandle(queue->not_full_event);
        return false;
    }
    
    return true;
}

bool message_queue_push(MessageQueue_T* queue, const Message_T* message, DWORD timeout_ms) {
    if (!queue || !message) {
        return false;
    }
    
    DWORD start_time = GetTickCount();
    
    while (true) {
        // Atomically read current indices
        long head = InterlockedCompareExchange(&queue->head, 0, 0);
        long tail = InterlockedCompareExchange(&queue->tail, 0, 0);
        long next_head = (head + 1) % MESSAGE_QUEUE_SIZE;
        
        // Calculate current size
        long size = (head >= tail) ? (head - tail) : (MESSAGE_QUEUE_SIZE - tail + head);
        
        // Check if queue is full
        if (next_head == tail || (queue->max_size > 0 && size >= queue->max_size)) {
            // If timeout is 0, return immediately
            if (timeout_ms == 0) {
                return false;
            }
            
            // Check if we've exceeded our timeout
            DWORD elapsed = GetTickCount() - start_time;
            if (elapsed >= timeout_ms) {
                return false;
            }
            
            // Wait for space (with remaining timeout)
            DWORD remaining = timeout_ms - elapsed;
            DWORD wait_result = WaitForSingleObject(queue->not_full_event, remaining);
            if (wait_result != WAIT_OBJECT_0) {
                return false;
            }
            
            // Try again
            continue;
        }
        
        // Try to reserve the slot by updating head
        if (InterlockedCompareExchange(&queue->head, next_head, head) == head) {
            // Successfully reserved, copy message data
            memcpy(&queue->entries[head], message, sizeof(Message_T));
            
            // Signal not empty
            SetEvent(queue->not_empty_event);
            
            // Check if queue is now full
            if (next_head == tail || (queue->max_size > 0 && size + 1 >= queue->max_size)) {
                ResetEvent(queue->not_full_event);
            }
            
            return true;
        }
        
        // Another thread modified head, retry
    }
}

bool message_queue_pop(MessageQueue_T* queue, Message_T* message, DWORD timeout_ms) {
    if (!queue || !message) {
        return false;
    }
    
    DWORD start_time = GetTickCount();
    
    while (true) {
        // Atomically read current indices
        long head = InterlockedCompareExchange(&queue->head, 0, 0);
        long tail = InterlockedCompareExchange(&queue->tail, 0, 0);
        
        // Check if queue is empty
        if (head == tail) {
            // If timeout is 0, return immediately
            if (timeout_ms == 0) {
                return false;
            }
            
            // Check if we've exceeded our timeout
            DWORD elapsed = GetTickCount() - start_time;
            if (elapsed >= timeout_ms) {
                return false;
            }
            
            // Wait for a message (with remaining timeout)
            DWORD remaining = timeout_ms - elapsed;
            DWORD wait_result = WaitForSingleObject(queue->not_empty_event, remaining);
            if (wait_result != WAIT_OBJECT_0) {
                return false;
            }
            
            // Try again
            continue;
        }
        
        // Copy message first, in case another thread updates tail
        Message_T copied_message = queue->entries[tail];
        
        // Try to atomically update tail
        long next_tail = (tail + 1) % MESSAGE_QUEUE_SIZE;
        if (InterlockedCompareExchange(&queue->tail, next_tail, tail) == tail) {
            // Successfully updated tail, provide the message
            memcpy(message, &copied_message, sizeof(Message_T));
            
            // Signal not full
            SetEvent(queue->not_full_event);
            
            // Check if queue is now empty
            if (next_tail == head) {
                ResetEvent(queue->not_empty_event);
            }
            
            return true;
        }
        
        // Another thread modified tail, retry
    }
}

bool message_queue_is_empty(MessageQueue_T* queue) {
    if (!queue) {
        return true;
    }
    
    long head = InterlockedCompareExchange(&queue->head, 0, 0);
    long tail = InterlockedCompareExchange(&queue->tail, 0, 0);
    
    return (head == tail);
}

bool message_queue_is_full(MessageQueue_T* queue) {
    if (!queue) {
        return false;
    }
    
    long head = InterlockedCompareExchange(&queue->head, 0, 0);
    long tail = InterlockedCompareExchange(&queue->tail, 0, 0);
    long next_head = (head + 1) % MESSAGE_QUEUE_SIZE;
    
    // Check if physically full or at max_size limit
    if (next_head == tail) {
        return true;
    }
    
    if (queue->max_size > 0) {
        long size = (head >= tail) ? (head - tail) : (MESSAGE_QUEUE_SIZE - tail + head);
        return (size >= queue->max_size);
    }
    
    return false;
}

uint32_t message_queue_get_size(MessageQueue_T* queue) {
    if (!queue) {
        return 0;
    }
    
    long head = InterlockedCompareExchange(&queue->head, 0, 0);
    long tail = InterlockedCompareExchange(&queue->tail, 0, 0);
    
    return (head >= tail) ? (head - tail) : (MESSAGE_QUEUE_SIZE - tail + head);
}

void message_queue_clear(MessageQueue_T* queue) {
    if (!queue) {
        return;
    }
    
    // In lock-free design, we just reset head and tail
    InterlockedExchange(&queue->head, 0);
    InterlockedExchange(&queue->tail, 0);
    
    // Reset synchronization events
    ResetEvent(queue->not_empty_event);
    SetEvent(queue->not_full_event);
}

void message_queue_destroy(MessageQueue_T* queue) {
    if (!queue) {
        return;
    }
    
    // Clean up synchronization objects
    CloseHandle(queue->not_empty_event);
    CloseHandle(queue->not_full_event);
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