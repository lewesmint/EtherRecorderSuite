#include "log_queue.h"
#include <stdio.h>
#include <stdbool.h>

#include "logger.h"
#include "platform_mutex.h"
#include "platform_atomic.h"

extern PlatformMutex_T logging_mutex; // Mutex for thread safety
LogQueue_T global_log_queue; // Define the log queue


#define QUEUE_HIGH_WATERMARK 0.99  // 99% full
#define QUEUE_LOW_WATERMARK 0.90   // 90% full

bool console_logging_suspended = false;

/**
 * @copydoc log_queue_init
 */
void log_queue_init(LogQueue_T *queue) {
    // No need for InterlockedExchange here as this function is called 
    // inside a mutex-protected section in init_logger_from_config
    queue->head.value = 0;
    queue->tail.value = 0;  
}

static double get_queue_capacity(const LogQueue_T* queue) {
    uint64_t head = platform_atomic_load_uint64(&queue->head);
    uint64_t tail = platform_atomic_load_uint64(&queue->tail);
    
    uint64_t used;
    if (head >= tail) {
        used = head - tail;
    } else {
        used = LOG_QUEUE_SIZE - (tail - head);
    }
    
    return (double)used / LOG_QUEUE_SIZE;
}

/**
 * @copydoc log_queue_push
 */
bool log_queue_push(LogQueue_T *log_queue, const LogEntry_T *entry) {
    if (!entry || entry->thread_label[0] == '\0') {
        return false;
    }

    // Check queue capacity
    double capacity = get_queue_capacity(log_queue);
    
    // If we hit high watermark, suspend console logging
    if (capacity >= QUEUE_HIGH_WATERMARK && !console_logging_suspended) {
        console_logging_suspended = true;
        LogEntry_T warning;
        create_log_entry(&warning, LOG_WARN, "Queue near capacity - suspending console output");
        log_now(&warning);  // Direct log to avoid recursion
    }
    // If we drop below low watermark, resume console logging
    else if (capacity <= QUEUE_LOW_WATERMARK && console_logging_suspended) {
        console_logging_suspended = false;
        LogEntry_T info;
        create_log_entry(&info, LOG_INFO, "Queue capacity normalized - resuming console output");
        log_now(&info);  // Direct log to avoid recursion
    }

    while (true) {
        // Atomically read current indices
        uint64_t head = platform_atomic_load_uint64(&log_queue->head);
        uint64_t tail = platform_atomic_load_uint64(&log_queue->tail);
        uint64_t next_head = (head + 1) % LOG_QUEUE_SIZE;

        // Check if queue is full
        if (next_head == tail) {
            // Handle overflow - we must acquire the mutex for this
            lock_mutex(&logging_mutex);
            
            // After acquiring mutex, reread indices and recheck if full
            head = platform_atomic_load_uint64(&log_queue->head);
            tail = platform_atomic_load_uint64(&log_queue->tail);
            
            if ((head + 1) % LOG_QUEUE_SIZE == tail) {
                // Queue is still full, need to purge some entries
                int purge_count = LOG_QUEUE_SIZE/10;
                LogEntry_T entry_1;
                char msg[LOG_MSG_BUFFER_SIZE];
                snprintf(msg, sizeof(msg), "Log queue overflow. Publishing oldest %d log entries immediately", purge_count);
                create_log_entry(&entry_1, LOG_ERROR, msg);
                log_now(&entry_1);
                
                while (purge_count > 0) {
                    head = platform_atomic_load_uint64(&log_queue->head);
                    tail = platform_atomic_load_uint64(&log_queue->tail);
                    
                    if (head == tail) break;
                    
                    LogEntry_T popped_entry;
                    // Pop entries directly (not using log_queue_pop to avoid potential mutex deadlock)
                    popped_entry = log_queue->entries[tail];
                    uint64_t next_tail = (tail + 1) % LOG_QUEUE_SIZE;
                    platform_atomic_compare_exchange_uint64(&log_queue->tail, &tail, next_tail);
                    
                    // Log the popped entry immediately
                    log_now(&popped_entry);
                    purge_count--;
                }
                
                LogEntry_T entry_2;
                create_log_entry(&entry_2, LOG_ERROR, "Log queue overflow. Purged complete");
                log_now(&entry_2);
                
                unlock_mutex(&logging_mutex);
            } else {
                // Queue is no longer full, release mutex and try again
                unlock_mutex(&logging_mutex);
            }
            
            // Try again from the beginning
            continue;
        }

        // Try to atomically reserve the slot by updating head
        if (platform_atomic_compare_exchange_uint64(&log_queue->head, &head, next_head)) {
            // Successfully reserved the slot at 'head'
            log_queue->entries[head] = *entry;
            return true;
        }
        
        // Failed to reserve - another thread updated head
        // Just retry from the beginning
    }
}

/**
 * @copydoc log_queue_pop
 */
bool log_queue_pop(LogQueue_T *queue, LogEntry_T *entry) {
    if (!entry) {
        return false; // Prevent null pointer access
    }

    while (true) {
        // Atomically read current indices
        uint64_t tail = platform_atomic_load_uint64(&queue->tail);
        uint64_t head = platform_atomic_load_uint64(&queue->head);

        // Check if queue is empty
        if (tail == head) {
            return false;
        }

        // Copy the entry at tail position
        LogEntry_T copied_entry = queue->entries[tail];
        
        // Try to atomically update the tail
        uint64_t next_tail = (tail + 1) % LOG_QUEUE_SIZE;
        if (platform_atomic_compare_exchange_uint64(&queue->tail, &tail, next_tail)) {
            // Successfully updated tail, return the copied entry
            *entry = copied_entry;
            return true;
        }
        
        // Another thread changed tail, retry the operation
    }
}

bool is_console_logging_suspended(void) {
    return console_logging_suspended;
}
