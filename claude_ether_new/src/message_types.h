#ifndef MESSAGE_TYPES_H
#define MESSAGE_TYPES_H

typedef enum {
    MSG_TYPE_RELAY = 1,
    MSG_TYPE_TEST = 2,
    MSG_TYPE_FILE_CHUNK = 3,  // New type for file chunks
    // ... other message types ...
} MessageType;

// Structure for file chunk metadata (goes in Message_T.content)
typedef struct {
    uint32_t chunk_index;     // Index of this chunk
    uint32_t total_chunks;    // Total number of chunks (0 if unknown)
    uint32_t chunk_size;      // Size of data in this chunk
    uint32_t file_offset;     // Offset in original file
    char filename[256];       // Original filename
    uint8_t data[];          // Flexible array member for chunk data
} FileChunkData;

#endif
