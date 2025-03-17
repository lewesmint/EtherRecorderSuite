#include "command_interface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform_sockets.h"

#include "logger.h"
#include "app_thread.h"
#include "thread_registry.h"

#define START_MARKER 0xDEADBEEF
#define END_MARKER   0xBEEFDEAD
#define MAX_BUFFER_SIZE 4096
#define DEFAULT_CMD_PORT 8080


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
} CommandState;

typedef struct {
    uint8_t buffer[MAX_BUFFER_SIZE];
    size_t buffer_length;
    size_t message_length;
    uint32_t received_index;
    uint32_t ack_index;
    CommandState current_state;
} CommandContext;

static void process_command(const char* command) {
    logger_log(LOG_INFO, "Processing command: %s", command);
    // TODO: Implement command processing logic
}

static ProcessResult process_wait_for_start(PlatformSocketHandle sock, CommandContext* ctx) {
    (void)sock;  // Unused parameter
    if (ctx->buffer_length < 4) {
        return PROCESS_NEED_MORE_DATA;
    }

    uint32_t marker;
    memcpy(&marker, ctx->buffer, 4);
    marker = platform_ntohl(marker);

    if (marker != START_MARKER) {
        logger_log(LOG_ERROR, "Invalid start marker: 0x%08X", marker);
        return PROCESS_FAIL;
    }

    ctx->current_state = WAIT_FOR_LENGTH;
    return PROCESS_OK;
}

static ProcessResult process_wait_for_length(PlatformSocketHandle sock, CommandContext* ctx) {
    (void)sock;  // Unused parameter
    if (ctx->buffer_length < 8) {
        return PROCESS_NEED_MORE_DATA;
    }

    uint32_t length;
    memcpy(&length, ctx->buffer + 4, 4);
    ctx->message_length = platform_ntohl(length);

    if (ctx->message_length > MAX_BUFFER_SIZE || ctx->message_length < 12) {
        logger_log(LOG_ERROR, "Invalid message length: %zu", ctx->message_length);
        return PROCESS_FAIL;
    }

    ctx->current_state = WAIT_FOR_MESSAGE;
    return PROCESS_OK;
}

static ProcessResult process_wait_for_message(PlatformSocketHandle sock, CommandContext* ctx) {
    (void)sock;  // Unused parameter
    if (ctx->buffer_length < ctx->message_length) {
        return PROCESS_NEED_MORE_DATA;
    }

    // Verify end marker
    uint32_t end_marker;
    memcpy(&end_marker, ctx->buffer + ctx->message_length - 4, 4);
    end_marker = platform_ntohl(end_marker);

    if (end_marker != END_MARKER) {
        logger_log(LOG_ERROR, "Invalid end marker: 0x%08X", end_marker);
        return PROCESS_FAIL;
    }

    // Extract and process message body
    size_t body_length = ctx->message_length - 12;  // Subtract markers and length
    char* message_body = (char*)malloc(body_length + 1);
    if (!message_body) {
        return PROCESS_FAIL;
    }

    memcpy(message_body, ctx->buffer + 8, body_length);
    message_body[body_length] = '\0';

    process_command(message_body);
    free(message_body);

    // Remove processed message from buffer
    memmove(ctx->buffer, 
            ctx->buffer + ctx->message_length, 
            ctx->buffer_length - ctx->message_length);
    ctx->buffer_length -= ctx->message_length;

    ctx->current_state = SEND_ACK;
    return PROCESS_OK;
}

static ProcessResult process_send_ack(PlatformSocketHandle sock, CommandContext* ctx) {
    char ack_body[32];
    int ack_body_len = snprintf(ack_body, sizeof(ack_body), "ACK %u", ctx->received_index);
    uint32_t ack_packet_length = 16 + ack_body_len;
    uint8_t ack_buffer[256];

    // Pack start marker
    uint32_t tmp = platform_htonl(START_MARKER);
    memcpy(ack_buffer, &tmp, 4);

    // Pack length
    tmp = platform_htonl(ack_packet_length);
    memcpy(ack_buffer + 4, &tmp, 4);

    // Pack ACK index
    tmp = platform_htonl(ctx->ack_index++);
    memcpy(ack_buffer + 8, &tmp, 4);

    // Pack ACK body
    memcpy(ack_buffer + 12, ack_body, ack_body_len);

    // Pack end marker
    tmp = platform_htonl(END_MARKER);
    memcpy(ack_buffer + 12 + ack_body_len, &tmp, 4);

    size_t bytes_sent = 0;
    PlatformErrorCode result = platform_socket_send(sock, 
                                                  ack_buffer, 
                                                  ack_packet_length, 
                                                  &bytes_sent);

    if (result != PLATFORM_ERROR_SUCCESS || bytes_sent != ack_packet_length) {
        logger_log(LOG_ERROR, "Failed to send ACK");
        return PROCESS_FAIL;
    }

    ctx->current_state = WAIT_FOR_START;
    return PROCESS_OK;
}

