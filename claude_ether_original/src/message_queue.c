#include "message_queue.h"
#include <stdlib.h>
#include <string.h>
#include "platform_threads.h"
#include "platform_atomic.h"
#include "platform_time.h"
#include "logger.h"
#include "app_thread.h"

#define MAX_THREAD_QUEUES 128  // Define MAX_THREAD_QUEUES if not defined elsewhere

static ThreadQueue_T thread_queues[MAX_THREAD_QUEUES];

bool message_queue_init(MessageQueue_T* queue, uint32_t max_size) {
    if (!queue) {
        return false;
    }
    
    // Initialise head and tail indices
    queue->head = 0;
    queue->tail = 0;
    
    // Set max_size (cannot exceed MESSAGE_QUEUE_SIZE)
    queue->max_size = (max_size > 0 && max_size < MESSAGE_QUEUE_SIZE) ? max_size : MESSAGE_QUEUE_SIZE;
    
    // Allocate memory for the message array
    queue->entries = (Message_T*)calloc(queue->max_size, sizeof(Message_T));
    if (!queue->entries) {
        logger_log(LOG_ERROR, "Failed to allocate memory for message queue");
        return false;
    }
    
    // Create events for optional blocking behavior
    if (!platform_event_create(&queue->not_empty_event, false, false) ||
        !platform_event_create(&queue->not_full_event, false, true)) {
        
        logger_log(LOG_ERROR, "Failed to create queue synchronization events");
        free(queue->entries);
        queue->entries = NULL;
        platform_event_destroy(&queue->not_empty_event);
        platform_event_destroy(&queue->not_full_event);
        return false;
    }
    
    return true;
}

bool message_queue_push(MessageQueue_T* queue, const Message_T* message, uint32_t timeout_ms) {
    if (!queue || !message) {
        return false;
    }
    
    uint32_t start_time = platform_get_tick_count();
    
    while (true) {
        // Atomically read current indices
        int32_t head = platform_atomic_compare_exchange((volatile long*)&queue->head, 0, 0);
        int32_t tail = platform_atomic_compare_exchange((volatile long*)&queue->tail, 0, 0);
        int32_t next_head = (head + 1) % MESSAGE_QUEUE_SIZE;
        
        // Calculate current size
        int32_t size = (head >= tail) ? (head - tail) : (MESSAGE_QUEUE_SIZE - tail + head);
        
        // Check if queue is full
        if (next_head == tail || (queue->max_size > 0 && size >= queue->max_size)) {
            // If timeout is 0, return immediately
            if (timeout_ms == 0) {
                return false;
            }
            
            // Check if we've exceeded our timeout
            uint32_t elapsed = platform_get_tick_count() - start_time;
            if (elapsed >= timeout_ms) {
                return false;
            }
            
            // Wait for space (with remaining timeout)
            uint32_t remaining = timeout_ms - elapsed;
            int wait_result = platform_event_wait(&queue->not_full_event, remaining);
            if (wait_result != PLATFORM_WAIT_SUCCESS) {
                return false;
            }
            
            // Try again
            continue;
        }
        
        // Try to reserve the slot by updating head
        if (platform_atomic_compare_exchange((volatile long*)&queue->head, next_head, head) == head) {
            // Successfully reserved, copy message data
            memcpy(&queue->entries[head], message, sizeof(Message_T));
            
            // Signal not empty
            platform_event_set(&queue->not_empty_event);
            
            // Check if queue is now full
            if (next_head == tail || (queue->max_size > 0 && size + 1 >= queue->max_size)) {
                platform_event_reset(&queue->not_full_event);
            }
            
            return true;
        }
        
        // Another thread modified head, retry
    }
}

