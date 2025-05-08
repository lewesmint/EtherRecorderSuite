/**
 * @file platform_error.c
 * @brief Implementation of platform-agnostic error handling utilities
 */

#include "platform_error.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief Utility function to sanitize error messages
 * 
 * Removes trailing whitespace, newlines, carriage returns, and periods.
 * 
 * @param message The message to sanitize
 * @return char* Pointer to the sanitized message
 */
static char* sanitize_error_message(char* message) {
    if (!message) {
        return message;
    }
    
    size_t len = strlen(message);
    
    // Trim trailing spaces, newlines, carriage returns, and periods
    while (len > 0 && (message[len-1] == ' ' || 
                        message[len-1] == '\n' || 
                        message[len-1] == '\r' || 
                        message[len-1] == '.')) {
        message[--len] = '\0';
    }
    
    return message;
}
  
char* platform_get_error_message_from_code(PlatformErrorCode error_code, char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return NULL;
    }
    
    // Provide consistent, human-readable messages for each error code
    switch (error_code) {
        // General errors
        case PLATFORM_ERROR_SUCCESS:               snprintf(buffer, buffer_size, "Success"); break;
        case PLATFORM_ERROR_UNKNOWN:               snprintf(buffer, buffer_size, "Unknown error"); break;
        case PLATFORM_ERROR_INVALID_ARGUMENT:      snprintf(buffer, buffer_size, "Invalid argument"); break;
        case PLATFORM_ERROR_NOT_IMPLEMENTED:       snprintf(buffer, buffer_size, "Not implemented"); break;
        case PLATFORM_ERROR_NOT_SUPPORTED:         snprintf(buffer, buffer_size, "Not supported"); break;
        case PLATFORM_ERROR_PERMISSION_DENIED:     snprintf(buffer, buffer_size, "Permission denied"); break;
        case PLATFORM_ERROR_TIMEOUT:               snprintf(buffer, buffer_size, "Operation timed out"); break;
        case PLATFORM_ERROR_BUFFER_TOO_SMALL:      snprintf(buffer, buffer_size, "Buffer too small"); break;
        case PLATFORM_ERROR_NOT_INITIALIZED:       snprintf(buffer, buffer_size, "Not initialized"); break;
        case PLATFORM_ERROR_NOT_FOUND:             snprintf(buffer, buffer_size, "Not found"); break;
        case PLATFORM_ERROR_ALREADY_EXISTS:        snprintf(buffer, buffer_size, "Already exists"); break;
        case PLATFORM_ERROR_OUT_OF_MEMORY:         snprintf(buffer, buffer_size, "Out of memory"); break;
        case PLATFORM_ERROR_BUSY:                  snprintf(buffer, buffer_size, "Resource busy"); break;
        case PLATFORM_ERROR_WOULD_BLOCK:           snprintf(buffer, buffer_size, "Operation would block"); break;
        case PLATFORM_ERROR_SYSTEM:                snprintf(buffer, buffer_size, "System error"); break;
        case PLATFORM_ERROR_INTERRUPTED:           snprintf(buffer, buffer_size, "Operation interrupted"); break;

        // Socket-specific errors
        case PLATFORM_ERROR_SOCKET_CREATE:         snprintf(buffer, buffer_size, "Failed to create socket"); break;
        case PLATFORM_ERROR_SOCKET_BIND:           snprintf(buffer, buffer_size, "Failed to bind socket"); break;
        case PLATFORM_ERROR_SOCKET_CONNECT:        snprintf(buffer, buffer_size, "Failed to connect socket"); break;
        case PLATFORM_ERROR_SOCKET_LISTEN:         snprintf(buffer, buffer_size, "Failed to listen on socket"); break;
        case PLATFORM_ERROR_SOCKET_ACCEPT:         snprintf(buffer, buffer_size, "Failed to accept connection"); break;
        case PLATFORM_ERROR_SOCKET_SEND:           snprintf(buffer, buffer_size, "Failed to send data"); break;
        case PLATFORM_ERROR_SOCKET_RECEIVE:        snprintf(buffer, buffer_size, "Failed to receive data"); break;
        case PLATFORM_ERROR_SOCKET_CLOSED:         snprintf(buffer, buffer_size, "Socket connection closed"); break;
        case PLATFORM_ERROR_HOST_NOT_FOUND:        snprintf(buffer, buffer_size, "Host not found or unreachable"); break;
        case PLATFORM_ERROR_CONNECTION_REFUSED:    snprintf(buffer, buffer_size, "Connection refused by server"); break;
        case PLATFORM_ERROR_NETWORK_DOWN:          snprintf(buffer, buffer_size, "Network interface is down"); break;
        case PLATFORM_ERROR_NETWORK_UNREACHABLE:   snprintf(buffer, buffer_size, "Network is unreachable"); break;
        case PLATFORM_ERROR_SOCKET_OPTION:         snprintf(buffer, buffer_size, "Failed to set socket option"); break;
        case PLATFORM_ERROR_SOCKET_RESOLVE:        snprintf(buffer, buffer_size, "Failed to resolve hostname"); break;
        case PLATFORM_ERROR_SOCKET_SELECT:         snprintf(buffer, buffer_size, "Socket select operation failed"); break;

        // Memory errors
        case PLATFORM_ERROR_MEMORY_ALLOC:          snprintf(buffer, buffer_size, "Memory allocation failed"); break;
        case PLATFORM_ERROR_MEMORY_FREE:           snprintf(buffer, buffer_size, "Memory free failed"); break;
        case PLATFORM_ERROR_MEMORY_ACCESS:         snprintf(buffer, buffer_size, "Invalid memory access"); break;

        // File errors
        case PLATFORM_ERROR_FILE_NOT_FOUND:        snprintf(buffer, buffer_size, "File not found"); break;
        case PLATFORM_ERROR_FILE_EXISTS:           snprintf(buffer, buffer_size, "File already exists"); break;
        case PLATFORM_ERROR_FILE_ACCESS:           snprintf(buffer, buffer_size, "File access error"); break;
        case PLATFORM_ERROR_FILE_OPEN:             snprintf(buffer, buffer_size, "Failed to open file"); break;
        case PLATFORM_ERROR_FILE_READ:             snprintf(buffer, buffer_size, "Failed to read from file"); break;
        case PLATFORM_ERROR_FILE_WRITE:            snprintf(buffer, buffer_size, "Failed to write to file"); break;
        case PLATFORM_ERROR_FILE_SEEK:             snprintf(buffer, buffer_size, "Failed to seek in file"); break;
        case PLATFORM_ERROR_FILE_LOCKED:           snprintf(buffer, buffer_size, "File is locked by another process"); break;
        case PLATFORM_ERROR_DIRECTORY_NOT_FOUND:   snprintf(buffer, buffer_size, "Directory not found"); break;
        case PLATFORM_ERROR_DIRECTORY_EXISTS:      snprintf(buffer, buffer_size, "Directory already exists"); break;
        case PLATFORM_ERROR_DIRECTORY_ACCESS:      snprintf(buffer, buffer_size, "Directory access error"); break;

        default:                                   snprintf(buffer, buffer_size, "Error code %d", error_code); break;
    }
    
    return buffer;
}
