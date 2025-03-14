#ifndef ERROR_TYPES_H
#define ERROR_TYPES_H

#include <stddef.h>

/**
 * @brief Defines all error domains in the system
 */
enum ErrorDomain {
    THREAD_REGISTRY_DOMAIN,    ///< Errors from thread registry operations
    THREAD_STATUS_DOMAIN,      ///< Thread status-related errors
    THREAD_RESULT_DOMAIN,      ///< Thread execution result errors
    ERROR_DOMAIN_MAX          ///< Maximum number of error domains (must be last)
};

/**
 * @brief Structure to hold an error code and its corresponding message
 */
typedef struct {
    int code;                  ///< Error code value
    const char* message;       ///< Human-readable error message
} ErrorTableEntry;

#endif // ERROR_TYPES_H
