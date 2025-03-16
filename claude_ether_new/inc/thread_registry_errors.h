#ifndef THREAD_REGISTRY_ERRORS_H
#define THREAD_REGISTRY_ERRORS_H

#include "error_types.h"

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
    THREAD_REG_CLEANUP_ERROR,
    THREAD_REG_UNAUTHORIZED,    
    THREAD_REG_ALLOCATION_FAILED,
    THREAD_REG_QUEUE_ERROR,
    THREAD_REG_STATUS_CHECK_FAILED  // Renamed from THREAD_REG_PLATFORM_ERROR
} ThreadRegistryError;

#ifdef DEFINE_ERROR_TABLES
const ErrorTableEntry thread_registry_errors[] = {
    {THREAD_REG_SUCCESS,                  "Success"},
    {THREAD_REG_NOT_INITIALIZED,          "Thread registry not initialized"},
    {THREAD_REG_INVALID_ARGS,             "Invalid arguments provided"},
    {THREAD_REG_LOCK_ERROR,               "Failed to acquire registry lock"},
    {THREAD_REG_WAIT_ERROR,               "Wait operation failed"},
    {THREAD_REG_TIMEOUT,                  "Operation timed out"},
    {THREAD_REG_DUPLICATE_THREAD,         "Thread already registered"},
    {THREAD_REG_CREATION_FAILED,          "Thread creation failed"},
    {THREAD_REG_NOT_FOUND,                "Thread not found in registry"},
    {THREAD_REG_INVALID_STATE_TRANSITION, "Invalid thread state transition"},
    {THREAD_REG_REGISTRATION_FAILED,      "Thread registration failed"},
    {THREAD_REG_QUEUE_FULL,               "Message queue is full"},
    {THREAD_REG_QUEUE_EMPTY,              "Message queue is empty"},
    {THREAD_REG_CLEANUP_ERROR,            "Thread cleanup failed"},
    {THREAD_REG_UNAUTHORIZED,             "Unauthorized queue access"},
    {THREAD_REG_ALLOCATION_FAILED,        "Memory allocation failed"},
    {THREAD_REG_QUEUE_ERROR,              "Message queue operation failed"},
    {THREAD_REG_STATUS_CHECK_FAILED,      "Failed to check thread status"}
};
#endif

#endif
