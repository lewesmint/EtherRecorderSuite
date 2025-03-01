#include "command_interface.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <limits.h>  // For INT_MAX
#include <stdlib.h>  // For malloc, free

#include "logger.h"
#include "app_config.h"
#include "app_thread.h"
#include "command_processor.h"
#include "shutdown_handler.h"
#include "comm_threads.h"
#include "platform_sockets.h"


uint16_t gs_listening_port = 4150;

#define START_MARKER  0xBAADF00D
#define END_MARKER    0xDEADBEEF

void init_from_config() {
    gs_listening_port = get_config_uint16("command_interface", "listening_port", 4100);
}

#define COMMAND_BUFFER_SIZE (size_t)(4096)
#define MAX_MESSAGE_SIZE    2016           // Maximum total packet size (16 bytes overhead + 2000 body)
#define TIMEOUT_SEC         5              // Timeout for select()

/* Process result enumeration to differentiate incomplete data from errors */
typedef enum {
    PROCESS_NEED_MORE_DATA = 0,
    PROCESS_OK = 1,
    PROCESS_FAIL = -1
} ProcessResult;

/* State enumeration for the state machine */
typedef enum {
    WAIT_FOR_START,
    WAIT_FOR_LENGTH,
    WAIT_FOR_MESSAGE,
    SEND_ACK
} State;

/* Function pointer type for state handlers */
typedef struct {
    ProcessResult(*process)(SOCKET sock, uint8_t* buffer, size_t* length);
} StateHandler;

/* Global state variables for the protocol */
static State current_state = WAIT_FOR_START;
static uint32_t message_length = 0; // careful of byte order
static uint32_t received_index = 0; // careful of byte order
static uint32_t ack_index = 1;      // ACK counter

/* Buffered stream for receiving data */
typedef struct {
    uint8_t buffer[COMMAND_BUFFER_SIZE];
    size_t buffer_length;
} StreamBuffer;

static StreamBuffer stream = { .buffer_length = 0 };

/* Function Prototypes */
bool wait_for_data(SOCKET sock, int timeout_sec);
int buffered_recv(SOCKET sock);
void consume_buffer(size_t size);

ProcessResult process_wait_for_start(SOCKET sock, uint8_t* buffer, size_t* length);
ProcessResult process_wait_for_length(SOCKET sock, uint8_t* buffer, size_t* length);
ProcessResult process_wait_for_message(SOCKET sock, uint8_t* buffer, size_t* length);
ProcessResult process_send_ack(SOCKET sock, uint8_t* buffer, size_t* length);
void command_interface_loop(SOCKET client_sock);

/* State handler array in order of the state machine */
static StateHandler states[] = {
    { process_wait_for_start },
    { process_wait_for_length },
    { process_wait_for_message },
    { process_send_ack }
};

// Add these state name strings for clarity
static const char* state_names[] = {
    "WAIT_FOR_START",
    "WAIT_FOR_LENGTH",
    "WAIT_FOR_MESSAGE",
    "SEND_ACK"
};

/**
 * @brief Waits for data on a socket without blocking indefinitely.
 */
bool wait_for_data(SOCKET sock, int timeout_sec) {
    fd_set read_fds;
    struct timeval timeout;

    FD_ZERO(&read_fds);
    FD_SET(sock, &read_fds);
    timeout.tv_sec = timeout_sec;
    timeout.tv_usec = 0;

    // The cast is safe because valid socket descriptors are small positive numbers
    int nfds = (int)(sock + 1);  // POSIX needs highest FD + 1, Windows ignores this
    int ret = select(nfds, &read_fds, NULL, NULL, &timeout);
    return (ret > 0);
}

/**
 * @brief Attempts to read data into the buffer.
 * @return Number of bytes read, 0 if no data, or -1 on error.
 */
int buffered_recv(SOCKET sock) {
    if (!wait_for_data(sock, TIMEOUT_SEC)) {
        return 0;  // No data available
    }

    // Calculate available space in the buffer
    size_t available_size = COMMAND_BUFFER_SIZE - stream.buffer_length;

    // Ensure the size is within `int` range for `recv()`
    int bytes_to_read = (available_size > INT_MAX) ? INT_MAX : (int)available_size;

    // Ensure we don’t pass a negative or zero buffer size
    if (bytes_to_read <= 0) {
        logger_log(LOG_WARN, "buffered_recv: No buffer space available.");
        return 0;  // No room to receive more data
    }

    // Read from socket
    int bytes_read = recv(sock, (char*)(stream.buffer + stream.buffer_length), bytes_to_read, 0);
    if (bytes_read <= 0) {
        return -1;  // Connection closed or error
    }

    // Update buffer length
    stream.buffer_length += bytes_read;
    return bytes_read;
}

