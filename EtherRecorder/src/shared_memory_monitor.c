
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shared_memory_monitor.h"
#include "shared_memory_sync.h"
#include "platform_shared_memory.h"
#include "platform_error.h"
#include "platform_time.h"
#include "platform_threads.h"
#include "platform_sockets.h"
#include "platform_string.h"
#include "platform_mutex.h"
#include "thread_registry.h"
#include "logger.h"
#include "app_config.h"
#include "shutdown_handler.h"

typedef struct {
    uint32_t offset;
    uint32_t length;
} ChangeRegion;

// Define a linked list node for change regions
typedef struct ChangeRegionNode {
    ChangeRegion region;
    struct ChangeRegionNode* next;
} ChangeRegionNode;

// Detect multiple distinct changed regions in memory using a linked list
static int detect_memory_changes(
    const void* current, 
    const void* previous, 
    size_t size, 
    ChangeRegionNode** regions_head
) {
    const uint8_t* curr_bytes = (const uint8_t*)current;
    const uint8_t* prev_bytes = (const uint8_t*)previous;
    int region_count = 0;
    bool in_region = false;
    uint32_t region_start = 0;
    
    // Initialize the head pointer to NULL
    *regions_head = NULL;
    ChangeRegionNode* last_node = NULL;
    
    // Scan through memory byte by byte
    for (uint32_t i = 0; i < size; i++) {
        bool is_different = (curr_bytes[i] != prev_bytes[i]);
        
        if (is_different && !in_region) {
            // Start of a new change region
            in_region = true;
            region_start = i;
        } 
        else if (!is_different && in_region) {
            // End of a change region
            in_region = false;
            uint32_t region_length = i - region_start;
            
            // Create a new node
            ChangeRegionNode* new_node = (ChangeRegionNode*)malloc(sizeof(ChangeRegionNode));
            if (!new_node) {
                logger_log(LOG_ERROR, "Failed to allocate memory for change region node");
                
                // Clean up any previously allocated nodes
                ChangeRegionNode* node = *regions_head;
                while (node) {
                    ChangeRegionNode* next = node->next;
                    free(node);
                    node = next;
                }
                
                *regions_head = NULL;
                return -1;  // Error condition
            }
            
            // Initialize the new node
            new_node->region.offset = region_start;
            new_node->region.length = region_length;
            new_node->next = NULL;
            
            // Add to the list
            if (!*regions_head) {
                *regions_head = new_node;
            } else if (last_node) {
                last_node->next = new_node;
            }
            
            last_node = new_node;
            region_count++;
        }
    }
    
    // Handle case where the last region extends to the end of the buffer
    if (in_region) {
        uint32_t region_length = (uint32_t)(size - region_start);
        
        // Create a new node
        ChangeRegionNode* new_node = (ChangeRegionNode*)malloc(sizeof(ChangeRegionNode));
        if (!new_node) {
            logger_log(LOG_ERROR, "Failed to allocate memory for change region node");
            
            // Clean up any previously allocated nodes
            ChangeRegionNode* node = *regions_head;
            while (node) {
                ChangeRegionNode* next = node->next;
                free(node);
                node = next;
            }
            
            *regions_head = NULL;
            return -1;  // Error condition
        }
        
        // Initialize the new node
        new_node->region.offset = region_start;
        new_node->region.length = region_length;
        new_node->next = NULL;
        
        // Add to the list
        if (!*regions_head) {
            *regions_head = new_node;
        } else if (last_node) {
            last_node->next = new_node;
        }
        
        region_count++;
    }
    
    return region_count;
}

// // Worker thread state
// typedef struct {
//     PlatformThreadId thread_id;
//     PlatformMutex_T mutex;
//     PlatformCondition_T condition;
//     bool shutdown;
//     bool busy;
//     const void* data;
//     size_t data_size;
//     void* context;
// } WorkerThreadState;

// // Shared memory monitor context
// typedef struct {
//     SharedMemoryMonitorConfig config;
//     PlatformSharedMemoryHandle handle;
//     void* mapped_data;
//     void* last_data;
//     size_t actual_size;
    
