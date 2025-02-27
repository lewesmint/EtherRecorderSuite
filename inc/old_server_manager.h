/**
* @file server_manager.h
 * @brief Header file for server_manager.c.
 * @details Header file for server_manager.c.
 * 
 */
#ifndef SERVER_MANAGER_H
#define SERVER_MANAGER_H

#include <stdbool.h>

/**
 * @brief Structure to hold server manager thread arguments and functions.
 */
typedef struct ServerThreadArgs_T {
    // const char *label;                  ///< Label for the thread (e.g., "CLIENT" or "SERVER")
    // ThreadFunc_T func;                  ///< Actual function to execute
    // PlatformThread_T thread_id;         ///< Thread ID
    void *data;                          ///< Server Thread-specific data
    // PreCreateFunc_T pre_create_func;    ///< Pre-create function
    // PostCreateFunc_T post_create_func;  ///< Post-create function
    // InitFunc_T init_func;               ///< Initialisation function
    // ExitFunc_T exit_func;               ///< Exit function
    int port;                            ///< Port number
    bool is_tcp;                         ///< Protocol is TCP (else UDP)
} ServerThreadArgs_T;

void* serverListenerThread(void* arg);

#endif // SERVER_MANAGER_H