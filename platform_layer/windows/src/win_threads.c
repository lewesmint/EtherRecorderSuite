/**
 * @file win_threads.c
 * @brief Windows implementation of platform thread management interface
 */
#include "platform_threads.h"
#include "platform_error.h"

#include <windows.h>
#include <process.h>
#include <stdlib.h>  // Add this for malloc/free functions

PlatformErrorCode platform_thread_init(void) {
    return PLATFORM_ERROR_SUCCESS;  // No specific init needed for Windows threads
}

void platform_thread_cleanup(void) {
    // No specific cleanup needed for Windows threads
}

// Wrapper for Windows thread start routine
typedef struct {
    PlatformThreadFunction function;
    void* arg;
} ThreadStartContext;

static unsigned __stdcall thread_start_routine(void* arg) {
    ThreadStartContext* context = (ThreadStartContext*)arg;
    PlatformThreadFunction function = context->function;
    void* thread_arg = context->arg;
    free(context);  // Free the context structure
    
    void* result = function(thread_arg);
    _endthreadex((unsigned)((uintptr_t)result));
    return (unsigned)((uintptr_t)result);
}

PlatformErrorCode platform_thread_create(
    PlatformThreadId* thread_id,
    const PlatformThreadAttributes* attributes,
    PlatformThreadFunction function,
    void* arg)  
{
    if (!thread_id || !function) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    // Allocate and initialize the thread context
    ThreadStartContext* context = (ThreadStartContext*)malloc(sizeof(ThreadStartContext));
    if (!context) {
        return PLATFORM_ERROR_OUT_OF_MEMORY;
    }
    context->function = function;
    context->arg = arg;

    // Set thread stack size
    size_t stack_size = 0;
    if (attributes && attributes->stack_size > 0) {
        stack_size = attributes->stack_size;
    }


    // Create the thread
    unsigned int temp_thread_id;
    uintptr_t thread_handle = _beginthreadex(
        NULL,           // Security attributes
        (unsigned)stack_size,  // Stack size
        thread_start_routine,  // Thread function
        context,       // Thread argument
        CREATE_SUSPENDED,  // Creation flags
        &temp_thread_id   // Thread ID
    );

    // Store the thread ID in platform-agnostic format
    *thread_id = (PlatformThreadId)temp_thread_id;

    if (thread_handle == 0) {
        free(context);
        return PLATFORM_ERROR_THREAD_CREATE;
    }

    // Set thread priority if specified
    if (attributes && attributes->priority != PLATFORM_THREAD_PRIORITY_NORMAL) {
        int win_priority;
        switch (attributes->priority) {
            case PLATFORM_THREAD_PRIORITY_LOWEST:
                win_priority = THREAD_PRIORITY_LOWEST;
                break;
            case PLATFORM_THREAD_PRIORITY_LOW:
                win_priority = THREAD_PRIORITY_BELOW_NORMAL;
                break;
            case PLATFORM_THREAD_PRIORITY_HIGH:
                win_priority = THREAD_PRIORITY_ABOVE_NORMAL;
                break;
            case PLATFORM_THREAD_PRIORITY_HIGHEST:
                win_priority = THREAD_PRIORITY_HIGHEST;
                break;
            case PLATFORM_THREAD_PRIORITY_REALTIME:
                win_priority = THREAD_PRIORITY_TIME_CRITICAL;
                break;
            default:
                win_priority = THREAD_PRIORITY_NORMAL;
        }
        SetThreadPriority((HANDLE)thread_handle, win_priority);
    }

    // Handle detached state
    if (attributes && attributes->detached) {
        CloseHandle((HANDLE)thread_handle);
        thread_handle = 0;  // Indicate detached state
    }

    // Start the thread
    ResumeThread((HANDLE)thread_handle);
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_thread_join(PlatformThreadHandle handle, void** result) {
    HANDLE thread_handle = (HANDLE)handle;
    
    if (WaitForSingleObject(thread_handle, INFINITE) != WAIT_OBJECT_0) {
        return PLATFORM_ERROR_THREAD_JOIN;
    }

    if (result) {
        DWORD exit_code;
        if (!GetExitCodeThread(thread_handle, &exit_code)) {
            CloseHandle(thread_handle);
            return PLATFORM_ERROR_THREAD_JOIN;
        }
        *result = (void*)(uintptr_t)exit_code;
    }

    CloseHandle(thread_handle);
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_thread_detach(PlatformThreadHandle handle) {
    HANDLE thread_handle = (HANDLE)handle;
    if (!CloseHandle(thread_handle)) {
        return PLATFORM_ERROR_THREAD_DETACH;
    }
    return PLATFORM_ERROR_SUCCESS;
}

PlatformThreadHandle platform_thread_get_handle(void) {
    return (PlatformThreadHandle)GetCurrentThread();
}

PlatformThreadId platform_thread_get_id(void) {
    return (PlatformThreadId)GetCurrentThreadId();
}

PlatformErrorCode platform_thread_set_priority(
    PlatformThreadHandle handle,
    PlatformThreadPriority priority)
{
    HANDLE thread_handle = (HANDLE)handle;
    int win_priority;

    switch (priority) {
        case PLATFORM_THREAD_PRIORITY_LOWEST:
            win_priority = THREAD_PRIORITY_LOWEST;
            break;
        case PLATFORM_THREAD_PRIORITY_LOW:
            win_priority = THREAD_PRIORITY_BELOW_NORMAL;
            break;
        case PLATFORM_THREAD_PRIORITY_NORMAL:
            win_priority = THREAD_PRIORITY_NORMAL;
            break;
        case PLATFORM_THREAD_PRIORITY_HIGH:
            win_priority = THREAD_PRIORITY_ABOVE_NORMAL;
            break;
        case PLATFORM_THREAD_PRIORITY_HIGHEST:
            win_priority = THREAD_PRIORITY_HIGHEST;
            break;
        case PLATFORM_THREAD_PRIORITY_REALTIME:
            win_priority = THREAD_PRIORITY_TIME_CRITICAL;
            break;
        default:
            return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    if (!SetThreadPriority(thread_handle, win_priority)) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_thread_get_priority(
    PlatformThreadHandle handle,
    PlatformThreadPriority* priority)
{
    if (!priority) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    HANDLE thread_handle = (HANDLE)handle;
    int win_priority = GetThreadPriority(thread_handle);
    
    if (win_priority == THREAD_PRIORITY_ERROR_RETURN) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    switch (win_priority) {
        case THREAD_PRIORITY_LOWEST:
            *priority = PLATFORM_THREAD_PRIORITY_LOWEST;
            break;
        case THREAD_PRIORITY_BELOW_NORMAL:
            *priority = PLATFORM_THREAD_PRIORITY_LOW;
            break;
        case THREAD_PRIORITY_NORMAL:
            *priority = PLATFORM_THREAD_PRIORITY_NORMAL;
            break;
        case THREAD_PRIORITY_ABOVE_NORMAL:
            *priority = PLATFORM_THREAD_PRIORITY_HIGH;
            break;
        case THREAD_PRIORITY_HIGHEST:
            *priority = PLATFORM_THREAD_PRIORITY_HIGHEST;
            break;
        case THREAD_PRIORITY_TIME_CRITICAL:
            *priority = PLATFORM_THREAD_PRIORITY_REALTIME;
            break;
        default:
            *priority = PLATFORM_THREAD_PRIORITY_NORMAL;
    }

    return PLATFORM_ERROR_SUCCESS;
}

void platform_thread_yield(void) {
    SwitchToThread();
}

PlatformErrorCode platform_thread_get_status(PlatformThreadId thread_id, PlatformThreadStatus* status) {
    if (!status) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    HANDLE thread_handle = OpenThread(THREAD_QUERY_INFORMATION, FALSE, (DWORD)thread_id);
    if (thread_handle == NULL) {
        *status = PLATFORM_THREAD_DEAD;
        return PLATFORM_ERROR_SUCCESS;
    }

    DWORD exit_code;
    if (GetExitCodeThread(thread_handle, &exit_code)) {
        if (exit_code == STILL_ACTIVE) {
            *status = PLATFORM_THREAD_ALIVE;
        } else {
            *status = PLATFORM_THREAD_TERMINATED;
        }
        CloseHandle(thread_handle);
        return PLATFORM_ERROR_SUCCESS;
    }

    CloseHandle(thread_handle);
    *status = PLATFORM_THREAD_UNKNOWN;
    return PLATFORM_ERROR_UNKNOWN;
}
