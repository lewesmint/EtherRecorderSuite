/**
 * @file shared_memory_sync.h
 * @brief UDP-based shared memory synchronization
 */
#ifndef SHARED_MEMORY_SYNC_H
#define SHARED_MEMORY_SYNC_H

#include <stdint.h>
#include <stddef.h>
#include "platform_shared_memory.h"  // Include for PlatformSharedMemoryAccess
#include "app_thread.h"

// Message types for sync protocol
typedef enum {
    SYNC_MSG_INIT = 1,     // Initial sync request
    SYNC_MSG_UPDATE = 2,   // Memory update
    SYNC_MSG_ACK = 3,      // Acknowledgment
    SYNC_MSG_HEARTBEAT = 4 // Heartbeat message
} SharedMemorySyncMessageType;

// Configuration for shared memory sync
typedef struct {
    char name[64];                // Name of shared memory block
    char hostname[128];           // Remote hostname/IP
    PlatformSharedMemoryAccess access;  // Access mode (using platform enum)
    size_t size;                  // Size of shared memory
    uint16_t port;                // UDP port
    bool create;                  // Create if not exists
    unsigned int update_interval_ms;  // Update interval
    unsigned int retry_count;     // Retry count
    unsigned int retry_interval_ms;   // Retry interval
} SharedMemorySyncConfig;

// Function declarations
PlatformErrorCode shared_memory_sync_init(const char* config_section);
void shared_memory_sync_shutdown(void);
ThreadConfig* get_udp_listener_thread(void);
void shared_memory_udp_sender_callback(const void* data, size_t size);
void shared_memory_udp_sender_callback_with_offset(const void* data, size_t size, uint32_t offset);

#endif // SHARED_MEMORY_SYNC_H
