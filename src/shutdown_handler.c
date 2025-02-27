#include "shutdown_handler.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

// Global shutdown flag (volatile for visibility across threads)
static volatile LONG shutdown_flag = FALSE;

// Global shutdown event handle.
static HANDLE shutdown_event = NULL;

/**
 * @brief Windows console control handler.
 * 
 * This is called when a console event (e.g., Ctrl-C) occurs.
 */
BOOL WINAPI console_ctrl_handler(DWORD signal) {
    if (signal == CTRL_C_EVENT) {
        fprintf(stderr, "\nCTRL-C detected. Initiating shutdown...\n");
        signal_shutdown();
        return TRUE;
    }
    return FALSE;
}

/**
 * @brief Installs the shutdown handler.
 */
bool install_shutdown_handler(void) {
    // Create a manual-reset event (not signaled initially)
    shutdown_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (shutdown_event == NULL) {
        logger_log(LOG_ERROR, "CreateEvent failed (%lu)", GetLastError());
        return false;
    }

    // Register the Ctrl-C console handler
    if (!SetConsoleCtrlHandler(console_ctrl_handler, TRUE)) {
        logger_log(LOG_ERROR, "Could not set control handler");
        CloseHandle(shutdown_event);
        shutdown_event = NULL;
        return false;
    }
    return true;
}

/**
 * @brief Signals that a shutdown should begin.
 */
void signal_shutdown(void) {
    InterlockedExchange(&shutdown_flag, TRUE);
    if (shutdown_event) {
        SetEvent(shutdown_event);
    }
}

/**
 * @brief Checks if a shutdown has been signaled.
 */
bool shutdown_signalled(void) {
    return InterlockedCompareExchange(&shutdown_flag, FALSE, FALSE);
}

/**
 * @brief Waits for the shutdown signal.
 */
bool wait_for_shutdown_event(int timeout_ms) {
    // Note: INFINITE (-1) is a valid timeout value.
    DWORD result = WaitForSingleObject(shutdown_event, timeout_ms);
    
    if (result == WAIT_OBJECT_0) {
        return true;
    }
    else if (result == WAIT_TIMEOUT) {
        logger_log(LOG_DEBUG, "Wait for shutdown timed out (%d ms)", timeout_ms);
    }
    else if (result == WAIT_ABANDONED) {
        logger_log(LOG_ERROR, "Wait for shutdown was abandoned (possible mutex issue).");
    }
    else if (result == WAIT_FAILED) {
        DWORD error_code = GetLastError();
        logger_log(LOG_ERROR, "Wait for shutdown failed with error code %lu", error_code);
    }
    else {
        logger_log(LOG_ERROR, "Wait for shutdown failed due to an unknown reason.");
    }
    
    return false;
}

/**
 * @brief Cleans up the shutdown event.
 */
void cleanup_shutdown_handler(void) {
    if (shutdown_event) {
        CloseHandle(shutdown_event);
        shutdown_event = NULL;
    }
}