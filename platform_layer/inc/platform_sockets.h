/**
 * @file platform_sockets.h
 * @brief Platform-agnostic network socket interface
 */
#ifndef PLATFORM_SOCKETS_H
#define PLATFORM_SOCKETS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "platform_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Socket type enumeration
 */
typedef enum {
    PLATFORM_SOCKET_TCP,  ///< TCP socket type
    PLATFORM_SOCKET_UDP   ///< UDP socket type
} PlatformSocketType;

/**
 * @brief Socket statistics
 */
typedef struct {
    uint64_t bytes_sent;     ///< Total bytes sent
    uint64_t bytes_received; ///< Total bytes received
    uint64_t packets_sent;   ///< Total packets sent
    uint64_t packets_received; ///< Total packets received
    uint32_t error_count;    ///< Number of errors encountered
} PlatformSocketStats;

/**
 * @brief Socket options
 */
typedef struct {
    bool blocking;           ///< Blocking mode
    bool reuse_address;      ///< Allow address reuse
    bool keep_alive;         ///< Enable keep-alive
    uint32_t send_timeout_ms; ///< Send timeout in milliseconds
    uint32_t recv_timeout_ms; ///< Receive timeout in milliseconds
    uint32_t connect_timeout_ms; ///< Connect timeout in milliseconds
    size_t send_buffer_size; ///< Send buffer size
    size_t recv_buffer_size; ///< Receive buffer size
} PlatformSocketOptions;

typedef struct PlatformSocket {
    int fd;                     // Socket file descriptor
    bool is_tcp;               // TCP or UDP
    PlatformSocketStats stats; // Socket statistics
    PlatformSocketOptions opts; // Socket options
} PlatformSocket;


/**
 * @brief Opaque socket handle type
 */
typedef struct PlatformSocket* PlatformSocketHandle;

/**
 * @brief Socket address structure
 */
typedef struct {
    char host[256];          ///< Host name or IP address
    uint16_t port;          ///< Port number
    bool is_ipv6;           ///< True if IPv6, false for IPv4
} PlatformSocketAddress;


/**
 * @brief Initialize the socket subsystem
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_socket_init(void);

/**
 * @brief Cleanup the socket subsystem
 */
void platform_socket_cleanup(void);

/**
 * @brief Create a new socket
 * @param[out] handle Pointer to store the socket handle
 * @param[in] is_tcp True for TCP, false for UDP
 * @param[in] options Socket options (NULL for defaults)
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_socket_create(
    PlatformSocketHandle* handle,
    bool is_tcp,
    const PlatformSocketOptions* options);

/**
 * @brief Close a socket
 * @param[in] handle Socket handle
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_socket_close(PlatformSocketHandle handle);

/**
 * @brief Connect to a remote host
 * @param[in] handle Socket handle
 * @param[in] address Remote address
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_socket_connect(
    PlatformSocketHandle handle,
    const PlatformSocketAddress* address);

/**
 * @brief Bind socket to local address
 * @param[in] handle Socket handle
 * @param[in] address Local address
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_socket_bind(
    PlatformSocketHandle handle,
    const PlatformSocketAddress* address);

/**
 * @brief Start listening for connections
 * @param[in] handle Socket handle
 * @param[in] backlog Maximum pending connections
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_socket_listen(
    PlatformSocketHandle handle,
    int backlog);

/**
 * @brief Accept incoming connection
 * @param[in] handle Listening socket handle
 * @param[out] client_handle Pointer to store new client socket handle
 * @param[out] client_address Pointer to store client address (optional)
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_socket_accept(
    PlatformSocketHandle handle,
    PlatformSocketHandle* client_handle,
    PlatformSocketAddress* client_address);

/**
 * @brief Send data
 * @param[in] handle Socket handle
 * @param[in] buffer Data buffer
 * @param[in] length Buffer length
 * @param[out] bytes_sent Pointer to store number of bytes sent
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_socket_send(
    PlatformSocketHandle handle,
    const void* buffer,
    size_t length,
    size_t* bytes_sent);

/**
 * @brief Receive data
 * @param[in] handle Socket handle
 * @param[out] buffer Buffer to store received data
 * @param[in] length Buffer length
 * @param[out] bytes_received Pointer to store number of bytes received
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_socket_receive(
    PlatformSocketHandle handle,
    void* buffer,
    size_t length,
    size_t* bytes_received);

/**
 * @brief Check if socket is connected
 * @param[in] handle Socket handle
 * @param[out] is_connected Pointer to store connection status
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_socket_is_connected(
    PlatformSocketHandle handle,
    bool* is_connected);

/**
 * @brief Get socket statistics
 * @param[in] handle Socket handle
 * @param[out] stats Pointer to store statistics
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_socket_get_stats(
    PlatformSocketHandle handle,
    PlatformSocketStats* stats);

/**
 * @brief Get string representation of socket error
 * @param[in] error_code Error code from PlatformErrorCode
 * @param[out] buffer Buffer to store error string
 * @param[in] buffer_size Buffer size
 * @param[in] additional_info Additional error information (can be NULL)
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_socket_error_string(
    PlatformErrorCode error_code,
    char* buffer,
    size_t buffer_size,
    const char* additional_info);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_SOCKETS_H
