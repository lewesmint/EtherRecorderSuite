#include "shutdown_handler.h"
#include "logger.h"
#include "platform_mutex.h"
#include "platform_atomic.h"
#include "platform_utils.h"

#include <stdio.h>
#include <stdlib.h>

// Global shutdown flag (volatile for visibility across threads)
static volatile long shutdown_flag = 0;

// Global shutdown event
static PlatformEvent_T shutdown_event;

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
    if (!platform_event_create(&shutdown_event, true, false)) {
        logger_log(LOG_ERROR, "Failed to create shutdown event");
        return false;
    }

    // Register platform-agnostic signal handlers
    if (!platform_register_shutdown_handler(shutdown_callback)) {
        logger_log(LOG_ERROR, "Failed to register shutdown handler");
        platform_event_destroy(&shutdown_event);
        return false;
    }

    return true;
}

/**
 * @brief Signals that a shutdown should begin.
 */
void signal_shutdown(void) {
    platform_atomic_exchange((volatile long*)&shutdown_flag, 1);
    platform_event_set(&shutdown_event);
}

/**
 * @brief Checks if a shutdown has been signaled.
 */
bool shutdown_signalled(void) {
    return platform_atomic_compare_exchange((volatile long*)&shutdown_flag, 0, 0) != 0;
}

/**
 * @brief Waits for the shutdown signal.
 */
bool wait_for_shutdown_event(int timeout_ms) {
    int result = platform_event_wait(&shutdown_event, timeout_ms);
    
    if (result == PLATFORM_WAIT_SUCCESS) {
        return true;
    }
    else if (result == PLATFORM_WAIT_TIMEOUT) {
        logger_log(LOG_DEBUG, "Wait for shutdown timed out (%d ms)", timeout_ms);
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
    platform_event_destroy(&shutdown_event);
}
