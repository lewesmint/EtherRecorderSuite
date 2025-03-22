#include "platform_sockets.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

#include "platform_time.h"
#include "platform_error.h"

PlatformErrorCode platform_socket_init(void) {
    return PLATFORM_ERROR_SUCCESS; // No initialization needed for POSIX
}

void platform_socket_cleanup(void) {
    // No cleanup needed for POSIX
}

static PlatformErrorCode set_socket_options(int fd, const PlatformSocketOptions* options) {
    if (!options) {
        return PLATFORM_ERROR_SUCCESS;
    }

    // Set non-blocking mode
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        flags = options->blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
        if (fcntl(fd, F_SETFL, flags) < 0) {
            return PLATFORM_ERROR_SOCKET_OPTION;
        }
    }

    // Set reuse address
    int reuse = options->reuse_address ? 1 : 0;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        return PLATFORM_ERROR_SOCKET_OPTION;
    }

    // Set keep alive
    int keepalive = options->keep_alive ? 1 : 0;
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)) < 0) {
        return PLATFORM_ERROR_SOCKET_OPTION;
    }

    // Set send/receive timeouts
    if (options->send_timeout_ms > 0) {
        struct timeval tv = {
            .tv_sec = options->send_timeout_ms / PLATFORM_MS_PER_SEC,
            .tv_usec = (options->send_timeout_ms % PLATFORM_MS_PER_SEC) * PLATFORM_US_PER_MS
        };
        if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
            return PLATFORM_ERROR_SOCKET_OPTION;
        }
    }

    if (options->recv_timeout_ms > 0) {
        struct timeval tv = {
            .tv_sec = options->recv_timeout_ms / PLATFORM_MS_PER_SEC,
            .tv_usec = (options->recv_timeout_ms % PLATFORM_MS_PER_SEC) * PLATFORM_US_PER_MS
        };
        if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            return PLATFORM_ERROR_SOCKET_OPTION;
        }
    }

    // Set buffer sizes
    if (options->send_buffer_size > 0) {
        if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &options->send_buffer_size, 
                      sizeof(options->send_buffer_size)) < 0) {
            return PLATFORM_ERROR_SOCKET_OPTION;
        }
    }

    if (options->recv_buffer_size > 0) {
        if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &options->recv_buffer_size, 
                      sizeof(options->recv_buffer_size)) < 0) {
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

    PlatformSocket* sock = (PlatformSocket*)malloc(sizeof(PlatformSocket));
    if (!sock) {
        return PLATFORM_ERROR_OUT_OF_MEMORY;
    }

    // Initialize the socket structure
    memset(sock, 0, sizeof(PlatformSocket));
    sock->is_tcp = is_tcp;
    
    // Create the socket
    sock->fd = socket(AF_INET, 
                     is_tcp ? SOCK_STREAM : SOCK_DGRAM, 
                     0);
    if (sock->fd == -1) {
        free(sock);
        return PLATFORM_ERROR_SOCKET_CREATE;
    }

    // Set socket options if provided
    if (options) {
        sock->opts = *options;
        PlatformErrorCode err = set_socket_options(sock->fd, options);
        if (err != PLATFORM_ERROR_SUCCESS) {
            close(sock->fd);
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

    close(handle->fd);
    free(handle);
    return PLATFORM_ERROR_SUCCESS;
}

static PlatformErrorCode map_connect_error(int error) {
    switch (error) {
        case ENETUNREACH:
        case EHOSTUNREACH:
            return PLATFORM_ERROR_HOST_NOT_FOUND;
        case ECONNREFUSED:
            return PLATFORM_ERROR_CONNECTION_REFUSED;
        case ETIMEDOUT:
            return PLATFORM_ERROR_TIMEOUT;
        case ENETDOWN:
            return PLATFORM_ERROR_NETWORK_DOWN;
        default:
            return PLATFORM_ERROR_SOCKET_CONNECT;
    }
}

static struct addrinfo* resolve_address(const PlatformSocketAddress* address, bool is_tcp) {
    struct addrinfo hints = {0};
    hints.ai_family = address->is_ipv6 ? AF_INET6 : AF_INET;
    hints.ai_socktype = is_tcp ? SOCK_STREAM : SOCK_DGRAM;
    
    struct addrinfo* result = NULL;
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%u", address->port);
    
    return (getaddrinfo(address->host, port_str, &hints, &result) == 0) ? result : NULL;
}

static PlatformErrorCode try_nonblocking_connect(
    int fd, 
    const struct addrinfo* addr)
{
    int connect_result = connect(fd, addr->ai_addr, addr->ai_addrlen);
    if (connect_result < 0) {
        if (errno == EINPROGRESS || errno == EWOULDBLOCK) {
            return PLATFORM_ERROR_WOULD_BLOCK;
        }
        return map_connect_error(errno);
    }
    return PLATFORM_ERROR_SUCCESS;
}

static PlatformErrorCode set_nonblocking_mode(int fd, int* original_flags) {
    *original_flags = fcntl(fd, F_GETFL, 0);
    if (*original_flags == -1) {
        return PLATFORM_ERROR_SOCKET_OPTION;
    }

    if (fcntl(fd, F_SETFL, *original_flags | O_NONBLOCK) == -1) {
        return PLATFORM_ERROR_SOCKET_OPTION;
    }

    return PLATFORM_ERROR_SUCCESS;
}

static void restore_blocking_mode(int fd, int original_flags) {
    fcntl(fd, F_SETFL, original_flags);
}

static PlatformErrorCode wait_for_connection(
    int fd, 
    int timeout_ms)
{
    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(fd, &write_fds);

    struct timeval timeout = {
        .tv_sec = timeout_ms / PLATFORM_MS_PER_SEC,
        .tv_usec = (timeout_ms % PLATFORM_MS_PER_SEC) * PLATFORM_US_PER_MS
    };

    int select_result = select(fd + 1, NULL, &write_fds, NULL, &timeout);

    if (select_result == 0) {
        return PLATFORM_ERROR_TIMEOUT;
    }
    if (select_result < 0) {
        return PLATFORM_ERROR_SOCKET_CONNECT;
    }

    if (FD_ISSET(fd, &write_fds)) {
        int error;
        socklen_t len = sizeof(error);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0) {
            return error == 0 ? PLATFORM_ERROR_SUCCESS : map_connect_error(error);
        }
    }

    return PLATFORM_ERROR_SOCKET_CONNECT;
}


PlatformErrorCode platform_socket_connect(
    PlatformSocketHandle handle,
    const PlatformSocketAddress* address)
{
    if (!handle || !address) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    struct addrinfo* addr_info = resolve_address(address, handle->is_tcp);
    if (!addr_info) {
        return PLATFORM_ERROR_SOCKET_RESOLVE;
    }

    PlatformErrorCode result;

    // Handle non-blocking sockets
    if (!handle->opts.blocking) {
        result = try_nonblocking_connect(handle->fd, addr_info);
        freeaddrinfo(addr_info);
        return result;
    }

    // Handle blocking sockets with timeout
    int original_flags;
    result = set_nonblocking_mode(handle->fd, &original_flags);
    if (result != PLATFORM_ERROR_SUCCESS) {
        freeaddrinfo(addr_info);
        return result;
    }

    // Attempt initial connection
    result = try_nonblocking_connect(handle->fd, addr_info);
    freeaddrinfo(addr_info);

    if (result == PLATFORM_ERROR_SUCCESS) {
        restore_blocking_mode(handle->fd, original_flags);
        return PLATFORM_ERROR_SUCCESS;
    }

    if (result != PLATFORM_ERROR_WOULD_BLOCK) {
        restore_blocking_mode(handle->fd, original_flags);
        return result;
    }

    // Wait for connection completion
    result = wait_for_connection(handle->fd, handle->opts.connect_timeout_ms);
    restore_blocking_mode(handle->fd, original_flags);
    
    return result;
}

PlatformErrorCode platform_socket_bind(
    PlatformSocketHandle handle,
    const PlatformSocketAddress* address)
{
    if (!handle || !address) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    struct sockaddr_storage addr = {0};
    socklen_t addr_len;

    if (address->is_ipv6) {
        struct sockaddr_in6* addr6 = (struct sockaddr_in6*)&addr;
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons(address->port);
        addr6->sin6_addr = in6addr_any; // Bind to all interfaces
        addr_len = sizeof(struct sockaddr_in6);
    } else {
        struct sockaddr_in* addr4 = (struct sockaddr_in*)&addr;
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons(address->port);
        addr4->sin_addr.s_addr = INADDR_ANY; // Bind to all interfaces
        addr_len = sizeof(struct sockaddr_in);
    }

    if (bind(handle->fd, (struct sockaddr*)&addr, addr_len) < 0) {
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

    if (listen(handle->fd, backlog) < 0) {
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

    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);

    int client_fd = accept(handle->fd, (struct sockaddr*)&addr, &addr_len);
    if (client_fd < 0) {
        return (errno == EWOULDBLOCK && !handle->opts.blocking) ? 
               PLATFORM_ERROR_WOULD_BLOCK : PLATFORM_ERROR_SOCKET_ACCEPT;
    }

    PlatformSocket* client = calloc(1, sizeof(PlatformSocket));
    if (!client) {
        close(client_fd);
        return PLATFORM_ERROR_OUT_OF_MEMORY;
    }

    client->fd = client_fd;
    client->is_tcp = true;
    client->opts = handle->opts;

    if (client_address) {
        if (addr.ss_family == AF_INET6) {
            struct sockaddr_in6* addr6 = (struct sockaddr_in6*)&addr;
            client_address->is_ipv6 = true;
            client_address->port = ntohs(addr6->sin6_port);
            inet_ntop(AF_INET6, &addr6->sin6_addr, 
                     client_address->host, sizeof(client_address->host));
        } else {
            struct sockaddr_in* addr4 = (struct sockaddr_in*)&addr;
            client_address->is_ipv6 = false;
            client_address->port = ntohs(addr4->sin_port);
            inet_ntop(AF_INET, &addr4->sin_addr, 
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

    *bytes_sent = 0;
    
    ssize_t sent = send(handle->fd, buffer, length, 0);
    if (sent < 0) {
        if (errno == EWOULDBLOCK && !handle->opts.blocking) {
            return PLATFORM_ERROR_WOULD_BLOCK;
        }
        return PLATFORM_ERROR_SOCKET_SEND;
    }

    *bytes_sent = (size_t)sent;
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

    *bytes_received = 0;

    ssize_t received = recv(handle->fd, buffer, length, 0);
    if (received < 0) {
        if (errno == EWOULDBLOCK && !handle->opts.blocking) {
            return PLATFORM_ERROR_WOULD_BLOCK;  // Timeout for non-blocking
        }
        return PLATFORM_ERROR_SOCKET_RECEIVE;   // Other errors
    }
    
    // A return value of 0 from recv() indicates that the peer has performed
    // an orderly shutdown (FIN packet received). This is different from
    // a timeout or error condition - it means the remote end has explicitly
    // closed their side of the connection.
    if (received == 0) {
        return PLATFORM_ERROR_PEER_SHUTDOWN;  // More specific than CONNECTION_CLOSED
    }

    *bytes_received = (size_t)received;
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_socket_is_connected(
    PlatformSocketHandle handle,
    bool* is_connected)
{
    if (!handle || !is_connected) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    int error = 0;
    socklen_t len = sizeof(error);
    int result = getsockopt(handle->fd, SOL_SOCKET, SO_ERROR, &error, &len);
    
    *is_connected = (result == 0 && error == 0);
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_socket_get_stats(
    PlatformSocketHandle handle,
    PlatformSocketStats* stats)
{
    if (!handle || !stats) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    *stats = handle->stats;
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_socket_error_string(
    PlatformErrorCode error_code,
    char* buffer,
    size_t buffer_size,
    const char* additional_info)
{
    if (!buffer || buffer_size == 0) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    const char* base_message;
    switch (error_code) {
        case PLATFORM_ERROR_SUCCESS:
            base_message = "No error";
            break;
        case PLATFORM_ERROR_SOCKET_CREATE:
            base_message = "Failed to create socket";
            break;
        case PLATFORM_ERROR_SOCKET_BIND:
            base_message = "Failed to bind socket";
            break;
        case PLATFORM_ERROR_SOCKET_LISTEN:
            base_message = "Failed to listen on socket";
            break;
        // ... other cases ...
        default:
            base_message = "Unknown error";
            break;
    }

    strncpy(buffer, base_message, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';

    if (additional_info && *additional_info) {
        size_t current_len = strlen(buffer);
        if (current_len < buffer_size - 2) {
            strncat(buffer, ": ", buffer_size - current_len - 1);
            strncat(buffer, additional_info, buffer_size - current_len - 2);
        }
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
        case PLATFORM_ERROR_SOCKET_CREATE:
            return "Failed to create socket";
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
            return "Local socket is closed or invalid";
        case PLATFORM_ERROR_SOCKET_SELECT:
            return "Socket select operation failed";
        case PLATFORM_ERROR_PEER_SHUTDOWN:
            return "Remote peer performed orderly shutdown";
        default:
            return "Unknown error";
    }
}

PlatformErrorCode platform_socket_wait_readable(
    PlatformSocketHandle handle,
    uint32_t timeout_ms)
{
    if (!handle) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    // Validate file descriptor
    if (handle->fd < 0) {
        return PLATFORM_ERROR_SOCKET_CLOSED;
    }

    // Check if fd is within the maximum allowed range
    // __DARWIN_FD_SETSIZE is typically 1024 on macOS
    if (handle->fd >= __DARWIN_FD_SETSIZE) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    // Initialize fd_set structure
    fd_set read_fds;
    FD_ZERO(&read_fds);  // Clear all bits first
    
    // Safely set the bit for our file descriptor
    if (fcntl(handle->fd, F_GETFD) == -1) {
        // Socket is invalid or closed
        return PLATFORM_ERROR_SOCKET_CLOSED;
    }
    
    FD_SET(handle->fd, &read_fds);

    struct timeval tv = {
        .tv_sec = timeout_ms / 1000,
        .tv_usec = (timeout_ms % 1000) * 1000
    };

    int result = select(handle->fd + 1, &read_fds, NULL, NULL, &tv);
    
    if (result < 0) {
        if (errno == EBADF) {
            return PLATFORM_ERROR_SOCKET_CLOSED;
        }
        return PLATFORM_ERROR_SOCKET_SELECT;
    }
    
    if (result == 0) {
        return PLATFORM_ERROR_TIMEOUT;
    }

    // Double check the fd is still valid and set
    if (!FD_ISSET(handle->fd, &read_fds)) {
        return PLATFORM_ERROR_SOCKET_CLOSED;
    }

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_socket_wait_writable(
    PlatformSocketHandle handle,
    uint32_t timeout_ms)
{
    if (!handle) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    // Use default timeout if none specified (0)
    if (timeout_ms == 0) {
        timeout_ms = PLATFORM_DEFAULT_WAIT_TIMEOUT_MS;
    }
    // Clamp timeout to valid range
    else if (timeout_ms > PLATFORM_MAX_WAIT_TIMEOUT_MS) {
        timeout_ms = PLATFORM_MAX_WAIT_TIMEOUT_MS;
    } else if (timeout_ms < PLATFORM_MIN_WAIT_TIMEOUT_MS) {
        timeout_ms = PLATFORM_MIN_WAIT_TIMEOUT_MS;
    }

    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(handle->fd, &write_fds);

    struct timeval tv = {
        .tv_sec = timeout_ms / PLATFORM_MS_PER_SEC,
        .tv_usec = (timeout_ms % PLATFORM_MS_PER_SEC) * PLATFORM_US_PER_MS
    };

    int result = select(handle->fd + 1, NULL, &write_fds, NULL, &tv);
    if (result < 0) {
        return PLATFORM_ERROR_SOCKET_SELECT;
    }
    if (result == 0) {
        return PLATFORM_ERROR_TIMEOUT;
    }
    
    return PLATFORM_ERROR_SUCCESS;
}

uint32_t platform_ntohl(uint32_t netlong) {
    return ntohl(netlong);
}

uint32_t platform_htonl(uint32_t hostlong) {
    return htonl(hostlong);
}
