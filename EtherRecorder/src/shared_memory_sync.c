#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shared_memory_sync.h"
#include "platform_sockets.h"
#include "platform_error.h"
#include "platform_shared_memory.h"
#include "platform_time.h"
#include "platform_string.h"
#include "thread_registry.h"
#include "app_thread.h"
#include "logger.h"
#include "app_config.h"
#include "shutdown_handler.h"


// Message header structure
#pragma pack(1)
typedef struct {
    uint8_t msg_type;        // Message type from SharedMemorySyncMessageType
    uint16_t seq_num;        // Sequence number for ordering/reliability
    uint8_t name_len;        // Length of block name
    char block_name[0];      // Variable-length block name (flexible array member)
    // After block_name, the following fields appear:
    // uint32_t offset;      // Offset in memory block
    // uint32_t data_len;    // Length of data
    // uint8_t data[];       // Variable-length data
} SyncMessageHeader;
#pragma pack()

// Global configuration
static SharedMemorySyncConfig sync_config = {
    .name = "TestSharedMemory",
    .hostname = "127.0.0.1",
    .access = PLATFORM_SHM_READWRITE,
    .size = 1024,
    .port = 5000,
    .create = true,
    .update_interval_ms = 100,
    .retry_count = 3,
    .retry_interval_ms = 500
};

// Communication state
static PlatformSocketHandle udp_socket = NULL;
static PlatformSocketAddress remote_addr;
static uint16_t sequence_counter = 0;

// Shared memory state for writer
static PlatformSharedMemoryHandle writer_shm = NULL;
static void* writer_data = NULL;

// Forward declarations
static void* udp_listener_thread(void* arg);
static PlatformErrorCode apply_memory_update(const uint8_t* message, size_t message_size);
static PlatformErrorCode send_udp_message(SharedMemorySyncMessageType type, const void* data, size_t size);

// Add this function to check and log socket state
static void log_socket_state(const char* function_name) {
    logger_log(LOG_DEBUG, "[%s] UDP socket state: %p", function_name, (void*)udp_socket);
}

