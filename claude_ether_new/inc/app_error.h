#ifndef APP_ERROR_H
#define APP_ERROR_H

#include "error_types.h"

/**
 * @brief Get a static error message for the given domain and code
 * 
 * @param domain The error domain
 * @param code The error code within that domain
 * @return const char* Static error message
 */
const char* app_error_get_message(enum ErrorDomain domain, int code);

#endif
