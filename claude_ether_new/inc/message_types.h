#ifndef MESSAGE_TYPES_H
#define MESSAGE_TYPES_H

#include <stdlib.h>
#include "platform_threads.h"
#include "platform_mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

// Message types
#define MSG_TYPE_RELAY 1
#define MSG_TYPE_CONTROL 2
#define MSG_TYPE_DATA 3

/**
 * @brief Message header structure
 */
typedef struct {
    uint32_t type;           ///< Message type identifier
    uint32_t content_size;   ///< Size of content in bytes
} MessageHeader_T;

/**
 * @brief Message structure for inter-thread communication
 */
typedef struct {
    MessageHeader_T header;  ///< Message header
    uint8_t content[1024];  ///< Message content buffer
} Message_T;

/**
 * @brief Queue structure for message storage
 */
typedef struct {
    Message_T* entries;               ///< Array of messages
    volatile int32_t head;           ///< Index of head (atomic access)
    volatile int32_t tail;           ///< Index of tail (atomic access)
    int32_t max_size;                ///< Maximum number of messages allowed
    PlatformEvent_T not_empty_event; ///< Event for signaling queue not empty
    PlatformEvent_T not_full_event;  ///< Event for signaling queue not full
} MessageQueue_T;

// Function declarations
bool message_queue_push(MessageQueue_T* queue, const Message_T* message, uint32_t timeout_ms);
bool message_queue_pop(MessageQueue_T* queue, Message_T* message, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // MESSAGE_TYPES_H
