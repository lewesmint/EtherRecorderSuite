/**
 * @file shared_memory_sync.h
 * @brief UDP-based shared memory synchronization
 */
#ifndef SHARED_MEMORY_SYNC_H
#define SHARED_MEMORY_SYNC_H

#include <stdint.h>
#include <stdbool.h>
#include "platform_shared_memory.h"
#include "platform_error.h"
#include "shared_memory_monitor.h"  // Include for SharedMemoryMonitorConfig

#define MAX_SYNC_MESSAGE_SIZE 1400

// Message types for shared memory synchronization
typedef enum {
    SYNC_MSG_INIT = 1,       // Initial full memory state
    SYNC_MSG_UPDATE = 2,     // Incremental update
    SYNC_MSG_ACK = 3,        // Acknowledgment
    SYNC_MSG_HEARTBEAT = 4   // Heartbeat/keep-alive
} SharedMemorySyncMessageType;

// Structure to pass shared memory information to the listener thread
typedef struct {
    int block_index;
    PlatformSharedMemoryHandle shm_handle;
    void* mapped_data;
    size_t data_size;
    char name[64];
} SharedMemoryListenerData;

// Initialize shared memory synchronization
PlatformErrorCode shared_memory_sync_init(const char* config_section);

// Shutdown shared memory synchronization
void shared_memory_sync_shutdown(void);

// Get the shared memory listener thread
ThreadConfig* get_shared_memory_listener_thread(void);

// Send memory update
PlatformErrorCode shared_memory_sync_send_update(const void* data, size_t size);

#endif // SHARED_MEMORY_SYNC_H
