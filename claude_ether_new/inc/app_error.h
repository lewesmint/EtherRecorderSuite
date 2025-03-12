#ifndef APP_ERROR_H
#define APP_ERROR_H

// Error domains
enum {
    THREAD_REGISTRY_DOMAIN,
    // Add other domains here
};

// Thread Registry error codes
typedef enum ThreadRegistryError {
    THREAD_REG_SUCCESS = 0,
    THREAD_REG_NOT_INITIALIZED,
    THREAD_REG_INVALID_ARGS,
    THREAD_REG_LOCK_ERROR,
    THREAD_REG_WAIT_ERROR,
    THREAD_REG_TIMEOUT,
    THREAD_REG_DUPLICATE_THREAD,
    THREAD_REG_CREATION_FAILED,
    THREAD_REG_NOT_FOUND,
    THREAD_REG_INVALID_STATE_TRANSITION,
    THREAD_REG_REGISTRATION_FAILED,
    THREAD_REG_QUEUE_FULL,
    THREAD_REG_QUEUE_EMPTY,
    THREAD_REG_CLEANUP_ERROR
} ThreadRegistryError;

/**
 * @brief Get a static error message for the given domain and code
 * 
 * @param domain The error domain
 * @param code The error code within that domain
 * @return const char* Static error message
 */
const char* app_error_get_message(int domain, int code);

#endif // APP_ERROR_H