//     // For network forwarding
//     PlatformSocketHandle socket_handle;
//     bool socket_connected;
    
//     // Worker thread pool
//     WorkerThreadState* workers;
//     int num_workers;
//     int next_worker;
//     PlatformMutex_T mutex;
// } MonitorContext;

// Default configuration
static SharedMemoryMonitorConfig default_monitor_config = {
    .name = "TestSharedMemory",
    .create = true,
    .data_size = 0,                 // Auto-detect size
    .access = PLATFORM_SHM_READ,   // Read-only access by default
    .interval_ms = 100,           // Check every 10th of a second
    .retry_interval_ms = 2000,     // Retry every 2 seconds
    .max_retries = 5,              // 5 retries by default
    .wait_indefinitely = true,     // Wait indefinitely by default
    // .num_threads = 2,              // Use 2 worker threads by default
    // .forwarding = {
    //     .hostname = "localhost",
    //     .port = 5000,
    //     .use_tcp = false,         // Default to UDP
    //     .retry_count = 3,
    //     .retry_interval_ms = 500
    // }
};

// Initialize a shared memory monitor configuration with defaults
PlatformErrorCode shared_memory_monitor_init_config(SharedMemoryMonitorConfig* config) {
    if (!config) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    // Start with default configuration
    *config = default_monitor_config;
    
    // Initialize socket fields
    config->socket = NULL;
    config->socket_initialized = false;
    
    return PLATFORM_ERROR_SUCCESS;
}

// Open and map shared memory
static PlatformErrorCode open_and_map_shared_memory(
    SharedMemoryMonitorConfig* config,
    PlatformSharedMemoryHandle* handle,
    void** mapped_data
) {
    logger_log(LOG_INFO, "Attempting to open shared memory '%s' (create=%s, access=%d, size=%zu)", 
              config->name, config->create ? "true" : "false", config->access, config->data_size);
    
    PlatformErrorCode result = platform_shared_memory_open(
        handle,
        config->name,
        config->data_size,
        config->access,
        config->create
    );
    
    if (result != PLATFORM_ERROR_SUCCESS) {
        char error_msg[256];
        platform_get_error_message_from_code(result, error_msg, sizeof(error_msg));
        logger_log(LOG_ERROR, "Failed to open shared memory '%s': error %d (%s)", 
                 config->name, result, error_msg);
        
        // Get Windows-specific error
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
        logger_log(LOG_ERROR, "Windows error: %d (%s)", win_error, win_error_msg);
        
        return result;
    }
    
    // If auto-detect size was requested (data_size == 0), get the actual size
    if (config->data_size == 0) {
        size_t actual_size = 0;
        result = platform_shared_memory_get_size(*handle, &actual_size);
        if (result != PLATFORM_ERROR_SUCCESS) {
            logger_log(LOG_ERROR, "Failed to get shared memory size for '%s': error %d", 
                     config->name, result);
            platform_shared_memory_close(*handle);
            *handle = NULL;
            return result;
        }
        
        config->data_size = actual_size;
        logger_log(LOG_INFO, "Auto-detected shared memory size for '%s': %zu bytes", 
                 config->name, config->data_size);
    }
    
    // Map the shared memory
    result = platform_shared_memory_map(*handle, mapped_data);
    if (result != PLATFORM_ERROR_SUCCESS) {
        logger_log(LOG_ERROR, "Failed to map shared memory '%s': error %d", 
                 config->name, result);
        platform_shared_memory_close(*handle);
        *handle = NULL;
        return result;
    }
    
    logger_log(LOG_INFO, "Successfully opened and mapped shared memory '%s' (%zu bytes)", 
             config->name, config->data_size);
    
    return PLATFORM_ERROR_SUCCESS;
}

