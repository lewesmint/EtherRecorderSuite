/**
 * @file platform_sockets.c
 * @brief Platform-specific socket initialisation and cleanup functions.
 */

#include "platform_sockets.h"
#include "platform_error.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>  // Add this for getaddrinfo and freeaddrinfo
#else
// #include <fcntl.h> // For fcntl on Unix
#include <unistd.h>
#include <netdb.h>     // Add this for getaddrinfo and freeaddrinfo
#endif

// Helper macro to safely convert error codes to SOCKET type
#ifdef _WIN32
#define SOCKET_ERROR_CODE(err) ((SOCKET)(err))
#else
#define SOCKET_ERROR_CODE(err) (err)
#endif

/**
 * @brief Initialises the socket library.
 *
 * On Windows, this function initialises the Winsock library. On other platforms,
 * it does nothing.
 */
void initialise_sockets(void) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        stream_print(stderr, "Winsock initialisation failed. Error: %d\n", WSAGetLastError());
        exit(1);
    }
#endif
}

/**
 * Closes an individual socket and marks it as INVALID_SOCKET.
 * This function does **not** call WSACleanup(), ensuring that Winsock
 * is only cleaned up once when the application fully exits.
 */
void close_socket(SOCKET *sock) {
    if (sock && *sock != INVALID_SOCKET) {
        CLOSESOCKET(*sock);
        *sock = INVALID_SOCKET;
    }
}

/**
 * @brief Cleans up the socket library.
 *
 * On Windows, this function cleans up the Winsock library. On other platforms,
 * it does nothing.
 */
void cleanup_sockets(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

int platform_getsockopt(SOCKET sock, int level, int optname, void *optval, socklen_t *optlen) {
#ifdef _WIN32
    // On Windows, optlen is of type int
    int optlen_windows = (int)*optlen;
    int result = getsockopt(sock, level, optname, (char *)optval, &optlen_windows);
    if (result == 0) {
        *optlen = (socklen_t)optlen_windows; // Update optlen to match POSIX type
    }
    return result;
#else
    // On Linux, directly use getsockopt
    return getsockopt(sock, level, optname, optval, optlen);
#endif
}

#ifdef _WIN32
    #define CLOSESOCKET closesocket
#else
    #define CLOSESOCKET close
#endif

/**
 * @brief Sets a socket to non-blocking mode.
 *
 * On Windows, this function uses `ioctlsocket` to set the socket to non-blocking mode.
 * On other platforms, it uses `fcntl` to set the `O_NONBLOCK` flag.
 *
 * @param sock The socket to set to non-blocking mode.
 * @return 0 on success, or -1 on error.
 */
int set_non_blocking_mode(SOCKET sock) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}

/**
 * @brief Restores a socket to blocking mode.
 *
 * On Windows, this function uses `ioctlsocket` to set the socket to blocking mode.
 * On other platforms, it uses `fcntl` to clear the `O_NONBLOCK` flag.
 *
 * @param sock The socket to restore to blocking mode.
 * @return 0 on success, or -1 on error.
 */
int restore_blocking_mode(SOCKET sock) {
#ifdef _WIN32
    u_long mode = 0;
    return ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
#endif
}

/**
 * Returns a human-readable string for platform socket errors.
 */
const char* platform_socket_strerror(PlatformSocketError error) {
    switch (error) {
        case PLATFORM_SOCKET_SUCCESS:
            return "Success";
        case PLATFORM_SOCKET_ERROR_CREATE:
            return "Error creating socket";
        case PLATFORM_SOCKET_ERROR_RESOLVE:
            return "Error resolving address";
        case PLATFORM_SOCKET_ERROR_BIND:
            return "Error binding socket";
        case PLATFORM_SOCKET_ERROR_LISTEN:
            return "Error listening on socket";
        case PLATFORM_SOCKET_ERROR_CONNECT:
            return "Error connecting to socket";
        case PLATFORM_SOCKET_ERROR_TIMEOUT:
            return "Socket operation timed out";
        case PLATFORM_SOCKET_ERROR_SEND:
            return "Error sending data";
        case PLATFORM_SOCKET_ERROR_RECV:
            return "Error receiving data";
        case PLATFORM_SOCKET_ERROR_SELECT:
            return "Error with select operation";
        case PLATFORM_SOCKET_ERROR_GETSOCKOPT:
            return "Error getting socket options";
        default:
            return "Unknown socket error";
    }
}

/**
 * Retrieves detailed socket error information as a string.
 * The error message is written to the provided buffer.
 *
 * @param buffer       The buffer to store the error message.
 * @param buffer_size  The size of the buffer.
 */
void get_socket_error_message(char* buffer, size_t buffer_size) {
    platform_get_socket_error_message(buffer, buffer_size);
}

/**
 * Retrieves detailed socket error information as a string.
 * The error message is written to the provided buffer.
 *
 * @param buffer       The buffer to store the error message.
 * @param buffer_size  The size of the buffer.
 */
const char* socket_error_string(void) {
    static char buffer[256];
    platform_get_socket_error_message(buffer, sizeof(buffer));
    return buffer;
}


SOCKET setup_listening_server_socket(struct sockaddr_in* addr, uint16_t port) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        return SOCKET_ERROR_CODE(PLATFORM_SOCKET_ERROR_CREATE);
    }

    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    
    addr->sin_addr.s_addr = INADDR_ANY; // Bind to all available interfaces
    if (bind(sock, (struct sockaddr*)addr, sizeof(*addr)) == SOCKET_ERROR) {
        close_socket(&sock);
        return SOCKET_ERROR_CODE(PLATFORM_SOCKET_ERROR_BIND);
    }
    if (listen(sock, SOMAXCONN) == SOCKET_ERROR) {
        close_socket(&sock);
        return SOCKET_ERROR_CODE(PLATFORM_SOCKET_ERROR_LISTEN);    
    }
    return sock;
}

