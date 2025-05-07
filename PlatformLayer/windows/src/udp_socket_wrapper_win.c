/**
 * UDP Socket Wrapper - Windows Implementation
 */

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

#include "udp_socket_wrapper.h"
#include "platform_error.h"

// Link with Winsock library
#pragma comment(lib, "ws2_32.lib")

// UDP socket wrapper structure
struct UdpSocketWrapper {
    SOCKET socket;
    bool initialized;
};

// Initialize Winsock (called once)
static bool winsock_initialized = false;
static PlatformErrorCode initialize_winsock(void) {
    if (winsock_initialized) {
        return PLATFORM_ERROR_SUCCESS;
    }
    
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        printf("WSAStartup failed: %d", result);
        return PLATFORM_ERROR_SOCKET_INIT;
    }
    
    winsock_initialized = true;
    return PLATFORM_ERROR_SUCCESS;
}

// Convert Windows socket error to platform error code
static PlatformErrorCode convert_socket_error(void) {
    int error = WSAGetLastError();
    
    switch (error) {
        case WSAEWOULDBLOCK:
            return PLATFORM_ERROR_WOULD_BLOCK;
        case WSAECONNRESET:
            return PLATFORM_ERROR_CONNECTION_RESET;
        case WSAEADDRINUSE:
            return PLATFORM_ERROR_ADDRESS_IN_USE;
        case WSAEADDRNOTAVAIL:
            return PLATFORM_ERROR_ADDRESS_NOT_AVAILABLE;
        case WSAENETDOWN:
            return PLATFORM_ERROR_NETWORK_DOWN;
        case WSAENETUNREACH:
            return PLATFORM_ERROR_NETWORK_UNREACHABLE;
        case WSAETIMEDOUT:
            return PLATFORM_ERROR_TIMEOUT;
        case WSAECONNREFUSED:
            return PLATFORM_ERROR_CONNECTION_REFUSED;
        case WSAEHOSTUNREACH:
            return PLATFORM_ERROR_HOST_NOT_FOUND;
        case WSAEINVAL:
            return PLATFORM_ERROR_INVALID_ARGUMENT;
        default:
            printf("Unhandled socket error: %d", error);
            return PLATFORM_ERROR_SOCKET_OPERATION;
    }
}

// Create a UDP socket
PlatformErrorCode udp_socket_create(UdpSocketHandle* handle) {
    if (!handle) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    // Initialize Winsock if needed
    PlatformErrorCode err = initialize_winsock();
    if (err != PLATFORM_ERROR_SUCCESS) {
        return err;
    }
    
    // Allocate socket wrapper
    struct UdpSocketWrapper* wrapper = (struct UdpSocketWrapper*)malloc(sizeof(struct UdpSocketWrapper));
    if (!wrapper) {
        return PLATFORM_ERROR_OUT_OF_MEMORY;
    }
    
    // Create socket
    wrapper->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (wrapper->socket == INVALID_SOCKET) {
        free(wrapper);
        return convert_socket_error();
    }
    
    wrapper->initialized = true;
    *handle = wrapper;
    
    printf("UDP socket created: %p", (void*)wrapper);
    return PLATFORM_ERROR_SUCCESS;
}

// Bind a UDP socket to a local port
PlatformErrorCode udp_socket_bind(UdpSocketHandle handle, uint16_t port) {
    if (!handle || !handle->initialized) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    // Set up address structure
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);  // Bind to all interfaces
    addr.sin_port = htons(port);
    
    // Bind the socket
    if (bind(handle->socket, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        PlatformErrorCode err = convert_socket_error();
        printf("Failed to bind UDP socket to port %d: %d", port, err);
        return err;
    }
    
    printf("UDP socket bound to port %d", port);
    return PLATFORM_ERROR_SUCCESS;
}

// Get the local port that a socket is bound to
PlatformErrorCode udp_socket_get_bound_port(UdpSocketHandle handle, uint16_t* port) {
    if (!handle || !handle->initialized || !port) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    struct sockaddr_in addr;
    int addr_len = sizeof(addr);
    
    if (getsockname(handle->socket, (struct sockaddr*)&addr, &addr_len) == SOCKET_ERROR) {
        return convert_socket_error();
    }
    
    *port = ntohs(addr.sin_port);
    return PLATFORM_ERROR_SUCCESS;
}

// Wait for data to be available on the socket
PlatformErrorCode udp_socket_wait_for_data(UdpSocketHandle handle, int timeout_ms) {
    if (!handle || !handle->initialized) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    fd_set read_fds;
    struct timeval timeout;
    
    FD_ZERO(&read_fds);
    FD_SET(handle->socket, &read_fds);
    
    if (timeout_ms >= 0) {
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
    }
    
    int result = select(0, &read_fds, NULL, NULL, (timeout_ms >= 0) ? &timeout : NULL);
    
    if (result == SOCKET_ERROR) {
        return convert_socket_error();
    }
    
    if (result == 0) {
        return PLATFORM_ERROR_TIMEOUT;
    }
    
    return PLATFORM_ERROR_SUCCESS;
}

// Receive data from the socket
PlatformErrorCode udp_socket_receive(
    UdpSocketHandle handle,
    void* buffer,
    size_t buffer_size,
    size_t* bytes_received,
    UdpSocketAddress* sender_addr
) {
    if (!handle || !handle->initialized || !buffer || buffer_size == 0 || !bytes_received) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    struct sockaddr_in addr;
    int addr_len = sizeof(addr);
    
    int result = recvfrom(
        handle->socket,
        (char*)buffer,
        (int)buffer_size,
        0,
        (struct sockaddr*)&addr,
        &addr_len
    );
    
    if (result == SOCKET_ERROR) {
        return convert_socket_error();
    }
    
    *bytes_received = (size_t)result;
    
    // Fill in sender address if requested
    if (sender_addr) {
        static char addr_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(addr.sin_addr), addr_str, INET_ADDRSTRLEN);
        
        sender_addr->host = addr_str;  // Note: this is static storage
        sender_addr->port = ntohs(addr.sin_port);
    }
    
    return PLATFORM_ERROR_SUCCESS;
}

// Send data to a specific address
PlatformErrorCode udp_socket_send(
    UdpSocketHandle handle,
    const void* buffer,
    size_t buffer_size,
    const UdpSocketAddress* dest_addr,
    size_t* bytes_sent
) {
    if (!handle || !handle->initialized || !buffer || buffer_size == 0 || 
        !dest_addr || !bytes_sent) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    // Set up destination address
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(dest_addr->port);
    
    // Convert IP address from string to binary
    if (inet_pton(AF_INET, dest_addr->host, &addr.sin_addr) <= 0) {
        return PLATFORM_ERROR_INVALID_ADDRESS;
    }
    
    int result = sendto(
        handle->socket,
        (const char*)buffer,
        (int)buffer_size,
        0,
        (struct sockaddr*)&addr,
        sizeof(addr)
    );
    
    if (result == SOCKET_ERROR) {
        return convert_socket_error();
    }
    
    *bytes_sent = (size_t)result;
    return PLATFORM_ERROR_SUCCESS;
}

// Close a UDP socket
PlatformErrorCode udp_socket_close(UdpSocketHandle handle) {
    if (!handle) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    if (handle->initialized) {
        closesocket(handle->socket);
        handle->initialized = false;
    }
    
    free(handle);
    return PLATFORM_ERROR_SUCCESS;
}
