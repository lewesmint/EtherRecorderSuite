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
#include "udp_socket_wrapper.h" 
#include "utils.h"


// Message header structure - fixed portion only
#pragma pack(1)
typedef struct {
    uint8_t msg_type;        // Message type from SharedMemorySyncMessageType
    uint16_t seq_num;        // Sequence number for ordering/reliability
    uint8_t name_len;        // Length of block name
    // Variable-length fields follow in the message buffer:
    // 1. char block_name[name_len]  - Variable-length block name
    // 2. uint32_t offset            - Offset in memory block
    // 3. uint32_t data_len          - Length of data
    // 4. uint8_t data[data_len]     - Variable-length data
} SyncMessageHeader;
#pragma pack()

/*
 * Complete message layout in memory:
 * +-------------------+------------------+------------+------------+--------------+
 * | SyncMessageHeader | block_name       | offset     | data_len   | data         |
 * | (4 bytes)         | (name_len bytes) | (4 bytes)  | (4 bytes)  | (data_len)   |
 * +-------------------+------------------+------------+------------+--------------+
 *
 * We access the variable-length fields by calculating offsets from the start of the buffer.
 */

// Process received message and apply to shared memory
static PlatformErrorCode process_received_message(uint8_t* buffer, size_t bytes_received, SharedMemoryListenerData* data) {
    if (!buffer || bytes_received < sizeof(SyncMessageHeader) || !data) {
        logger_log(LOG_ERROR, "Invalid parameters for process_received_message");
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    // Parse the fixed-size header
    SyncMessageHeader* header = (SyncMessageHeader*)buffer;
    
    // Log message type
    logger_log(LOG_DEBUG, "Processing message: type=%d, seq=%u, name_len=%u", 
              header->msg_type, header->seq_num, header->name_len);
    
    // Calculate offsets for variable-length fields
    size_t name_offset = sizeof(SyncMessageHeader);
    size_t name_len = header->name_len;
    
    // Ensure message is large enough to contain the name
    if (bytes_received < name_offset + name_len) {
        logger_log(LOG_WARN, "Message too small to contain block name");
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    // Extract block name
    char block_name[65] = {0}; // Max 64 chars + null terminator
    if (name_len > 64) name_len = 64;
    memcpy(block_name, buffer + name_offset, name_len);
    block_name[name_len] = '\0';
    
    logger_log(LOG_DEBUG, "Message for block: '%s'", block_name);
    
    // Check if this message is for our block
    if (strcmp(block_name, data->name) != 0) {
        logger_log(LOG_INFO, "Ignoring message for different block: '%s' (ours is '%s')", 
                  block_name, data->name);
        return PLATFORM_ERROR_SUCCESS; // Not an error, just not for us
    }
    
    // Calculate offsets for remaining fields
    size_t offset_field_offset = name_offset + name_len;
    size_t len_field_offset = offset_field_offset + sizeof(uint32_t);
    size_t data_offset = len_field_offset + sizeof(uint32_t);
    
    // Ensure message is large enough to contain offset and length fields
    if (bytes_received < len_field_offset + sizeof(uint32_t)) {
        logger_log(LOG_WARN, "Message too small to contain offset and length fields");
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    // Extract offset and data length
    uint32_t offset = *(uint32_t*)(buffer + offset_field_offset);
    uint32_t data_len = *(uint32_t*)(buffer + len_field_offset);
    
    logger_log(LOG_DEBUG, "Update details: offset=%u, length=%u", offset, data_len);
    
    // Validate offset and length
    if (offset + data_len > data->data_size) {
        logger_log(LOG_WARN, "Invalid memory update range: offset=%u, len=%u, max=%zu",
                  offset, data_len, data->data_size);
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    // Ensure message contains enough data
    if (bytes_received < data_offset + data_len) {
        logger_log(LOG_WARN, "Message truncated, expected %zu bytes, got %zu",
                  data_offset + data_len, bytes_received);
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    // Apply the update to our shared memory
    if (data->mapped_data) {
        // Add more detailed logging before and after the memory update
        logger_log(LOG_INFO, "About to apply memory update: offset=%u, len=%u to block '%s'", 
                  offset, data_len, data->name);
                  
        // Log first few bytes before update
        if (data_len > 0) {
            char before_hex[128] = {0};
            size_t bytes_to_log = data_len > 16 ? 16 : data_len;
            for (size_t i = 0; i < bytes_to_log; i++) {
                char hex[4];
                snprintf(hex, sizeof(hex), "%02X ", ((uint8_t*)data->mapped_data + offset)[i]);
                strcat(before_hex, hex);
            }
            logger_log(LOG_DEBUG, "Before update (first %zu bytes): %s", bytes_to_log, before_hex);
        }
        
        // Perform the actual memory update
        memcpy((uint8_t*)data->mapped_data + offset, buffer + data_offset, data_len);
        
        // Log first few bytes after update
        if (data_len > 0) {
            char after_hex[128] = {0};
            size_t bytes_to_log = data_len > 16 ? 16 : data_len;
            for (size_t i = 0; i < bytes_to_log; i++) {
                char hex[4];
                snprintf(hex, sizeof(hex), "%02X ", ((uint8_t*)data->mapped_data + offset)[i]);
                strcat(after_hex, hex);
            }
            logger_log(LOG_DEBUG, "After update (first %zu bytes): %s", bytes_to_log, after_hex);
        }
        
        logger_log(LOG_INFO, "Successfully applied memory update: offset=%u, len=%u to block '%s'", 
                  offset, data_len, data->name);
    } else {
        logger_log(LOG_ERROR, "Cannot apply update - no mapped memory");
        return PLATFORM_ERROR_NOT_INITIALIZED;
    }
    
    return PLATFORM_ERROR_SUCCESS;
}

// Global configuration
static SharedMemoryMonitorConfig sync_config = {0};  // Initialize to zeros

// Communication state
static PlatformSocketHandle udp_socket = NULL;
static PlatformSocketAddress remote_addr;
static uint16_t sequence_counter = 0;

// Shared memory state for writer
static PlatformSharedMemoryHandle writer_shm = NULL;
static void* writer_data = NULL;

// Forward declarations
static void* shared_memory_listener_thread(void* arg);
static PlatformErrorCode apply_memory_update(const uint8_t* message, size_t message_size);
static PlatformErrorCode send_udp_message(SharedMemorySyncMessageType type, const void* data, size_t size);

// Add this function to check and log socket state
static void log_socket_state(const char* function_name) {
    logger_log(LOG_DEBUG, "[%s] UDP socket state: %p", function_name, (void*)udp_socket);
    
    // Log the configuration we're using
    if (udp_socket) {
        PlatformSocketAddress local_addr;
        if (platform_socket_get_local_address(udp_socket, &local_addr) == PLATFORM_ERROR_SUCCESS) {
            logger_log(LOG_INFO, "[%s] Socket bound to %s:%d", 
                      function_name, local_addr.host, local_addr.port);
        } else {
            logger_log(LOG_INFO, "[%s] Configured to listen on port %d", 
                      function_name, sync_config.forwarding.listen_port);
        }
    }
}

// Initialize socket for shared memory sync
static PlatformErrorCode initialize_sync_socket(void) {
    logger_log(LOG_DEBUG, "[initialize_sync_socket] Starting with listen_port=%d, forward_port=%d",
              sync_config.forwarding.listen_port, sync_config.forwarding.forward_port);
    
    // Make sure socket handle is NULL before creating
    if (udp_socket != NULL) {
        logger_log(LOG_WARN, "UDP socket already initialized, closing existing socket");
        platform_socket_close(udp_socket);
        udp_socket = NULL;
    }

    // Set up UDP socket for both sending and receiving
    PlatformSocketOptions sock_opts = {
        .blocking = true,              // Use blocking mode with select
        .recv_timeout_ms = 1000,       // Longer timeout for blocking operations
        .reuse_address = true,         // Allow socket reuse
    };

    logger_log(LOG_DEBUG, "Creating UDP socket with options: blocking=%d, timeout=%u, reuse=%d", 
              sock_opts.blocking, sock_opts.recv_timeout_ms, sock_opts.reuse_address);

    PlatformErrorCode err = platform_socket_create(&udp_socket, false, &sock_opts); // false = UDP
    if (err != PLATFORM_ERROR_SUCCESS) {
        char error_msg[256];
        platform_get_error_message_from_code(err, error_msg, sizeof(error_msg));
        logger_log(LOG_ERROR, "Failed to create UDP socket: %d (%s)", err, error_msg);
        return err;
    }
    
    logger_log(LOG_DEBUG, "UDP socket created successfully: %p", (void*)udp_socket);

    // Bind socket to receive data
    PlatformSocketAddress bind_addr = {
        .port = sync_config.forwarding.listen_port,  // Use the forwarding.listen_port
        .is_ipv6 = false
    };
    strcpy(bind_addr.host, "0.0.0.0"); // Bind to all interfaces

    logger_log(LOG_DEBUG, "[initialize_sync_socket] Attempting to bind socket to port %d (from sync_config.forwarding.listen_port)", 
              sync_config.forwarding.listen_port);

    err = platform_socket_bind(udp_socket, &bind_addr);
    if (err != PLATFORM_ERROR_SUCCESS) {
        char error_msg[256];
        platform_get_error_message_from_code(err, error_msg, sizeof(error_msg));
        logger_log(LOG_ERROR, "Failed to bind UDP socket to port %d: %d (%s)", 
                  sync_config.forwarding.listen_port, err, error_msg);
        platform_socket_close(udp_socket);
        udp_socket = NULL;
        return err;
    }
    
    // Get the actual bound port to verify
    PlatformSocketAddress actual_addr = {0};
    err = platform_socket_get_local_address(udp_socket, &actual_addr);
    if (err == PLATFORM_ERROR_SUCCESS) {
        logger_log(LOG_INFO, "Socket actually bound to %s:%d", actual_addr.host, actual_addr.port);
    } else {
        logger_log(LOG_WARN, "Could not get actual bound address: %d", err);
    }
    
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode shared_memory_sync_init(const char* config_section) {
    logger_log(LOG_DEBUG, "[shared_memory_sync_init] Starting with config section: %s", config_section);
    log_socket_state("shared_memory_sync_init (start)");
    
    if (!config_section) {
        logger_log(LOG_ERROR, "NULL config_section passed to shared_memory_sync_init");
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    // Initialize with default values
    shared_memory_monitor_init_config(&sync_config);
    logger_log(LOG_DEBUG, "[shared_memory_sync_init] After init_config: listen_port=%d, forward_port=%d",
              sync_config.forwarding.listen_port, sync_config.forwarding.forward_port);

    // Read configuration from INI file
    const char* name = get_config_string(config_section, "name", "TestSharedMemory");
    strncpy(sync_config.name, name, sizeof(sync_config.name) - 1);
    sync_config.name[sizeof(sync_config.name) - 1] = '\0';

    const char* hostname = get_config_string(config_section, "hostname", "127.0.0.1");
    strncpy(sync_config.forwarding.hostname, hostname, sizeof(sync_config.forwarding.hostname) - 1);
    sync_config.forwarding.hostname[sizeof(sync_config.forwarding.hostname) - 1] = '\0';

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

    sync_config.data_size = (size_t)get_config_int(config_section, "size", 1024);
    
    // Read the listen_port and forward_port directly from the shared_memory section
    // This is the key change - we need to read from "shared_memory" section, not the block section
    char key[64];
    snprintf(key, sizeof(key), "%s.listen_port", config_section);
    int listen_port = get_config_int("shared_memory", key, sync_config.forwarding.listen_port);
    logger_log(LOG_DEBUG, "[shared_memory_sync_init] Read listen_port from shared_memory.%s: %d", 
              key, listen_port);
    sync_config.forwarding.listen_port = (uint16_t)listen_port;
    
    snprintf(key, sizeof(key), "%s.forward_port", config_section);
    int forward_port = get_config_int("shared_memory", key, sync_config.forwarding.listen_port);
    logger_log(LOG_DEBUG, "[shared_memory_sync_init] Read forward_port from shared_memory.%s: %d", 
              key, forward_port);
    sync_config.forwarding.forward_port = (uint16_t)forward_port;
    
    logger_log(LOG_INFO, "[shared_memory_sync_init] Final port configuration: listen_port=%d, forward_port=%d",
              sync_config.forwarding.listen_port, sync_config.forwarding.forward_port);

    sync_config.create = get_config_bool(config_section, "create", true);
    sync_config.interval_ms = (unsigned int)get_config_int(config_section, "update_interval_ms", 100);
    sync_config.forwarding.retry_count = (uint8_t)get_config_int(config_section, "retry_count", 3);
    sync_config.retry_interval_ms = (unsigned int)get_config_int(config_section, "retry_interval_ms", 500);

    logger_log(LOG_INFO, "Shared memory sync configuration loaded: name=%s, listen_port=%d, forward_port=%d",
              sync_config.name, sync_config.forwarding.listen_port, sync_config.forwarding.forward_port);

    // Initialize socket subsystem
    PlatformErrorCode err = platform_socket_init();
    if (err != PLATFORM_ERROR_SUCCESS) {
        char error_msg[256];
        platform_get_error_message_from_code(err, error_msg, sizeof(error_msg));
        logger_log(LOG_ERROR, "Failed to initialize socket subsystem: %d (%s)", err, error_msg);
        return err;
    }

    // Initialize socket
    err = initialize_sync_socket();
    if (err != PLATFORM_ERROR_SUCCESS) {
        platform_socket_cleanup();
        return err;
    }

    // Set up remote address for sending
    remote_addr.is_ipv6 = false;
    remote_addr.port = sync_config.forwarding.forward_port;
    strncpy(remote_addr.host, sync_config.forwarding.hostname, sizeof(remote_addr.host) - 1);
    remote_addr.host[sizeof(remote_addr.host) - 1] = '\0';

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

ThreadConfig* get_shared_memory_listener_thread(void) {
    // Allocate memory for the thread config and label
    ThreadConfig* listener_thread = (ThreadConfig*)malloc(sizeof(ThreadConfig));
    if (!listener_thread) {
        logger_log(LOG_ERROR, "Failed to allocate memory for listener thread config");
        return NULL;
    }
    
    char* thread_label = (char*)malloc(32);
    if (!thread_label) {
        logger_log(LOG_ERROR, "Failed to allocate memory for listener thread label");
        free(listener_thread);
        return NULL;
    }
    
    // Create thread name (block index will be set later)
    snprintf(thread_label, 32, "SM_LISTEN");
    
    // Initialize thread config
    *listener_thread = (ThreadConfig){
        .label = thread_label,
        .func = shared_memory_listener_thread,
        .data = NULL,  // Will be set by the caller
        .suppressed = false
    };
    
    return listener_thread;
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

// Backup of the UDP wrapper version
static void* shared_memory_listener_thread_udp_wrapper(void* arg) {
    ThreadConfig* config = (ThreadConfig*)arg;
    thread_registry_update_state(config->label, THREAD_STATE_RUNNING);
    
    logger_log(LOG_INFO, "Shared memory listener thread started (UDP wrapper version)");
    
    // Create a UDP socket for listening using the UDP wrapper
    UdpSocketHandle listen_socket = NULL;
    
    // Create the socket
    PlatformErrorCode err = udp_socket_create(&listen_socket);
    if (err != PLATFORM_ERROR_SUCCESS) {
        logger_log(LOG_ERROR, "Failed to create UDP socket: %d", err);
        thread_registry_update_state(config->label, THREAD_STATE_TERMINATED);
        return NULL;
    }
    
    // Note: UDP wrapper doesn't have a set_reuse_address function
    // We'll have to rely on the implementation's default behavior
    
    // Try a few different ports if the default is in use
    uint16_t port_to_try = 5002;
    const uint16_t max_port = 5010; // Try ports 5002-5010
    
    while (port_to_try <= max_port) {
        // Bind to port
        err = udp_socket_bind(listen_socket, port_to_try);
        if (err == PLATFORM_ERROR_SUCCESS) {
            // Successfully bound
            break;
        } else if (err == PLATFORM_ERROR_ADDRESS_IN_USE) {
            logger_log(LOG_WARN, "Port %d is already in use, trying next port", port_to_try);
            port_to_try++;
        } else {
            // Some other error
            logger_log(LOG_ERROR, "Failed to bind UDP socket to port %d: %d", port_to_try, err);
            udp_socket_close(listen_socket);
            thread_registry_update_state(config->label, THREAD_STATE_TERMINATED);
            return NULL;
        }
    }
    
    if (port_to_try > max_port) {
        logger_log(LOG_ERROR, "Failed to find an available port in range %d-%d", 5002, max_port);
        udp_socket_close(listen_socket);
        thread_registry_update_state(config->label, THREAD_STATE_TERMINATED);
        return NULL;
    }
    
    // Get the actual bound port to verify
    uint16_t bound_port = 0;
    if (udp_socket_get_bound_port(listen_socket, &bound_port) == PLATFORM_ERROR_SUCCESS) {
        logger_log(LOG_INFO, "UDP socket bound to port %d", bound_port);
    } else {
        logger_log(LOG_WARN, "Could not get bound port, assuming %d", port_to_try);
        bound_port = port_to_try;
    }
    
    // Buffer for receiving data
    uint8_t buffer[UDP_MAX_MESSAGE_SIZE];
    
    // Stats counters
    uint32_t packets_received = 0;
    uint64_t bytes_received_total = 0;
    uint64_t last_stats_time = get_time_ms();
    
    logger_log(LOG_INFO, "UDP wrapper listener ready on port %d", bound_port);
    
    // Main receive loop
    while (!shutdown_signalled()) {
        // Wait for data with timeout
        PlatformErrorCode wait_err = udp_socket_wait_for_data(listen_socket, 100);
        if (wait_err == PLATFORM_ERROR_TIMEOUT) {
            // No data available, just continue
            continue;
        } else if (wait_err != PLATFORM_ERROR_SUCCESS) {
            logger_log(LOG_WARN, "Error waiting for socket data: %d", wait_err);
            sleep_ms(100);
            continue;
        }
        
        // Data is available, receive it
        size_t bytes_received;
        UdpSocketAddress sender_addr;
        
        PlatformErrorCode recv_err = udp_socket_receive(
            listen_socket,
            buffer,
            sizeof(buffer),
            &bytes_received,
            &sender_addr
        );
        
        if (recv_err == PLATFORM_ERROR_SUCCESS && bytes_received > 0) {
            // Update stats
            packets_received++;
            bytes_received_total += bytes_received;
            
            // Log packet info
            logger_log(LOG_INFO, "Received %zu bytes from %s:%d (packet #%u)", 
                      bytes_received, sender_addr.host, sender_addr.port, packets_received);
            
            // Log first few bytes in hex for debugging
            char hex_buffer[128] = {0};
            size_t bytes_to_log = bytes_received > 16 ? 16 : bytes_received;
            for (size_t i = 0; i < bytes_to_log; i++) {
                char hex[4];
                snprintf(hex, sizeof(hex), "%02X ", buffer[i]);
                strcat(hex_buffer, hex);
            }
            logger_log(LOG_DEBUG, "First %zu bytes: %s", bytes_to_log, hex_buffer);
            
            // Try to interpret as text if it looks like ASCII
            bool is_text = true;
            for (size_t i = 0; i < bytes_received && i < 32; i++) {
                if (buffer[i] < 32 && buffer[i] != '\r' && buffer[i] != '\n' && buffer[i] != '\t') {
                    is_text = false;
                    break;
                }
            }
            
            if (is_text) {
                // Null-terminate the buffer for safe printing
                char text_buffer[33] = {0};
                size_t text_len = bytes_received < 32 ? bytes_received : 32;
                memcpy(text_buffer, buffer, text_len);
                logger_log(LOG_INFO, "Text content: %s", text_buffer);
            }
        } else if (recv_err != PLATFORM_ERROR_WOULD_BLOCK) {
            logger_log(LOG_WARN, "Error receiving UDP data: %d", recv_err);
            sleep_ms(100);
        }
        
        // Log stats periodically (every 5 seconds)
        uint64_t current_time = get_time_ms();
        if (current_time - last_stats_time > 5000) {
            logger_log(LOG_INFO, "UDP Stats: packets=%u, total bytes=%llu", 
                      packets_received, (unsigned long long)bytes_received_total);
            last_stats_time = current_time;
        }
    }
    
    // Clean up
    logger_log(LOG_INFO, "Shared memory listener thread stopping");
    udp_socket_close(listen_socket);
    thread_registry_update_state(config->label, THREAD_STATE_TERMINATED);
    return NULL;
}

// Reinstated version using Platform Socket Abstraction
static void* shared_memory_listener_thread(void* arg) {
    ThreadConfig* config = (ThreadConfig*)arg;
    thread_registry_update_state(config->label, THREAD_STATE_RUNNING);

    SharedMemoryListenerData* data = (SharedMemoryListenerData*)config->data;
    
    logger_log(LOG_INFO, "Shared memory listener thread started (Platform Socket version)");
    
    // Create a UDP socket for listening
    PlatformSocketHandle listen_socket = NULL;
    PlatformSocketOptions sock_opts = {
        .blocking = false,         // Non-blocking mode
        .recv_timeout_ms = 100,    // Short timeout
        .reuse_address = true      // Allow reuse
    };
    
    // Create the socket
    PlatformErrorCode err = platform_socket_create(&listen_socket, false, &sock_opts); // false = UDP
    if (err != PLATFORM_ERROR_SUCCESS) {
        logger_log(LOG_ERROR, "Failed to create UDP socket: %d", err);
        thread_registry_update_state(config->label, THREAD_STATE_TERMINATED);
        return NULL;
    }
    
    // Bind to port 5002
    PlatformSocketAddress bind_addr = {
        .port = 5002,
        .is_ipv6 = false
    };
    strcpy(bind_addr.host, "0.0.0.0"); // Bind to all interfaces
    
    err = platform_socket_bind(listen_socket, &bind_addr);
    if (err != PLATFORM_ERROR_SUCCESS) {
        logger_log(LOG_ERROR, "Failed to bind UDP socket to port 5002: %d", err);
        platform_socket_close(listen_socket);
        thread_registry_update_state(config->label, THREAD_STATE_TERMINATED);
        return NULL;
    }
    
    // Get the actual bound port to verify
    PlatformSocketAddress actual_addr = {0};
    err = platform_socket_get_local_address(listen_socket, &actual_addr);
    if (err == PLATFORM_ERROR_SUCCESS) {
        logger_log(LOG_INFO, "Socket bound to %s:%d", actual_addr.host, actual_addr.port);
    } else {
        logger_log(LOG_WARN, "Could not get actual bound address: %d", err);
    }
    
    // Buffer for receiving data
    uint8_t buffer[1400]; // Safe size below MTU
    
    // Stats counters
    uint32_t packets_received = 0;
    uint64_t bytes_received_total = 0;
    uint64_t last_stats_time = get_time_ms();
    
    logger_log(LOG_INFO, "UDP listener ready on port 5002");
    
    // Main receive loop
    while (!shutdown_signalled()) {
        // Use select to wait for data with a timeout
        PlatformErrorCode wait_err = platform_socket_wait_readable(listen_socket, 100);
        if (wait_err == PLATFORM_ERROR_TIMEOUT) {
            // No data available, just continue
            continue;
        } else if (wait_err != PLATFORM_ERROR_SUCCESS) {
            logger_log(LOG_WARN, "Error waiting for socket to be readable: %d", wait_err);
            sleep_ms(100);
            continue;
        }
        
        // Socket is readable, receive data
        size_t bytes_received;
        PlatformSocketAddress sender;
        
        PlatformErrorCode recv_err = platform_socket_recvfrom(
            listen_socket, 
            buffer, 
            sizeof(buffer), 
            &sender, 
            &bytes_received
        );
        
        if (recv_err == PLATFORM_ERROR_SUCCESS && bytes_received > 0) {
            // Update stats
            packets_received++;
            bytes_received_total += bytes_received;
            
            // Log packet info with more details
            logger_log(LOG_INFO, "Received %zu bytes from %s:%d (packet #%u)", 
                      bytes_received, sender.host, sender.port, packets_received);
            
            // Log first few bytes in hex for debugging
            char hex_buffer[128] = {0};
            size_t bytes_to_log = bytes_received > 16 ? 16 : bytes_received;
            for (size_t i = 0; i < bytes_to_log; i++) {
                char hex[4];
                snprintf(hex, sizeof(hex), "%02X ", buffer[i]);
                strcat(hex_buffer, hex);
            }
            logger_log(LOG_DEBUG, "First %zu bytes: %s", bytes_to_log, hex_buffer);
            
            // Process the received message
            if (bytes_received >= sizeof(SyncMessageHeader)) {
                // Extract message type for better logging
                SyncMessageHeader* header = (SyncMessageHeader*)buffer;
                const char* msg_type_str = "UNKNOWN";
                
                switch(header->msg_type) {
                    case SYNC_MSG_INIT: msg_type_str = "INIT"; break;
                    case SYNC_MSG_UPDATE: msg_type_str = "UPDATE"; break;
                    case SYNC_MSG_ACK: msg_type_str = "ACK"; break;
                    case SYNC_MSG_HEARTBEAT: msg_type_str = "HEARTBEAT"; break;
                }
                
                // Extract block name for better logging
                size_t name_offset = sizeof(SyncMessageHeader);
                size_t name_len = header->name_len;
                
                if (bytes_received >= name_offset + name_len) {
                    char block_name[65] = {0};
                    if (name_len > 64) name_len = 64;
                    memcpy(block_name, buffer + name_offset, name_len);
                    block_name[name_len] = '\0';
                    
                    logger_log(LOG_INFO, "Message details: Type=%s (%d), Seq=%u, Block='%s'", 
                              msg_type_str, header->msg_type, header->seq_num, block_name);
                } else {
                    logger_log(LOG_INFO, "Message details: Type=%s (%d), Seq=%u", 
                              msg_type_str, header->msg_type, header->seq_num);
                }
                
                PlatformErrorCode process_err = process_received_message(buffer, bytes_received, data);
                if (process_err != PLATFORM_ERROR_SUCCESS) {
                    logger_log(LOG_WARN, "Failed to process received message: %d", process_err);
                } else {
                    logger_log(LOG_INFO, "Successfully processed %s message", msg_type_str);
                }
            } else {
                logger_log(LOG_WARN, "Received message too small to be valid (%zu bytes)", bytes_received);
            }
        } else if (recv_err != PLATFORM_ERROR_WOULD_BLOCK) {
            logger_log(LOG_WARN, "Error receiving UDP data: %d", recv_err);
            sleep_ms(100);
        }
        
        // Log stats periodically (every 5 seconds)
        uint64_t current_time = get_time_ms();
        if (current_time - last_stats_time > 5000) {
            logger_log(LOG_INFO, "UDP Stats: packets=%u, total bytes=%llu", 
                      packets_received, (unsigned long long)bytes_received_total);
            last_stats_time = current_time;
        }
    }
    
    // Clean up
    logger_log(LOG_INFO, "Shared memory listener thread stopping");
    platform_socket_close(listen_socket);
    thread_registry_update_state(config->label, THREAD_STATE_TERMINATED);
    return NULL;
}

static PlatformErrorCode send_udp_message(SharedMemorySyncMessageType type, const void* data, size_t size) {
    if (!udp_socket) {
        logger_log(LOG_ERROR, "UDP socket not initialized");
        return PLATFORM_ERROR_NOT_INITIALIZED;
    }
    
    // Calculate total message size with header
    size_t name_len = strlen(sync_config.name);
    size_t header_size = sizeof(SyncMessageHeader);  // Fixed size for header only
    size_t total_size = header_size + name_len + sizeof(uint32_t) + sizeof(uint32_t) + size;  // Fixed size for header + name + offset + length + data
    
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
    
    // Fill in block name (after the header)
    memcpy(message + sizeof(SyncMessageHeader), sync_config.name, name_len);
    
    // Fill in offset and data length
    uint32_t* offset_ptr = (uint32_t*)(message + sizeof(SyncMessageHeader) + name_len);
    *offset_ptr = 0;  // For now, always use offset 0 (full block update)
    
    uint32_t* len_ptr = offset_ptr + 1;
    *len_ptr = (uint32_t)size;
    
    // Fill in data (if provided)
    if (data && size > 0) {
        memcpy(message + header_size + name_len + sizeof(uint32_t) + sizeof(uint32_t), data, size);
    }
    
    // Set up remote address
    remote_addr.port = sync_config.forwarding.forward_port;  // Use the forwarding.forward_port
    strncpy(remote_addr.host, sync_config.forwarding.hostname, sizeof(remote_addr.host) - 1);
    remote_addr.host[sizeof(remote_addr.host) - 1] = '\0';
    
    logger_log(LOG_DEBUG, "Sending to %s:%d", remote_addr.host, remote_addr.port);
    
    // Send the message
    size_t sent;
    PlatformErrorCode err = platform_socket_sendto(
        udp_socket, 
        message, 
        total_size, 
        &remote_addr,
        &sent
    );
    
    if (err != PLATFORM_ERROR_SUCCESS) {
        logger_log(LOG_ERROR, "Failed to send UDP message: %d", err);
        free(message);
        return err;
    }
    
    logger_log(LOG_DEBUG, "Sent UDP message: type=%d, seq=%u, size=%zu, sent=%zu", 
              type, header->seq_num, total_size, sent);
    
    free(message);
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
