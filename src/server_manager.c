#include "server_manager.h"
#include "winsock2.h"
#include "logger.h"
#include "comm_threads.h"

// External declarations for stub functions
extern void* pre_create_stub(void* arg);
extern void* post_create_stub(void* arg);
extern void* init_stub(void* arg);
extern void* exit_stub(void* arg);
extern void* init_wait_for_logger(void* arg);
extern bool shutdown_signalled(void);

AppThreadArgs_T send_thread_args = {
	.suppressed = true,
    .label = "SERVER.SEND",
    .func = send_thread,
    .data = NULL,
    .pre_create_func = pre_create_stub,
    .post_create_func = post_create_stub,
    .init_func = init_wait_for_logger,
    .exit_func = exit_stub
};

AppThreadArgs_T receive_thread_args = {
    .suppressed = true,
    .label = "SERVER.RECEIVE",
    .func = receive_thread,
    .data = NULL,
    .pre_create_func = pre_create_stub,
    .post_create_func = post_create_stub,
    .init_func = init_wait_for_logger,
    .exit_func = exit_stub
};

void* serverMainThread(void* arg) {
    AppThreadArgs_T* thread_info = (AppThreadArgs_T*)arg;
    set_thread_label(thread_info->label);
    CommsThreadArgs_T* server_info = (CommsThreadArgs_T*)thread_info->data;

    int port = server_info->port;
    bool is_tcp = server_info->is_tcp;
    bool is_server = true;
    struct sockaddr_in addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    memset(&addr, 0, sizeof(addr));
    memset(&client_addr, 0, sizeof(client_addr));

    SOCKET sock = setup_socket(is_server, is_tcp, &addr, &client_addr, "0.0.0.0", port);
    if (sock == INVALID_SOCKET) {
        logger_log(LOG_ERROR, "Server setup failed.");
        return NULL;
    }

    logger_log(LOG_INFO, "Server is listening on port %d", port);

    while (!shutdown_signalled()) {
        SOCKET client_sock = accept(sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock == INVALID_SOCKET) {
            if (shutdown_signalled()) break;
            logger_log(LOG_ERROR, "Accept failed.");
            continue;
        }

        logger_log(LOG_INFO, "Client connected.");

        // Allocate new comm args per client
        CommArgs_T* comm_args = (CommArgs_T*)malloc(sizeof(CommArgs_T));
        comm_args->sock = &client_sock;
        comm_args->client_addr = client_addr;
//        comm_args->thread_info = server_info; // Pass server details
        comm_args->connection_closed = false;

        create_app_thread(&send_thread_args);
        create_app_thread(&receive_thread_args);
    }

    logger_log(LOG_INFO, "Server shutting down...");
    close_socket(&sock);
    return NULL;
}