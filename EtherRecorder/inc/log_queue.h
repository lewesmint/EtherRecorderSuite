/*
* @file log_queue.h
* @brief Contains the log queue functions.
*/
#ifndef LOG_QUEUE_H
#define LOG_QUEUE_H


#include "logger.h"
#include "platform_atomic.h"


#define LOG_QUEUE_SIZE 0x8000  // Size of the log queue

// Slot states - using uint8_t since we only need 2 bits
typedef enum {
    SLOT_EMPTY    = 0,  // Available for writing
    SLOT_RESERVED = 1,  // Being written to
    SLOT_WRITTEN  = 2   // Ready for reading
} LogQueueSlotState;

/**
 * @brief Structure representing a log queue.
 */
typedef struct {
    LogEntry_T entries[LOG_QUEUE_SIZE];
    PlatformAtomicUInt32 head;
    PlatformAtomicUInt32 tail;
    PlatformAtomicUInt8 slot_states[LOG_QUEUE_SIZE];  // Uses LogQueueSlotState values
} LogQueue_T;

extern LogQueue_T global_log_queue; // Declare the log queue

/**
 * @brief Initialises the log queue.
 * @param queue The log queue to initialise.
 */
void log_queue_init(LogQueue_T *queue);


bool log_queue_push(LogQueue_T *log_queue, const LogEntry_T *entry);

/**
 * @brief Pops a log entry from the log queue.
 * @param queue The log queue.
 * @param entry The log entry to populate.
 * @return 0 on success, -1 if the queue is empty.
 */
bool log_queue_pop(LogQueue_T *queue, LogEntry_T *entry);

#endif // LOG_QUEUE_H
