#include "command_interface.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <limits.h>  // For INT_MAX

#include "common_socket.h"
#include "logger.h"
#include "app_config.h"
#include "app_thread.h"
#include "command_processor.h"


// Global shutdown event handle.
extern HANDLE shutdown_event = NULL;

uint32_t gs_listening_port = 4150;

void init_from_config() {
    gs_listening_port = get_config_int("command_interface", "listening_port", 4100);
}

#define BUFFER_SIZE       (size_t)(4096)
#define MAX_MESSAGE_SIZE  2016        // Maximum total packet size (16 bytes overhead + 2000 body)
#define TIMEOUT_SEC       5           // Timeout for select()

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
static uint32_t message_length = 0; //careful of byte order
static uint32_t received_index = 0; // carful of byte order
static uint32_t ack_index = 1;  // ACK counter

/* Buffered stream for receiving data */
typedef struct {
    uint8_t buffer[BUFFER_SIZE];
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
void command_interface_loop(SOCKET client_sock, struct sockaddr_in* client_addr);

/* State handler array in order of the state machine */
static StateHandler states[] = {
    { process_wait_for_start },
    { process_wait_for_length },
    { process_wait_for_message },
    { process_send_ack }
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

    int ret = select((int)sock + 1, &read_fds, NULL, NULL, &timeout);
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
    size_t available_size = BUFFER_SIZE - stream.buffer_length;

    // Ensure the size is within `int` range for `recv()`
    int bytes_to_read = (available_size > INT_MAX) ? INT_MAX : (int)available_size;

    // Ensure we don�t pass a negative or zero buffer size
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

ProcessResult process_wait_for_start(SOCKET sock, uint8_t* buffer, size_t* length) {
    logger_log(LOG_DEBUG, "top of process_wait_for_start length = %d", *length);
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

    uint32_t expected_marker = htonl(START_MARKER);
    if (memcmp(buffer, &expected_marker, 4) != 0) {
        uint32_t received_marker;
        memcpy(&received_marker, buffer, 4);
        logger_log(
            LOG_DEBUG,
            "Invalid Start Marker [%h], position: %x", ntohl(received_marker));

        consume_buffer(1);
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
    message_length = ntohl(message_length); // Convert from big-endian,    
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
 * @brief State: Send an ACK message back.
 *
 * ACK packet structure:
 *   - Start Marker: 4 bytes
 *   - Length: 4 bytes (16 + ack body length)
 *   - ACK Index: 4 bytes
 *   - ACK Body: Variable (e.g. "ACK <received_index>")
 *   - End Marker: 4 bytes
 */
ProcessResult process_send_ack(SOCKET sock, uint8_t* buffer, size_t* length) {
    logger_log(LOG_DEBUG, "top of process_send_ack length = %d", *length);
    char ack_body[32];
    int ack_body_len = snprintf(ack_body, sizeof(ack_body), "ACK %u", received_index);
    if (ack_body_len < 0) {
        logger_log(LOG_ERROR, "print failed for ACK message.");
        return PROCESS_FAIL;
    }

    uint32_t ack_packet_length = 16 + ack_body_len;
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

    logger_log(LOG_INFO, "Sent ACK %u", tmp);
    current_state = WAIT_FOR_START;
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
    logger_log(LOG_DEBUG, "top of process_wait_for_message length = %d", *length);
    logger_log(LOG_DEBUG, "top of process_wait_for_message message_length = %d", message_length);
    // Ensure we have at least the minimal message length
    if (*length < message_length) {
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
    // Extract message index (first 4 bytes)
    memcpy(&received_index, buffer, 4);
    uint32_t be_received_index = received_index;
    received_index = ntohl(received_index); // Convert from big-endian,    
    // Calculate message body length
    uint32_t message_body_length = message_length - 16;
    if (message_body_length > *length - 8) { // Prevent reading past buffer
        logger_log(LOG_ERROR, "Message body length (%u) exceeds available data.", message_body_length);
        consume_buffer(message_length);  // Discard the corrupted message
        return PROCESS_FAIL;
    }

    // Ensure end marker is valid before processing the message
    uint32_t received_end_marker;
    memcpy(&received_end_marker, buffer + 4 + message_body_length, 4);
    received_end_marker = ntohl(received_end_marker);
    if (received_end_marker != END_MARKER) {
        logger_log(LOG_ERROR, "Invalid end marker: 0x%08X", received_end_marker);
        consume_buffer(message_length);  // Discard the corrupted message
        return PROCESS_FAIL;
    }

    // Allocate buffer for message body (safely)
    char* message_body = (char*)malloc(message_body_length + 1);
    if (!message_body) {
        logger_log(LOG_ERROR, "Memory allocation failed for message body.");
        consume_buffer(message_length);
        return PROCESS_FAIL;
    }

    // Copy the message body
    memcpy(message_body, buffer + 4, message_body_length);
    message_body[message_body_length] = '\0';  // Ensure null termination

    // Process the command contained in the message
    process_command(message_body);

    logger_log(LOG_INFO, "Received message (index %u): %s", received_index, message_body);

    free(message_body);  // Free allocated memory

    // Consume the full packet (including the end marker)
    consume_buffer(message_length);
    current_state = SEND_ACK;

    logger_log(LOG_DEBUG, "bottom of process_wait_for_message");
    return PROCESS_OK;
}

/**
 * @brief Main command interface loop handling the state machine.
 */
void command_interface_loop(SOCKET client_sock, struct sockaddr_in* client_addr) {

    while (!shutdown_flag) {
        int bytes = buffered_recv(client_sock);
        if (bytes < 0) {
            logger_log(LOG_ERROR, "Connection closed.");
            break;
        }

        while (stream.buffer_length > 0) {
            logger_log(LOG_DEBUG, "stream.buffer_length: %dcurrent State : %d", stream.buffer_length, current_state);
            ProcessResult res = states[current_state].process(client_sock, stream.buffer, &stream.buffer_length);
            
            if (res == PROCESS_FAIL) {
                logger_log(LOG_ERROR, "Error processing packet. Resetting state.");
                current_state = WAIT_FOR_START;
                consume_buffer(stream.buffer_length);
                break;
            }
            else if (res == PROCESS_NEED_MORE_DATA) {
                logger_log(LOG_DEBUG, "Will wait for more more data");
                break;  // Wait for more data
            }
            // PROCESS_OK: continue to the next state in the state machine.
            logger_log(LOG_DEBUG, "State transition: %d -> %d", current_state, current_state);
        }
        logger_log(LOG_DEBUG, "Outside while loop current State : %d", current_state);
    }

    close_socket(&client_sock);
    logger_log(LOG_INFO, "Client disconnected.");
}

/**
 * Main client thread entry point.
 */
void* command_interface_thread(void* arg) {
    // AppThreadArgs_T* thread_info = (AppThreadArgs_T*)arg;
    init_from_config();

    bool is_tcp = true;
    bool is_server = true;
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
    while (!shutdown_flag) {
        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);  // Use int for Windows compatibility
        memset(&client_addr, 0, sizeof(client_addr));

        SOCKET client_sock = accept(sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock == INVALID_SOCKET) {
            if (shutdown_flag)
                break;  // Graceful shutdown

            logger_log(LOG_ERROR, "Accept failed.");
            continue;
        }

        logger_log(LOG_INFO, "Client connected.");

        // Handle client communication /
        command_interface_loop(client_sock, &client_addr);

        // Handle client disconnection
        close_socket(&client_sock);
        logger_log(LOG_INFO, "Client disconnected. Waiting for a new connection...");
    }

    /* Final cleanup before exiting */
    logger_log(LOG_INFO, "%s is shutting down...", get_thread_label());
    close_socket(&sock);
    return NULL;
}
