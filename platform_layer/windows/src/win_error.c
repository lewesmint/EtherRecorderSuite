/**
 * @file win_error.c
 * @brief Windows implementation of platform error handling
 */
#include "platform_error.h"

#include <windows.h>
#include <string.h>
#include <stdio.h>

// Thread-local storage for last error information
static __declspec(thread) struct {
    PlatformErrorDomain domain;
    int32_t code;
    DWORD system_error;
    char message[256];
} g_last_error = {
    .domain = PLATFORM_ERROR_DOMAIN_SYSTEM,
    .code = PLATFORM_ERROR_SUCCESS,
    .system_error = 0,
    .message = {0}
};

static void sanitize_error_message(char* message) {
    if (!message) {
        return;
    }
    
    size_t len = strlen(message);
    while (len > 0 && (message[len-1] == ' ' || 
                       message[len-1] == '\n' || 
                       message[len-1] == '\r' || 
                       message[len-1] == '.')) {
        message[--len] = '\0';
    }
}

static int32_t map_windows_error(DWORD error_code, PlatformErrorDomain domain) {
    switch (domain) {
        case PLATFORM_ERROR_DOMAIN_NETWORK:
            switch (error_code) {
                case ERROR_ACCESS_DENIED:       return PLATFORM_ERROR_PERMISSION_DENIED;
                case WSAEADDRINUSE:            return PLATFORM_ERROR_SOCKET_BIND;
                case WSAECONNREFUSED:          return PLATFORM_ERROR_SOCKET_CONNECT;
                case WSAETIMEDOUT:             return PLATFORM_ERROR_TIMEOUT;
                case WSAENOTSOCK:              return PLATFORM_ERROR_INVALID_ARGUMENT;
                case WSAEINVAL:                return PLATFORM_ERROR_INVALID_ARGUMENT;
                case WSAEAFNOSUPPORT:          return PLATFORM_ERROR_NOT_SUPPORTED;
                case WSAECONNRESET:            return PLATFORM_ERROR_SOCKET_CLOSED;
                case WSAEHOSTUNREACH:          return PLATFORM_ERROR_HOST_NOT_FOUND;
                case WSAENETUNREACH:           return PLATFORM_ERROR_NOT_FOUND;
                case WSAEWOULDBLOCK:           return PLATFORM_ERROR_WOULD_BLOCK;
                default:                       return PLATFORM_ERROR_UNKNOWN;
            }

        case PLATFORM_ERROR_DOMAIN_IO:
            switch (error_code) {
                case ERROR_ACCESS_DENIED:       return PLATFORM_ERROR_PERMISSION_DENIED;
                case ERROR_FILE_NOT_FOUND:      return PLATFORM_ERROR_NOT_FOUND;
                case ERROR_PATH_NOT_FOUND:      return PLATFORM_ERROR_NOT_FOUND;
                case ERROR_ALREADY_EXISTS:      return PLATFORM_ERROR_ALREADY_EXISTS;
                case ERROR_INVALID_PARAMETER:   return PLATFORM_ERROR_INVALID_ARGUMENT;
                case ERROR_DIRECTORY:           return PLATFORM_ERROR_DIRECTORY_ACCESS;
                case ERROR_TOO_MANY_OPEN_FILES: return PLATFORM_ERROR_OUT_OF_MEMORY;
                case ERROR_DISK_FULL:          return PLATFORM_ERROR_OUT_OF_MEMORY;
                default:                       return PLATFORM_ERROR_UNKNOWN;
            }

        case PLATFORM_ERROR_DOMAIN_THREAD:
            switch (error_code) {
                case ERROR_NOT_ENOUGH_MEMORY:   return PLATFORM_ERROR_OUT_OF_MEMORY;
                case ERROR_INVALID_PARAMETER:   return PLATFORM_ERROR_INVALID_ARGUMENT;
                case ERROR_ACCESS_DENIED:       return PLATFORM_ERROR_PERMISSION_DENIED;
                case ERROR_INVALID_THREAD_ID:   return PLATFORM_ERROR_NOT_FOUND;
                default:                       return PLATFORM_ERROR_UNKNOWN;
            }

        case PLATFORM_ERROR_DOMAIN_SYSTEM:
        default:
            switch (error_code) {
                case ERROR_SUCCESS:             return PLATFORM_ERROR_SUCCESS;
                case ERROR_INVALID_PARAMETER:   return PLATFORM_ERROR_INVALID_ARGUMENT;
                case ERROR_NOT_SUPPORTED:       return PLATFORM_ERROR_NOT_IMPLEMENTED;
                case ERROR_ACCESS_DENIED:       return PLATFORM_ERROR_PERMISSION_DENIED;
                case ERROR_TIMEOUT:             return PLATFORM_ERROR_TIMEOUT;
                case ERROR_FILE_NOT_FOUND:      return PLATFORM_ERROR_NOT_FOUND;
                case ERROR_PATH_NOT_FOUND:      return PLATFORM_ERROR_NOT_FOUND;
                case ERROR_ALREADY_EXISTS:      return PLATFORM_ERROR_ALREADY_EXISTS;
                case ERROR_NOT_ENOUGH_MEMORY:   return PLATFORM_ERROR_OUT_OF_MEMORY;
                case ERROR_BUSY:                return PLATFORM_ERROR_BUSY;
                case ERROR_IO_PENDING:          return PLATFORM_ERROR_WOULD_BLOCK;
                case ERROR_BAD_ARGUMENTS:       return PLATFORM_ERROR_INVALID_ARGUMENT;
                default:                       return PLATFORM_ERROR_UNKNOWN;
            }
    }
}

