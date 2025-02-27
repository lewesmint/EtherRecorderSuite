#include "common_socket.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include "platform_utils.h"
#include "logger.h"

extern bool shutdown_signalled(void);


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
 * Generates a payload with:
 *   - start_marker = START_MARKER
 *   - data_length = (random block count) * 4
 *   - random bytes in data[]
 *   - a 4-byte END_MARKER at the end of data
 * Returns: total valid data size (bytes), including overhead.
 */
int generateRandomData(DummyPayload* packet) {
    if (!packet) {
        return -1; // Invalid pointer
    }

    // Write the start marker
    packet->start_marker = START_MARKER;

    // Choose a random number of blocks from MIN_BLOCKS to MAX_BLOCKS (inclusive)

    int numBlocks = MIN_BLOCKS + (platform_random() % (MAX_BLOCKS - MIN_BLOCKS + 1));
    // unsigned int random = 944;
    // if (rand_s(&random) != 0) {
    //      return -1;
    // }
    // int numBlocks = MIN_BLOCKS + (random % (MAX_BLOCKS - MIN_BLOCKS + 1));
    logger_log(LOG_DEBUG, "Generating random data with %d blocks", numBlocks);

    // Convert block count to total bytes of random data
    packet->data_length = numBlocks * 4;

    // Fill the data area with random bytes
    for (unsigned int i = 0; i < packet->data_length; i++) {
        packet->data[i] = (unsigned char)(platform_random() % 256);
    }

    // // Fill the data area with random bytes
    // for (int i = 0; i < packet->data_length; i++) {
    //     if (rand_s(&random) != 0) {
    //         return -1;
    //     }
    //     packet->data[i] = (unsigned char)(random % 256);
    // }

    // Write the 4-byte END_MARKER at the correct position
    int marker_pos = packet->data_length;
    unsigned int end_marker = END_MARKER;
    memcpy(&packet->data[marker_pos], &end_marker, sizeof(end_marker));

    // Correctly return the valid payload size (including overhead)
    return sizeof(packet->start_marker) +  // 4 bytes
           sizeof(packet->data_length) +   // 4 bytes
           packet->data_length +           // variable data size
           sizeof(end_marker);             // 4 bytes
}

