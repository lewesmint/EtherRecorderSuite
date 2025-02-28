/**
* @file client_manager.h
* @brief Client manager thread functions.
* 
* This file contains declarations for the client manager, which is responsible
* for establishing and maintaining connections to servers, and spawning the
* necessary communication threads.
*/
#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H
#include <winsock2.h>
#include <stdbool.h>
#include "app_thread.h"

/**
 * @brief Main client thread function.
 * 
 * This function is responsible for:
 * - Establishing connections to the server
 * - Creating send and receive threads for communication
 * - Handling reconnection on connection loss
 * - Cleaning up resources on exit
 * 
 * @param arg Thread arguments (AppThreadArgs_T*)
 * @return void* NULL
 */
void* clientMainThread(void* arg);

#endif // CLIENT_MANAGER_H