PlatformErrorCode platform_set_error(PlatformErrorDomain domain, int32_t code, DWORD system_error) {
    if (domain >= PLATFORM_ERROR_DOMAIN_MAX) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    g_last_error.domain = domain;
    g_last_error.code = code;
    g_last_error.system_error = system_error;

    DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD lang_id = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);

    if (FormatMessageA(
            flags,
            NULL,
            system_error,
            lang_id,
            g_last_error.message,
            sizeof(g_last_error.message),
            NULL) == 0) {
        snprintf(g_last_error.message, sizeof(g_last_error.message),
                "Unknown error (0x%08lX)", system_error);
    }

    sanitize_error_message(g_last_error.message);
    g_last_error.message[sizeof(g_last_error.message) - 1] = '\0';

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_get_error(PlatformError* error) {
    if (!error) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    error->domain = g_last_error.domain;
    error->code = g_last_error.code;
    error->system_error = g_last_error.system_error;
    strncpy(error->message, g_last_error.message, sizeof(error->message) - 1);
    error->message[sizeof(error->message) - 1] = '\0';

    return PLATFORM_ERROR_SUCCESS;
}

void platform_clear_error(PlatformErrorDomain domain) {
    if (domain >= PLATFORM_ERROR_DOMAIN_MAX) {
        return;
    }

    g_last_error.domain = domain;
    g_last_error.code = PLATFORM_ERROR_SUCCESS;
    g_last_error.system_error = 0;
    g_last_error.message[0] = '\0';
}

int32_t platform_get_last_error(PlatformErrorDomain domain) {
    return (domain == g_last_error.domain) ? g_last_error.code : PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_set_system_error(DWORD system_error) {
    return platform_set_error(
        PLATFORM_ERROR_DOMAIN_SYSTEM,
        map_windows_error(system_error, PLATFORM_ERROR_DOMAIN_SYSTEM),
        system_error
    );
}

PlatformErrorCode platform_get_error_message(
    int32_t code,
    char* buffer,
    size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    const char* message;
    switch (code) {
        case PLATFORM_ERROR_SUCCESS:           message = "Success"; break;
        case PLATFORM_ERROR_UNKNOWN:           message = "Unknown error"; break;
        case PLATFORM_ERROR_INVALID_ARGUMENT:  message = "Invalid argument"; break;
        case PLATFORM_ERROR_NOT_IMPLEMENTED:   message = "Not implemented"; break;
        case PLATFORM_ERROR_NOT_SUPPORTED:     message = "Not supported"; break;
        case PLATFORM_ERROR_PERMISSION_DENIED: message = "Permission denied"; break;
        case PLATFORM_ERROR_TIMEOUT:           message = "Operation timed out"; break;
        case PLATFORM_ERROR_NOT_FOUND:         message = "Not found"; break;
        case PLATFORM_ERROR_ALREADY_EXISTS:    message = "Already exists"; break;
        case PLATFORM_ERROR_OUT_OF_MEMORY:     message = "Out of memory"; break;
        case PLATFORM_ERROR_BUSY:              message = "Resource busy"; break;
        case PLATFORM_ERROR_WOULD_BLOCK:       message = "Operation would block"; break;
        case PLATFORM_ERROR_SOCKET_CREATE:     message = "Failed to create socket"; break;
        case PLATFORM_ERROR_SOCKET_BIND:       message = "Failed to bind socket"; break;
        case PLATFORM_ERROR_SOCKET_CONNECT:    message = "Failed to connect"; break;
        case PLATFORM_ERROR_SOCKET_CLOSED:     message = "Connection closed"; break;
        case PLATFORM_ERROR_HOST_NOT_FOUND:    message = "Host not found"; break;
        default:                               message = "Unrecognized error code";
    }

    strncpy(buffer, message, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
    return PLATFORM_ERROR_SUCCESS;
}