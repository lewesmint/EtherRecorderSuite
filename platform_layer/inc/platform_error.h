/**
 * @file platform_error.h
 * @brief Platform-agnostic error handling and reporting
 */
#ifndef PLATFORM_ERROR_H
#define PLATFORM_ERROR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Error domains for categorizing different types of errors
 */
typedef enum {
    PLATFORM_ERROR_DOMAIN_SYSTEM,    ///< General system errors
    PLATFORM_ERROR_DOMAIN_IO,        ///< I/O related errors
    PLATFORM_ERROR_DOMAIN_NETWORK,   ///< Network and socket errors
    PLATFORM_ERROR_DOMAIN_THREAD,    ///< Thread and synchronization errors
    PLATFORM_ERROR_DOMAIN_MEMORY,    ///< Memory management errors
    PLATFORM_ERROR_DOMAIN_TIME,      ///< Time-related errors
    PLATFORM_ERROR_DOMAIN_RESOURCE,  ///< Resource management errors
    PLATFORM_ERROR_DOMAIN_MAX        ///< Maximum domain value (must be last)
} PlatformErrorDomain;

/**
 * @brief Platform error codes with explicit numeric values for ABI compatibility
 */
typedef enum PlatformErrorCode {
    // General errors (0-99)
    PLATFORM_ERROR_SUCCESS = 0,
    PLATFORM_ERROR_UNKNOWN = 1,
    PLATFORM_ERROR_INVALID_ARGUMENT = 2,
    PLATFORM_ERROR_NOT_IMPLEMENTED = 3,
    PLATFORM_ERROR_NOT_SUPPORTED = 4,
    PLATFORM_ERROR_PERMISSION_DENIED = 5,
    PLATFORM_ERROR_TIMEOUT = 6,
    PLATFORM_ERROR_BUFFER_TOO_SMALL = 7,
    PLATFORM_ERROR_NOT_INITIALIZED = 8,
    PLATFORM_ERROR_NOT_FOUND = 9,
    PLATFORM_ERROR_ALREADY_EXISTS = 10,
    PLATFORM_ERROR_OUT_OF_MEMORY = 11,
    PLATFORM_ERROR_BUSY = 12,
    PLATFORM_ERROR_WOULD_BLOCK = 13,
    PLATFORM_ERROR_SYSTEM = 14,        // Generic system error

    // Socket errors (100-199)
    PLATFORM_ERROR_SOCKET_CREATE = 100,
    PLATFORM_ERROR_SOCKET_BIND = 101,
    PLATFORM_ERROR_SOCKET_CONNECT = 102,
    PLATFORM_ERROR_SOCKET_LISTEN = 103,
    PLATFORM_ERROR_SOCKET_ACCEPT = 104,
    PLATFORM_ERROR_SOCKET_SEND = 105,
    PLATFORM_ERROR_SOCKET_RECEIVE = 106,
    PLATFORM_ERROR_SOCKET_CLOSED = 107,
    PLATFORM_ERROR_HOST_NOT_FOUND = 108,
    PLATFORM_ERROR_SOCKET_OPTION = 109,
    PLATFORM_ERROR_SOCKET_RESOLVE = 110,

    // Thread errors (200-299)
    PLATFORM_ERROR_THREAD_CREATE = 200,
    PLATFORM_ERROR_THREAD_JOIN = 201,
    PLATFORM_ERROR_THREAD_DETACH = 202,
    PLATFORM_ERROR_MUTEX_INIT = 203,
    PLATFORM_ERROR_MUTEX_LOCK = 204,
    PLATFORM_ERROR_MUTEX_UNLOCK = 205,
    PLATFORM_ERROR_CONDITION_INIT = 206,
    PLATFORM_ERROR_CONDITION_WAIT = 207,
    PLATFORM_ERROR_CONDITION_SIGNAL = 208,

    // File system errors (300-399)
    PLATFORM_ERROR_FILE_NOT_FOUND = 300,
    PLATFORM_ERROR_FILE_EXISTS = 301,
    PLATFORM_ERROR_FILE_ACCESS = 302,
    PLATFORM_ERROR_DIRECTORY_NOT_FOUND = 303,
    PLATFORM_ERROR_DIRECTORY_EXISTS = 304,
    PLATFORM_ERROR_DIRECTORY_ACCESS = 305,

    // Memory errors (400-499)
    PLATFORM_ERROR_MEMORY_ALLOC = 400,
    PLATFORM_ERROR_MEMORY_FREE = 401,
    PLATFORM_ERROR_MEMORY_ACCESS = 402
} PlatformErrorCode;

/**
 * @brief Error information structure
 */
typedef struct PlatformError {
    PlatformErrorDomain domain;      ///< Error domain
    int32_t code;                    ///< Error code
    uint32_t system_error;           ///< Platform-specific system error code
    char message[256];               ///< Human-readable error message
} PlatformError;

/**
 * @brief Get the last platform error
 * @param[in] domain Error domain to check
 * @return Error code corresponding to the last error
 */
int32_t platform_get_last_error(PlatformErrorDomain domain);

/**
 * @brief Get detailed error information
 * @param[in] domain Error domain to check
 * @param[out] error Error information structure to fill
 * @return PlatformResult indicating success of the operation
 */
PlatformErrorCode platform_get_error_info(PlatformErrorDomain domain, PlatformError* error);

/**
 * @brief Convert system error to platform error code
 * @param[in] system_error Platform-specific system error code
 * @param[in] domain Error domain for categorization
 * @return Error code corresponding to the system error
 */
int32_t platform_error_from_system(uint32_t system_error, PlatformErrorDomain domain);

/**
 * @brief Get error message for a platform error code
 * @param[in] code Platform error code
 * @param[out] buffer Buffer to store the message
 * @param[in] buffer_size Size of the buffer
 * @return PlatformResult indicating success of the operation
 */
PlatformErrorCode platform_get_error_message(
    int32_t code,
    char* buffer,
    size_t buffer_size);

/**
 * @brief Clear the last error in specified domain
 * @param[in] domain Error domain to clear
 */
void platform_clear_error(PlatformErrorDomain domain);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_ERROR_H
