/**
 * @file win_shared_memory.c
 * @brief Windows-specific shared memory implementation
 */
#include "win_shared_memory.h"

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Internal structure for Windows shared memory
 */
struct WinSharedMemory {
    HANDLE file_mapping;     ///< Windows file mapping handle
    HANDLE mutex;            ///< Named mutex for synchronization
    void* mapped_view;       ///< Pointer to mapped memory
    char* name;              ///< Name of the shared memory
    char* mutex_name;        ///< Name of the mutex
    size_t size;             ///< Size of the shared memory
    WinSharedMemoryAccess access; ///< Access mode
    bool is_mapped;          ///< Whether the memory is currently mapped
    bool is_valid;           ///< Whether the handle is valid
};

/**
 * @brief Convert WinSharedMemoryAccess to Windows page protection flags
 */
static DWORD get_page_protection(WinSharedMemoryAccess access) {
    switch (access) {
        case WIN_SHM_READ:
            return PAGE_READONLY;
        case WIN_SHM_WRITE:
        case WIN_SHM_READWRITE:
            return PAGE_READWRITE;
        default:
            return PAGE_READONLY;
    }
}

/**
 * @brief Convert WinSharedMemoryAccess to Windows file mapping access flags
 */
static DWORD get_file_map_access(WinSharedMemoryAccess access) {
    switch (access) {
        case WIN_SHM_READ:
            return FILE_MAP_READ;
        case WIN_SHM_WRITE:
            return FILE_MAP_WRITE;
        case WIN_SHM_READWRITE:
            return FILE_MAP_ALL_ACCESS;
        default:
            return FILE_MAP_READ;
    }
}

/**
 * @brief Create a mutex name from a shared memory name
 */
static char* create_mutex_name(const char* name) {
    // Allocate memory for "Mutex_" prefix + name
    size_t len = strlen(name) + 7; // 6 for "Mutex_" + 1 for null terminator
    char* mutex_name = (char*)malloc(len);
    if (mutex_name) {
        sprintf(mutex_name, "Mutex_%s", name);
    }
    return mutex_name;
}

