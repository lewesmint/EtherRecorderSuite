/**
 * Simple UDP listener test program
 * 
 * This program creates a UDP socket bound to port 5002 and listens for
 * incoming packets using select() for efficient waiting.
 * 
 * Compile with: gcc -o udp_listener_test udp_listener_test.c
 * Run with: ./udp_listener_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    
    typedef SOCKET socket_t;
    #define SOCKET_ERROR_VAL INVALID_SOCKET
    #define close_socket closesocket
    
    // Initialize Winsock
    int init_socket_lib() {
        WSADATA wsaData;
        return WSAStartup(MAKEWORD(2, 2), &wsaData);
    }
    
    // Cleanup Winsock
    void cleanup_socket_lib() {
        WSACleanup();
    }
    
    // Get last error
    int get_last_error() {
        return WSAGetLastError();
    }
    
    // Get error string
    const char* get_error_string(int err) {
        static char buffer[256];
        FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            buffer, sizeof(buffer), NULL);
        return buffer;
    }
    
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    
    typedef int socket_t;
    #define SOCKET_ERROR_VAL (-1)
    #define close_socket close
    
    // No initialization needed for POSIX sockets
    int init_socket_lib() {
        return 0;
    }
    
    // No cleanup needed for POSIX sockets
    void cleanup_socket_lib() {
    }
    
    // Get last error
    int get_last_error() {
        return errno;
    }
    
    // Get error string
    const char* get_error_string(int err) {
        return strerror(err);
    }
#endif

#define UDP_PORT 5002
#define BUFFER_SIZE 4096

// Get current timestamp as string
const char* get_timestamp() {
    static char buffer[64];
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
}

// Print buffer in hex format
void print_hex_dump(const unsigned char* buffer, size_t length) {
    printf("Hex dump (%zu bytes):\n", length);
    
    // Print up to 32 bytes
    size_t print_len = length > 32 ? 32 : length;
    
    for (size_t i = 0; i < print_len; i++) {
        printf("%02X ", buffer[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    
    if (print_len % 16 != 0) printf("\n");
    
    // Try to interpret as text
    printf("ASCII: ");
    for (size_t i = 0; i < print_len; i++) {
        if (buffer[i] >= 32 && buffer[i] <= 126) {
            printf("%c", buffer[i]);
        } else {
            printf(".");
        }
    }
    printf("\n");
}

int main() {
    socket_t sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len;
    unsigned char buffer[BUFFER_SIZE];
    int bytes_received;
    fd_set read_fds;
    struct timeval timeout;
    int result;
    
    // Initialize socket library (Windows only)
    if (init_socket_lib() != 0) {
        printf("Failed to initialize socket library\n");
        return 1;
    }
    
    // Create socket
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == SOCKET_ERROR_VAL) {
        int err = get_last_error();
        printf("Failed to create socket: %s\n", get_error_string(err));
        cleanup_socket_lib();
        return 1;
    }
    
    // Set up server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(UDP_PORT);
    
    // Bind socket
    if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        int err = get_last_error();
        printf("Failed to bind socket to port %d: %s\n", UDP_PORT, get_error_string(err));
        close_socket(sock);
        cleanup_socket_lib();
        return 1;
    }
    
    printf("[%s] UDP listener started on port %d\n", get_timestamp(), UDP_PORT);
    printf("Press Ctrl+C to exit\n\n");
    
    // Packet counters
    unsigned long packets_received = 0;
    unsigned long bytes_total = 0;
    time_t start_time = time(NULL);
    time_t last_stats_time = start_time;
    
    // Main loop
    while (1) {
        // Set up select timeout (1 second)
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        // Wait for data using select
        result = select(sock + 1, &read_fds, NULL, NULL, &timeout);
        
        // Check for select error
        if (result < 0) {
            int err = get_last_error();
            printf("[%s] Select error: %s\n", get_timestamp(), get_error_string(err));
            break;
        }
        
        // Check if socket is readable
        if (result > 0 && FD_ISSET(sock, &read_fds)) {
            // Receive data
            client_addr_len = sizeof(client_addr);
            bytes_received = recvfrom(sock, (char*)buffer, BUFFER_SIZE, 0,
                                     (struct sockaddr*)&client_addr, &client_addr_len);
            
            if (bytes_received < 0) {
                int err = get_last_error();
                printf("[%s] Error receiving data: %s\n", 
                      get_timestamp(), get_error_string(err));
                continue;
            }
            
            // Update statistics
            packets_received++;
            bytes_total += bytes_received;
            
            // Print packet info
            printf("[%s] Received %d bytes from %s:%d (packet #%lu)\n",
                  get_timestamp(), bytes_received,
                  inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port),
                  packets_received);
            
            // Print packet contents
            print_hex_dump(buffer, bytes_received);
            printf("\n");
        }
        
        // Print statistics every 10 seconds
        time_t current_time = time(NULL);
        if (current_time - last_stats_time >= 10) {
            double elapsed = difftime(current_time, start_time);
            double rate = (elapsed > 0) ? packets_received / elapsed : 0;
            
            printf("\n[%s] Statistics:\n", get_timestamp());
            printf("  Running time: %.0f seconds\n", elapsed);
            printf("  Packets received: %lu\n", packets_received);
            printf("  Bytes received: %lu\n", bytes_total);
            printf("  Packet rate: %.2f packets/second\n\n", rate);
            
            last_stats_time = current_time;
        }
    }
    
    // Cleanup
    close_socket(sock);
    cleanup_socket_lib();
    
    return 0;
}