/**
 * @brief Consumes 'size' bytes from the global buffer.
 */
void consume_buffer(size_t size) {
    if (size > stream.buffer_length) {
        size = stream.buffer_length;
    }
    memmove(stream.buffer, stream.buffer + size, stream.buffer_length - size);
    stream.buffer_length -= size;
}

/**
 * @brief State: Wait for the start marker.
 */
ProcessResult process_wait_for_start(SOCKET sock, uint8_t* buffer, size_t* length) {
    (void)sock;  // Unused parameter
    logger_log(LOG_DEBUG, "top of process_wait_for_start length = %d", *length);
    if (*length < 4) {
        return PROCESS_NEED_MORE_DATA;
    }

    // Log buffer contents for debugging
    for (size_t i = 0; i < *length; i++) {
        char ch = buffer[i];
        if (ch >= 0x20 && ch <= 0x7E) {
            logger_log(LOG_DEBUG, "<- [%04d]: %c", i, ch);
        }
        else {
            logger_log(LOG_DEBUG, "<- [%04d]: 0x%02X", i, (unsigned char)ch);
        }
    }

    uint32_t expected_marker = htonl(START_MARKER);
    if (memcmp(buffer, &expected_marker, 4) != 0) {
        uint32_t received_marker;
        memcpy(&received_marker, buffer, 4);
        // Fix: Remove the second %x format specifier that has no matching argument
        logger_log(LOG_DEBUG, "Invalid Start Marker [0x%08X]", ntohl(received_marker));
        
        // Fix: Consume more bytes to avoid byte-by-byte loop
        // Find the first occurrence of a potential marker byte
        size_t skip = 1;
        for (size_t i = 0; i < *length - 3; i++) {
            if (buffer[i] == ((char*)&expected_marker)[0]) {
                skip = i;
                break;
            }
        }
        
        logger_log(LOG_DEBUG, "Skipping %zu bytes to next potential marker", skip);
        consume_buffer(skip);
        return PROCESS_OK;
    }

    logger_log(LOG_DEBUG, "Valid Start Marker [%x]", START_MARKER);
    consume_buffer(4);
    current_state = WAIT_FOR_LENGTH;
    return PROCESS_OK;
}

/**
 * @brief State: Wait for the message length.
 */
ProcessResult process_wait_for_length(SOCKET sock, uint8_t* buffer, size_t* length) {
    (void)sock;  // Unused parameter
    logger_log(LOG_DEBUG, "top of process_wait_for_length length: %d", *length);
    if (*length < 4) {
        return PROCESS_NEED_MORE_DATA;
    }

    for (size_t i = 0; i < *length; i++) {
        char ch = buffer[i];
        if (ch >= 0x20 && ch <= 0x7E) {
            logger_log(LOG_DEBUG, "<- [%04d]: %c", i, ch);
        }
        else {
            logger_log(LOG_DEBUG, "<- [%04d]: 0x%02X", i, (unsigned char)ch);
        }
    }

    memcpy(&message_length, buffer, 4);
    uint32_t be_message_length = message_length;
    message_length = ntohl(message_length); // Convert from big-endian
    if (message_length < 16 || message_length > MAX_MESSAGE_SIZE) {
        logger_log(LOG_ERROR, "Invalid message length: %u", message_length);
        return PROCESS_FAIL;
    }

    logger_log(LOG_DEBUG, "Valid Message length:  [%u]", message_length);
    logger_log(LOG_DEBUG, "Message length - wrong-endian (in case):  [%u]", be_message_length);
    consume_buffer(4);
    current_state = WAIT_FOR_MESSAGE;
    return PROCESS_OK;
}

/**
 * @brief State: Wait for the rest of the packet (message index, body, end marker).
 *
 * Packet structure (remaining part):
 *   - Message Index: 4 bytes
 *   - Message Body: (message_length - 16) bytes
 *   - End Marker: 4 bytes
 */
