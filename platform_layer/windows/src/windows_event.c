/**
 * @file windows_event.c
 * @brief Windows implementation of platform event synchronization primitive
 */
#include "platform_event.h"
#include "platform_error.h"
#include <windows.h>
#include <stdlib.h>

struct platform_event {
    HANDLE handle;
};

PlatformErrorCode platform_event_create(PlatformEvent_T* event, bool manual_reset, bool initial_state) {
    if (!event) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    struct platform_event* evt = malloc(sizeof(struct platform_event));
    if (!evt) {
        return PLATFORM_ERROR_OUT_OF_MEMORY;
    }

    evt->handle = CreateEventA(NULL, manual_reset, initial_state, NULL);
    if (evt->handle == NULL) {
        free(evt);
        return PLATFORM_ERROR_SYSTEM;
    }

    *event = evt;
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_event_destroy(PlatformEvent_T event) {
    if (!event) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    if (!CloseHandle(event->handle)) {
        free(event);
        return PLATFORM_ERROR_SYSTEM;
    }

    free(event);
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_event_set(PlatformEvent_T event) {
    if (!event) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    if (!SetEvent(event->handle)) {
        return PLATFORM_ERROR_SYSTEM;
    }

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_event_reset(PlatformEvent_T event) {
    if (!event) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    if (!ResetEvent(event->handle)) {
        return PLATFORM_ERROR_SYSTEM;
    }

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_event_wait(PlatformEvent_T event, uint32_t timeout_ms) {
    if (!event) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    DWORD result = WaitForSingleObject(event->handle, timeout_ms);
    
    switch (result) {
        case WAIT_OBJECT_0:
            return PLATFORM_ERROR_SUCCESS;
        case WAIT_TIMEOUT:
            return PLATFORM_ERROR_TIMEOUT;
        default:
            return PLATFORM_ERROR_SYSTEM;
    }
}