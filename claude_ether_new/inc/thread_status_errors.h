#ifndef THREAD_STATUS_ERRORS_H
#define THREAD_STATUS_ERRORS_H

#include "error_types.h"

typedef enum ThreadStatus {
    THREAD_STATUS_SUCCESS = 0,           
    THREAD_STATUS_ERROR = -1,            
    THREAD_STATUS_INIT_FAILED = -2,      
    THREAD_STATUS_UNAUTHORIZED = -3,      
    THREAD_STATUS_SHUTDOWN = -4,         
    THREAD_STATUS_TIMEOUT = -5           
} ThreadStatus;

#ifdef DEFINE_ERROR_TABLES
const ErrorTableEntry thread_status_errors[] = {
    {THREAD_STATUS_SUCCESS,           "Thread completed successfully"},
    {THREAD_STATUS_ERROR,            "Thread encountered an error"},
    {THREAD_STATUS_INIT_FAILED,      "Thread initialization failed"},
    {THREAD_STATUS_UNAUTHORIZED,     "Thread operation not authorized"},
    {THREAD_STATUS_SHUTDOWN,         "Thread was shut down"},
    {THREAD_STATUS_TIMEOUT,          "Thread operation timed out"}
};
#endif

#endif