bool message_queue_pop(MessageQueue_T* queue, Message_T* message, uint32_t timeout_ms) {
    if (!queue || !message) {
        return false;
    }
    
    uint32_t start_time = platform_get_tick_count();
    
    while (true) {
        // Atomically read current indices
        int32_t head = platform_atomic_compare_exchange((volatile long*)&queue->head, 0, 0);
        int32_t tail = platform_atomic_compare_exchange((volatile long*)&queue->tail, 0, 0);
        
        // Check if queue is empty
        if (head == tail) {
            // If timeout is 0, return immediately
            if (timeout_ms == 0) {
                return false;
            }
            
            // Check if we've exceeded our timeout
            uint32_t elapsed = platform_get_tick_count() - start_time;
            if (elapsed >= timeout_ms) {
                return false;
            }
            
            // Wait for a message (with remaining timeout)
            uint32_t remaining = timeout_ms - elapsed;
            int wait_result = platform_event_wait(&queue->not_empty_event, remaining);
            if (wait_result != PLATFORM_WAIT_SUCCESS) {
                return false;
            }
            
            // Try again
            continue;
        }
        
        // Copy message first, in case another thread updates tail
        Message_T copied_message = queue->entries[tail];
        
        // Try to atomically update tail
        int32_t next_tail = (tail + 1) % MESSAGE_QUEUE_SIZE;
        if (platform_atomic_compare_exchange((volatile long*)&queue->tail, next_tail, tail) == tail) {
            // Successfully updated tail, provide the message
            memcpy(message, &copied_message, sizeof(Message_T));
            
            // Signal not full
            platform_event_set(&queue->not_full_event);
            
            // Check if queue is now empty
            if (next_tail == head) {
                platform_event_reset(&queue->not_empty_event);
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
    
    int32_t head = platform_atomic_compare_exchange((volatile long*)&queue->head, 0, 0);
    int32_t tail = platform_atomic_compare_exchange((volatile long*)&queue->tail, 0, 0);
    
    return (head == tail);
}

bool message_queue_is_full(MessageQueue_T* queue) {
    if (!queue) {
        return false;
    }
    
    int32_t head = platform_atomic_compare_exchange((volatile long*)&queue->head, 0, 0);
    int32_t tail = platform_atomic_compare_exchange((volatile long*)&queue->tail, 0, 0);
    int32_t next_head = (head + 1) % MESSAGE_QUEUE_SIZE;
    
    // Check if physically full or at max_size limit
    if (next_head == tail) {
        return true;
    }
    
    if (queue->max_size > 0) {
        int32_t size = (head >= tail) ? (head - tail) : (MESSAGE_QUEUE_SIZE - tail + head);
        return (size >= queue->max_size);
    }
    
    return false;
}

uint32_t message_queue_get_size(MessageQueue_T* queue) {
    if (!queue) {
        return 0;
    }
    
    int32_t head = platform_atomic_compare_exchange((volatile long*)&queue->head, 0, 0);
    int32_t tail = platform_atomic_compare_exchange((volatile long*)&queue->tail, 0, 0);
    
    return (head >= tail) ? (head - tail) : (MESSAGE_QUEUE_SIZE - tail + head);
}

void message_queue_clear(MessageQueue_T* queue) {
    if (!queue) {
        return;
    }
    
    // In lock-free design, we just reset head and tail
    platform_atomic_exchange((volatile long*)&queue->head, 0);
    platform_atomic_exchange((volatile long*)&queue->tail, 0);
    
    // Reset synchronization events
    platform_event_reset(&queue->not_empty_event);
    platform_event_set(&queue->not_full_event);
}

void message_queue_destroy(MessageQueue_T* queue) {
    if (!queue) {
        return;
    }
    
    // Free the allocated message array
    if (queue->entries) {
        free(queue->entries);
        queue->entries = NULL;
    }
    
    // Clean up synchronization objects
    platform_event_destroy(&queue->not_empty_event);
    platform_event_destroy(&queue->not_full_event);
}

ThreadQueue_T* register_thread(PlatformThread_T thread_handle) {
    for (int i = 0; i < MAX_THREAD_QUEUES; i++) {
        long prev_active = platform_atomic_compare_exchange((volatile long*)&thread_queues[i].active, 1, 0);
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

void deregister_thread(PlatformThread_T thread_handle) {
    for (int i = 0; i < MAX_THREAD_QUEUES; i++) {
        if (thread_queues[i].active && platform_thread_equal(thread_queues[i].thread_handle, thread_handle)) {
            message_queue_destroy(&thread_queues[i].queue);
            platform_atomic_exchange((volatile long*)&thread_queues[i].active, 0);
            break;
        }
    }
}

bool is_thread_active(PlatformThread_T thread_handle) {
    return platform_thread_is_active(thread_handle);
}

/**
 * @brief Get the thread queue for a given thread ID
 * @param thread_id Thread identifier to look up
 * @return Pointer to ThreadQueue_T or NULL if not found
 */
ThreadQueue_T* get_thread_queue(PlatformThread_T thread_id) {
    if (thread_id == NULL) {
        return NULL;
    }
    
    // Search through the thread queues array
    for (int i = 0; i < MAX_THREAD_QUEUES; i++) {
        if (thread_queues[i].active && 
            platform_thread_equal(thread_queues[i].thread_handle, thread_id)) {
            return &thread_queues[i];
        }
    }
    
    return NULL;
}
