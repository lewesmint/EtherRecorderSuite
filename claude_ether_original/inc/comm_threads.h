#ifndef COMM_THREADS_H
#define COMM_THREADS_H

#include "platform_sockets.h"

#include <stdbool.h>
#include <stdint.h>

#include "app_thread.h"

// Configuration constants with sensible defaults
#define COMMS_BUFFER_SIZE         8192
#define SOCKET_ERROR_BUFFER_SIZE  256
#define BLOCKING_TIMEOUT_SEC      10  // Blocking timeout in seconds

typedef struct CommContext {
    SOCKET* socket;                      ///< Socket handle
    volatile long connection_closed;     ///< Connection status flag
    uint32_t group_id;                   ///< Group identifier
    struct sockaddr_in client_addr;      ///< Client address
    bool is_relay_enabled;               ///< Relay mode enabled flag
    bool is_tcp;                         ///< Protocol type (TCP/UDP)
    void* foreign_queue;                 ///< Message queue
    bool send_test_data;                ///< Send test data flag
    int send_interval_ms;               ///< Send interval
    int data_size;                      ///< Data size
    // uint32_t thread_id;                  ///< Thread identifier
} CommContext;

typedef struct CommsThreadArgs_T{
    int port;                           ///< Port number
    bool is_tcp;                         ///< Protocol is TCP (else UDP)
    bool send_test_data;                ///< Send test data flag
    int send_interval_ms;               ///< Send interval
    int data_size;                      ///< Data size
} CommsThreadArgs_T;

typedef struct ClientCommsThreadArgs_T {
    char server_hostname[100];           ///< Server hostname or IP address
    CommsThreadArgs_T comm_args;
} ClientCommsThreadArgs_T;

/**
 * @brief Thread function for sending data to a socket
 * @param arg Pointer to thread arguments (AppThread_T structure)
 * @return NULL
 */
void* send_thread_function(void* arg);

void* receive_thread_function(void* arg);

// Function declarations
bool create_communication_threads(AppThread_T* send_thread,
                                  AppThread_T* receive_thread,
                                  CommContext* context,
                                  CommsThreadArgs_T* config);
                                  
void cleanup_communication_threads(CommContext* context);

#endif // COMM_THREADS_H
