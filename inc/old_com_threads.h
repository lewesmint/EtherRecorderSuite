#ifndef COMM_THREADS_H
#define COMM_THREADS_H

#include "common_socket.h"
#include "app_thread.h"

// TODO consider using a dynamic buffer, adjusting to conditions
#define OLD_BUFFER_SIZE               8192
#define OLD_SOCKET_ERROR_BUFFER_SIZE  256
#define OLD_BLOCKING_TIMEOUT_SEC 10  // Blocking timeout in seconds

// Struct for client and server communication
typedef struct {
    SOCKET* sock;
    struct sockaddr_in client_addr;
    void* thread_info; // Could be ClientThreadArgs_T or ServerThreadArgs_T
    volatile bool connection_closed;
} OLD_CommArgs_T;

/**
 * @brief Structure to hold server manager thread arguments and functions.
 */
//typedef struct CommsThreadArgs_T {
//    // const char *label;                  ///< Label for the thread (e.g., "CLIENT" or "SERVER")
//    // ThreadFunc_T func;                  ///< Actual function to execute
//    // PlatformThread_T thread_id;         ///< Thread ID
//    void* data;                          ///< Server Thread-specific data
//    // PreCreateFunc_T pre_create_func;    ///< Pre-create function
//    // PostCreateFunc_T post_create_func;  ///< Post-create function
//    // InitFunc_T init_func;               ///< Initialisation function
//    // ExitFunc_T exit_func;               ///< Exit function
//    int port;                            ///< Port number
//    bool is_tcp;                         ///< Protocol is TCP (else UDP)
//} CommsThreadArgs_T;

/**
 * @brief Structure to hold server manager thread arguments and functions.
 */
typedef struct OLD_CommsThreadArgs_T {
    void* data;                          ///< Server Thread-specific data
    int data_size;                       ///< Data size
    bool send_test_data;                 ///< Send test data
    int send_interval_ms;                ///< Send interval
    char server_hostname[100];           ///< Server hostname or IP address
    int port;                            ///< Port number
    bool is_tcp;                         ///< Protocol is TCP (else UDP)
} OLD_CommsThreadArgs_T;


void* receive_thread(void* arg);
void* send_thread(void* arg);

#endif