// Initialize socket for a shared memory monitor
static PlatformErrorCode initialize_monitor_socket(SharedMemoryMonitorConfig* config) {
    if (!config) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    logger_log(LOG_DEBUG, "Initializing socket for '%s'", config->name);
    
    // If socket already initialized, close it first
    if (config->socket_initialized && config->socket) {
        logger_log(LOG_DEBUG, "Closing existing socket for '%s'", config->name);
        platform_socket_close(config->socket);
        config->socket = NULL;
        config->socket_initialized = false;
    }
    
    // Set up UDP socket
    PlatformSocketOptions sock_opts = {0}; // Initialize all fields to zero first
    sock_opts.blocking = false;             // Non-blocking mode
    sock_opts.recv_timeout_ms = 100;        // Short timeout for receive operations
    sock_opts.reuse_address = true;         // Allow socket reuse
    
    logger_log(LOG_DEBUG, "Creating UDP socket for '%s' with options: blocking=%d, timeout=%u, reuse=%d", 
              config->name, sock_opts.blocking, sock_opts.recv_timeout_ms, sock_opts.reuse_address);
    
    PlatformErrorCode err = platform_socket_create(&config->socket, false, &sock_opts); // false = UDP
    if (err != PLATFORM_ERROR_SUCCESS) {
        char error_msg[256];
        platform_get_error_message_from_code(err, error_msg, sizeof(error_msg));
        logger_log(LOG_ERROR, "Failed to create UDP socket for '%s': %d (%s)", 
                  config->name, err, error_msg);
        
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
        logger_log(LOG_ERROR, "System error: %d (%s)", win_error, win_error_msg);
        
        // After socket creation fails:
        logger_log(LOG_ERROR, "Socket creation details - handle: %p, config ptr: %p", 
                  (void*)config->socket, (void*)config);
        
        return err;
    }
    
    logger_log(LOG_DEBUG, "UDP socket created successfully for '%s': %p", 
              config->name, (void*)config->socket);
    
    // // Verify socket is valid
    // bool is_valid = false;
    // err = platform_socket_is_valid(config->socket, &is_valid);
    // if (err != PLATFORM_ERROR_SUCCESS || !is_valid) {
    //     logger_log(LOG_ERROR, "Socket created but not valid for '%s'", config->name);
    //     platform_socket_close(config->socket);
    //     config->socket = NULL;
    //     return PLATFORM_ERROR_INVALID_HANDLE;
    // }
    
    config->socket_initialized = true;
    return PLATFORM_ERROR_SUCCESS;
}

