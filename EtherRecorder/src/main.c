#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Platform includes - new modular approach
#include "platform_error.h"
#include "platform_console.h"
#include "platform_string.h"
#include "platform_sockets.h"
#include "platform_path.h"

// Windows-specific includes for detaching console
#ifdef _WIN32
#include <windows.h>
#endif

// Project includes
#include "utils.h"
#include "logger.h"
#include "app_config.h"
#include "app_thread.h"
#include "thread_registry.h"
#include "shutdown_handler.h"
#include "message_types.h"
#include "version_info.h"

#define APP_MAX_PATH_LEN 256

// default config file
static char config_file_name[APP_MAX_PATH_LEN] = "config.ini";
extern void check_watchdog(void);

/**
 * @brief Detach the application from its console on Windows
 * 
 * This allows the application to continue running after the console window is closed.
 * 
 * @param stdout_path Path to redirect stdout (NULL for no redirection)
 * @param stderr_path Path to redirect stderr (NULL for no redirection)
 * @return true on success, false on failure
 */
static bool detach_from_console(const char* stdout_path, const char* stderr_path) {
#ifdef _WIN32
    // Redirect stdout if requested
    if (stdout_path) {
        FILE* new_stdout = freopen(stdout_path, "a", stdout);
        if (!new_stdout) {
            return false;
        }
    }

    // Redirect stderr if requested
    if (stderr_path) {
        FILE* new_stderr = freopen(stderr_path, "a", stderr);
        if (!new_stderr) {
            return false;
        }
    }

    // Detach from console - this allows the application to continue running when the console is closed
    if (!FreeConsole()) {
        return false;
    }

    return true;
#else
    // Not implemented for non-Windows platforms
    return false;
#endif
}

static void print_usage(const char *progname) {
    printf("Usage: %s [options]\n", progname);
    printf("  -c <config_file>  Specify the configuration file (optional).\n");
    printf("  --headless        Run in headless mode without console UI.\n");
    printf("  -h                Show this help message.\n");
}

static bool parse_args(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && (i + 1) < argc) {
            char *config_file = argv[++i];  // Optional config file argument
            if (*config_file != '\0') {
                // Replace strncpy with platform_strcat
                config_file_name[0] = '\0';  // Initialize to empty string
                platform_strcat(config_file_name, config_file, sizeof(config_file_name));
                return true;  // Successfully parsed config file argument
            }
        } else if (strcmp(argv[i], "--headless") == 0) {
            // Set headless mode in configuration
            set_config_value("app", "headless", "true");
            return true;
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
    // ensure we never try to log without the mutex in place by calling this here
    init_logger_mutex();

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

    // Check if running in headless mode
    bool headless = get_config_bool("app", "headless", false);
    if (headless) {
        logger_log(LOG_INFO, "Running in headless mode");
        
        // Set log destination to file only if not specifically configured otherwise
        if (!get_config_bool("logger", "log_destination", false)) {
            set_config_value("logger", "log_destination", "file");
        }
        
        // Ensure we have log file paths set
        const char* log_file_path = get_config_string("logger", "log_file_path", "logs");
        const char* stdout_log = NULL;
        const char* stderr_log = NULL;
        
        // Create paths for stdout and stderr redirection
        char stdout_path[APP_MAX_PATH_LEN] = {0};
        char stderr_path[APP_MAX_PATH_LEN] = {0};
        
        platform_strcat(stdout_path, log_file_path, sizeof(stdout_path));
        platform_strcat(stdout_path, "/stdout.log", sizeof(stdout_path));
        platform_strcat(stderr_path, log_file_path, sizeof(stderr_path));
        platform_strcat(stderr_path, "/stderr.log", sizeof(stderr_path));
        
        stdout_log = stdout_path;
        stderr_log = stderr_path;
        
        logger_log(LOG_INFO, "Detaching from console. Console window can now be closed.");
        logger_log(LOG_INFO, "Redirecting stdout to %s", stdout_log);
        logger_log(LOG_INFO, "Redirecting stderr to %s", stderr_log);
        
        // Flush the logs before detaching
        logger_flush();
        
        // Print message to console before detaching
        printf("Application is now running in headless mode.\n");
        printf("The console window can be closed.\n");
        printf("Stdout is redirected to: %s\n", stdout_log);
        printf("Stderr is redirected to: %s\n", stderr_log);
        printf("Application logs are available in: %s\n", log_file_path);
        
        // Detach from console - allows application to continue when console is closed
        if (!detach_from_console(stdout_log, stderr_log)) {
            logger_log(LOG_ERROR, "Failed to detach from console");
            // Continue anyway, as this is non-critical
        }
    }

    // Initialize logger
    char logger_init_result[LOG_MSG_BUFFER_SIZE];
    if (!init_logger_from_config(logger_init_result)) {
        printf("Failed to initialise logger: %s\n", logger_init_result);
        return PLATFORM_ERROR_SYSTEM;
    }

    // Initialize thread registry system
    ThreadRegistryError reg_result = init_global_thread_registry();
    if (reg_result != THREAD_REG_SUCCESS) {
        logger_log(LOG_ERROR, "Failed to initialize thread registry: %s",
                  app_error_get_message(THREAD_REGISTRY_DOMAIN, reg_result));
        return PLATFORM_ERROR_THREAD_CREATE;
    }

    // Initialize thread management
    ThreadRegistryError thread_result = register_main_thread();
    if (thread_result != THREAD_REG_SUCCESS) {
        logger_log(LOG_ERROR, "Failed to initialize thread management");
        return PLATFORM_ERROR_THREAD_CREATE;
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

static bool send_demo_text_message(void) {
    const char* msg_text = "Message from main thread";
    Message_T message = {0};  // Zero-initialize the entire structure
    message.header.type = MSG_TYPE_TEST;
    size_t content_len = platform_strlen(msg_text) + 1;  // Include null terminator
    if (content_len > UINT32_MAX) {
        logger_log(LOG_ERROR, "Message length exceeds maximum allowed size");
        return false;
    }
    message.header.content_size = (uint32_t)content_len;

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
        platform_get_error_message_from_code(result, error_message, sizeof(error_message));
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
   //     check_watchdog();
        // Send message to demo thread
        if (!send_demo_text_message()) {
            // logger_log(LOG_ERROR, "Failed to send demo message");
        }

        // Comment out the heartbeat log message to reduce memory usage
        logger_log(LOG_DEBUG, "HEARTBEAT");
        sleep_ms(763);
    }

    result = cleanup_app();
    if (result != PLATFORM_ERROR_SUCCESS) {
        platform_get_error_message_from_code(result, error_message, sizeof(error_message));
        char buffer[512];
        platform_strformat(buffer, sizeof(buffer), "Error during cleanup: %s\n", error_message);
        stream_print(stderr, "%s", buffer);
        return EXIT_FAILURE;
    }

    stream_print(stdout, "We are done!\n");
    return EXIT_SUCCESS;
}
