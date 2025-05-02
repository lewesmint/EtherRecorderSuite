/**
 * @file platform_shared_memory.h
 * @brief Platform-agnostic shared memory operations
 */
#ifndef PLATFORM_SHARED_MEMORY_H
#define PLATFORM_SHARED_MEMORY_H

#include <stddef.h>
#include <stdbool.h>
#include "platform_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque shared memory handle
 */
typedef struct PlatformSharedMemory* PlatformSharedMemoryHandle;

/**
 * @brief Shared memory access modes
 */
typedef enum {
    PLATFORM_SHM_READ = 1,       ///< Read-only access
    PLATFORM_SHM_WRITE = 2,      ///< Write-only access
    PLATFORM_SHM_READWRITE = 3   ///< Read-write access
} PlatformSharedMemoryAccess;

/**
 * @brief Create or open a shared memory segment
 * 
 * @param handle Pointer to store the shared memory handle
 * @param name Name of the shared memory segment
 * @param size Size of the shared memory segment in bytes
 * @param access Access mode (read, write, or read-write)
 * @param create_new If true, create a new segment; if false, open existing
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_shared_memory_open(
    PlatformSharedMemoryHandle* handle,
    const char* name,
    size_t size,
    PlatformSharedMemoryAccess access,
    bool create_new
);

/**
 * @brief Map shared memory into the process address space
 * 
 * @param handle Shared memory handle
 * @param data Pointer to store the mapped memory address
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_shared_memory_map(
    PlatformSharedMemoryHandle handle,
    void** data
);

/**
 * @brief Unmap shared memory from the process address space
 * 
 * @param handle Shared memory handle
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_shared_memory_unmap(
    PlatformSharedMemoryHandle handle
);

/**
 * @brief Close a shared memory handle
 * 
 * @param handle Shared memory handle
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_shared_memory_close(
    PlatformSharedMemoryHandle handle
);

/**
 * @brief Get the size of a shared memory segment
 * 
 * @param handle Shared memory handle
 * @param size Pointer to store the size
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_shared_memory_get_size(
    PlatformSharedMemoryHandle handle,
    size_t* size
);

/**
 * @brief Get the data pointer of a mapped shared memory segment
 * 
 * @param handle Shared memory handle
 * @param data Pointer to store the data pointer
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_shared_memory_get_data(
    PlatformSharedMemoryHandle handle,
    void** data
);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_SHARED_MEMORY_H
