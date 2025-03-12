#include "app_error.h"

#include <stddef.h>

#define REG_ERROR(domain, code, message) { domain##_DOMAIN, code, message }

static const struct {
    int domain;
    int code;
    const char* message;
} error_table[] = {
    REG_ERROR(THREAD_REGISTRY, THREAD_REG_SUCCESS,                   "Success"),
    REG_ERROR(THREAD_REGISTRY, THREAD_REG_NOT_INITIALIZED,          "Thread registry not initialized"),
    REG_ERROR(THREAD_REGISTRY, THREAD_REG_INVALID_ARGS,             "Invalid arguments provided"),
    REG_ERROR(THREAD_REGISTRY, THREAD_REG_LOCK_ERROR,               "Failed to acquire registry lock"),
    REG_ERROR(THREAD_REGISTRY, THREAD_REG_DUPLICATE_THREAD,         "Thread already registered"),
    REG_ERROR(THREAD_REGISTRY, THREAD_REG_CREATION_FAILED,          "Failed to create thread entry"),
    REG_ERROR(THREAD_REGISTRY, THREAD_REG_NOT_FOUND,                "Thread not found in registry"),
    REG_ERROR(THREAD_REGISTRY, THREAD_REG_INVALID_STATE_TRANSITION, "Invalid thread state transition"),
    REG_ERROR(THREAD_REGISTRY, THREAD_REG_WAIT_ERROR,               "Error waiting for thread"),
    REG_ERROR(THREAD_REGISTRY, THREAD_REG_TIMEOUT,                  "Timeout waiting for thread"),
    REG_ERROR(THREAD_REGISTRY, THREAD_REG_REGISTRATION_FAILED,      "Thread registration failed"),
    REG_ERROR(THREAD_REGISTRY, THREAD_REG_QUEUE_FULL,               "Message queue is full"),
    REG_ERROR(THREAD_REGISTRY, THREAD_REG_QUEUE_EMPTY,              "Message queue is empty"),
    REG_ERROR(THREAD_REGISTRY, THREAD_REG_CLEANUP_ERROR,            "Thread cleanup failed")
};

const char* app_error_get_message(int domain, int code) {
    for (size_t i = 0; i < sizeof(error_table)/sizeof(error_table[0]); i++) {
        if (error_table[i].domain == domain && error_table[i].code == code) {
            return error_table[i].message;
        }
    }
    return "Unknown error";
}
