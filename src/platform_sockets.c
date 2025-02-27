
/**
 * @file platform_sockets.c
 * @brief Platform-specific socket initialisation and cleanup functions.
 */

#include "platform_sockets.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
#else
// #include <fcntl.h> // For fcntl on Unix
#include <unistd.h>
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
        fprintf(stderr, "Winsock initialisation failed. Error: %d\n", WSAGetLastError());
        exit(1);
    }
#endif
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

int platform_getsockopt(int sock, int level, int optname, void *optval, socklen_t *optlen) {
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


#include "platform_sockets.h"
#include <stdio.h>
#include <string.h>

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
#ifdef _WIN32
    int errorCode = WSAGetLastError();
    LPSTR lpMsgBuf = NULL;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&lpMsgBuf,
        0,
        NULL
    );
    snprintf(buffer, buffer_size, "Socket operation failed with error: %d: %s",
        errorCode, lpMsgBuf ? lpMsgBuf : "Unknown error");
    if (lpMsgBuf) {
        LocalFree(lpMsgBuf);
    }
#else
    int errsv = errno;
    snprintf(buffer, buffer_size, "Socket operation failed with error: %d: %s",
        errsv, strerror(errsv));
#endif
}
