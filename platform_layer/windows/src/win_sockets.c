/**
 * @file win_sockets.c
 * @brief Windows implementation of platform socket operations using Winsock2
 */
#include "platform_sockets.h"
#include "platform_error.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>


PlatformErrorCode platform_socket_init(void) {
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
        return PLATFORM_ERROR_NOT_INITIALIZED;
    }
    return PLATFORM_ERROR_SUCCESS;
}

void platform_socket_cleanup(void) {
    WSACleanup();
}

static PlatformErrorCode set_socket_options(SOCKET sock, const PlatformSocketOptions* options) {
    if (!options) {
        return PLATFORM_ERROR_SUCCESS;
    }

    // Set blocking mode
    u_long mode = options->blocking ? 0 : 1;
    if (ioctlsocket(sock, FIONBIO, &mode) == SOCKET_ERROR) {
        return PLATFORM_ERROR_SOCKET_OPTION;
    }

    // Set reuse address
    BOOL reuse = options->reuse_address ? TRUE : FALSE;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) == SOCKET_ERROR) {
        return PLATFORM_ERROR_SOCKET_OPTION;
    }

    // Set keep alive
    BOOL keepalive = options->keep_alive ? TRUE : FALSE;
    if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char*)&keepalive, sizeof(keepalive)) == SOCKET_ERROR) {
        return PLATFORM_ERROR_SOCKET_OPTION;
    }

    // Set send timeout
    if (options->send_timeout_ms > 0) {
        DWORD timeout = options->send_timeout_ms;
        if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
            return PLATFORM_ERROR_SOCKET_OPTION;
        }
    }

    // Set receive timeout
    if (options->recv_timeout_ms > 0) {
        DWORD timeout = options->recv_timeout_ms;
        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
            return PLATFORM_ERROR_SOCKET_OPTION;
        }
    }

    // Set buffer sizes
    if (options->send_buffer_size > 0) {
        int size = (int)options->send_buffer_size;
        if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&size, sizeof(size)) == SOCKET_ERROR) {
            return PLATFORM_ERROR_SOCKET_OPTION;
        }
    }

    if (options->recv_buffer_size > 0) {
        int size = (int)options->recv_buffer_size;
        if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&size, sizeof(size)) == SOCKET_ERROR) {
            return PLATFORM_ERROR_SOCKET_OPTION;
        }
    }

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_socket_create(
    PlatformSocketHandle* handle,
    bool is_tcp,
    const PlatformSocketOptions* options)
{
    if (!handle) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    PlatformSocket* sock = (PlatformSocket*)calloc(1, sizeof(PlatformSocket));
    if (!sock) {
        return PLATFORM_ERROR_OUT_OF_MEMORY;
    }

    sock->is_tcp = is_tcp;
    sock->fd = (int)socket(AF_INET, is_tcp ? SOCK_STREAM : SOCK_DGRAM, 0);
    
    if (sock->fd == INVALID_SOCKET) {
        free(sock);
        return PLATFORM_ERROR_SOCKET_CREATE;
    }

    if (options) {
        sock->opts = *options;
        PlatformErrorCode err = set_socket_options(sock->fd, options);
        if (err != PLATFORM_ERROR_SUCCESS) {
            closesocket(sock->fd);
            free(sock);
            return err;
        }
    }

    *handle = sock;
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_socket_close(PlatformSocketHandle handle) {
    if (!handle) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    closesocket(handle->fd);
    free(handle);
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_socket_connect(
    PlatformSocketHandle handle,
    const PlatformSocketAddress* address)
{
    if (!handle || !address) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    struct addrinfo hints = {0};
    hints.ai_family = address->is_ipv6 ? AF_INET6 : AF_INET;
    hints.ai_socktype = handle->is_tcp ? SOCK_STREAM : SOCK_DGRAM;
    
    struct addrinfo* result = NULL;
    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%u", address->port);
    
    int err = getaddrinfo(address->host, port_str, &hints, &result);
    if (err != 0) {
        return PLATFORM_ERROR_SOCKET_RESOLVE;
    }

    int connect_result = connect(handle->fd, result->ai_addr, (int)result->ai_addrlen);
    freeaddrinfo(result);

    if (connect_result == SOCKET_ERROR) {
        return (WSAGetLastError() == WSAEWOULDBLOCK && !handle->opts.blocking) ? 
               PLATFORM_ERROR_WOULD_BLOCK : PLATFORM_ERROR_SOCKET_CONNECT;
    }

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_socket_bind(
    PlatformSocketHandle handle,
    const PlatformSocketAddress* address)
{
    if (!handle || !address) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    SOCKADDR_STORAGE addr = {0};
    int addr_len;

    if (address->is_ipv6) {
        SOCKADDR_IN6* addr6 = (SOCKADDR_IN6*)&addr;
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons(address->port);
        addr6->sin6_addr = in6addr_any;
        addr_len = sizeof(SOCKADDR_IN6);
    } else {
        SOCKADDR_IN* addr4 = (SOCKADDR_IN*)&addr;
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons(address->port);
        addr4->sin_addr.s_addr = INADDR_ANY;
        addr_len = sizeof(SOCKADDR_IN);
    }

    if (bind(handle->fd, (SOCKADDR*)&addr, addr_len) == SOCKET_ERROR) {
        return PLATFORM_ERROR_SOCKET_BIND;
    }

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_socket_listen(
    PlatformSocketHandle handle,
    int backlog)
{
    if (!handle || !handle->is_tcp) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    if (listen(handle->fd, backlog) == SOCKET_ERROR) {
        return PLATFORM_ERROR_SOCKET_LISTEN;
    }

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_socket_accept(
    PlatformSocketHandle handle,
    PlatformSocketHandle* client_handle,
    PlatformSocketAddress* client_address)
{
    if (!handle || !client_handle || !handle->is_tcp) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    SOCKADDR_STORAGE addr;
    int addr_len = sizeof(addr);

    SOCKET client_fd = accept(handle->fd, (SOCKADDR*)&addr, &addr_len);
    if (client_fd == INVALID_SOCKET) {
        return (WSAGetLastError() == WSAEWOULDBLOCK && !handle->opts.blocking) ? 
               PLATFORM_ERROR_WOULD_BLOCK : PLATFORM_ERROR_SOCKET_ACCEPT;
    }

    PlatformSocket* client = calloc(1, sizeof(PlatformSocket));
    if (!client) {
        closesocket(client_fd);
        return PLATFORM_ERROR_OUT_OF_MEMORY;
    }

    client->fd = (int)((UINT_PTR)client_fd);
    client->is_tcp = true;
    client->opts = handle->opts;

    if (client_address) {
        if (addr.ss_family == AF_INET6) {
            SOCKADDR_IN6* addr6 = (SOCKADDR_IN6*)&addr;
            client_address->is_ipv6 = true;
            client_address->port = ntohs(addr6->sin6_port);
            InetNtop(AF_INET6, &addr6->sin6_addr, 
                    client_address->host, sizeof(client_address->host));
        } else {
            SOCKADDR_IN* addr4 = (SOCKADDR_IN*)&addr;
            client_address->is_ipv6 = false;
            client_address->port = ntohs(addr4->sin_port);
            InetNtop(AF_INET, &addr4->sin_addr, 
                    client_address->host, sizeof(client_address->host));
        }
    }

    *client_handle = client;
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_socket_send(
    PlatformSocketHandle handle,
    const void* buffer,
    size_t length,
    size_t* bytes_sent)
{
    if (!handle || !buffer || !bytes_sent) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    int result = send(handle->fd, (const char*)buffer, (int)length, 0);
    if (result == SOCKET_ERROR) {
        *bytes_sent = 0;
        return (WSAGetLastError() == WSAEWOULDBLOCK && !handle->opts.blocking) ? 
               PLATFORM_ERROR_WOULD_BLOCK : PLATFORM_ERROR_SOCKET_SEND;
    }

    *bytes_sent = result;
    handle->stats.bytes_sent += result;
    handle->stats.packets_sent++;
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_socket_receive(
    PlatformSocketHandle handle,
    void* buffer,
    size_t length,
    size_t* bytes_received)
{
    if (!handle || !buffer || !bytes_received) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    int result = recv(handle->fd, (char*)buffer, (int)length, 0);
    if (result == SOCKET_ERROR) {
        *bytes_received = 0;
        return (WSAGetLastError() == WSAEWOULDBLOCK && !handle->opts.blocking) ? 
               PLATFORM_ERROR_WOULD_BLOCK : PLATFORM_ERROR_SOCKET_RECEIVE;
    }

    if (result == 0) {
        *bytes_received = 0;
        return PLATFORM_ERROR_SOCKET_CLOSED;
    }

    *bytes_received = result;
    handle->stats.bytes_received += result;
    handle->stats.packets_received++;
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_socket_is_connected(
    PlatformSocketHandle handle,
    bool* is_connected)
{
    if (!handle || !is_connected) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    // Try to peek at the socket
    char buffer;
    int result = recv(handle->fd, &buffer, 1, MSG_PEEK);
    
    if (result == 0) {
        *is_connected = false;  // Connection closed
    } else if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        *is_connected = (error != WSAECONNRESET && 
                        error != WSAECONNABORTED && 
                        error != WSAENETRESET);
    } else {
        *is_connected = true;
    }

    return PLATFORM_ERROR_SUCCESS;
}

const char* platform_socket_error_to_string(PlatformErrorCode error) {
    switch (error) {
        case PLATFORM_ERROR_SUCCESS:
            return "No error";
        case PLATFORM_ERROR_INVALID_ARGUMENT:
            return "Invalid parameter";
        case PLATFORM_ERROR_OUT_OF_MEMORY:
            return "Out of memory";
        case PLATFORM_ERROR_NOT_INITIALIZED:
            return "Failed to initialize Winsock";
        case PLATFORM_ERROR_SOCKET_CREATE:
            return "Failed to create socket";
        case PLATFORM_ERROR_SOCKET_OPTION:
            return "Failed to set socket option";
        case PLATFORM_ERROR_SOCKET_BIND:
            return "Failed to bind socket";
        case PLATFORM_ERROR_SOCKET_LISTEN:
            return "Failed to listen on socket";
        case PLATFORM_ERROR_SOCKET_ACCEPT:
            return "Failed to accept connection";
        case PLATFORM_ERROR_SOCKET_CONNECT:
            return "Failed to connect";
        case PLATFORM_ERROR_SOCKET_SEND:
            return "Failed to send data";
        case PLATFORM_ERROR_SOCKET_RECEIVE:
            return "Failed to receive data";
        case PLATFORM_ERROR_SOCKET_CLOSED:
            return "Connection closed";
        case PLATFORM_ERROR_SOCKET_RESOLVE:
            return "Failed to resolve address";
        case PLATFORM_ERROR_WOULD_BLOCK:
            return "Operation would block";
        default:
            return "Unknown error";
    }
}
