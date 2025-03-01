/**
* @file common_socket.h
* @brief Common socket functions.
*/

#ifndef COMMON_SOCKET_H
#define COMMON_SOCKET_H


#include <stdbool.h>
#include "platform_sockets.h"

#ifdef __cplusplus
extern "C" {
#endif

// #define START_MARKER  0xBAADF00D
// #define END_MARKER    0xDEADBEEF

//
// 1,500 bytes total in the structure:
//
//  - 4 bytes for start_marker
//  - 4 bytes for data_length
//  - 1,488 bytes of random data (372 blocks * 4 bytes each)
//  - 4 bytes for the end marker
//
// => 4 + 4 + (372 * 4) + 4 = 1500
//
// #define MAX_BLOCKS  372  // The maximum number of 4-byte blocks for random data
// #define MIN_BLOCKS    5  // Minimum number of 4-byte blocks

// // The DummyPayload structure has a **fixed size of 1500 bytes**
// typedef struct DummyPayload
// {
//     unsigned int start_marker;    // 4 bytes
//     unsigned int data_length;     // 4 bytes (always a multiple of 4)
//     unsigned char data[MAX_BLOCKS * 4 + 4];  // 1492 bytes (including 4 bytes for END_MARKER)
// } DummyPayload;

// int generateRandomData(DummyPayload* packet);


// // these will be used by dedicated threads at some point, so exposed in the header
// int send_all_data(SOCKET sock, void *data, int buffer_size, int is_tcp, struct sockaddr_in *client_addr, socklen_t addr_len);
// int receive_all_data(SOCKET sock, void *data, int buffer_size, int is_tcp, struct sockaddr_in *client_addr, socklen_t *addr_len);
// void communication_loop(SOCKET sock, int is_server, int is_tcp, struct sockaddr_in *client_addr);

#ifdef __cplusplus
}
#endif

#endif // COMMON_SOCKET_H