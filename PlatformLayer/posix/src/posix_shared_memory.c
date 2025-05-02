#include "platform_shared_memory.h"
#include "platform_error.h"
#include <stdlib.h>
#include <string.h>

// POSIX implementation of platform shared memory functions
// This is a stub implementation for platforms where we don't have full support yet

PlatformErrorCode platform_shared_memory_open(
    PlatformSharedMemoryHandle* handle,
    const char* name,
    size_t size,
    PlatformSharedMemoryAccess access,
    bool create_new)
{
    // Stub implementation - not supported on this platform
    *handle = NULL;
    return PLATFORM_ERROR_NOT_IMPLEMENTED;
}

PlatformErrorCode platform_shared_memory_map(
    PlatformSharedMemoryHandle handle,
    void** data)
{
    // Stub implementation - not supported on this platform
    *data = NULL;
    return PLATFORM_ERROR_NOT_IMPLEMENTED;
}

PlatformErrorCode platform_shared_memory_unmap(
    PlatformSharedMemoryHandle handle)
{
    // Stub implementation - not supported on this platform
    return PLATFORM_ERROR_NOT_IMPLEMENTED;
}

PlatformErrorCode platform_shared_memory_close(
    PlatformSharedMemoryHandle handle)
{
    // Stub implementation - not supported on this platform
    return PLATFORM_ERROR_NOT_IMPLEMENTED;
}

PlatformErrorCode platform_shared_memory_get_size(
    PlatformSharedMemoryHandle handle,
    size_t* size)
{
    // Stub implementation - not supported on this platform
    *size = 0;
    return PLATFORM_ERROR_NOT_IMPLEMENTED;
}

PlatformErrorCode platform_shared_memory_get_data(
    PlatformSharedMemoryHandle handle,
    void** data)
{
    // Stub implementation - not supported on this platform
    *data = NULL;
    return PLATFORM_ERROR_NOT_IMPLEMENTED;
}

PlatformErrorCode platform_shared_memory_lock(
    PlatformSharedMemoryHandle handle,
    uint32_t timeout_ms)
{
    // Stub implementation - not supported on this platform
    return PLATFORM_ERROR_NOT_IMPLEMENTED;
}

PlatformErrorCode platform_shared_memory_unlock(
    PlatformSharedMemoryHandle handle)
{
    // Stub implementation - not supported on this platform
    return PLATFORM_ERROR_NOT_IMPLEMENTED;
}
