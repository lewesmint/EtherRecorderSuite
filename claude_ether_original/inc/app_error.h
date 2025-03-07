/**
* @file app_error.h
* @brief Contains the error codes for the application.
*/

#ifndef APP_ERROR_H
#define APP_ERROR_H


#ifdef __cplusplus
extern "C" {
#endif

typedef enum AppError {
    APP_EXIT_SUCCESS = 0,
    APP_EXIT_FAILURE = 1,
    APP_CONFIG_ERROR = 2,
    APP_LOGGER_ERROR = 3
} AppError;

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // APP_ERROR_H
