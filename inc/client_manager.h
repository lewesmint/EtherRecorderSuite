/**
* @file client_manager.h
* @brief Client manager thread functions.
* 
*/
#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H
#include <winsock2.h>
#include <stdbool.h>



void* clientMainThread(void* arg);

#endif // CLIENT_MANAGER_H