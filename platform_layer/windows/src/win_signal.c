#include "platform_signal.h"
#include <windows.h>
#include "logger.h"

// Array to store handlers for different signal types
static PlatformSignalHandler g_handlers[2] = {NULL};

static BOOL WINAPI console_handler(DWORD ctrl_type) {
    switch (ctrl_type) {
        case CTRL_C_EVENT:
            if (g_handlers[PLATFORM_SIGNAL_INT]) {
                logger_log(LOG_INFO, "Ctrl+C signal received");
                g_handlers[PLATFORM_SIGNAL_INT]();
                return TRUE;
            }
            break;
            
        case CTRL_CLOSE_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            if (g_handlers[PLATFORM_SIGNAL_TERM]) {
                logger_log(LOG_INFO, "Termination signal received");
                g_handlers[PLATFORM_SIGNAL_TERM]();
                return TRUE;
            }
            break;
    }
    return FALSE;
}

bool platform_signal_register_handler(PlatformSignalType signal_type, 
                                    PlatformSignalHandler handler) {
    if (!handler || signal_type < 0 || signal_type >= 2) {
        return false;
    }

    // Store the handler
    g_handlers[signal_type] = handler;

    // Register the Windows console handler if this is the first handler
    static bool console_handler_registered = false;
    if (!console_handler_registered) {
        if (!SetConsoleCtrlHandler(console_handler, TRUE)) {
            g_handlers[signal_type] = NULL;
            return false;
        }
        console_handler_registered = true;
    }

    return true;
}

bool platform_signal_unregister_handler(PlatformSignalType signal_type) {
    if (signal_type < 0 || signal_type >= 2) {
        return false;
    }

    g_handlers[signal_type] = NULL;

    // Check if all handlers are NULL
    for (int i = 0; i < 2; i++) {
        if (g_handlers[i] != NULL) {
            return true;
        }
    }

    // If all handlers are NULL, unregister the console handler
    return SetConsoleCtrlHandler(console_handler, FALSE);
}