// Send memory update via UDP
static PlatformErrorCode send_memory_update(
    SharedMemoryMonitorConfig* config,
    const void* data,
    size_t size,
    uint32_t offset
) {
    if (!config || !data || size == 0) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    // Make sure socket is initialized
    if (!config->socket_initialized || !config->socket) {
        logger_log(LOG_WARN, "Socket not initialized for '%s', initializing now", config->name);
        PlatformErrorCode err = initialize_monitor_socket(config);
        if (err != PLATFORM_ERROR_SUCCESS) {
            logger_log(LOG_ERROR, "Failed to initialize socket, will retry next time");
            return err;
        }
    }
    
    // Define message header structure
    #pragma pack(push, 1)  // Ensure no padding in the structure
    typedef struct {
        uint8_t msg_type;
        uint16_t seq_num;
        uint8_t name_len;
        char block_name[64];  // Fixed size for simplicity
        uint32_t offset;
        uint32_t length;
    } MessageHeader;
    #pragma pack(pop)
    
    static uint16_t sequence_counter = 0;
    
    // Calculate total message size
    size_t header_size = sizeof(MessageHeader);
    size_t total_size = header_size + size;
    
    // Allocate buffer for message
    uint8_t* message = (uint8_t*)malloc(total_size);
    if (!message) {
        logger_log(LOG_ERROR, "Failed to allocate memory for UDP message (size=%zu)", total_size);
        return PLATFORM_ERROR_OUT_OF_MEMORY;
    }
    
    // Fill in header
    MessageHeader* header = (MessageHeader*)message;
    memset(header, 0, sizeof(MessageHeader));  // Initialize all fields to zero
    
    header->msg_type = SYNC_MSG_UPDATE;  // Use UPDATE message type
    header->seq_num = ++sequence_counter;
    
    // Set block name
    size_t name_len = strlen(config->name);
    if (name_len > sizeof(header->block_name) - 1) {
        name_len = sizeof(header->block_name) - 1;
    }
    header->name_len = (uint8_t)name_len;
    memcpy(header->block_name, config->name, name_len);
    // No need to null-terminate since we're using name_len
    
    header->offset = offset;
    header->length = (uint32_t)size;
    
    // Log the raw header bytes for debugging
    logger_log(LOG_DEBUG, "Header bytes: %02X %02X %02X %02X %02X %02X %02X %02X",
              message[0], message[1], message[2], message[3], 
              message[4], message[5], message[6], message[7]);
    
    // Fill in data
    memcpy(message + header_size, data, size);
    
    // Set up remote address
    PlatformSocketAddress remote_addr = {
        .port = config->forwarding.port,
        .is_ipv6 = false
    };
    strncpy(remote_addr.host, config->forwarding.hostname, sizeof(remote_addr.host) - 1);
    remote_addr.host[sizeof(remote_addr.host) - 1] = '\0';
    
    // Send the message
    size_t sent;
    PlatformErrorCode err = platform_socket_sendto(
        config->socket, 
        message, 
        total_size, 
        &remote_addr,
        &sent
    );
    
    // Log the message details
    logger_log(LOG_DEBUG, "Sending UDP message: type=%d, seq=%u, block='%s', offset=%u, len=%u, total_size=%zu",
              header->msg_type, header->seq_num, config->name, header->offset, header->length, total_size);
    
    // Clean up buffer
    free(message);
    
    // Check for errors
    if (err != PLATFORM_ERROR_SUCCESS) {
        char error_msg[256];
        platform_get_error_message_from_code(err, error_msg, sizeof(error_msg));
        logger_log(LOG_ERROR, "UDP send failed for '%s': error %d (%s)", 
                  config->name, err, error_msg);
        return err;
    }
    
    if (sent != total_size) {
        logger_log(LOG_WARN, "Incomplete UDP send for '%s': %zu of %zu bytes", 
                  config->name, sent, total_size);
        return PLATFORM_ERROR_UNKNOWN;
    }
    
    logger_log(LOG_DEBUG, "Memory update sent for '%s' (%zu bytes at offset %u)", 
              config->name, size, offset);
    
    return PLATFORM_ERROR_SUCCESS;
}

// Send full memory state via UDP
static PlatformErrorCode send_full_memory(
    SharedMemoryMonitorConfig* config,
    void* mapped_data
) {
    // Prepare message header with block name
    typedef struct {
        uint8_t msg_type;
        uint16_t seq_num;
        uint8_t name_len;
        char block_name[64];  // Fixed size for simplicity
        uint32_t offset;
        uint32_t length;
    } MessageHeader;
    
    static uint16_t sequence_counter = 0;
    
    // Calculate total message size
    size_t header_size = sizeof(MessageHeader);
    size_t total_size = header_size + config->data_size;
    
    // Allocate buffer for message
    uint8_t* message = (uint8_t*)malloc(total_size);
    if (!message) {
        logger_log(LOG_ERROR, "Failed to allocate memory for UDP message (size=%zu)", total_size);
        return PLATFORM_ERROR_OUT_OF_MEMORY;
    }
    
    // Fill in header
    MessageHeader* header = (MessageHeader*)message;
    header->msg_type = SYNC_MSG_INIT;  // Use INIT message type for full state
    header->seq_num = ++sequence_counter;
    
    // Set block name
    size_t name_len = strlen(config->name);
    if (name_len > sizeof(header->block_name) - 1) {
        name_len = sizeof(header->block_name) - 1;
    }
    header->name_len = (uint8_t)name_len;
    memcpy(header->block_name, config->name, name_len);
    header->block_name[name_len] = '\0';  // Ensure null termination
    
    header->offset = 0;
    header->length = (uint32_t)config->data_size;
    
    // Fill in data
    memcpy(message + header_size, mapped_data, config->data_size);
    
    // Set up remote address
    PlatformSocketAddress remote_addr = {
        .port = config->forwarding.port,
        .is_ipv6 = false
    };
    strncpy(remote_addr.host, config->forwarding.hostname, sizeof(remote_addr.host) - 1);
    remote_addr.host[sizeof(remote_addr.host) - 1] = '\0';
    
    // Send the message
    size_t sent;
    PlatformErrorCode err = platform_socket_sendto(
        config->socket, 
        message, 
        total_size, 
        &remote_addr,
        &sent
    );
    
    // Log the message details
    logger_log(LOG_DEBUG, "Sending UDP message: type=%d (INIT), seq=%u, block='%s', offset=%u, len=%u",
              header->msg_type, header->seq_num, header->block_name, header->offset, header->length);
    
    // Clean up buffer
    free(message);
    
    // Check for errors
    if (err != PLATFORM_ERROR_SUCCESS) {
        char error_msg[256];
        platform_get_error_message_from_code(err, error_msg, sizeof(error_msg));
        logger_log(LOG_ERROR, "UDP send failed for '%s': error %d (%s)", 
                  config->name, err, error_msg);
        return err;
    }
    
    if (sent != total_size) {
        logger_log(LOG_WARN, "Incomplete UDP send for '%s': %zu of %zu bytes", 
                  config->name, sent, total_size);
        return PLATFORM_ERROR_UNKNOWN;
    }
    
    return PLATFORM_ERROR_SUCCESS;
}

