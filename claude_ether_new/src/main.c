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
#include "thread_registry.h"
#include "shutdown_handler.h"
#include "message_types.h"
#include "version_info.h"

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

    // Print version information immediately after console init
    print_version_info();
    
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
    } else {
        logger_log(LOG_INFO, "Using config file: %s\n", config_file_name); 
        logger_log(LOG_INFO, "Configuration: %s", config_load_result);
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
    if (thread_registry_wait_all(7620) != THREAD_REG_SUCCESS) {
        logger_log(LOG_WARN, "Timeout waiting for threads to complete");
    }
    
    // Clean up in reverse order of initialization
    app_thread_cleanup();
    platform_socket_cleanup();
    cleanup_shutdown_handler();
    logger_close();
    free_config();
    
    // Ensure terminal is in a good state before exit
    platform_console_reset_formatting();
    platform_console_set_echo(true);
    platform_console_set_line_buffering(true);
    platform_console_show_cursor(true);
    platform_console_cleanup();
    
    // Flush any remaining output
    fflush(stdout);
    fflush(stderr);
    
    return result;
}

static bool send_demo_text_message(const char* msg_text) {
    Message_T message = {0};  // Zero-initialize the entire structure
    message.header.type = MSG_TYPE_TEST;
    message.header.content_size = strlen(msg_text) + 1;  // Include null terminator
    
    if (message.header.content_size > sizeof(message.content)) {
        logger_log(LOG_ERROR, "Message too long for content buffer");
        return false;
    }
    
    memcpy(message.content, msg_text, message.header.content_size);
    
    MessageQueue_T* demo_queue = get_queue_by_label("DEMO_HEARTBEAT");
    if (!demo_queue) {
        return false;
    }
    
    return message_queue_push(demo_queue, &message, 100);
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
        stream_print(stderr, "%s", buffer);
        return EXIT_FAILURE;
    }

    // Start application threads
    start_threads();
    logger_log(LOG_INFO, "Application threads started");

    // Main application loop with heartbeat
    while (!shutdown_signalled()) {
        // Send message to demo thread
        if (!send_demo_text_message("Message from main thread")) {
            logger_log(LOG_ERROR, "Failed to send demo message");
        }
        
        logger_log(LOG_DEBUG, "HEARTBEAT");
        stream_print(stdout, "Main about to sleep\n");
        sleep_ms(762);
        stream_print(stdout, "Main has awoken\n");
    }
    
    result = cleanup_app();
    if (result != PLATFORM_ERROR_SUCCESS) {
        platform_get_error_message(result, error_message, sizeof(error_message));
        char buffer[512];
        platform_strformat(buffer, sizeof(buffer), "Error during cleanup: %s\n", error_message);
        stream_print(stderr, "%s", buffer);
        return EXIT_FAILURE;
    }

    stream_print(stdout, "We are done!\n");
    return EXIT_SUCCESS;
}