SOCKET setup_listening_server_socket(struct sockaddr_in* addr, int port) {

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        return PLATFORM_SOCKET_ERROR_CREATE;
    }

    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr = INADDR_ANY; // Bind to all available interfaces
    if (bind(sock, (struct sockaddr*)addr, sizeof(*addr)) == SOCKET_ERROR) {
        close_socket(&sock);
        return PLATFORM_SOCKET_ERROR_BIND;
    }
    if (listen(sock, SOMAXCONN) == SOCKET_ERROR) {
        close_socket(&sock);
        return PLATFORM_SOCKET_ERROR_LISTEN;    
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
SOCKET setup_socket(bool is_server, bool is_tcp, struct sockaddr_in *addr, struct sockaddr_in *client_addr, const char *host, int port) {
    SOCKET sock = socket(AF_INET, is_tcp ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        return PLATFORM_SOCKET_ERROR_CREATE;
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
            return PLATFORM_SOCKET_ERROR_RESOLVE;
        }

        // Copy resolved address into `addr`
        struct sockaddr_in *resolved_addr = (struct sockaddr_in *)res->ai_addr;
        addr->sin_addr = resolved_addr->sin_addr;
        freeaddrinfo(res);
    }

    if (is_server) {
        if (bind(sock, (struct sockaddr *)addr, sizeof(*addr)) == SOCKET_ERROR) {
            close_socket(&sock);
            return PLATFORM_SOCKET_ERROR_BIND;
        }
        if (is_tcp && listen(sock, SOMAXCONN) == SOCKET_ERROR) {
            close_socket(&sock);
            return PLATFORM_SOCKET_ERROR_LISTEN;
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

        result = select((int)sock + 1, NULL, &write_set, NULL, &timeout_val);
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


///**
// * Sends data reliably, ensuring the full buffer is sent over TCP.
// */
//int send_all_data(SOCKET sock, void *data, int buffer_size, int is_tcp, struct sockaddr_in *client_addr, socklen_t addr_len) {
//    // hex_dump(data, buffer_size);
//
//    int total_sent = 0;
//
//    if (is_tcp) {
//        while (total_sent < buffer_size) {
//            int bytes_sent = send(sock, ((char *)data) + total_sent, buffer_size - total_sent, 0);
//            if (bytes_sent <= 0) {
//                print_socket_error("Error on send");
//                return -1;
//            }
//            total_sent += bytes_sent;
//        }
//    } else {
//        if (sendto(sock, data, buffer_size, 0, (struct sockaddr *)client_addr, addr_len) <= 0) {
//            print_socket_error("Error on sendto");
//            return -1;
//        }
//    }
//
//    return 0;
//}

// Include socket headers here (e.g. winsock2.h or sys/socket.h, etc.)


// Define an arbitrary buffer size for TCP stream accumulation.
#define BUFFER_SIZE   65536

// Dummy logger definitions.
#define LOG_ERROR 1
#define LOG_DEBUG 2

// Stub function to process a complete payload.
void process_payload(const char *payload, unsigned int payload_length) {
    logger_log(LOG_DEBUG, "Processing payload of length %u", payload_length);
    // Insert processing code here.
}

/**
 * Searches the given buffer (of length buffer_length) for the 4-byte marker.
 *
 * @param buffer         The buffer to search.
 * @param buffer_length  The number of valid bytes in the buffer.
 * @param marker         The 4-byte marker to search for.
 *
 * @return The index where the marker begins, or -1 if not found.
 */
int find_marker_in_buffer(const char *buffer, int buffer_length, unsigned int marker) {
    if (marker != START_MARKER && marker != END_MARKER) {
        return -1;
    } else if (buffer == NULL) {
        return -1;
    } else if (buffer_length < 4) {
        return -1;
    } else {
        if (marker == START_MARKER) {
            logger_log(LOG_DEBUG, "Searching for START_MARKER marker 0x%X in buffer of length %d", marker, buffer_length);
        } else {
            logger_log(LOG_DEBUG, "Searching for END_MARKER marker 0x%X in buffer of length %d", marker, buffer_length);
        }
        for (int i = 0; i <= buffer_length - 4; i++) {
            unsigned int candidate;
            memcpy(&candidate, buffer + i, sizeof(candidate));
            if (candidate == marker) {
                if (marker == START_MARKER) {
                    logger_log(LOG_DEBUG, "Found START_MARKER  at index %d", i);
                } else {
                    logger_log(LOG_DEBUG, "Found END_MARKER at index %d", i);
                }
                return i;
            }
        }
        return -1;
    }
}

/**
 * Reads from a TCP stream, extracts complete packets and processes them.
 *
 * Packet format:
 *   - 4 bytes: START_MARKER (0xBAADF00D)
 *   - 4 bytes: payload length (unsigned int) - number of payload bytes.
 *   - N bytes: payload data (N == payload length)
 *   - 4 bytes: END_MARKER (0xDEADBEEF)
 *
 * If the END_MARKER is not found where expected, an error is logged and the
 * code searches for the next START_MARKER.
 *
 * @param sock  The TCP socket to read from.
 */
/**
 * Reads from the TCP stream until a complete packet is found,
 * then copies the complete packet into out_buffer.
 *
 * Packet format:
 *   - 4 bytes: START_MARKER (0xBAADF00D)
 *   - 4 bytes: payload length (unsigned int)
 *   - N bytes: payload data (N == payload length)
 *   - 4 bytes: END_MARKER (0xDEADBEEF)
 *
 * Returns the total packet size on success, or -1 on error.
 */
int process_tcp_stream(SOCKET sock, void *out_buffer, int out_buffer_size) {
    // Static accumulation buffer for the TCP stream.
    static char buffer[BUFFER_SIZE];
    static int buffered_length = 0;

    while (1) {
        // Read data from the socket.
        int bytes_to_read = BUFFER_SIZE - buffered_length;
        if (bytes_to_read <= 0) {
            logger_log(LOG_ERROR, "Buffer overflow; clearing buffer.");
            buffered_length = 0;
            continue;
        }

        int bytes_received = recv(sock, buffer + buffered_length, bytes_to_read, 0);
        if (bytes_received <= 0) {
            // An error occurred or the connection was closed.
            logger_log(LOG_ERROR, "recv error or connection closed (received %d bytes)", bytes_received);
            return -1;
        }
        logger_log(LOG_DEBUG, "Received %d bytes", bytes_received);
        buffered_length += bytes_received;

        // Process the buffer for complete packets.
        while (1) {
            // Look for a START_MARKER in the buffered data.
            int start_index = find_marker_in_buffer(buffer, buffered_length, START_MARKER);
            if (start_index < 0) {
                logger_log(LOG_DEBUG, "No START_MARKER in current batch");
                // To handle the possibility that the marker might be split
                // across recv() calls, keep the last 3 bytes.
                if (buffered_length > 3) {
                    memmove(buffer, buffer + buffered_length - 3, 3);
                    buffered_length = 3;
                }
                break;
            }
            logger_log(LOG_DEBUG, "Found START_MARKER index = %d", start_index);
            // Discard any data before the start marker.
            if (start_index > 0) {
                memmove(buffer, buffer + start_index, buffered_length - start_index);
                buffered_length -= start_index;
            }

            // Ensure we have at least 8 bytes (start marker + payload length).
            if (buffered_length < 8) {
                // Not enough data yet.
                break;
            }

            // Read the payload length from the 4 bytes following the start marker.
            unsigned int payload_length = 0;
            memcpy(&payload_length, buffer + 4, sizeof(payload_length));

            // Calculate the total expected packet size.
            unsigned int total_packet_size = 4 + 4 + payload_length + 4;
            if (buffered_length < (int)total_packet_size) {
                // The complete packet has not yet been received.
                break;
            }

            // Check for the end marker.
            unsigned int end_marker = 0;
            memcpy(&end_marker, buffer + 8 + payload_length, 4);
            if (end_marker != END_MARKER) {
                logger_log(LOG_ERROR,
                           "End marker not found where expected. Expected 0x%X, got 0x%X.",
                           END_MARKER, end_marker);
                // Discard the first byte (or you might choose a different strategy)
                // and search again for a new start marker.
                memmove(buffer, buffer + 1, buffered_length - 1);
                buffered_length -= 1;
                continue;
            }

            // A complete, valid packet has been found.
            if (total_packet_size > (unsigned int)out_buffer_size) {
                logger_log(LOG_ERROR, "Output buffer too small: required %d, available %d", total_packet_size, out_buffer_size);
                // Remove the faulty packet from the buffer.
                memmove(buffer, buffer + total_packet_size, buffered_length - total_packet_size);
                buffered_length -= total_packet_size;
                return -1;
            }

            // Copy the complete packet to out_buffer.
            memcpy(out_buffer, buffer, total_packet_size);

            // Remove the processed packet from the buffer.
            if (buffered_length > (int)total_packet_size) {
                memmove(buffer, buffer + total_packet_size, buffered_length - total_packet_size);
            }
            buffered_length -= total_packet_size;

            // Return the complete packet size.
            return total_packet_size;
        }
    }
}



/**
 * Receives data reliably, ensuring no partial reads for TCP.
 */
//int receive_all_data(SOCKET sock, void *data, int buffer_size, int is_tcp,
//                     struct sockaddr_in *client_addr, socklen_t *addr_len) {
//    if (is_tcp) {
//        // For TCP, process_tcp_stream now returns a complete packet in the provided buffer.
//        int total_received = process_tcp_stream(sock, data, buffer_size);
//        return total_received;
//    } else {
//        int bytes_received = recvfrom(sock, data, buffer_size, 0, (struct sockaddr *)client_addr, addr_len);
//        if (bytes_received <= 0) {
//            print_socket_error("Error on recvfrom");
//            return -1;
//        }
//        return bytes_received;
//    }
//}

/**
 * The main communication loop, ensuring full sends/receives.
 */
void communication_loop(SOCKET sock, int is_server, int is_tcp, struct sockaddr_in *client_addr) {
    int addr_len = sizeof(struct sockaddr_in);
    bool valid_socket_connection = true;

    while (valid_socket_connection) {
        // Check for shutdown condition if needed.
        if (shutdown_signalled()) {
            logger_log(LOG_DEBUG, "Shutdown requested, exiting communication loop.");
            break;
        }

        DummyPayload sendPayload;
        int send_buffer_len = generateRandomData(&sendPayload);
        if (send_buffer_len > 0) {
            logger_log(LOG_DEBUG, "Sending %d bytes of data.", send_buffer_len);
            //if (send_all_data(sock, &sendPayload, send_buffer_len, is_tcp, client_addr, addr_len) != 0) {
            //    logger_log(LOG_ERROR, "Send error. Exiting loop.");
            //    valid_socket_connection = false;
            //    close_socket(&sock);
            //    break;
            //}
            logger_log(LOG_DEBUG, "Sleeping in comms loop");
            sleep_ms(100);
            logger_log(LOG_DEBUG, "Woken in comms loop");
        }

        DummyPayload receivePayload;
        int receive_buffer_len = sizeof(DummyPayload);
        logger_log(LOG_DEBUG, "Entering receive_all_data");
        //int packet_size = receive_all_data(sock, &receivePayload, receive_buffer_len, is_tcp, client_addr, &addr_len);
        //if (packet_size <= 0) {
        //    logger_log(LOG_ERROR, "Receive error. Exiting loop.");
        //    valid_socket_connection = false;
        //    close_socket(&sock);
        //    break;
        //}
//        logger_log(LOG_DEBUG, "Received complete packet (%d bytes)", packet_size);

        // Extract payload length from the received packet.
        unsigned int payload_length = 0;
        memcpy(&payload_length, ((char *)&receivePayload) + 4, sizeof(payload_length));

        // Process the received dummy packet.
        // The payload is located after the start marker and the length field (i.e. at offset 8).
        process_payload(((char *)&receivePayload) + 8, payload_length);

        logger_log(LOG_DEBUG, "Bottom of comms loop while valid connection");
    }
}


// /**
//  * The main communication loop, ensuring full sends/receives.
//  */
// void communication_loop(SOCKET sock, int is_server, int is_tcp, struct sockaddr_in *client_addr) {
//     int addr_len = sizeof(struct sockaddr_in);
//     bool valid_socket_connection = true;

//     while (valid_socket_connection) {
//         DummyPayload sendPayload;
//         int send_buffer_len = generateRandomData(&sendPayload);
//         logger_log(LOG_DEBUG, "Sending %d bytes of data... 1", send_buffer_len);
//         if (send_buffer_len > 0) {
//             logger_log(LOG_DEBUG, "Sending %d bytes of data... 2", send_buffer_len);
//             if (send_all_data(sock, &sendPayload, send_buffer_len, is_tcp, client_addr, addr_len) != 0) {
//                 logger_log(LOG_ERROR, "Send error. Exiting loop.");
//                 valid_socket_connection = false;
//                 close_socket(&sock);
//                 break;
//             }
//             logger_log(LOG_DEBUG, "Sleeping");
//             sleep_ms(100);
//         }

//         DummyPayload receivePayload;
//         int receive_buffer_len = sizeof(DummyPayload);
//         logger_log(LOG_DEBUG, "Entering receive_all_data");
//         if (receive_all_data(sock, &receivePayload, receive_buffer_len, is_tcp, client_addr, &addr_len) != receive_buffer_len) {
//             logger_log(LOG_ERROR, "Receive error. Exiting loop.");
//             valid_socket_connection = false;
//             close_socket(&sock);
//             break;
//         }
//         logger_log(LOG_DEBUG, "Bottom of loop coms loop while vlid connection");
//     }
// }