/**
 * Creates and sets up a socket (server or client).
 * 
 * @param is_server 1 if setting up a server, 0 if client.
 * @param is_tcp 1 for TCP, 0 for UDP.
 * @param addr Pointer to the socket address structure.
 * @param client_addr Pointer to the client address structure (for UDP).
 * @param ip_addr IP address to bind to (for client mode).
 * @param port Port number.
 * @return A valid socket descriptor, or INVALID_SOCKET on failure.
 */
SOCKET setup_socket(bool is_server, bool is_tcp, struct sockaddr_in *addr, struct sockaddr_in *client_addr, const char *host, uint16_t port) {
    SOCKET sock = socket(AF_INET, is_tcp ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        return SOCKET_ERROR_CODE(PLATFORM_SOCKET_ERROR_CREATE);
    }

    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);

    if (is_server) {
        addr->sin_addr.s_addr = INADDR_ANY; // Bind to all available interfaces
    } else {
        // Use `getaddrinfo()` instead of `inet_pton()` for hostname resolution
        struct addrinfo hints, *res = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = is_tcp ? SOCK_STREAM : SOCK_DGRAM;

        int err = getaddrinfo(host, NULL, &hints, &res);
        if (err != 0 || res == NULL) {
            close_socket(&sock);
            return SOCKET_ERROR_CODE(PLATFORM_SOCKET_ERROR_RESOLVE);
        }

        // Copy resolved address into `addr`
        struct sockaddr_in *resolved_addr = (struct sockaddr_in *)res->ai_addr;
        addr->sin_addr = resolved_addr->sin_addr;
        freeaddrinfo(res);
    }

    if (is_server) {
        if (bind(sock, (struct sockaddr *)addr, sizeof(*addr)) == SOCKET_ERROR) {
            close_socket(&sock);
            return SOCKET_ERROR_CODE(PLATFORM_SOCKET_ERROR_BIND);
        }
        if (is_tcp && listen(sock, SOMAXCONN) == SOCKET_ERROR) {
            close_socket(&sock);
            return SOCKET_ERROR_CODE(PLATFORM_SOCKET_ERROR_LISTEN);
        }
    } else if (!is_tcp) {
        client_addr->sin_family = AF_INET;
        client_addr->sin_port = htons(port);
        client_addr->sin_addr = addr->sin_addr;
    }

    return sock;
}

/**
 * Attempts to connect to a server with a timeout.
 */
PlatformSocketError connect_with_timeout(SOCKET sock, struct sockaddr_in *server_addr, int timeout_seconds) {
    if (timeout_seconds <= 0) {
        return PLATFORM_SOCKET_ERROR_TIMEOUT;
    }

    if (set_non_blocking_mode(sock) != 0) {
        return PLATFORM_SOCKET_ERROR_CONNECT;
    }

    int result = connect(sock, (struct sockaddr *)server_addr, sizeof(*server_addr));
    int last_error = GET_LAST_SOCKET_ERROR();

    if (result < 0) {
        if (last_error != PLATFORM_SOCKET_WOULDBLOCK &&
            last_error != PLATFORM_SOCKET_INPROGRESS) {
            restore_blocking_mode(sock);  // Restore blocking mode before returning
            return PLATFORM_SOCKET_ERROR_CONNECT;
        }

        /* Connection is in progress; use select() to wait for completion. */
        fd_set write_set;
        FD_ZERO(&write_set);
        FD_SET(sock, &write_set);

        struct timeval timeout_val;
        timeout_val.tv_sec  = timeout_seconds;
        timeout_val.tv_usec = 0;
        
        // The cast is safe because valid socket descriptors are small positive numbers
        int nfds = (int)(sock + 1);  // POSIX needs highest FD + 1, Windows ignores this
        result = select(nfds, NULL, &write_set, NULL, &timeout_val);
        if (result < 0) {
            restore_blocking_mode(sock);
            return PLATFORM_SOCKET_ERROR_SELECT;  // Better error classification
        } else if (result == 0) {
            restore_blocking_mode(sock);
            return PLATFORM_SOCKET_ERROR_TIMEOUT;
        } else {
            /* We expect the socket to be ready for writing. */
            if (FD_ISSET(sock, &write_set)) {
                int so_error = 0;
                socklen_t len = sizeof(so_error);
                if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (void *)&so_error, &len) != 0) {
                    restore_blocking_mode(sock);
                    return PLATFORM_SOCKET_ERROR_GETSOCKOPT;
                }
                restore_blocking_mode(sock);
                return (so_error == 0) ? PLATFORM_SOCKET_SUCCESS : PLATFORM_SOCKET_ERROR_CONNECT;
            } else {
                /* This branch is unexpected if select() returned a positive value.
                   We treat it as a connection error. */
                restore_blocking_mode(sock);
                return PLATFORM_SOCKET_ERROR_CONNECT;
            }
        }
    }

    /* If connect() succeeded immediately. */
    restore_blocking_mode(sock);
    return PLATFORM_SOCKET_SUCCESS;
}

/**
 * Checks if a socket connection is still healthy without sending or receiving data.
 * This uses getsockopt() to check the socket error state.
 * 
 * @param sock The socket to check
 * @return true if the socket appears healthy, false otherwise
 */
bool is_socket_healthy(SOCKET sock) {
    if (sock == INVALID_SOCKET) {
        return false;
    }
    
    int error = 0;
    socklen_t len = sizeof(error);
    
    // Use the cross-platform getsockopt wrapper
    if (platform_getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) != 0) {
        // Failed to get socket option
        return false;
    }
    
    // If error is non-zero, the socket has some error condition
    return (error == 0);
}
