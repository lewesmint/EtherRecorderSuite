#ifndef THREAD_RESULT_ERRORS_H
#define THREAD_RESULT_ERRORS_H

#include "error_types.h"

typedef enum ThreadResult {
    THREAD_SUCCESS = 0,
    THREAD_ERROR = -1,              
    THREAD_ERROR_INIT_FAILED = -2,
    THREAD_ERROR_LOGGER_TIMEOUT = -3,
    THREAD_ERROR_MUTEX_ERROR = -4,
    THREAD_ERROR_CONFIG_ERROR = -5,
    THREAD_ERROR_QUEUE_ERROR = -6   
} ThreadResult;

#ifdef DEFINE_ERROR_TABLES
const ErrorTableEntry thread_result_errors[] = {
    {THREAD_SUCCESS,                 "Thread operation successful"},
    {THREAD_ERROR,                   "Thread operation failed"},
    {THREAD_ERROR_INIT_FAILED,       "Thread initialization failed"},
    {THREAD_ERROR_LOGGER_TIMEOUT,    "Logger communication timeout"},
    {THREAD_ERROR_MUTEX_ERROR,       "Thread mutex operation failed"},
    {THREAD_ERROR_CONFIG_ERROR,      "Thread configuration error"},
    {THREAD_ERROR_QUEUE_ERROR,       "Thread queue operation failed"}
};
#endif

#endif
