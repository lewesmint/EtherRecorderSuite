/**
 * UDP Socket Wrapper - Simple abstraction for UDP sockets
 * 
 * This provides a thin wrapper around UDP socket operations that closely
 * follows the approach used in our test program.
 */

#ifndef UDP_SOCKET_WRAPPER_H
#define UDP_SOCKET_WRAPPER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "platform_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Maximum UDP message size to avoid fragmentation
 * 1400 bytes is safely below the typical MTU of 1500 bytes,
 * allowing room for IP and UDP headers
 */
#define UDP_MAX_MESSAGE_SIZE 1400

/**
 * UDP socket handle
 */
typedef struct UdpSocketWrapper* UdpSocketHandle;

/**
 * UDP socket address
 */
typedef struct {
    const char* host;  // Host address (IP or hostname)
    uint16_t port;     // Port number
} UdpSocketAddress;

/**
 * Create a UDP socket
 * 
 * @param[out] handle Pointer to receive the socket handle
 * @return PLATFORM_ERROR_SUCCESS on success, error code otherwise
 */
PlatformErrorCode udp_socket_create(UdpSocketHandle* handle);

/**
 * Bind a UDP socket to a local address and port
 * 
 * @param handle Socket handle
 * @param port Port to bind to (0 for any available port)
 * @return PLATFORM_ERROR_SUCCESS on success, error code otherwise
 */
PlatformErrorCode udp_socket_bind(UdpSocketHandle handle, uint16_t port);

/**
 * Get the local port that a socket is bound to
 * 
 * @param handle Socket handle
 * @param[out] port Pointer to receive the port number
 * @return PLATFORM_ERROR_SUCCESS on success, error code otherwise
 */
PlatformErrorCode udp_socket_get_bound_port(UdpSocketHandle handle, uint16_t* port);

/**
 * Wait for data to be available on the socket
 * 
 * @param handle Socket handle
 * @param timeout_ms Timeout in milliseconds (0 for non-blocking, -1 for infinite)
 * @return PLATFORM_ERROR_SUCCESS if data is available, PLATFORM_ERROR_TIMEOUT on timeout,
 *         other error code otherwise
 */
PlatformErrorCode udp_socket_wait_for_data(UdpSocketHandle handle, int timeout_ms);

/**
 * Receive data from the socket
 * 
 * @param handle Socket handle
 * @param buffer Buffer to receive data
 * @param buffer_size Size of the buffer
 * @param[out] bytes_received Pointer to receive the number of bytes received
 * @param[out] sender_addr Pointer to receive the sender's address (can be NULL)
 * @return PLATFORM_ERROR_SUCCESS on success, error code otherwise
 */
PlatformErrorCode udp_socket_receive(
    UdpSocketHandle handle,
    void* buffer,
    size_t buffer_size,
    size_t* bytes_received,
    UdpSocketAddress* sender_addr
);

/**
 * Send data to a specific address
 * 
 * @param handle Socket handle
 * @param buffer Data to send
 * @param buffer_size Size of the data
 * @param dest_addr Destination address
 * @param[out] bytes_sent Pointer to receive the number of bytes sent
 * @return PLATFORM_ERROR_SUCCESS on success, error code otherwise
 */
PlatformErrorCode udp_socket_send(
    UdpSocketHandle handle,
    const void* buffer,
    size_t buffer_size,
    const UdpSocketAddress* dest_addr,
    size_t* bytes_sent
);

/**
 * Close a UDP socket
 * 
 * @param handle Socket handle
 * @return PLATFORM_ERROR_SUCCESS on success, error code otherwise
 */
PlatformErrorCode udp_socket_close(UdpSocketHandle handle);

#ifdef __cplusplus
}
#endif

#endif /* UDP_SOCKET_WRAPPER_H */
