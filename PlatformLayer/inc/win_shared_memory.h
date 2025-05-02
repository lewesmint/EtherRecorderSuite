/**
 * @file win_shared_memory.h
 * @brief Windows-specific shared memory implementation
 */
#ifndef WIN_SHARED_MEMORY_H
#define WIN_SHARED_MEMORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "platform_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque shared memory handle
 */
typedef struct WinSharedMemory* WinSharedMemoryHandle;

/**
 * @brief Shared memory access modes
 */
typedef enum {
    WIN_SHM_READ = 1,       ///< Read-only access
    WIN_SHM_WRITE = 2,      ///< Write-only access
    WIN_SHM_READWRITE = 3   ///< Read-write access
} WinSharedMemoryAccess;

/**
 * @brief Create or open a shared memory segment
 * 
 * @param handle Pointer to store the shared memory handle
 * @param name Name of the shared memory segment
 * @param size Size of the shared memory segment in bytes (ignored if create_new is false)
 * @param access Access mode (read, write, or read-write)
 * @param create_new If true, create a new segment; if false, open existing
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode win_shared_memory_open(
    WinSharedMemoryHandle* handle,
    const char* name,
    size_t size,
    WinSharedMemoryAccess access,
    bool create_new
);

/**
 * @brief Map shared memory into the process address space
 * 
 * @param handle Shared memory handle
 * @param data Pointer to store the mapped memory address
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode win_shared_memory_map(
    WinSharedMemoryHandle handle,
    void** data
);

/**
 * @brief Unmap shared memory from the process address space
 * 
 * @param handle Shared memory handle
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode win_shared_memory_unmap(
    WinSharedMemoryHandle handle
);

/**
 * @brief Close a shared memory handle
 * 
 * @param handle Shared memory handle
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode win_shared_memory_close(
    WinSharedMemoryHandle handle
);

/**
 * @brief Get the size of a shared memory segment
 * 
 * @param handle Shared memory handle
 * @param size Pointer to store the size
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode win_shared_memory_get_size(
    WinSharedMemoryHandle handle,
    size_t* size
);

/**
 * @brief Get the data pointer of a mapped shared memory segment
 * 
 * @param handle Shared memory handle
 * @param data Pointer to store the data pointer
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode win_shared_memory_get_data(
    WinSharedMemoryHandle handle,
    void** data
);

/**
 * @brief Lock the shared memory for exclusive access
 * 
 * @param handle Shared memory handle
 * @param timeout_ms Timeout in milliseconds (INFINITE for no timeout)
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode win_shared_memory_lock(
    WinSharedMemoryHandle handle,
    uint32_t timeout_ms
);

/**
 * @brief Unlock the shared memory
 * 
 * @param handle Shared memory handle
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode win_shared_memory_unlock(
    WinSharedMemoryHandle handle
);

#ifdef __cplusplus
}
#endif

#endif // WIN_SHARED_MEMORY_H
