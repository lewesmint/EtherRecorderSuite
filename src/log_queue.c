#include "log_queue.h"
#include "logger.h"
#include "platform_mutex.h"
#include "platform_utils.h"
#include "platform_atomic.h"

extern PlatformMutex_T logging_mutex; // Mutex for thread safety
LogQueue_T global_log_queue; // Define the log queue

/**
 * @copydoc log_queue_init
 */
void log_queue_init(LogQueue_T *queue) {
    // No need for InterlockedExchange here as this function is called 
    // inside a mutex-protected section in init_logger_from_config
    queue->head = 0;
    queue->tail = 0;  
}

/**
 * @copydoc log_queue_push
 */
bool log_queue_push(LogQueue_T *log_queue, const LogEntry_T *entry) {
    if (!entry || entry->thread_label[0] == '\0') {
        return false; // Prevent null pointer access and empty thread labels
    }

    while (true) {
        // Atomically read current indices
        long head = platform_atomic_compare_exchange(&log_queue->head, 0, 0);
        long tail = platform_atomic_compare_exchange(&log_queue->tail, 0, 0);
        long next_head = (head + 1) % LOG_QUEUE_SIZE;

        // Check if queue is full
        if (next_head == tail) {
            // Handle overflow - we must acquire the mutex for this
            lock_mutex(&logging_mutex);
            
            // After acquiring mutex, reread indices and recheck if full
            if ((log_queue->head + 1) % LOG_QUEUE_SIZE == log_queue->tail) {
                // Queue is still full, need to purge some entries
                int purge_count = 3;
                LogEntry_T entry_1;
                char msg[LOG_MSG_BUFFER_SIZE];
                snprintf(msg, sizeof(msg), "Log queue overflow. Publishing oldest %d log entries immediately", purge_count);
                create_log_entry(&entry_1, LOG_ERROR, msg);
                log_now(&entry_1);
                
                while (purge_count > 0 && log_queue->head != log_queue->tail) {
                    LogEntry_T popped_entry;
                    // Pop entries directly (not using log_queue_pop to avoid potential mutex deadlock)
                    long current_tail = log_queue->tail;
                    popped_entry = log_queue->entries[current_tail];
                    log_queue->tail = (current_tail + 1) % LOG_QUEUE_SIZE;
                    
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

        // First, try to atomically reserve the slot by updating head
        // This is the crucial step - reserve BEFORE writing
        if (platform_atomic_compare_exchange(&log_queue->head, next_head, head) == head) {
            // Successfully reserved the slot at 'head'
            // Now we can safely write to it without races
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
        long tail = platform_atomic_compare_exchange(&queue->tail, 0, 0);
        long head = platform_atomic_compare_exchange(&queue->head, 0, 0);

        // Check if queue is empty
        if (tail == head) {
            return false;
        }

        // Copy the entry at tail position
        LogEntry_T copied_entry = queue->entries[tail];
        
        // Try to atomically update the tail
        long next_tail = (tail + 1) % LOG_QUEUE_SIZE;
        if (platform_atomic_compare_exchange(&queue->tail, next_tail, tail) == tail) {
            // Successfully updated tail, return the copied entry
            *entry = copied_entry;
            return true;
        }
        
        // Another thread changed tail, retry the operation
    }
}


