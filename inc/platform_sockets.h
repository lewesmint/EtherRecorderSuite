/**
 * @file platform_sockets.h
 * @brief Platform-specific socket functions
 */

#ifndef PLATFORM_SOCKETS_H
#define PLATFORM_SOCKETS_H

// Platform-independent socket definitions
#include <stdbool.h>
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h> // inet_ntop etc.
#else // !_WIN32
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <errno.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <netdb.h> // For getaddrinfo and freeaddrinfo
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
    #define CLOSESOCKET closesocket
    typedef int socklen_t;
    #define PLATFORM_SOCKET_WOULDBLOCK WSAEWOULDBLOCK
    #define PLATFORM_SOCKET_INPROGRESS WSAEINPROGRESS
    #define GET_LAST_SOCKET_ERROR() WSAGetLastError()
#else // !_WIN32
    typedef int SOCKET;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define CLOSESOCKET close
    #define PLATFORM_SOCKET_WOULDBLOCK EWOULDBLOCK
    #define PLATFORM_SOCKET_INPROGRESS EINPROGRESS
    #define GET_LAST_SOCKET_ERROR() errno
#endif // _WIN32


typedef enum PlatformSocketError {
    PLATFORM_SOCKET_SUCCESS = 0,
    PLATFORM_SOCKET_ERROR_CREATE = -1,
    PLATFORM_SOCKET_ERROR_RESOLVE = -2,
    PLATFORM_SOCKET_ERROR_BIND = -3,
    PLATFORM_SOCKET_ERROR_LISTEN = -4,
    PLATFORM_SOCKET_ERROR_CONNECT = -5,
    PLATFORM_SOCKET_ERROR_TIMEOUT = -6,
    PLATFORM_SOCKET_ERROR_SEND = -7,
    PLATFORM_SOCKET_ERROR_RECV = -8,
    PLATFORM_SOCKET_ERROR_SELECT = -9,
    PLATFORM_SOCKET_ERROR_GETSOCKOPT = -10
} PlatformSocketError;


/**
 * @brief Initialises the socket library.
 *
 * On Windows, this function initialises the Winsock library. On other platforms,
 * it does nothing.
 */
void initialise_sockets(void);

/**
 * @brief Cleans up the socket library.
 *
 * On Windows, this function cleans up the Winsock library. On other platforms,
 * it does nothing.
 */
void cleanup_sockets(void);

int platform_getsockopt(int sock, int level, int optname, void *optval, socklen_t *optlen);

void get_socket_error_message(char* buffer, size_t buffer_size);


/**
 * @brief Sets a socket to non-blocking mode.
 *
 * On Windows, this function uses `ioctlsocket` to set the socket to non-blocking mode.
 * On other platforms, it uses `fcntl` to set the `O_NONBLOCK` flag.
 *
 * @param sock The socket to set to non-blocking mode.
 * @return 0 on success, or -1 on error.
 */
int set_non_blocking_mode(SOCKET sock);

/**
 * @brief Restores a socket to blocking mode.
 *
 * On Windows, this function uses `ioctlsocket` to set the socket to blocking mode.
 * On other platforms, it uses `fcntl` to clear the `O_NONBLOCK` flag.
 *
 * @param sock The socket to restore to blocking mode.
 * @return 0 on success, or -1 on error.
 */
int restore_blocking_mode(SOCKET sock);

/**
 * @brief Closes a socket and sets it to INVALID_SOCKET.
 * @param sock Pointer to the socket to close.
 */
void close_socket(SOCKET* sock);

/**
 * @brief Sets up a listening server socket.
 * @param addr Address structure to fill.
 * @param port Port to listen on.
 * @return Socket handle or error code on failure.
 */
SOCKET setup_listening_server_socket(struct sockaddr_in* addr, uint16_t port);

/**
 * @brief Gets a string describing the last socket error.
 * @return String containing the error message.
 */
const char* socket_error_string(void);

SOCKET setup_socket(bool is_server, bool is_tcp, struct sockaddr_in *addr, struct sockaddr_in *client_addr, const char *host, uint16_t port);
PlatformSocketError connect_with_timeout(SOCKET sock, struct sockaddr_in *server_addr, int timeout_seconds);


#ifdef __cplusplus
}
#endif

#endif // PLATFORM_SOCKETS_H
