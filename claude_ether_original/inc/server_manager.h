/**
* @file server_manager.h
* @brief Server management thread functions.
* 
* This file contains declarations for the server manager, which is responsible
* for listening for incoming connections and spawning the necessary 
* communication threads.
*/
#ifndef SERVER_MANAGER_H
#define SERVER_MANAGER_H

/**
 * @brief Main server thread function.
 * 
 * This function is responsible for:
 * - Setting up a listening socket
 * - Accepting incoming client connections
 * - Creating send and receive threads for communication
 * - Handling disconnections and reconnections
 * - Cleaning up resources on exit
 * 
 * @param arg Thread arguments (void*)
 * @return void* NULL
 */
void* serverListenerThread(void* arg);

#endif // SERVER_MANAGER_H
