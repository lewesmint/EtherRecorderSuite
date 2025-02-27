/**
* @file server_manager.h
 * @brief Header file for server_manager.c.
 * @details Header file for server_manager.c.
 * 
 */
#ifndef SERVER_MANAGER_H
#define SERVER_MANAGER_H

#include <stdbool.h>
#include "comm_threads.h"

void* serverListenerThread(void* arg);

#endif // SERVER_MANAGER_H