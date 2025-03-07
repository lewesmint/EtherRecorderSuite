#ifndef COMM_THREADS_H
#define COMM_THREADS_H

#include "app_thread.h"
#include "message_queue.h"
#include "thread_group.h"
#include "platform_sockets.h"

// Configuration constants with sensible defaults
#define COMMS_BUFFER_SIZE         8192
#define SOCKET_ERROR_BUFFER_SIZE  256
#define BLOCKING_TIMEOUT_SEC      10  // Blocking timeout in seconds

/**
 * Utility macros for common operations
 */
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

/**
 * @brief Relay operation mode
 */
typedef enum {
    RELAY_MODE_NONE,       // No relay (normal operation)
    RELAY_MODE_PASSTHROUGH, // Pass messages through without modification
    RELAY_MODE_TRANSFORM,   // Transform messages according to rules
    RELAY_MODE_FILTER       // Filter messages according to rules
} RelayMode;

/**
 * @brief Transform a message according to relay rules
 * 
 * @param message Message to transform
 * @param mode Relay mode
 * @return true if transformation was successful, false otherwise
 */
bool transform_message(Message_T* message, RelayMode mode);

/**
 * @brief Communication thread group structure
 */
typedef struct {
    ThreadGroup base;              // Base thread group functionality
    SOCKET* socket;               // Socket handle
    bool is_relay_enabled;        // Relay mode enabled flag
    volatile long* connection_closed;  // Connection status flag
    char name[64];               // Group name
    // Removing relay_queue_x_to_y and relay_queue_y_to_x as we'll use thread queues
} CommsThreadGroup;

/**
 * @brief Structure to hold server manager thread arguments and functions.
 */
typedef struct {
    void* data;                          ///< Server Thread-specific data
    int port;                            ///< Port number
    bool is_tcp;                         ///< Protocol is TCP (else UDP)
    char server_hostname[100];           ///< Server hostname or IP address
    bool send_test_data;                 ///< Send test data flag
    int send_interval_ms;                ///< Send interval in milliseconds
    int data_size;                       ///< Size of data to send
} CommsThreadArgs_T;

/**
 * @brief Initialise a communication thread group
 * 
 * @param group Pointer to the communication thread group
 * @param name Group name
 * @param socket Pointer to the socket
 * @param connection_closed Pointer to the connection closed flag
 * @return bool true on success, false on failure
 */
bool comms_thread_group_init(CommsThreadGroup* group, const char* name, SOCKET* socket, volatile long* connection_closed);

/**
 * @brief Create send and receive threads for a connection
 * 
 * @param group Pointer to the communication thread group
 * @param client_addr Pointer to the client address structure
 * @param thread_info Pointer to thread info structure
 * @return bool true on success, false on failure
 */
bool comms_thread_group_create_threads(CommsThreadGroup* group, struct sockaddr_in* client_addr, CommsThreadArgs_T* thread_info);

/**
 * @brief Check if a communication connection is closed
 * 
 * @param group Pointer to the communication thread group
 * @return bool true if the connection is closed, false otherwise
 */
bool comms_thread_group_is_closed(CommsThreadGroup* group);

/**
 * @brief Mark a communication connection as closed
 * 
 * @param group Pointer to the communication thread group
 */
void comms_thread_group_close(CommsThreadGroup* group);

/**
 * @brief Wait for all communication threads to complete
 * 
 * @param group Pointer to the communication thread group
 * @param timeout_ms Maximum time to wait in milliseconds
 * @return bool true if all threads completed, false on timeout or error
 */
bool comms_thread_group_wait(CommsThreadGroup* group, uint32_t timeout_ms);

/**
 * @brief Clean up communication thread group resources
 * 
 * @param group Pointer to the communication thread group
 */
void comms_thread_group_cleanup(CommsThreadGroup* group);

/**
 * Checks if the communication threads show signs of activity or if the connection is alive.
 * 
 * @param group Pointer to the communication thread group
 * @return true if the connection shows signs of being active/healthy, false otherwise
 */
bool comms_thread_has_activity(CommsThreadGroup* group);

typedef struct {
    SOCKET* sock;
    struct sockaddr_in client_addr;
    CommsThreadArgs_T* thread_info;
    CommsThreadGroup* comm_group;
    MessageQueue_T* queue;
    PlatformThread_T thread_id;
} CommArgs_T;

#endif // COMM_THREADS_H
