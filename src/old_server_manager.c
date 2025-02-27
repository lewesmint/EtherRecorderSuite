#include "server_manager.h"

#include <stdio.h>  // for perror
#include <stdbool.h>
#include <string.h>

#include "common_socket.h"
#include "app_thread.h"
#include "logger.h"

extern bool shutdown_signalled(void);

void* serverListenerThread(void* arg) {
    AppThreadArgs_T* thread_info = (AppThreadArgs_T*)arg;
    set_thread_label(thread_info->label);
    ServerThreadArgs_T* server_info = (ServerThreadArgs_T*)thread_info->data;
    
    int port = server_info->port;
    bool is_tcp = server_info->is_tcp;
    bool is_server = true;
    struct sockaddr_in addr, client_addr;
    memset(&addr, 0, sizeof(addr));
    memset(&client_addr, 0, sizeof(client_addr));
    socklen_t client_len = sizeof(client_addr);
    char *host = "0.0.0.0"; // Any host can connect

    //handles everything required for the server to listen for incoming connections
    SOCKET sock = setup_socket(is_server, is_tcp, &addr, &client_addr, host, port);
    if (sock == INVALID_SOCKET) {
        logger_log(LOG_ERROR, "Server setup failed.");
        return NULL;
    }

    logger_log(LOG_INFO, "Server is listening on port %d", port);

    // Main loop: Accept and handle client connections
    while (!shutdown_signalled()) {
        SOCKET client_sock = accept(sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock == INVALID_SOCKET) {
            if (shutdown_signalled())
                break;  // Graceful shutdown

            logger_log(LOG_ERROR, "Accept failed.");
            continue;
        }
        logger_log(LOG_INFO, "Client connected.");

        // TODO: Create separate threads for sending and receiving
        // Currently, we handle everything in a single loop

        // Handle client communication
        communication_loop(client_sock, is_server, is_tcp, is_tcp ? NULL : &client_addr);

        // Handle client disconnection
        close_socket(&client_sock);
        logger_log(LOG_INFO, "Client disconnected. Waiting for a new connection...");
    }

    /* Final cleanup before exiting */
    logger_log(LOG_INFO, "Server is shutting down...");
    close_socket(&sock);
    return NULL;
}