#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform_sockets.h"
#include "app_error.h"
#include "logger.h"
#include "app_config.h"
#include "platform_utils.h"
#include "app_thread.h"
#include "shutdown_handler.h"


extern bool wait_for_all_threads_to_complete(int time_ms);

CONDITION_VARIABLE shutdown_condition;
CRITICAL_SECTION shutdown_mutex;

void print_usage(const char *progname) {
    printf("Usage: %s [-c <config_file>]\n", progname);
    printf("  -c <config_file>  Specify the configuration file (optional).\n");
    printf("  -h                Show this help message.\n");
}

//default config file
char config_file_name[MAX_PATH] = "config.ini";

bool parse_args(int argc, char *argv[]) {
    char *config_file = NULL;  // Optional config file

    // Iterate through arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && (i + 1) < argc) {
            config_file = argv[++i];  // Get the next argument as the filename
            if (*config_file != '\0') {
                strncpy(config_file_name, config_file, sizeof(config_file_name)-1);
                config_file_name[sizeof(config_file_name)-1] = '\0';
                return false;
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
    return true;
}

int print_working_directory() {
    char cwd[MAX_PATH];
    if (get_cwd(cwd, sizeof(cwd)) != NULL) {
        printf("Current working directory: %s\n", cwd);
        return EXIT_SUCCESS;
    }
    else {
        // Handle an error from get_cwd.
        perror("getcwd_error");
        return EXIT_FAILURE;
    }
}

static AppError init_app() {
    init_thread_timestamp_system();
    set_thread_label("MAIN");
    install_shutdown_handler();

    char config_load_result[LOG_MSG_BUFFER_SIZE];
    char logger_init_result[LOG_MSG_BUFFER_SIZE];

    init_console();
    // Load configuration, if config not found use defaults
    // This will only return false if the defaults cannot be set
    // for some reason.
    print_working_directory();
    if (!load_config(config_file_name, config_load_result)) {
        printf("Failed to initialise configuration: %s\n", config_load_result);
        // we don't return, we use the default config.
    }

    // for the moment at least this can never happen, even if we can't use a log file
    // we'll still attempt to screen
    if (!init_logger_from_config(logger_init_result)) {
        printf("Configuration: %s\n", config_load_result);
        printf("Failed to initialise logger: %s\n", logger_init_result);
        return APP_LOGGER_ERROR;
    }
//    logger_log(LOG_INFO, "Configuration: %s", config_load_result);
//    logger_log(LOG_INFO, "Logger: %s", logger_init_result);

    /* Initialise sockets (WSAStartup on Windows, etc.) */
    initialise_sockets();
    return APP_EXIT_SUCCESS;
}

static AppError app_exit() {
    logger_log(LOG_INFO, "Exiting application");
    // Close the logger
    logger_close();

    // Deallocate memory used by the configuration
    // TODO: We might be able to deallocate earlier
    // if we're finished with the config, but it doesn't
    // use much memory so safe to do it here.
    free_config();
    printf("Main thread finally exiting\n");
    return APP_EXIT_SUCCESS;
}

int main(int argc, char* argv[]) {
    if (!parse_args(argc, argv)) {
        return APP_EXIT_FAILURE;
    }

    int app_error = init_app();
    if (app_error != APP_EXIT_SUCCESS) {
        return app_error;
    }

    // Now it's safe to log messages, though the dedicated logger thread
    // is not yet running
    logger_log(LOG_INFO, "Logger initialised successfully");

    // Start threads.
    // Successfully starting the logging thread will mean that logging will
    // utilize a log message queue, for asynchronous operation, to avoid threads 
    // blocking on logging.
    start_threads();
    logger_log(LOG_DEBUG, "App threads started");

    while (true) {
        if (wait_for_all_threads_to_complete(7620)) {
            break;
        } else {
            logger_log(LOG_DEBUG, "HEARTBEAT");
        }
    }
    
    // Clean up thread management system
    app_thread_cleanup();
    
    return app_exit();
}
