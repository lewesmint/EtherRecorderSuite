/**
 * @file posix_shared_memory.c
 * @brief POSIX stub implementation of platform shared memory operations
 */
#include "platform_shared_memory.h"
#include "platform_error.h"

#include <stdlib.h>
#include <string.h>

struct PlatformSharedMemory {
    void* dummy;  // Placeholder to avoid empty struct
};

PlatformErrorCode platform_shared_memory_open(
    PlatformSharedMemoryHandle* handle,
    const char* name,
    size_t size,
    PlatformSharedMemoryAccess access,
    bool create_new
) {
    // Stub implementation - not supported
    return PLATFORM_ERROR_NOT_SUPPORTED;
}

PlatformErrorCode platform_shared_memory_map(
    PlatformSharedMemoryHandle handle,
    void** data
) {
    // Stub implementation - not supported
    return PLATFORM_ERROR_NOT_SUPPORTED;
}

PlatformErrorCode platform_shared_memory_unmap(
    PlatformSharedMemoryHandle handle
) {
    // Stub implementation - not supported
    return PLATFORM_ERROR_NOT_SUPPORTED;
}

PlatformErrorCode platform_shared_memory_close(
    PlatformSharedMemoryHandle handle
) {
    // Stub implementation - not supported
    return PLATFORM_ERROR_NOT_SUPPORTED;
}

PlatformErrorCode platform_shared_memory_get_size(
    PlatformSharedMemoryHandle handle,
    size_t* size
) {
    // Stub implementation - not supported
    return PLATFORM_ERROR_NOT_SUPPORTED;
}

PlatformErrorCode platform_shared_memory_get_data(
    PlatformSharedMemoryHandle handle,
    void** data
) {
    // Stub implementation - not supported
    return PLATFORM_ERROR_NOT_SUPPORTED;
}