PlatformErrorCode shared_memory_sync_init(const char* config_section) {
    log_socket_state("shared_memory_sync_init (start)");
    
    if (!config_section) {
        logger_log(LOG_ERROR, "NULL config_section passed to shared_memory_sync_init");
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    // Read configuration from INI file
    const char* name = get_config_string(config_section, "name", sync_config.name);
    sync_config.name[0] = '\0';  // Initialize to empty string
    platform_strcat(sync_config.name, name, sizeof(sync_config.name));

    const char* hostname = get_config_string(config_section, "hostname", sync_config.hostname);
    strncpy(sync_config.hostname, hostname, sizeof(sync_config.hostname) - 1);
    sync_config.hostname[sizeof(sync_config.hostname) - 1] = '\0';

    const char* access_str = get_config_string(config_section, "access", NULL);
    if (access_str) {
        if (strcmp(access_str, "read") == 0) {
            sync_config.access = PLATFORM_SHM_READ;
        } else if (strcmp(access_str, "write") == 0) {
            sync_config.access = PLATFORM_SHM_WRITE;
        } else if (strcmp(access_str, "readwrite") == 0) {
            sync_config.access = PLATFORM_SHM_READWRITE;
        }
    }

    sync_config.size = (size_t)get_config_int(config_section, "size", (int)sync_config.size);
    sync_config.port = (uint16_t)get_config_int(config_section, "port", sync_config.port);
    sync_config.create = get_config_bool(config_section, "create", sync_config.create);
    sync_config.update_interval_ms = (unsigned int)get_config_int(config_section, "update_interval_ms", sync_config.update_interval_ms);
    sync_config.retry_count = (unsigned int)get_config_int(config_section, "retry_count", sync_config.retry_count);
    sync_config.retry_interval_ms = (unsigned int)get_config_int(config_section, "retry_interval_ms", sync_config.retry_interval_ms);

    // Initialize socket subsystem
    PlatformErrorCode err = platform_socket_init();
    if (err != PLATFORM_ERROR_SUCCESS) {
        char error_msg[256];
        platform_get_error_message_from_code(err, error_msg, sizeof(error_msg));
        logger_log(LOG_ERROR, "Failed to initialize socket subsystem: %d (%s)", err, error_msg);
        return err;
    }

    // Make sure socket handle is NULL before creating
    if (udp_socket != NULL) {
        logger_log(LOG_WARN, "UDP socket already initialized, closing existing socket");
        platform_socket_close(udp_socket);
        udp_socket = NULL;
    }

    // Set up UDP socket for both sending and receiving
    PlatformSocketOptions sock_opts = {
        .blocking = false,             // Non-blocking mode for receiver thread
        .recv_timeout_ms = 100,        // Short timeout for receive operations
        .reuse_address = true,         // Allow socket reuse
    };

    err = platform_socket_create(&udp_socket, false, &sock_opts); // false = UDP
    if (err != PLATFORM_ERROR_SUCCESS) {
        char error_msg[256];
        platform_get_error_message_from_code(err, error_msg, sizeof(error_msg));
        logger_log(LOG_ERROR, "Failed to create UDP socket: %d (%s)", err, error_msg);
        platform_socket_cleanup();
        return err;
    }
    
    log_socket_state("shared_memory_sync_init (after create)");

    // Add debug log to verify socket was created
    logger_log(LOG_DEBUG, "UDP socket created successfully: %p", (void*)udp_socket);

    // Bind socket to receive data
    PlatformSocketAddress bind_addr = {
        .port = sync_config.port,
        .is_ipv6 = false
    };
    strcpy(bind_addr.host, "0.0.0.0"); // Bind to all interfaces

    err = platform_socket_bind(udp_socket, &bind_addr);
    if (err != PLATFORM_ERROR_SUCCESS) {
        logger_log(LOG_ERROR, "Failed to bind UDP socket: %d", err);
        platform_socket_close(udp_socket);
        platform_socket_cleanup();
        return err;
    }

    // Set up remote address for sending
    remote_addr.is_ipv6 = false;
    remote_addr.port = sync_config.port;
    strncpy(remote_addr.host, sync_config.hostname, sizeof(remote_addr.host) - 1);
    remote_addr.host[sizeof(remote_addr.host) - 1] = '\0';

    // // If we're in write mode, initialize the shared memory
    // if (sync_config.access == SYNC_ACCESS_WRITE || sync_config.access == SYNC_ACCESS_READWRITE) {
    //     err = platform_shared_memory_open(
    //         &writer_shm,
    //         sync_config.name,
    //         sync_config.size,
    //         PLATFORM_SHM_READWRITE,
    //         sync_config.create
    //     );

    //     if (err != PLATFORM_ERROR_SUCCESS) {
    //         logger_log(LOG_ERROR, "Failed to open shared memory for writing: %d", err);
    //         platform_socket_close(udp_socket);
    //         platform_socket_cleanup();
    //         return err;
    //     }

    //     // Map memory
    //     err = platform_shared_memory_map(writer_shm, &writer_data);
    //     if (err != PLATFORM_ERROR_SUCCESS) {
    //         logger_log(LOG_ERROR, "Failed to map shared memory for writing: %d", err);
    //         platform_shared_memory_close(writer_shm);
    //         platform_socket_close(udp_socket);
    //         platform_socket_cleanup();
    //         return err;
    //     }

    //     logger_log(LOG_INFO, "Shared memory initialized for writing: %s (%zu bytes)",
    //               sync_config.name, sync_config.size);
    // }

    // // Send initial message if in read mode
    // if (sync_config.access == SYNC_ACCESS_READ || sync_config.access == SYNC_ACCESS_READWRITE) {
    //     // Request initial sync
    //     logger_log(LOG_INFO, "Sending INIT message to %s:%d", remote_addr.host, remote_addr.port);
    //     err = send_udp_message(SYNC_MSG_INIT, NULL, 0);
    //     if (err != PLATFORM_ERROR_SUCCESS) {
    //         logger_log(LOG_WARN, "Failed to send initial sync message: %d", err);
    //         // Continue anyway, as this is not fatal
    //     }
    // }

    logger_log(LOG_INFO, "Shared memory sync initialized successfully");
    log_socket_state("shared_memory_sync_init (end)");
    return PLATFORM_ERROR_SUCCESS;
}

void shared_memory_sync_shutdown(void) {
    log_socket_state("shared_memory_sync_shutdown (start)");
    
    // Clean up writer shared memory if open
    if (writer_shm) {
        // platform_shared_memory_unmap(writer_shm);
        // platform_shared_memory_close(writer_shm);
        writer_shm = NULL;
        writer_data = NULL;
    }

    // Clean up UDP socket
    if (udp_socket) {
        platform_socket_close(udp_socket);
        udp_socket = NULL;
    }

    // Clean up socket subsystem
    platform_socket_cleanup();
    
    log_socket_state("shared_memory_sync_shutdown (end)");
    logger_log(LOG_INFO, "Shared memory sync shutdown completed");
}

ThreadConfig* get_udp_listener_thread(void) {
    static ThreadConfig listener_thread = {
        .label = "UDP_LISTENER",
        .func = udp_listener_thread,
        .data = NULL,
        .suppressed = false
    };
    
    return &listener_thread;
}

void shared_memory_udp_sender_callback(const void* data, size_t size) {
    if (!data || size == 0 || !udp_socket) {
        logger_log(LOG_WARN, "Invalid parameters in udp_sender_callback: data=%p, size=%zu, socket=%p", 
                  data, size, (void*)udp_socket);
        return;
    }

    // Only send if we're in read mode
    if (sync_config.access != PLATFORM_SHM_READ && sync_config.access != PLATFORM_SHM_READWRITE) {
        logger_log(LOG_DEBUG, "Not sending update - access mode not READ or READWRITE (mode=%d)", 
                  sync_config.access);
        return;
    }

    // Send memory update
    logger_log(LOG_DEBUG, "Attempting to send memory update (%zu bytes)", size);
    PlatformErrorCode err = send_udp_message(SYNC_MSG_UPDATE, data, size);
    if (err != PLATFORM_ERROR_SUCCESS) {
        logger_log(LOG_WARN, "Failed to send memory update: error=%d", err);
    } else {
        logger_log(LOG_DEBUG, "Memory update sent successfully (%zu bytes)", size);
    }
}

static void* udp_listener_thread(void* arg) {
    ThreadConfig* thread_config = (ThreadConfig*)arg;
    
    logger_log(LOG_INFO, "UDP listener thread starting");
    log_socket_state("udp_listener_thread (start)");
    thread_registry_update_state(thread_config->label, THREAD_STATE_RUNNING);
    
    uint8_t buffer[8192];  // Buffer for receiving UDP messages
    
    // Loop until shutdown
    while (!shutdown_signalled()) {
        // Periodically log socket state
        static uint32_t log_counter = 0;
        if (++log_counter % 1000 == 0) {
            log_socket_state("udp_listener_thread (periodic)");
        }
        
        size_t bytes_received;
        PlatformSocketAddress sender;
        
        // Try to receive data
        // PlatformErrorCode err = platform_socket_receive_from(
            // udp_socket, buffer, sizeof(buffer), &bytes_received, &sender);
 
        if (false) {
        // if (err == PLATFORM_ERROR_SUCCESS && bytes_received > 0) {
            logger_log(LOG_DEBUG, "Received %zu bytes from %s:%d", 
                      bytes_received, sender.host, sender.port);
            
            // Process the received message
            if (bytes_received >= sizeof(SyncMessageHeader)) {
                SyncMessageHeader* header = (SyncMessageHeader*)buffer;
                
                switch (header->msg_type) {
                    case SYNC_MSG_INIT:
                        logger_log(LOG_INFO, "Received INIT message (seq: %u)", header->seq_num);
                        // If we're in READ mode, respond with full memory state
                        if (sync_config.access == PLATFORM_SHM_READ || 
                            sync_config.access == PLATFORM_SHM_READWRITE) {
                            // Get current memory and send full sync
                            // This would require opening and reading from shared memory
                            // For simplicity, we'll just send whatever data we have if in READ mode
                            // In real implementation, you'd open shared memory here and read current state
                        }
                        break;
                        
                    case SYNC_MSG_UPDATE:
                        logger_log(LOG_INFO, "Received UPDATE message (seq: %u)", header->seq_num);
                        // If we're in WRITE mode, apply the update
                        if (sync_config.access == PLATFORM_SHM_WRITE || 
                            sync_config.access == PLATFORM_SHM_READWRITE) {
                            // err = apply_memory_update(buffer, bytes_received);
                            // if (err != PLATFORM_ERROR_SUCCESS) {
                            //     logger_log(LOG_ERROR, "Failed to apply memory update: %d", err);
                            // }
                        }
                        break;
                        
                    case SYNC_MSG_ACK:
                        logger_log(LOG_DEBUG, "Received ACK message (seq: %u)", header->seq_num);
                        // Handle acknowledgment (could update statistics or retry queue)
                        break;
                        
                    case SYNC_MSG_HEARTBEAT:
                        logger_log(LOG_DEBUG, "Received HEARTBEAT message (seq: %u)", header->seq_num);
                        // For now, just log it
                        break;
                        
                    default:
                        logger_log(LOG_WARN, "Received unknown message type: %u", header->msg_type);
                        break;
                }
            } else {
                logger_log(LOG_WARN, "Received message too small to be valid");
            }
        // } else if (err != PLATFORM_ERROR_WOULD_BLOCK) {
        //     // Some error other than would block (timeout)
        //     logger_log(LOG_WARN, "Error receiving UDP data: %d", err);
        //     sleep_ms(100);  // Avoid spinning too fast on errors
        }
        
        // Short sleep to avoid CPU spinning (could be removed if using blocking mode)
        sleep_ms(10);
    }
    
    thread_registry_update_state(thread_config->label, THREAD_STATE_TERMINATED);
    log_socket_state("udp_listener_thread (end)");
    logger_log(LOG_INFO, "UDP listener thread stopping");
    return NULL;
}

static PlatformErrorCode apply_memory_update(const uint8_t* message, size_t message_size) {
    if (!message || message_size < sizeof(SyncMessageHeader) || !writer_shm || !writer_data) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    SyncMessageHeader* header = (SyncMessageHeader*)message;
    
    // Navigate to variable-length fields
    size_t name_offset = sizeof(SyncMessageHeader);
    size_t offset_field_offset = name_offset + header->name_len;
    size_t len_field_offset = offset_field_offset + sizeof(uint32_t);
    size_t data_offset = len_field_offset + sizeof(uint32_t);
    
    // Ensure message is large enough
    if (message_size < data_offset) {
        logger_log(LOG_WARN, "Message too small to contain expected fields");
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    // Get offset and length from message
    uint32_t offset = *(uint32_t*)(message + offset_field_offset);
    uint32_t data_len = *(uint32_t*)(message + len_field_offset);
    
    // Ensure offset and length are valid
    if (offset + data_len > sync_config.size) {
        logger_log(LOG_WARN, "Invalid memory update range: offset=%u, len=%u, max=%zu",
                  offset, data_len, sync_config.size);
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    // Ensure message contains enough data
    if (message_size < data_offset + data_len) {
        logger_log(LOG_WARN, "Message truncated, expected %zu bytes, got %zu",
                  data_offset + data_len, message_size);
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    // Apply the update
    memcpy((uint8_t*)writer_data + offset, message + data_offset, data_len);
         
    logger_log(LOG_DEBUG, "Applied memory update: offset=%u, len=%u", offset, data_len);
    return PLATFORM_ERROR_SUCCESS;
}

static PlatformErrorCode send_udp_message(SharedMemorySyncMessageType type, const void* data, size_t size) {
    log_socket_state("send_udp_message");
    
    if (!udp_socket) {
        logger_log(LOG_ERROR, "UDP socket not initialized in send_udp_message");
        return PLATFORM_ERROR_NOT_INITIALIZED;
    }
    
    // Check if socket is still valid
    bool is_connected = false;
    PlatformErrorCode err = platform_socket_is_connected(udp_socket, &is_connected);
    if (err != PLATFORM_ERROR_SUCCESS || !is_connected) {
        logger_log(LOG_ERROR, "UDP socket is not connected or invalid");
        return PLATFORM_ERROR_NOT_INITIALIZED;
    }
    
    // Log message details
    logger_log(LOG_DEBUG, "Preparing to send UDP message: type=%d, size=%zu, name='%s'", 
               type, size, sync_config.name);
    
    // Calculate total message size with header
    size_t name_len = strlen(sync_config.name);
    size_t header_size = sizeof(SyncMessageHeader) + name_len + sizeof(uint32_t) + sizeof(uint32_t);
    size_t total_size = header_size + size;
    
    logger_log(LOG_DEBUG, "Message details: name_len=%zu, header_size=%zu, total_size=%zu", 
               name_len, header_size, total_size);
    
    // Allocate buffer for message
    uint8_t* message = (uint8_t*)malloc(total_size);
    if (!message) {
        logger_log(LOG_ERROR, "Failed to allocate memory for UDP message (size=%zu)", total_size);
        return PLATFORM_ERROR_OUT_OF_MEMORY;
    }
    
    // Fill in header
    SyncMessageHeader* header = (SyncMessageHeader*)message;
    header->msg_type = type;
    header->seq_num = ++sequence_counter;  // Increment and assign sequence number
    header->name_len = (uint8_t)name_len;
    
    // Fill in block name
    memcpy(header->block_name, sync_config.name, name_len);
    
    // Fill in offset and data length
    uint32_t* offset_ptr = (uint32_t*)(message + sizeof(SyncMessageHeader) + name_len);
    *offset_ptr = 0;  // For now, always use offset 0 (full block update)
    
    uint32_t* len_ptr = offset_ptr + 1;
    *len_ptr = (uint32_t)size;
    
    // Fill in data (if provided)
    if (data && size > 0) {
        memcpy(message + header_size, data, size);
    }
    
    // Log destination details
    logger_log(LOG_DEBUG, "Sending to %s:%d, socket=%p", 
               sync_config.hostname, sync_config.port, (void*)udp_socket);
    
    // Send the message
    size_t sent;
    err = platform_socket_send(udp_socket, message, total_size, &sent);
    
    // Get detailed error information if send failed
    if (err != PLATFORM_ERROR_SUCCESS) {
        char error_msg[256];
        platform_get_error_message_from_code(err, error_msg, sizeof(error_msg));
        
        // Get system-specific error
        DWORD win_error = GetLastError();
        char win_error_msg[256];
        FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            win_error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            win_error_msg,
            sizeof(win_error_msg),
            NULL
        );
        
        logger_log(LOG_ERROR, "UDP send failed: error %d (%s)", err, error_msg);
        logger_log(LOG_ERROR, "System error: %d (%s)", win_error, win_error_msg);
        
        // Clean up buffer
        free(message);
        return err;
    }
    
    logger_log(LOG_DEBUG, "UDP send result: sent=%zu of %zu bytes", sent, total_size);
    
    // Clean up buffer
    free(message);
    
    if (sent != total_size) {
        logger_log(LOG_WARN, "Incomplete UDP send: %zu of %zu bytes", sent, total_size);
        return PLATFORM_ERROR_UNKNOWN;
    }
    
    return PLATFORM_ERROR_SUCCESS;
}

// Stub implementation - this would need to be properly implemented
void shared_memory_udp_sender_callback_with_offset(const void* data, size_t size, uint32_t offset) {
    if (!data || size == 0) {
        return;
    }

    // Only send if we're in read mode
    if (sync_config.access != PLATFORM_SHM_READ && sync_config.access != PLATFORM_SHM_READWRITE) {
        return;
    }

    // Send memory update with offset
    PlatformErrorCode err = send_udp_message(SYNC_MSG_UPDATE, data, size);
    if (err != PLATFORM_ERROR_SUCCESS) {
        logger_log(LOG_WARN, "Failed to send memory update: %d", err);
    } else {
        logger_log(LOG_DEBUG, "Memory update sent (%zu bytes at offset %u)", size, offset);
    }
}
