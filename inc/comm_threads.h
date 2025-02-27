#ifndef COMM_THREADS_H
#define COMM_THREADS_H

#include "common_socket.h"
#include "app_thread.h"

// Configuration constants with sensible defaults
#define BUFFER_SIZE               8192
#define SOCKET_ERROR_BUFFER_SIZE  256
#define BLOCKING_TIMEOUT_SEC      10  // Blocking timeout in seconds

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
    int port;                     // Port number
    bool is_tcp;                  // Protocol is TCP (else UDP)
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
void* receive_thread(void* arg);

/**
 * @brief Thread function for sending data
 * 
 * This function handles sending data to a socket. It runs in a dedicated
 * thread and continues until the connection is closed or an error occurs.
 * 
 * @param arg Thread arguments (CommArgs_T*)
 * @return void* NULL
 */
void* send_thread(void* arg);

#endif // COMM_THREADS_H