ProcessResult process_wait_for_message(SOCKET sock, uint8_t* buffer, size_t* length) {
    (void)sock;  // Unused parameter
    logger_log(LOG_DEBUG, "top of process_wait_for_message length = %d", *length);
    logger_log(LOG_DEBUG, "top of process_wait_for_message message_length = %d", message_length);
    // Since we already consumed 8 bytes (start marker and length), check for remaining packet length.
    if (*length < (message_length - 8)) {
        logger_log(LOG_DEBUG, "need more data");
        return PROCESS_NEED_MORE_DATA;
    }
    for (size_t i = 0; i < *length; i++) {
        char ch = buffer[i];
        if (ch >= 0x20 && ch <= 0x7E) {
            logger_log(LOG_DEBUG, "-< [%04d]: %c", i, ch);
        }
        else {
            logger_log(LOG_DEBUG, "<- [%04d]: 0x%02X", i, (unsigned char)ch);
        }
    }

    logger_log(LOG_DEBUG, "extracted to buffer");
    // Extract message index (first 4 bytes of the remaining buffer)
    memcpy(&received_index, buffer, 4);
    // uint32_t be_received_index = received_index;
    received_index = ntohl(received_index); // Convert from big-endian

    // Calculate message body length
    uint32_t message_body_length = message_length - 16;

    // Verify we have enough buffer for the entire message structure:
    // 4 bytes (index) + message_body_length + 4 bytes (end marker)
    if (4 + message_body_length + 4 > *length) {
        logger_log(LOG_ERROR, "Message structure exceeds available buffer size.");
        consume_buffer(message_length - 8);
        return PROCESS_FAIL;
    }

    // Now safe to access the end marker
    uint32_t received_end_marker;
    memcpy(&received_end_marker, buffer + 4 + message_body_length, 4);
    received_end_marker = ntohl(received_end_marker);
    if (received_end_marker != END_MARKER) {
        logger_log(LOG_ERROR, "Invalid end marker: 0x%08X", received_end_marker);
        consume_buffer(message_length - 8);  // Discard the corrupted message
        return PROCESS_FAIL;
    }

    // Allocate buffer for message body (safely)
    char* message_body = malloc(message_body_length + 1);
    if (!message_body) {
        logger_log(LOG_ERROR, "Memory allocation failed for message body.");
        consume_buffer(message_length - 8);
        return PROCESS_FAIL;
    }

    // Copy the message body and ensure null termination
    memcpy(message_body, buffer + 4, message_body_length);
    message_body[message_body_length] = '\0';  // Ensure null termination

    // Process the command contained in the message
    process_command(message_body);

    logger_log(LOG_INFO, "Received message (index %u): %s", received_index, message_body);

    free(message_body);  // Free allocated memory

    // Consume the full remaining packet (message_length - 8 bytes) from the buffer.
    consume_buffer(message_length - 8);
    current_state = SEND_ACK;
    logger_log(LOG_DEBUG, "bottom of process_wait_for_message");
    return PROCESS_OK;
}

/**
 @brief State: Send an ACK message back.
 *
 * ACK packet structure:
 *   - Start Marker: 4 bytes
 *   - Length: 4 bytes (16 + ack body length)
 *   - ACK Index: 4 bytes
 *   - ACK Body: Variable (e.g. "ACK <received_index>")
 *   - End Marker: 4 bytes
 */
ProcessResult process_send_ack(SOCKET sock, uint8_t* buffer, size_t* length) {
    (void) buffer;
    logger_log(LOG_DEBUG, "top of process_send_ack length = %d", *length);
    char ack_body[32];
    int ack_body_len = snprintf(ack_body, sizeof(ack_body), "ACK %u", received_index);
    if (ack_body_len < 0) {
        logger_log(LOG_ERROR, "print failed for ACK message.");
        return PROCESS_FAIL;
    }

    int32_t ack_packet_length = 16 + ack_body_len;
    uint8_t ack_buffer[256];
    if (ack_packet_length > sizeof(ack_buffer)) {
        logger_log(LOG_ERROR, "ACK message too long.");
        return PROCESS_FAIL;
    }

    uint32_t tmp;
    // Pack start marker
    tmp = htonl(START_MARKER);
    memcpy(ack_buffer, &tmp, 4);

    // Pack length field
    tmp = htonl(ack_packet_length);
    memcpy(ack_buffer + 4, &tmp, 4);

    // Pack ACK index
    tmp = htonl(ack_index++);
    memcpy(ack_buffer + 8, &tmp, 4);

    // Pack ACK body (without null terminator)
    memcpy(ack_buffer + 12, ack_body, ack_body_len);

    // Pack end marker
    tmp = htonl(END_MARKER);
    memcpy(ack_buffer + 12 + ack_body_len, &tmp, 4);

    if (send(sock, (char*)ack_buffer, ack_packet_length, 0) != ack_packet_length) {
        logger_log(LOG_ERROR, "Failed to send ACK.");
        return PROCESS_FAIL;
    }

    for (size_t i = 0; i < ack_packet_length; i++) {
        char ch = ack_buffer[i];
        if (ch >= 0x20 && ch <= 0x7E) {
            logger_log(LOG_DEBUG, "-> [%04d]: %c", i, ch);
        }
        else {
            logger_log(LOG_DEBUG, "-> ch[%04d]: 0x%02X", i, (unsigned char)ch);
        }
    }

    logger_log(LOG_INFO, "Sent ACK %u", ack_index - 1);

    // Reset for next packet
    message_length = 0;
    current_state = WAIT_FOR_START;
    return PROCESS_OK;
}

