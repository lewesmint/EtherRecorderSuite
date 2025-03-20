#ifndef MESSAGE_QUEUE_TYPES_H
#define MESSAGE_QUEUE_TYPES_H

#include <stdint.h>
#include "platform_sync.h"

// Message types
typedef enum {
    MSG_TYPE_RELAY = 1,
    MSG_TYPE_TEST = 2,
    MSG_TYPE_FILE_CHUNK = 3,  // For file chunks
    MSG_TYPE_CONTROL = 4,
    MSG_TYPE_DATA = 5
} MessageType;

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
    const char* owner_label;         ///< Label identifying the queue owner
} MessageQueue_T;

#endif // MESSAGE_QUEUE_TYPES_H
