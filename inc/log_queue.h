/*
* @file log_queue.h
* @brief Contains the log queue functions.
*/
#ifndef LOG_QUEUE_H
#define LOG_QUEUE_H

#include <winsock2.h>

#include "logger.h"


#define LOG_QUEUE_SIZE 20000 // Size of the log queue

/**
 * @brief Structure representing a log queue.
 */
typedef struct LogQueue_T {
    long head;
    long tail;
    LogEntry_T entries[LOG_QUEUE_SIZE];
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
