#include "message_types.h"

#include "logger.h"
#include <string.h>
#include <stdlib.h>

#include "platform_threads.h"

bool message_queue_push(MessageQueue_T* queue, const Message_T* message, uint32_t timeout_ms) {
    if (!queue || !message) {
        logger_log(LOG_ERROR, "Invalid parameters for message queue push");
        return false;
    }

    if (!platform_event_wait(queue->not_full_event, timeout_ms)) {
        logger_log(LOG_ERROR, "Queue full timeout");
        return false;
    }

    int32_t next_tail = (queue->tail + 1) % queue->max_size;
    if (next_tail == queue->head) {
        logger_log(LOG_ERROR, "Queue full");
        return false;
    }

    memcpy(&queue->entries[queue->tail], message, sizeof(Message_T));
    queue->tail = next_tail;

    platform_event_set(queue->not_empty_event);
    return true;
}

bool message_queue_pop(MessageQueue_T* queue, Message_T* message, uint32_t timeout_ms) {
    if (!queue || !message) {
        logger_log(LOG_ERROR, "Invalid parameters for message queue pop");
        return false;
    }

    if (!platform_event_wait(queue->not_empty_event, timeout_ms)) {
        logger_log(LOG_ERROR, "Queue empty timeout");
        return false;
    }

    if (queue->head == queue->tail) {
        logger_log(LOG_ERROR, "Queue empty");
        return false;
    }

    memcpy(message, &queue->entries[queue->head], sizeof(Message_T));
    queue->head = (queue->head + 1) % queue->max_size;

    platform_event_set(queue->not_full_event);
    return true;
}