// Process and forward memory changes
static void process_memory_changes(
    SharedMemoryMonitorConfig* config,
    void* mapped_data,
    void* last_data,
    bool* first_read
) {
    // Check for changes in shared memory data
    if (*first_read) {
        // On first read, send entire memory block with INIT message type
        PlatformErrorCode result = send_full_memory(config, mapped_data);
        if (result == PLATFORM_ERROR_SUCCESS) {
            logger_log(LOG_DEBUG, "Sent initial full memory state (%zu bytes)", config->data_size);
        } else {
            logger_log(LOG_ERROR, "Failed to send initial full memory state: %d", result);
        }
        
        // Copy current state to comparison buffer
        memcpy(last_data, mapped_data, config->data_size);
        *first_read = false;
    } else {
        // Compare with last state to find changes
        ChangeRegionNode* regions_head = NULL;
        int num_regions = detect_memory_changes(
            mapped_data, 
            last_data, 
            config->data_size,
            &regions_head
        );
        
        if (num_regions > 0) {
            logger_log(LOG_DEBUG, "Detected %d changed regions in shared memory '%s'", 
                      num_regions, config->name);
            
            // Forward each changed region
            ChangeRegionNode* current = regions_head;
            while (current) {
                uint32_t offset = current->region.offset;
                uint32_t length = current->region.length;
                
                // Send the changed region
                send_memory_update(
                    config,
                    (uint8_t*)mapped_data + offset, 
                    length, 
                    offset
                );
                
                logger_log(LOG_DEBUG, "Forwarded changed region: offset=%u, length=%u", 
                         offset, length);
                
                current = current->next;
            }
            
            // Update our comparison buffer with the new state
            memcpy(last_data, mapped_data, config->data_size);
            
            // Free the linked list
            current = regions_head;
            while (current) {
                ChangeRegionNode* next = current->next;
                free(current);
                current = next;
            }
        }
    }
}

