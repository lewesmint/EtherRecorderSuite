/**
 * @file win_sync.c
 * @brief Windows implementation of platform synchronization primitives
 */
#include "platform_sync.h"
#include "platform_error.h"
#include <windows.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef _WIN32

struct platform_event {
    HANDLE handle;
    bool manual_reset;
    bool is_valid;
};

PlatformErrorCode platform_event_create(PlatformEvent_T* event, bool manual_reset, bool initial_state) {
    if (!event) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    struct platform_event* evt = (struct platform_event*)calloc(1, sizeof(struct platform_event));
    if (!evt) {
        return PLATFORM_ERROR_OUT_OF_MEMORY;
    }

    evt->handle = CreateEventW(
        NULL,               // default security attributes
        manual_reset,       // manual-reset or auto-reset
        initial_state,      // initial state
        NULL               // unnamed event object
    );

    if (evt->handle == NULL) {
        free(evt);
        return PLATFORM_ERROR_SYSTEM;
    }

    evt->manual_reset = manual_reset;
    evt->is_valid = true;
    *event = evt;
    
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_event_destroy(PlatformEvent_T event) {
    if (!event || !event->is_valid) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    event->is_valid = false;  // Mark as invalid before cleanup
    BOOL result = CloseHandle(event->handle);
    event->handle = NULL;
    free(event);
    
    return result ? PLATFORM_ERROR_SUCCESS : PLATFORM_ERROR_SYSTEM;
}

PlatformErrorCode platform_event_set(PlatformEvent_T event) {
    if (!event || !event->is_valid || !event->handle) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    return SetEvent(event->handle) ? PLATFORM_ERROR_SUCCESS : PLATFORM_ERROR_SYSTEM;
}

PlatformErrorCode platform_event_reset(PlatformEvent_T event) {
    if (!event || !event->is_valid || !event->handle) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    return ResetEvent(event->handle) ? PLATFORM_ERROR_SUCCESS : PLATFORM_ERROR_SYSTEM;
}

PlatformErrorCode platform_event_wait(PlatformEvent_T event, uint32_t timeout_ms) {
    if (!event || !event->is_valid || !event->handle) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    DWORD wait_result = WaitForSingleObject(
        event->handle,
        (timeout_ms == PLATFORM_WAIT_INFINITE) ? INFINITE : timeout_ms
    );

    switch (wait_result) {
        case WAIT_OBJECT_0:
            return PLATFORM_ERROR_SUCCESS;
        case WAIT_TIMEOUT:
            return PLATFORM_ERROR_TIMEOUT;
        default:
            return PLATFORM_ERROR_SYSTEM;
    }
}

PlatformWaitResult platform_wait_single(PlatformThreadId thread_id, uint32_t timeout_ms) {
    if (!thread_id) {
        return PLATFORM_WAIT_ERROR;
    }

    // Convert thread ID to handle
    if (thread_id > (PlatformThreadId)MAXDWORD) {
        return PLATFORM_WAIT_ERROR;
    }
    HANDLE handle = OpenThread(SYNCHRONIZE, FALSE, (DWORD)(uintptr_t)thread_id);
    if (handle == NULL) {
        return PLATFORM_WAIT_ERROR;
    }

    DWORD wait_result = WaitForSingleObject(
        handle,
        (timeout_ms == PLATFORM_WAIT_INFINITE) ? INFINITE : timeout_ms
    );

    // Clean up the handle
    CloseHandle(handle);

    switch (wait_result) {
        case WAIT_OBJECT_0:
            return PLATFORM_WAIT_SUCCESS;
        case WAIT_TIMEOUT:
            return PLATFORM_WAIT_TIMEOUT;
        default:
            return PLATFORM_WAIT_ERROR;
    }
}

PlatformWaitResult platform_wait_multiple(PlatformThreadId* thread_list, 
                                          uint32_t count, 
                                          bool wait_all, 
                                          uint32_t timeout_ms) {
    if (!thread_list || count == 0 || count > MAXIMUM_WAIT_OBJECTS) {
        return PLATFORM_WAIT_ERROR;
    }

    // Allocate temporary array for handles
    HANDLE* handles = (HANDLE*)malloc(count * sizeof(HANDLE));
    if (!handles) {
        return PLATFORM_WAIT_ERROR;
    }

    // Convert thread IDs to handles
    for (uint32_t i = 0; i < count; i++) {
        // Validate thread ID fits in DWORD
        if (thread_list[i] > (PlatformThreadId)MAXDWORD) {
            // Clean up already opened handles
            for (uint32_t j = 0; j < i; j++) {
                CloseHandle(handles[j]);
            }
            free(handles);
            return PLATFORM_WAIT_ERROR;
        }
        handles[i] = OpenThread(SYNCHRONIZE, FALSE, (DWORD)(uintptr_t)thread_list[i]);
        if (handles[i] == NULL) {
            // Clean up already opened handles
            for (uint32_t j = 0; j < i; j++) {
                CloseHandle(handles[j]);
            }
            free(handles);
            return PLATFORM_WAIT_ERROR;
        }
    }

    DWORD wait_result = WaitForMultipleObjects(
        count,
        handles,
        wait_all,
        (timeout_ms == PLATFORM_WAIT_INFINITE) ? INFINITE : timeout_ms
    );

    // Clean up handles
    for (uint32_t i = 0; i < count; i++) {
        CloseHandle(handles[i]);
    }
    free(handles);

    if (wait_result >= WAIT_OBJECT_0 && 
        wait_result < WAIT_OBJECT_0 + count) {
        return PLATFORM_WAIT_SUCCESS;
    } else if (wait_result == WAIT_TIMEOUT) {
        return PLATFORM_WAIT_TIMEOUT;
    }

    return PLATFORM_WAIT_ERROR;
}

// Signal handling
#define PLATFORM_SIGNAL_MAX 2
static PlatformSignalHandler g_signal_handlers[PLATFORM_SIGNAL_MAX] = {NULL};
static CRITICAL_SECTION g_signal_lock;
static bool g_signal_initialized = false;

static void initialize_signal_handling(void) {
    if (!g_signal_initialized) {
        InitializeCriticalSection(&g_signal_lock);
        g_signal_initialized = true;
    }
}

static BOOL WINAPI console_handler(DWORD ctrl_type) {
    if (!g_signal_initialized) {
        return FALSE;
    }

    EnterCriticalSection(&g_signal_lock);
    PlatformSignalHandler handler = NULL;

    switch (ctrl_type) {
        case CTRL_C_EVENT:
            handler = g_signal_handlers[PLATFORM_SIGNAL_INT];
            break;
        case CTRL_CLOSE_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            handler = g_signal_handlers[PLATFORM_SIGNAL_TERM];
            break;
    }

    if (handler) {
        LeaveCriticalSection(&g_signal_lock);
        handler();
        return TRUE;
    }

    LeaveCriticalSection(&g_signal_lock);
    return FALSE;
}

bool platform_signal_register_handler(PlatformSignalType signal_type, 
                                    PlatformSignalHandler handler) {
    if (!handler || signal_type >= PLATFORM_SIGNAL_MAX) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }

    if (!g_signal_initialized) {
        initialize_signal_handling();
    }

    EnterCriticalSection(&g_signal_lock);

    static bool console_handler_registered = false;
    if (!console_handler_registered) {
        if (!SetConsoleCtrlHandler(console_handler, TRUE)) {
            LeaveCriticalSection(&g_signal_lock);
            return false;
        }
        console_handler_registered = true;
    }

    g_signal_handlers[signal_type] = handler;
    LeaveCriticalSection(&g_signal_lock);

    return true;
}

bool platform_signal_unregister_handler(PlatformSignalType signal_type) {
    if (!g_signal_initialized || signal_type >= PLATFORM_SIGNAL_MAX) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }

    EnterCriticalSection(&g_signal_lock);

    g_signal_handlers[signal_type] = NULL;

    // Check if all handlers are NULL
    bool all_handlers_null = true;
    for (int i = 0; i < PLATFORM_SIGNAL_MAX; i++) {
        if (g_signal_handlers[i] != NULL) {
            all_handlers_null = false;
            break;
        }
    }

    // If all handlers are NULL, unregister the console handler
    if (all_handlers_null) {
        if (SetConsoleCtrlHandler(console_handler, FALSE)) {
            DeleteCriticalSection(&g_signal_lock);
            g_signal_initialized = false;
            return true;
        }
        LeaveCriticalSection(&g_signal_lock);
        return false;
    }

    LeaveCriticalSection(&g_signal_lock);
    return true;
}

#endif // _WIN32
