#include "shutdown_handler.h"
#include "platform_mutex.h"
#include "platform_atomic.h"
#include "platform_sync.h"
#include "platform_error.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>

// Global shutdown flag
static PlatformAtomicPtr shutdown_flag = {0};

// Global shutdown event
static PlatformEvent_T shutdown_event = NULL;

/**
 * @brief Callback function for when termination signal is received
 */
static void shutdown_callback(void) {
    signal_shutdown();
}

/**
 * @brief Installs the shutdown handler.
 */
bool install_shutdown_handler(void) {
    // Create a manual-reset event (not signaled initially)
    PlatformErrorCode result = platform_event_create(&shutdown_event, true, false);
    if (result != PLATFORM_ERROR_SUCCESS) {
        logger_log(LOG_ERROR, "Failed to create shutdown event");
        return false;
    }

    // Register handlers for both SIGINT and SIGTERM
    if (!platform_signal_register_handler(PLATFORM_SIGNAL_INT, shutdown_callback) ||
        !platform_signal_register_handler(PLATFORM_SIGNAL_TERM, shutdown_callback)) {
        logger_log(LOG_ERROR, "Failed to register shutdown handler");
        platform_event_destroy(shutdown_event);
        return false;
    }

    logger_log(LOG_INFO, "Shutdown handler installed successfully");
    return true;
}

/**
 * @brief Signals that a shutdown should begin.
 */
void signal_shutdown(void) {
    platform_atomic_exchange_ptr(&shutdown_flag, (void*)1);
    platform_event_set(shutdown_event);
    logger_log(LOG_INFO, "Shutdown signaled");
}

/**
 * @brief Checks if a shutdown has been signaled.
 * @return true if shutdown has been signaled, false otherwise
 */
bool shutdown_signalled(void) {
    return platform_atomic_load_ptr(&shutdown_flag) != NULL;
}

/**
 * @brief Waits for the shutdown signal.
 */
bool wait_for_shutdown_event(int timeout_ms) {
    PlatformErrorCode result = platform_event_wait(shutdown_event, timeout_ms);
    
    if (result == PLATFORM_ERROR_SUCCESS) {
        logger_log(LOG_INFO, "Shutdown event received");
        return true;
    }
    else if (result == PLATFORM_ERROR_TIMEOUT) {
        logger_log(LOG_WARN, "Wait for shutdown timed out (%d ms)", timeout_ms);
    }
    else {
        logger_log(LOG_ERROR, "Wait for shutdown failed");
    }
    
    return false;
}

/**
 * @brief Cleans up the shutdown event.
 */
void cleanup_shutdown_handler(void) {
    platform_event_destroy(shutdown_event);
    logger_log(LOG_INFO, "Shutdown handler cleaned up");
}
