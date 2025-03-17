#ifndef MESSAGE_TYPES_H
#define MESSAGE_TYPES_H

#include <stdlib.h>
#include "platform_threads.h"
#include "platform_mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Message type identifiers
 */
typedef enum MessageType {
    MSG_TYPE_RELAY = 1,    ///< Relay message type
    MSG_TYPE_CONTROL = 2,  ///< Control message type
    MSG_TYPE_DATA = 3,     ///< Data message type
    MSG_TYPE_TEST = 4      ///< Test message type (used in demo)
} MessageType;

/**
 * @brief Message header structure
 */
typedef struct {
    uint32_t type;           ///< Message type identifier
    size_t content_size;     ///< Size of message content
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
    const char* owner_label;         ///< Label of the thread that owns this queue
} MessageQueue_T;

// Function declarations
bool message_queue_push(MessageQueue_T* queue, const Message_T* message, uint32_t timeout_ms);
bool message_queue_pop(MessageQueue_T* queue, Message_T* message, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // MESSAGE_TYPES_H
