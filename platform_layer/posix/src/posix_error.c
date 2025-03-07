/**
 * @file platform_error.c
 * @brief POSIX implementation of platform error handling
 */
#include "platform_error.h"

#include <errno.h>
#include <string.h>
#include <stdio.h>

// Thread-local storage for last error information
static __thread struct {
    PlatformErrorDomain domain;
    int32_t code;
    int system_error;
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

static int32_t map_posix_error(int error_code, PlatformErrorDomain domain) {
    switch (domain) {
        case PLATFORM_ERROR_DOMAIN_NETWORK:
            switch (error_code) {
                case EACCES:         return PLATFORM_ERROR_PERMISSION_DENIED;
                case EADDRINUSE:     return PLATFORM_ERROR_SOCKET_BIND;
                case ECONNREFUSED:   return PLATFORM_ERROR_SOCKET_CONNECT;
                case ETIMEDOUT:      return PLATFORM_ERROR_TIMEOUT;
                case ENOTSOCK:       return PLATFORM_ERROR_INVALID_ARGUMENT;
                case EINVAL:         return PLATFORM_ERROR_INVALID_ARGUMENT;
                case EAFNOSUPPORT:   return PLATFORM_ERROR_NOT_SUPPORTED;
                case ECONNRESET:     return PLATFORM_ERROR_SOCKET_CLOSED;
                case EHOSTUNREACH:   return PLATFORM_ERROR_HOST_NOT_FOUND;
                case ENETUNREACH:    return PLATFORM_ERROR_NOT_FOUND;
                case EWOULDBLOCK:    return PLATFORM_ERROR_WOULD_BLOCK;
                default:             return PLATFORM_ERROR_UNKNOWN;
            }
            
        case PLATFORM_ERROR_DOMAIN_IO:
            switch (error_code) {
                case EACCES:         return PLATFORM_ERROR_PERMISSION_DENIED;
                case ENOENT:         return PLATFORM_ERROR_NOT_FOUND;
                case EEXIST:         return PLATFORM_ERROR_ALREADY_EXISTS;
                case EINVAL:         return PLATFORM_ERROR_INVALID_ARGUMENT;
                case EISDIR:         return PLATFORM_ERROR_DIRECTORY_ACCESS;
                case ENOTDIR:        return PLATFORM_ERROR_DIRECTORY_NOT_FOUND;
                case EMFILE:         return PLATFORM_ERROR_OUT_OF_MEMORY;
                case ENOSPC:         return PLATFORM_ERROR_OUT_OF_MEMORY;
                default:             return PLATFORM_ERROR_UNKNOWN;
            }

        case PLATFORM_ERROR_DOMAIN_THREAD:
            switch (error_code) {
                case EAGAIN:         return PLATFORM_ERROR_OUT_OF_MEMORY;
                case EINVAL:         return PLATFORM_ERROR_INVALID_ARGUMENT;
                case EPERM:          return PLATFORM_ERROR_PERMISSION_DENIED;
                case ESRCH:          return PLATFORM_ERROR_NOT_FOUND;
                default:             return PLATFORM_ERROR_UNKNOWN;
            }
            
        case PLATFORM_ERROR_DOMAIN_SYSTEM:
        default:
            switch (error_code) {
                case 0:              return PLATFORM_ERROR_SUCCESS;
                case EINVAL:         return PLATFORM_ERROR_INVALID_ARGUMENT;
                case ENOSYS:         return PLATFORM_ERROR_NOT_IMPLEMENTED;
                case ENOTSUP:        return PLATFORM_ERROR_NOT_SUPPORTED;
                case EACCES:         return PLATFORM_ERROR_PERMISSION_DENIED;
                case ETIMEDOUT:      return PLATFORM_ERROR_TIMEOUT;
                case ENOENT:         return PLATFORM_ERROR_NOT_FOUND;
                case EEXIST:         return PLATFORM_ERROR_ALREADY_EXISTS;
                case ENOMEM:         return PLATFORM_ERROR_OUT_OF_MEMORY;
                case EBUSY:          return PLATFORM_ERROR_BUSY;
                case EAGAIN:         return PLATFORM_ERROR_WOULD_BLOCK;
                case EPERM:          return PLATFORM_ERROR_PERMISSION_DENIED;
                case EFAULT:         return PLATFORM_ERROR_INVALID_ARGUMENT;
                case ERANGE:         return PLATFORM_ERROR_OUT_OF_MEMORY;
                default:             return PLATFORM_ERROR_UNKNOWN;
            }
    }
}

PlatformErrorCode platform_set_error(PlatformErrorDomain domain, int32_t code, int system_error) {
    if (domain >= PLATFORM_ERROR_DOMAIN_MAX) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    g_last_error.domain = domain;
    g_last_error.code = code;
    g_last_error.system_error = system_error;
    
    #if defined(_GNU_SOURCE)
        char* sys_msg = strerror_r(system_error, g_last_error.message, sizeof(g_last_error.message));
        if (sys_msg != g_last_error.message) {
            strncpy(g_last_error.message, sys_msg, sizeof(g_last_error.message) - 1);
        }
    #else
        strerror_r(system_error, g_last_error.message, sizeof(g_last_error.message));
    #endif

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

PlatformErrorCode platform_set_system_error(int system_error) {
    return platform_set_error(
        PLATFORM_ERROR_DOMAIN_SYSTEM,
        map_posix_error(system_error, PLATFORM_ERROR_DOMAIN_SYSTEM),
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
        default:                               message = "Unrecognized error code";
    }

    strncpy(buffer, message, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
    return PLATFORM_ERROR_SUCCESS;
}