PlatformErrorCode win_shared_memory_open(
    WinSharedMemoryHandle* handle,
    const char* name,
    size_t size,
    WinSharedMemoryAccess access,
    bool create_new
) {
    if (!handle || !name) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    // Allocate memory for the handle
    struct WinSharedMemory* shm = (struct WinSharedMemory*)calloc(1, sizeof(struct WinSharedMemory));
    if (!shm) {
        return PLATFORM_ERROR_OUT_OF_MEMORY;
    }

    // Initialize the handle
    shm->name = _strdup(name);
    shm->mutex_name = create_mutex_name(name);
    shm->access = access;
    shm->size = size;
    shm->is_mapped = false;
    shm->is_valid = false;
    shm->mapped_view = NULL;
    shm->file_mapping = NULL;
    shm->mutex = NULL;

    if (!shm->name || !shm->mutex_name) {
        win_shared_memory_close((WinSharedMemoryHandle)shm);
        return PLATFORM_ERROR_OUT_OF_MEMORY;
    }

    // Create or open the mutex
    shm->mutex = CreateMutexA(NULL, FALSE, shm->mutex_name);
    if (!shm->mutex) {
        win_shared_memory_close((WinSharedMemoryHandle)shm);
        return PLATFORM_ERROR_SYSTEM;
    }

    // Create or open the file mapping
    if (create_new) {
        shm->file_mapping = CreateFileMappingA(
            INVALID_HANDLE_VALUE,    // Use paging file
            NULL,                    // Default security attributes
            get_page_protection(access),
            0,                       // Maximum size high DWORD
            (DWORD)size,             // Maximum size low DWORD
            name                     // Name of the mapping
        );
    } else {
        // Open existing file mapping
        shm->file_mapping = OpenFileMappingA(
            get_file_map_access(access),
            FALSE,                   // Do not inherit handle
            name                     // Name of the mapping
        );
    }

    if (!shm->file_mapping) {
        DWORD error = GetLastError();
        win_shared_memory_close((WinSharedMemoryHandle)shm);
        
        if (error == ERROR_FILE_NOT_FOUND) {
            return PLATFORM_ERROR_NOT_FOUND;
        }
        return PLATFORM_ERROR_SYSTEM;
    }

    // If we're opening an existing mapping, get its size
    if (!create_new) {
        // Map a view just to get the size
        void* temp_view = MapViewOfFile(
            shm->file_mapping,
            FILE_MAP_READ,
            0, 0, 0  // Map the entire file
        );
        
        if (!temp_view) {
            win_shared_memory_close((WinSharedMemoryHandle)shm);
            return PLATFORM_ERROR_SYSTEM;
        }
        
        MEMORY_BASIC_INFORMATION info;
        if (VirtualQuery(temp_view, &info, sizeof(info)) == 0) {
            UnmapViewOfFile(temp_view);
            win_shared_memory_close((WinSharedMemoryHandle)shm);
            return PLATFORM_ERROR_SYSTEM;
        }
        
        shm->size = info.RegionSize;
        UnmapViewOfFile(temp_view);
    }

    shm->is_valid = true;
    *handle = shm;
    
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode win_shared_memory_map(
    WinSharedMemoryHandle handle,
    void** data
) {
    if (!handle || !handle->is_valid || !data) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    if (handle->is_mapped) {
        *data = handle->mapped_view;
        return PLATFORM_ERROR_SUCCESS;
    }

    // Map the view of the file
    handle->mapped_view = MapViewOfFile(
        handle->file_mapping,
        get_file_map_access(handle->access),
        0, 0, 0  // Map the entire file
    );

    if (!handle->mapped_view) {
        return PLATFORM_ERROR_SYSTEM;
    }

    handle->is_mapped = true;
    *data = handle->mapped_view;
    
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode win_shared_memory_unmap(
    WinSharedMemoryHandle handle
) {
    if (!handle || !handle->is_valid) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    if (!handle->is_mapped || !handle->mapped_view) {
        return PLATFORM_ERROR_SUCCESS;  // Already unmapped
    }

    if (!UnmapViewOfFile(handle->mapped_view)) {
        return PLATFORM_ERROR_SYSTEM;
    }

    handle->mapped_view = NULL;
    handle->is_mapped = false;
    
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode win_shared_memory_close(
    WinSharedMemoryHandle handle
) {
    if (!handle) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    // Unmap the view if it's mapped
    if (handle->is_mapped && handle->mapped_view) {
        UnmapViewOfFile(handle->mapped_view);
        handle->mapped_view = NULL;
        handle->is_mapped = false;
    }

    // Close the file mapping handle
    if (handle->file_mapping) {
        CloseHandle(handle->file_mapping);
        handle->file_mapping = NULL;
    }

    // Close the mutex handle
    if (handle->mutex) {
        CloseHandle(handle->mutex);
        handle->mutex = NULL;
    }

    // Free allocated strings
    if (handle->name) {
        free(handle->name);
        handle->name = NULL;
    }

    if (handle->mutex_name) {
        free(handle->mutex_name);
        handle->mutex_name = NULL;
    }

    // Free the handle itself
    free(handle);
    
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode win_shared_memory_get_size(
    WinSharedMemoryHandle handle,
    size_t* size
) {
    if (!handle || !handle->is_valid || !size) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    *size = handle->size;
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode win_shared_memory_get_data(
    WinSharedMemoryHandle handle,
    void** data
) {
    if (!handle || !handle->is_valid || !data) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    if (!handle->is_mapped || !handle->mapped_view) {
        return PLATFORM_ERROR_NOT_INITIALIZED;
    }

    *data = handle->mapped_view;
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode win_shared_memory_lock(
    WinSharedMemoryHandle handle,
    uint32_t timeout_ms
) {
    if (!handle || !handle->is_valid) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    if (!handle->mutex) {
        return PLATFORM_ERROR_NOT_INITIALIZED;
    }

    DWORD result = WaitForSingleObject(handle->mutex, timeout_ms);
    
    switch (result) {
        case WAIT_OBJECT_0:
            return PLATFORM_ERROR_SUCCESS;
        case WAIT_TIMEOUT:
            return PLATFORM_ERROR_TIMEOUT;
        case WAIT_ABANDONED:
            // The mutex was abandoned (owning thread terminated without releasing it)
            // We now own the mutex, but should be cautious as the state may be inconsistent
            return PLATFORM_ERROR_SUCCESS;
        default:
            return PLATFORM_ERROR_SYSTEM;
    }
}

PlatformErrorCode win_shared_memory_unlock(
    WinSharedMemoryHandle handle
) {
    if (!handle || !handle->is_valid) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    if (!handle->mutex) {
        return PLATFORM_ERROR_NOT_INITIALIZED;
    }

    if (!ReleaseMutex(handle->mutex)) {
        return PLATFORM_ERROR_SYSTEM;
    }

    return PLATFORM_ERROR_SUCCESS;
}
