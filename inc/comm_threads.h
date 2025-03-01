#ifndef COMM_THREADS_H
#define COMM_THREADS_H

// #include "common_socket.h"
#include "app_thread.h"
#include "message_queue.h"
#include "thread_group.h"
#include "platform_sockets.h"

// Configuration constants with sensible defaults
#define COMMS_BUFFER_SIZE         8192
#define SOCKET_ERROR_BUFFER_SIZE  256
#define BLOCKING_TIMEOUT_SEC      10  // Blocking timeout in seconds

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
 * @brief Structure for client and server communication arguments
 * 
 * This structure contains all the information needed for send and receive
 * threads to communicate over a socket.
 */
typedef struct {
    SOCKET* sock;                  // Pointer to socket (allows updating from owner thread)
    struct sockaddr_in client_addr; // Client address information
    void* thread_info;             // Could be ClientThreadArgs_T or ServerThreadArgs_T
    volatile LONG* connection_closed; // Atomic flag indicating connection state
    
    // Relay functionality
    MessageQueue_T* incoming_queue; // Queue for incoming messages to be processed
    MessageQueue_T* outgoing_queue; // Queue for outgoing messages to be sent
    RelayMode relay_mode;          // Relay operation mode
    bool disconnect_on_peer_close; // Whether to disconnect when the other side disconnects
} CommArgs_T;

/**
 * @brief Structure to hold common communications thread arguments and settings
 */
typedef struct CommsThreadArgs_T {
    void* data;                   // Thread-specific data
    int data_size;                // Data size
    bool send_test_data;          // Send test data flag
    int send_interval_ms;         // Send interval in milliseconds
    char server_hostname[100];    // Server hostname or IP address
    uint16_t port;                // Port number
    bool is_tcp;                  // Protocol is TCP (else UDP)
    
    // Relay configuration
    RelayMode relay_mode;         // Relay operation mode
    bool is_relay;                // Whether this is part of a relay
    uint32_t max_queue_size;      // Maximum size of message queues (0 for unlimited)
    uint32_t relay_timeout_ms;    // Timeout for relay operations
    bool preserve_connection;     // Preserve connection when peer disconnects
} CommsThreadArgs_T;

/**
 * @brief Thread function for receiving data
 * 
 * This function handles receiving data from a socket. It runs in a dedicated
 * thread and continues until the connection is closed or an error occurs.
 * 
 * @param arg Thread arguments (CommArgs_T*)
 * @return void* NULL
 */
void* receive_thread_function(void* arg);

/**
 * @brief Thread function for sending data
 * 
 * This function handles sending data to a socket. It runs in a dedicated
 * thread and continues until the connection is closed or an error occurs.
 * 
 * @param arg Thread arguments (CommArgs_T*)
 * @return void* NULL
 */
void* send_thread_function(void* arg);

/**
 * @brief Initialize relay functionality
 * 
 * Sets up message queues and establishes relay connections between
 * client and server threads.
 * 
 * @param client_args Client communication arguments
 * @param server_args Server communication arguments
 * @param client_to_server_queue Queue for messages from client to server
 * @param server_to_client_queue Queue for messages from server to client
 * @param relay_config Relay configuration settings
 * @return true if initialization was successful, false otherwise
 */
bool init_relay(
    CommArgs_T* client_args,
    CommArgs_T* server_args,
    MessageQueue_T* client_to_server_queue,
    MessageQueue_T* server_to_client_queue
);

/**
 * @brief Create a message from received data
 * 
 * @param buffer Source buffer containing received data
 * @param length Length of data in the buffer
 * @param msg_type Type of message to create
 * @param is_relay Whether this message should be relayed
 * @return Message_T Created message
 */
Message_T create_message(const char* buffer, uint32_t length, MessageType msg_type, bool is_relay);

/**
 * @brief Transform a message according to relay rules
 * 
 * @param message Message to transform
 * @param mode Relay mode
 * @return true if transformation was successful, false otherwise
 */
bool transform_message(Message_T* message, RelayMode mode);

/**
 * @brief Communication thread group for managing send/receive threads
 */
typedef struct CommsThreadGroup {
    ThreadGroup base;              // Base thread group
    SOCKET* socket;                // Socket pointer (owned by parent thread)
    volatile LONG* connection_closed; // Connection closed flag (owned by parent thread)
    MessageQueue_T* incoming_queue; // Queue for incoming messages
    MessageQueue_T* outgoing_queue; // Queue for outgoing messages
} CommsThreadGroup;

/**
 * @brief Initialize a communication thread group
 * 
 * @param group Pointer to the communication thread group
 * @param name Group name
 * @param socket Pointer to the socket
 * @param connection_closed Pointer to the connection closed flag
 * @return bool true on success, false on failure
 */
bool comms_thread_group_init(CommsThreadGroup* group, const char* name, SOCKET* socket, volatile LONG* connection_closed);

/**
 * @brief Create send and receive threads for a connection
 * 
 * @param group Pointer to the communication thread group
 * @param client_addr Pointer to the client address structure
 * @param thread_info Pointer to thread info structure
 * @return bool true on success, false on failure
 */
bool comms_thread_group_create_threads(CommsThreadGroup* group, struct sockaddr_in* client_addr, void* thread_info);

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
bool comms_thread_group_wait(CommsThreadGroup* group, DWORD timeout_ms);

/**
 * @brief Clean up communication thread group resources
 * 
 * @param group Pointer to the communication thread group
 */
void comms_thread_group_cleanup(CommsThreadGroup* group);

#endif // COMM_THREADS_H