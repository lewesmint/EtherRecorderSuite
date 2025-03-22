#ifndef MESSAGE_QUEUE_TYPES_H
#define MESSAGE_QUEUE_TYPES_H

#include <stdint.h>
#include "platform_sync.h"

#ifdef _MSC_VER
#pragma warning(disable: 4200)  // Disable warning about zero-sized array
#endif

// Message content size matches typical MTU minus protocol headers
#define MESSAGE_CONTENT_SIZE 1472  // 1500 (MTU) - 20 (IP) - 8 (UDP/Other headers)

// Message types
typedef enum {
    MSG_TYPE_RELAY = 1,
    MSG_TYPE_TEST = 2,
    MSG_TYPE_FILE_CHUNK = 3,  // For file chunks
    MSG_TYPE_CONTROL = 4,
    MSG_TYPE_DATA = 5
} MessageType;


// Structure for file chunk metadata (goes in Message_T.content)
typedef struct {
    uint32_t chunk_index;     // Index of this chunk
    uint32_t total_chunks;    // Total number of chunks (0 if unknown)
    uint32_t chunk_size;      // Size of data in this chunk
    uint32_t file_offset;     // Offset in original file
    char filename[256];       // Original filename
    uint8_t data[];           // Flexible array member for chunk data
} FileChunkData;


/**
 * @brief Message header structure
 */
typedef struct {
    MessageType type;         ///< Message type identifier
    size_t content_size;   ///< Size of content in bytes
} MessageHeader_T;

/**
 * @brief Message structure for inter-thread communication
 */
typedef struct {
    MessageHeader_T header;  ///< Message header
    uint8_t content[MESSAGE_CONTENT_SIZE];  ///< Message content buffer
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