/**
 * @brief Main command interface loop handling the state machine.
 */ 
void command_interface_loop(SOCKET client_sock) {
    current_state = WAIT_FOR_START;
    while (!shutdown_signalled()) {
        int bytes = buffered_recv(client_sock);
        if (bytes < 0) {
            logger_log(LOG_ERROR, "Connection closed.");
            break;
        }

        // Process as long as there is data in the stream buffer.
        while (stream.buffer_length > 0) {
            logger_log(LOG_DEBUG, "stream.buffer_length: %d. Current State: %d", stream.buffer_length, current_state);
            ProcessResult res;
            switch(current_state) {
                case WAIT_FOR_START:
                    res = process_wait_for_start(client_sock, stream.buffer, &stream.buffer_length);
                    break;
                case WAIT_FOR_LENGTH:
                    res = process_wait_for_length(client_sock, stream.buffer, &stream.buffer_length);
                    break;
                case WAIT_FOR_MESSAGE:
                    res = process_wait_for_message(client_sock, stream.buffer, &stream.buffer_length);
                    break;
                case SEND_ACK:
                    res = process_send_ack(client_sock, stream.buffer, &stream.buffer_length);
                    break;
                default:
                    logger_log(LOG_ERROR, "Unknown state: %d", current_state);
                    res = PROCESS_FAIL;
                    break;
            }

            if (res == PROCESS_FAIL) {
                logger_log(LOG_ERROR, "Error processing packet. Resetting state.");
                current_state = WAIT_FOR_START;
                consume_buffer(stream.buffer_length);
                break;
            }
            else if (res == PROCESS_NEED_MORE_DATA) {
                logger_log(LOG_DEBUG, "Will wait for more data");
                break;  // Wait for more data
            }
            // PROCESS_OK: continue to the next state in the state machine.
            State previous_state = current_state;
            logger_log(LOG_DEBUG, "State transition: %s -> %s", 
                       state_names[previous_state], 
                       state_names[current_state]);
        }

        // If we are in the SEND_ACK state even though the buffer is empty,
        // process the ACK. This covers the case where a complete packet was processed,
        // leaving no extra data in the stream.
        if (current_state == SEND_ACK) {
            logger_log(LOG_DEBUG, "Processing SEND_ACK state with empty buffer");
            ProcessResult res = process_send_ack(client_sock, stream.buffer, &stream.buffer_length);
            if (res == PROCESS_FAIL) {
                logger_log(LOG_ERROR, "Error processing ack state.");
                // Optionally, reset state here if needed:
                current_state = WAIT_FOR_START;
            }
        }
        logger_log(LOG_DEBUG, "Outside while loop, current State: %d", current_state);
    }

    close_socket(&client_sock);
    logger_log(LOG_INFO, "Client disconnected.");
}

/**
 * Command Interface thread Function
 */
void* command_interface_thread_function(void* arg) {
    (void) arg;
    // AppThread_T* thread_info = (AppThread_T*)arg;
    init_from_config();

    // bool is_tcp = true;
    // bool is_server = true;
    SOCKET sock = INVALID_SOCKET;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    // Handles everything required for the server to listen for incoming connections
    sock = setup_listening_server_socket(&addr, gs_listening_port);
    if (sock == INVALID_SOCKET) {
        logger_log(LOG_ERROR, "Server setup failed.");
        return NULL;
    }

    logger_log(LOG_INFO, "%s is listening on port %d", get_thread_label(), gs_listening_port);

    // Main loop: Accept and handle client connections
    while (!shutdown_signalled()) {
        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);  // Use int for Windows compatibility
        memset(&client_addr, 0, sizeof(client_addr));

        SOCKET client_sock = accept(sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock == INVALID_SOCKET) {
            if (shutdown_signalled())
                break;  // Graceful shutdown

            logger_log(LOG_ERROR, "Accept failed: %s", socket_error_string());
            continue;
        }

        logger_log(LOG_INFO, "Client connected.");

        // Handle client communication
        command_interface_loop(client_sock);

        // Handle client disconnection
        close_socket(&client_sock);
        logger_log(LOG_INFO, "Client disconnected. Waiting for a new connection...");
    }

    /* Final cleanup before exiting */
    logger_log(LOG_INFO, "%s is shutting down...", get_thread_label());
    close_socket(&sock);
    return NULL;
}