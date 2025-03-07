/**
* @file common_winsock.h
* 
* @brief Common functions and constants for WinSock applications
*/
#ifndef COMMON_WINSOCK_H
#define COMMON_WINSOCK_H

// #include <winsock2.h>
// #include <ws2tcpip.h>
// #include <windows.h> // Don't remove this. If a file includes windows.h                
//                      // before including winsock2.h, it will cause a compilation error
//                      // best to insure that windows.h is included with this common file
//                      // and include this file instead of windows.h in other files
// #include <stdbool.h>

// // Shared constants
// #define PORT 42790
// #define MAX_BLOCKS 375
// #define MIN_BLOCKS 5

// // Data structure sent between client/server
// typedef struct {
//     unsigned int start_marker;
//     unsigned int data_length;
//     unsigned char data[MAX_BLOCKS * 4];
// } DataPacket;

// // Sets both send and receive timeouts (in milliseconds)
// void setSocketTimeouts(SOCKET sock, int timeoutMs);

// // Generates random data in a DataPacket
// void generateRandomData(DataPacket* packet);

// // Sends a DataPacket in a partial-send safe manner
// bool sendData(SOCKET sockfd, DataPacket* packet);

// // Receives a DataPacket in a partial-receive safe manner
// bool receiveData(SOCKET sockfd);

#endif // COMMON_WINSOCK_H