#include "message_types.h"
#include "platform_defs.h"
#include "platform_threads.h"
#include "logger.h"
#include <string.h>
#include <stdlib.h>

bool message_queue_push(MessageQueue_T* queue, const Message_T* message, uint32_t timeout_ms) {
    if (!queue || !message) {
        logger_log(LOG_ERROR, "Invalid parameters for message queue push");
        return false;
    }

    if (!platform_event_wait(&queue->not_full_event, timeout_ms)) {
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

    platform_event_set(&queue->not_empty_event);
    return true;
}

bool message_queue_pop(MessageQueue_T* queue, Message_T* message, uint32_t timeout_ms) {
    if (!queue || !message) {
        logger_log(LOG_ERROR, "Invalid parameters for message queue pop");
        return false;
    }

    if (!platform_event_wait(&queue->not_empty_event, timeout_ms)) {
        return false;
    }

    if (queue->head == queue->tail) {
        return false;
    }

    memcpy(message, &queue->entries[queue->head], sizeof(Message_T));
    queue->head = (queue->head + 1) % queue->max_size;

    platform_event_set(&queue->not_full_event);
    return true;
}

MessageQueue_T* message_queue_create_for_thread(ThreadHandle_T thread) {
    (void)thread; // Unused parameter

    MessageQueue_T* queue = (MessageQueue_T*)malloc(sizeof(MessageQueue_T));
    if (!queue) {
        logger_log(LOG_ERROR, "Failed to allocate message queue");
        return NULL;
    }

    queue->entries = (Message_T*)malloc(sizeof(Message_T) * 100); // Default size
    if (!queue->entries) {
        free(queue);
        logger_log(LOG_ERROR, "Failed to allocate message queue entries");
        return NULL;
    }

    queue->head = 0;
    queue->tail = 0;
    queue->max_size = 100;

    if (!platform_event_create(&queue->not_empty_event, false, false) ||
        !platform_event_create(&queue->not_full_event, false, true)) {
        free(queue->entries);
        free(queue);
        logger_log(LOG_ERROR, "Failed to create queue events");
        return NULL;
    }

    return queue;
}
