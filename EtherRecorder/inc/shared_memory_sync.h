/**
 * @file shared_memory_sync.h
 * @brief UDP-based shared memory synchronization
 */
#ifndef SHARED_MEMORY_SYNC_H
#define SHARED_MEMORY_SYNC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "platform_error.h"
#include "platform_shared_memory.h"
#include "platform_sockets.h"
#include "thread_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Message types for shared memory synchronization
 */
typedef enum {
    SYNC_MSG_INIT = 0x01,       ///< Initial synchronization request/response
    SYNC_MSG_UPDATE = 0x02,     ///< Memory update message
    SYNC_MSG_ACK = 0x03,        ///< Acknowledgment message
    SYNC_MSG_FULL_SYNC = 0x04,  ///< Full memory state
    SYNC_MSG_HEARTBEAT = 0x05   ///< Connection keepalive
} SharedMemorySyncMessageType;

/**
 * @brief Access mode for shared memory synchronization
 */
typedef enum {
    SYNC_ACCESS_READ = 1,       ///< Read from local and send to remote
    SYNC_ACCESS_WRITE = 2,      ///< Receive from remote and write to local
    SYNC_ACCESS_READWRITE = 3   ///< Both read and write
} SharedMemorySyncAccessMode;

/**
 * @brief Configuration for shared memory synchronization
 */
typedef struct {
    char name[256];              ///< Name of the shared memory block
    char hostname[256];          ///< Remote hostname or IP address
    SharedMemorySyncAccessMode access; ///< Access mode
    size_t size;                 ///< Size of shared memory block in bytes
    uint16_t port;               ///< UDP port for communication
    bool create;                 ///< Whether to create shared memory if it doesn't exist
    unsigned int update_interval_ms; ///< Interval for checking changes
    unsigned int retry_count;    ///< Number of retries for failed transmissions
    unsigned int retry_interval_ms; ///< Interval between retries
} SharedMemorySyncConfig;

/**
 * @brief Initialize shared memory synchronization
 * 
 * @param config_section The INI file section name to read configuration from
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode shared_memory_sync_init(const char* config_section);

/**
 * @brief Shutdown shared memory synchronization
 */
void shared_memory_sync_shutdown(void);

/**
 * @brief Get thread configuration for UDP listener thread
 * 
 * @return Pointer to thread configuration
 */
ThreadConfig* get_udp_listener_thread(void);

/**
 * @brief Shared memory change callback that sends updates via UDP
 * 
 * @param data Pointer to shared memory data
 * @param size Size of data in bytes
 */
void shared_memory_udp_sender_callback(const void* data, size_t size);

#ifdef __cplusplus
}
#endif

#endif // SHARED_MEMORY_SYNC_H
