/**
 * @file platform_error.h
 * @brief Platform-agnostic error handling utilities
 */
#ifndef PLATFORM_ERROR_H
#define PLATFORM_ERROR_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Common error domains for platform-agnostic error handling
 */
typedef enum PlatformErrorDomain {
    PLATFORM_ERROR_DOMAIN_GENERAL,  // General system errors
    PLATFORM_ERROR_DOMAIN_SOCKET,   // Socket-related errors
    PLATFORM_ERROR_DOMAIN_THREAD,   // Thread-related errors
    PLATFORM_ERROR_DOMAIN_FILE,     // File system-related errors
    PLATFORM_ERROR_DOMAIN_MEMORY    // Memory allocation/access errors
} PlatformErrorDomain;

/**
 * @brief Common error codes for platform-agnostic error handling
 */
typedef enum PlatformErrorCode {
    // General error codes
    PLATFORM_ERROR_SUCCESS = 0,          // Operation succeeded
    PLATFORM_ERROR_UNKNOWN,              // Unknown error
    PLATFORM_ERROR_INVALID_ARGUMENT,     // Invalid argument provided
    PLATFORM_ERROR_NOT_IMPLEMENTED,      // Function not implemented
    PLATFORM_ERROR_NOT_SUPPORTED,        // Operation not supported
    PLATFORM_ERROR_PERMISSION_DENIED,    // Insufficient permissions
    PLATFORM_ERROR_TIMEOUT,              // Operation timed out

    // Socket-specific error codes
    PLATFORM_ERROR_SOCKET_CREATE,        // Error creating socket
    PLATFORM_ERROR_SOCKET_BIND,          // Error binding socket
    PLATFORM_ERROR_SOCKET_CONNECT,       // Error connecting socket
    PLATFORM_ERROR_SOCKET_LISTEN,        // Error listening on socket
    PLATFORM_ERROR_SOCKET_ACCEPT,        // Error accepting connection
    PLATFORM_ERROR_SOCKET_SEND,          // Error sending data
    PLATFORM_ERROR_SOCKET_RECEIVE,       // Error receiving data
    PLATFORM_ERROR_SOCKET_CLOSED,        // Socket has been closed
    PLATFORM_ERROR_HOST_NOT_FOUND,       // Host not found

    // Thread-specific error codes
    PLATFORM_ERROR_THREAD_CREATE,        // Error creating thread
    PLATFORM_ERROR_THREAD_JOIN,          // Error joining thread
    PLATFORM_ERROR_THREAD_DETACH,        // Error detaching thread
    PLATFORM_ERROR_MUTEX_INIT,           // Error initializing mutex
    PLATFORM_ERROR_MUTEX_LOCK,           // Error locking mutex
    PLATFORM_ERROR_MUTEX_UNLOCK,         // Error unlocking mutex
    PLATFORM_ERROR_CONDITION_INIT,       // Error initializing condition variable
    PLATFORM_ERROR_CONDITION_WAIT,       // Error waiting on condition variable
    PLATFORM_ERROR_CONDITION_SIGNAL,     // Error signaling condition variable

    // File system-specific error codes
    PLATFORM_ERROR_FILE_NOT_FOUND,       // File not found
    PLATFORM_ERROR_FILE_EXISTS,          // File already exists
    PLATFORM_ERROR_FILE_ACCESS,          // File access error
    PLATFORM_ERROR_DIRECTORY_NOT_FOUND,  // Directory not found
    PLATFORM_ERROR_DIRECTORY_EXISTS,     // Directory already exists
    PLATFORM_ERROR_DIRECTORY_ACCESS,     // Directory access error

    // Memory-specific error codes
    PLATFORM_ERROR_MEMORY_ALLOC,         // Memory allocation error
    PLATFORM_ERROR_MEMORY_FREE,          // Memory free error
    PLATFORM_ERROR_MEMORY_ACCESS         // Memory access error
} PlatformErrorCode;

/**
 * @brief Gets a platform-agnostic error code based on the last system error
 * 
 * @param domain The error domain to use for categorizing the error
 * @return PlatformErrorCode The platform-agnostic error code
 */
PlatformErrorCode platform_get_error_code(PlatformErrorDomain domain);

/**
 * @brief Gets a human-readable error message for a platform-agnostic error code
 * 
 * @param error_code The platform-agnostic error code
 * @param buffer Buffer to store the error message
 * @param buffer_size Size of the buffer
 * @return char* Pointer to the buffer containing the error message
 */
char* platform_get_error_message_from_code(PlatformErrorCode error_code, char* buffer, size_t buffer_size);

/**
 * @brief Gets a human-readable error message for the last system error
 * 
 * @param domain The error domain to use for categorizing the error
 * @param buffer Buffer to store the error message
 * @param buffer_size Size of the buffer
 * @return char* Pointer to the buffer containing the error message
 */
char* platform_get_error_message(PlatformErrorDomain domain, char* buffer, size_t buffer_size);

/**
 * @brief Gets a platform-agnostic socket error code based on the last socket error
 * 
 * @return PlatformErrorCode The platform-agnostic error code
 */
PlatformErrorCode platform_get_socket_error_code(void);

/**
 * @brief Gets a human-readable error message for the last socket error
 * 
 * @param buffer Buffer to store the error message
 * @param buffer_size Size of the buffer
 * @return char* Pointer to the buffer containing the error message
 */
char* platform_get_socket_error_message(char* buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_ERROR_H