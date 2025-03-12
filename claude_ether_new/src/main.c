#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Platform includes - new modular approach
#include "platform_error.h"
#include "platform_console.h"
#include "platform_string.h"
#include "platform_sockets.h"

// Project includes
#include "utils.h"
#include "logger.h"
#include "app_config.h"
#include "app_thread.h"
#include "shutdown_handler.h"

#define MAX_PATH_LEN 256

// default config file
static char config_file_name[MAX_PATH_LEN] = "config.ini";

static void print_usage(const char *progname) {
    printf("Usage: %s [-c <config_file>]\n", progname);
    printf("  -c <config_file>  Specify the configuration file (optional).\n");
    printf("  -h                Show this help message.\n");
}

static bool parse_args(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && (i + 1) < argc) {
            char *config_file = argv[++i];  // Optional config file argument
            if (*config_file != '\0') {
                strncpy(config_file_name, config_file, sizeof(config_file_name)-1);
                config_file_name[sizeof(config_file_name)-1] = '\0';  // Ensure null termination
                return true;  // Successfully parsed config file argument
            }
        } else if (strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return false;
        } else {
            printf("Unknown argument: %s\n", argv[i]);
            print_usage(argv[0]);
            return false;
        }
    }
    return true;  // No args or successful parsing
}

static PlatformErrorCode init_app(void) {
    PlatformErrorCode result;
    
    // Initialize console first for basic output
    result = platform_console_init();
    if (result != PLATFORM_ERROR_SUCCESS) {
        return result;
    }
    
    // Initialize thread timestamp system
    init_thread_timestamp_system();
    set_thread_label("MAIN");
    
    // Install shutdown handler
    if (!install_shutdown_handler()) {
        return PLATFORM_ERROR_SYSTEM;
    }
    
    // Load configuration
    char config_load_result[LOG_MSG_BUFFER_SIZE];
    if (!load_config(config_file_name, config_load_result)) {
        printf("Failed to initialise configuration: %s\n", config_load_result);
        // Continue with defaults
    }
    
    // Initialize logger
    char logger_init_result[LOG_MSG_BUFFER_SIZE];
    if (!init_logger_from_config(logger_init_result)) {
        printf("Failed to initialise logger: %s\n", logger_init_result);
        return PLATFORM_ERROR_SYSTEM;
    }
    
    // Initialize sockets
    result = platform_socket_init();
    if (result != PLATFORM_ERROR_SUCCESS) {
        logger_log(LOG_ERROR, "Failed to initialize sockets");
        return result;
    }
    
    logger_log(LOG_INFO, "Application initialization complete");
    return PLATFORM_ERROR_SUCCESS;
}

static PlatformErrorCode cleanup_app(void) {
    PlatformErrorCode result = PLATFORM_ERROR_SUCCESS;
    
    // Wait for all threads to complete
    if (!wait_for_all_threads_to_complete(7620)) {
        logger_log(LOG_WARN, "Timeout waiting for threads to complete");
    }
    
    // Clean up in reverse order of initialization
    app_thread_cleanup();
    platform_socket_cleanup();
    cleanup_shutdown_handler();
    logger_close();
    free_config();
    platform_console_cleanup();
    
    return result;
}

int main(int argc, char *argv[]) {
    PlatformErrorCode result;
    char error_message[256];

    if (!parse_args(argc, argv)) {
        return EXIT_SUCCESS;  // Help was displayed or invalid args
    }

    result = init_app();
    if (result != PLATFORM_ERROR_SUCCESS) {
        platform_get_error_message(result, error_message, sizeof(error_message));
        char buffer[512];
        platform_strformat(buffer, sizeof(buffer), "Failed to initialize application: %s\n", error_message);
        fwrite(buffer, 1, strlen(buffer), stderr);
        return EXIT_FAILURE;
    }

    logger_log(LOG_INFO, "Using config file: %s", config_file_name);

    // Start application threads
    start_threads();
    logger_log(LOG_INFO, "Application threads started");

    // Main application loop with heartbeat
    while (!shutdown_signalled()) {
        logger_log(LOG_DEBUG, "HEARTBEAT");
        sleep_ms(7620);  // Same timeout as original
    }
    
    result = cleanup_app();
    if (result != PLATFORM_ERROR_SUCCESS) {
        platform_get_error_message(result, error_message, sizeof(error_message));
        char buffer[512];
        platform_strformat(buffer, sizeof(buffer), "Error during cleanup: %s\n", error_message);
        fwrite(buffer, 1, strlen(buffer), stderr);
        return EXIT_FAILURE;
    }

    stream_print(stdout, "We are done!\n");
    return EXIT_SUCCESS;
}