// Main thread function for monitoring shared memory
void* shared_memory_monitor_thread(void* arg) {
    ThreadConfig* thread_config = (ThreadConfig*)arg;
    if (!thread_config) {
        logger_log(LOG_ERROR, "Invalid thread configuration");
        return NULL;
    }
    
    SharedMemoryMonitorConfig* config = (SharedMemoryMonitorConfig*)thread_config->data;
    if (!config) {
        logger_log(LOG_ERROR, "Invalid shared memory configuration");
        return NULL;
    }
    
    int block_index = config->block_index;
    
    logger_log(LOG_INFO, "Block %d monitor thread starting for '%s'", 
              block_index, config->name);
    
    // Log the configuration details
    logger_log(LOG_INFO, "Configuration for block %d:", block_index);
    logger_log(LOG_INFO, "  Name: %s", config->name);
    logger_log(LOG_INFO, "  Size: %zu bytes", config->data_size);
    logger_log(LOG_INFO, "  Access: %d", config->access);
    logger_log(LOG_INFO, "  Create: %s", config->create ? "true" : "false");
    logger_log(LOG_INFO, "  Interval: %u ms", config->interval_ms);
    logger_log(LOG_INFO, "  Network: %s:%d", 
             config->forwarding.hostname, config->forwarding.port);
    
    // Validate configuration
    if (config->create && config->data_size == 0) {
        logger_log(LOG_ERROR, "Cannot create shared memory with zero size for block %d", block_index);
        return NULL;
    }
    
    // Initialize socket for this monitor
    if (config->access == PLATFORM_SHM_READ || config->access == PLATFORM_SHM_READWRITE) {
        PlatformErrorCode socket_result = initialize_monitor_socket(config);
        if (socket_result != PLATFORM_ERROR_SUCCESS) {
            logger_log(LOG_ERROR, "Failed to initialize socket for block %d: %d", 
                     block_index, socket_result);
            // Continue anyway, we'll try again when needed
        }
    }
    
    PlatformSharedMemoryHandle shm_handle = NULL;
    void* mapped_data = NULL;
    unsigned int retry_count = 0;
    
    // Main loop - try to open shared memory and monitor it
    while (!shutdown_signalled()) {
        // If we don't have an open handle, try to open it
        if (shm_handle == NULL) {
            PlatformErrorCode result = open_and_map_shared_memory(config, &shm_handle, &mapped_data);
            
            if (result != PLATFORM_ERROR_SUCCESS) {
                retry_count++;
                logger_log(LOG_WARN, "Failed to open shared memory '%s' (attempt %u): error %d", 
                         config->name, retry_count, result);
                
                // Wait before retrying
                sleep_ms(config->retry_interval_ms);
                continue;
            }
            
            // Reset retry count on success
            retry_count = 0;
        }
        
        // At this point, we have successfully opened and mapped the shared memory
        logger_log(LOG_DEBUG, "Block %d monitor thread for '%s' is alive", 
                 block_index, config->name);
        
        // Allocate buffer for last data to detect changes if we're in read mode
        void* last_data = NULL;
        bool should_detect_changes = (config->access == PLATFORM_SHM_READ || 
                                     config->access == PLATFORM_SHM_READWRITE);

        if (should_detect_changes) {
            last_data = malloc(config->data_size);
            if (!last_data) {
                logger_log(LOG_ERROR, "Failed to allocate memory for data comparison");
                platform_shared_memory_unmap(shm_handle);
                platform_shared_memory_close(shm_handle);
                shm_handle = NULL;
                continue;
            }
            
            // Initialize with zeros
            memset(last_data, 0, config->data_size);
        }

        // Main monitoring loop
        bool first_read = true;
        PlatformMutex_T shm_mutex;
        platform_mutex_init(&shm_mutex);

        while (!shutdown_signalled()) {
            // Lock shared memory for reading
            PlatformErrorCode lock_result = platform_mutex_lock(&shm_mutex);
            if (lock_result != PLATFORM_ERROR_SUCCESS) {
                logger_log(LOG_WARN, "Failed to lock shared memory for reading: %d", lock_result);
                sleep_ms(config->interval_ms);
                continue;
            }
            
            if (should_detect_changes) {
                process_memory_changes(config, mapped_data, last_data, &first_read);
            } else {
                // For write-only mode, we don't need to send updates
                logger_log(LOG_DEBUG, "Write-only mode, not sending updates");
            }
            
            // Unlock shared memory
            platform_mutex_unlock(&shm_mutex);
            
            // Sleep for the configured interval
            sleep_ms(config->interval_ms);
        }

        // Clean up mutex
        platform_mutex_destroy(&shm_mutex);

        // Clean up
        if (last_data) {
            free(last_data);
            last_data = NULL;
        }
    }
    
    // Clean up resources
    if (mapped_data != NULL) {
        platform_shared_memory_unmap(shm_handle);
    }
    
    if (shm_handle != NULL) {
        platform_shared_memory_close(shm_handle);
    }
    
    logger_log(LOG_INFO, "Block %d monitor thread shutting down for '%s'", 
              block_index, config->name);
    
    return NULL;
}