static void handle_client_connection(PlatformSocketHandle client_sock) {
    CommandContext ctx = {
        .buffer_length = 0,
        .message_length = 0,
        .received_index = 0,
        .ack_index = 0,
        .current_state = WAIT_FOR_START
    };

    while (!shutdown_signalled()) {
        // Receive data
        if (ctx.buffer_length < MAX_BUFFER_SIZE) {
            size_t bytes_received = 0;
            PlatformErrorCode result = platform_socket_receive(
                client_sock,
                ctx.buffer + ctx.buffer_length,
                MAX_BUFFER_SIZE - ctx.buffer_length,
                &bytes_received
            );

            if (result != PLATFORM_ERROR_SUCCESS || bytes_received == 0) {
                break;
            }

            ctx.buffer_length += bytes_received;
        }

        // Process state machine
        ProcessResult result = PROCESS_FAIL;  // Initialize with default value
        switch (ctx.current_state) {
            case WAIT_FOR_START:
                result = process_wait_for_start(client_sock, &ctx);
                break;
            case WAIT_FOR_LENGTH:
                result = process_wait_for_length(client_sock, &ctx);
                break;
            case WAIT_FOR_MESSAGE:
                result = process_wait_for_message(client_sock, &ctx);
                break;
            case SEND_ACK:
                result = process_send_ack(client_sock, &ctx);
                break;
            default:
                logger_log(LOG_ERROR, "Invalid command state: %d", ctx.current_state);
                break;
        }

        if (result == PROCESS_FAIL) {
            break;
        }
        else if (result == PROCESS_NEED_MORE_DATA) {
            continue;
        }
    }
}

static void* command_interface_thread_function(void* arg) {
    ThreadConfig* config = (ThreadConfig*)arg;
    thread_registry_update_state(config->label, THREAD_STATE_RUNNING);

    PlatformSocketHandle sock = NULL;
    PlatformSocketAddress addr = {
        .port = DEFAULT_CMD_PORT,
        .is_ipv6 = false,
        .host = "0.0.0.0"
    };

    PlatformSocketOptions sock_opts = {
        .blocking = true,
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000,
        .reuse_address = true,
        .keep_alive = true,
        .no_delay = true
    };

    if (platform_socket_create(&sock, true, &sock_opts) != PLATFORM_ERROR_SUCCESS) {
        logger_log(LOG_ERROR, "Failed to create command interface socket");
        thread_registry_update_state(config->label, THREAD_STATE_FAILED);
        return NULL;
    }

    if (platform_socket_bind(sock, &addr) != PLATFORM_ERROR_SUCCESS ||
        platform_socket_listen(sock, 5) != PLATFORM_ERROR_SUCCESS) {
        platform_socket_close(sock);
        thread_registry_update_state(config->label, THREAD_STATE_FAILED);
        return NULL;
    }

    logger_log(LOG_INFO, "Command interface listening on port %d", DEFAULT_CMD_PORT);

    while (!shutdown_signalled()) {
        PlatformSocketAddress client_addr = {0};
        PlatformSocketHandle client_sock = NULL;

        if (platform_socket_accept(sock, &client_sock, &client_addr) != PLATFORM_ERROR_SUCCESS) {
            if (shutdown_signalled()) break;
            continue;
        }

        logger_log(LOG_INFO, "Command client connected from %s:%d", 
                  client_addr.host, client_addr.port);

        handle_client_connection(client_sock);
        platform_socket_close(client_sock);
    }

    platform_socket_close(sock);
    thread_registry_update_state(config->label, THREAD_STATE_TERMINATED);
    return NULL;
}

ThreadConfig* get_command_interface_thread(void) {
    static ThreadConfig config = {
        .label = "CMD_INTERFACE",
        .func = command_interface_thread_function,
        .data = NULL,
        .thread_id = 0,
        .pre_create_func = NULL,
        .post_create_func = NULL,
        .init_func = NULL,
        .exit_func = NULL,
        .suppressed = false,
        .msg_processor = NULL,
        .queue_process_interval_ms = 0,
        .max_process_time_ms = 100,
        .msg_batch_size = 10
    };
    return &config;
}
