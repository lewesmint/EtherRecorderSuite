#include "log_queue.h"
#include <stdio.h>
#include <string.h>  // Add this include for memset
#include <stdbool.h>

#include "platform_random.h"
#include "platform_mutex.h"
#include "platform_atomic.h"

#include "logger.h"

extern void log_immediately(const LogEntry_T* entry);

extern PlatformMutex_T logging_mutex; // Mutex for thread safety
LogQueue_T global_log_queue; // Define the log queue


#define QUEUE_HIGH_WATERMARK 0.99  // 99% full
#define QUEUE_LOW_WATERMARK 0.60   // 60% full

bool console_logging_suspended = false;

/**
 * @copydoc log_queue_init
 */
void log_queue_init(LogQueue_T *queue) {
    queue->head.value = 0;
    queue->tail.value = 0;
    // Initialize all slot states to empty (0)
    memset(queue->slot_states, 0, LOG_QUEUE_SIZE * sizeof(uint32_t));
}

static double get_queue_capacity(const LogQueue_T* queue) {
    uint32_t head = platform_atomic_load_uint32(&queue->head);
    uint32_t tail = platform_atomic_load_uint32(&queue->tail);
    
    uint32_t used;
    if (head >= tail) {
        used = head - tail;
    } else {
        used = LOG_QUEUE_SIZE - (tail - head);
    }
    
    return (double)used / LOG_QUEUE_SIZE;
}

static void handle_queue_capacity_state(double capacity) {
    // If we hit high watermark, suspend console logging
    if (capacity >= QUEUE_HIGH_WATERMARK && !console_logging_suspended) {
        console_logging_suspended = true;
        LogEntry_T warning;
        create_log_entry(&warning, LOG_WARN, "Queue near capacity - suspending console output");
        log_now(&warning);  // Direct log to avoid recursion
        fflush(stdout);
        fflush(stderr);
        console_logging_suspended = true;
        sleep_ms(100);
    }
    // If we drop below low watermark, resume console logging
    else if (capacity <= QUEUE_LOW_WATERMARK && console_logging_suspended) {
        console_logging_suspended = false;
        LogEntry_T info;
        create_log_entry(&info, LOG_INFO, "Queue capacity normalized - resuming console output");
        log_now(&info);  // Direct log to avoid recursion
    }
}

/**
 * Helper function for exponential backoff
 */
static void backoff_delay(int attempt) {
    if (attempt <= 0) return;
    
    // Exponential backoff with a cap
    int delay = (1 << (attempt > 10 ? 10 : attempt)); // Cap at 2^10
    
    // Add some randomness to avoid thundering herd problem
    delay = delay + (int)platform_random_range(0, delay);
    
    sleep_ms(delay);
}

/**
 * @copydoc log_queue_push
 */
bool log_queue_push(LogQueue_T *log_queue, const LogEntry_T *entry) {
    if (!entry || entry->thread_label[0] == '\0') {
        return false;
    }

    // Check queue capacity and deal with any issues
    handle_queue_capacity_state(get_queue_capacity(log_queue));

    #define MAX_RETRY_ATTEMPTS 100
    int attempt = 0;
    
    while (attempt < MAX_RETRY_ATTEMPTS) {
        uint32_t head = platform_atomic_load_uint32(&log_queue->head);
        uint32_t tail = platform_atomic_load_uint32(&log_queue->tail);
        uint32_t next_head = (head + 1) % LOG_QUEUE_SIZE;

        // Check if next slot would be tail
        if (next_head == tail) {
            // Handle overflow - we must acquire the mutex for this
            lock_mutex(&logging_mutex);
            
            // After acquiring mutex, reread indices and recheck if full
            head = platform_atomic_load_uint32(&log_queue->head);
            tail = platform_atomic_load_uint32(&log_queue->tail);
            
            if ((head + 1) % LOG_QUEUE_SIZE == tail) {
                // Queue is still full, need to purge some entries
                int purge_count = LOG_QUEUE_SIZE/10;
                LogEntry_T entry_1;
                char msg[LOG_MSG_BUFFER_SIZE];
                snprintf(msg, sizeof(msg), "Log queue overflow. Publishing oldest %d log entries immediately", purge_count);
                create_log_entry(&entry_1, LOG_ERROR, msg);
                // note: log_immediately does not acquire the mutex, log_now does.
                log_immediately(&entry_1);
                
                while (purge_count > 0) {
                    head = platform_atomic_load_uint32(&log_queue->head);
                    tail = platform_atomic_load_uint32(&log_queue->tail);
                    
                    if (head == tail) break;
                    
                    LogEntry_T popped_entry;
                    // Pop entries directly (not using log_queue_pop to avoid potential mutex deadlock)
                    popped_entry = log_queue->entries[tail];
                    uint32_t next_tail = (tail + 1) % LOG_QUEUE_SIZE;
                    if (!platform_atomic_compare_exchange_uint32(&log_queue->tail, &tail, next_tail)) {
                        // CAS failed - another thread modified tail
                        // We should retry this iteration
                        continue;
                    }
                    
                    // Log the popped entry immediately
                    log_immediately(&popped_entry);
                    purge_count--;
                }
                
                LogEntry_T entry_2;
                create_log_entry(&entry_2, LOG_ERROR, "Log queue overflow. Purged complete");
                log_immediately(&entry_2);
                
                unlock_mutex(&logging_mutex);
            } else {
                // Queue is no longer full, release mutex and try again
                unlock_mutex(&logging_mutex);
            }
            
            // Try again from the beginning
            attempt++; // Increment attempt counter
            backoff_delay(attempt);
            continue;
        }

        // Check if current slot is already reserved or written
        uint8_t state = platform_atomic_load_uint8(&log_queue->slot_states[head]);
        if (state == SLOT_RESERVED || state == SLOT_WRITTEN) {
            // Slot is either reserved or written, move to next slot
            head = next_head;
            attempt++; // Increment attempt counter
            backoff_delay(attempt);
            continue;
        }

        // Try to reserve the slot with atomic compare-exchange
        uint8_t expected_state = SLOT_EMPTY;
        if (!platform_atomic_compare_exchange_uint8(
                &log_queue->slot_states[head],
                &expected_state,
                SLOT_RESERVED)) {
            // Another thread modified the state, try again
            attempt++; // Increment attempt counter
            backoff_delay(attempt);
            continue;
        }
        
        // Try to claim the slot
        if (platform_atomic_compare_exchange_uint32(&log_queue->head, &head, next_head)) {
            // Write entry
            log_queue->entries[head] = *entry;
            // Mark as fully written
            platform_atomic_store_uint8(&log_queue->slot_states[head], SLOT_WRITTEN);
            return true;
        }
        // If CAS failed, reset state and retry
        platform_atomic_store_uint8(&log_queue->slot_states[head], SLOT_EMPTY);
        attempt++; // Increment attempt counter
        backoff_delay(attempt);
    }
    
    // If we've reached here, we've failed after MAX_RETRY_ATTEMPTS
    // Fall back to a guaranteed approach using the mutex
    lock_mutex(&logging_mutex);
    
    // After acquiring mutex, check if we can now add to the queue
    uint32_t head = platform_atomic_load_uint32(&log_queue->head);
    uint32_t tail = platform_atomic_load_uint32(&log_queue->tail);
    
    if ((head + 1) % LOG_QUEUE_SIZE == tail) {
        // Queue is still full, log directly
        log_immediately(entry);
        unlock_mutex(&logging_mutex);
        return true;
    } else {
        // Queue has space now, add entry with mutex held
        uint32_t new_head = (head + 1) % LOG_QUEUE_SIZE;
        
        // Write the entry
        log_queue->entries[head] = *entry;
        
        // Mark as written
        platform_atomic_store_uint8(&log_queue->slot_states[head], SLOT_WRITTEN);
        
        // Update head pointer
        platform_atomic_store_uint32(&log_queue->head, new_head);
        
        unlock_mutex(&logging_mutex);
        return true;
    }
}


bool log_queue_pop(LogQueue_T *queue, LogEntry_T *entry) {
    if (!entry) {
        return false;
    }

    while (true) {
        uint32_t tail = platform_atomic_load_uint32(&queue->tail);
        uint32_t head = platform_atomic_load_uint32(&queue->head);

        if (tail == head) {
            return false;
        }

        // Only read if state is written 
        if (platform_atomic_load_uint8(&queue->slot_states[tail]) != SLOT_WRITTEN) {
            continue;
        }

        uint32_t next_tail = (tail + 1) % LOG_QUEUE_SIZE;
        if (platform_atomic_compare_exchange_uint32(&queue->tail, &tail, next_tail)) {
            // Copy the entry
            *entry = queue->entries[tail];
            // Mark as available using the constant instead of 0
            platform_atomic_store_uint8(&queue->slot_states[tail], SLOT_EMPTY);
            return true;
        }
    }
}

bool is_console_logging_suspended(void) {
    return console_logging_suspended;
}
