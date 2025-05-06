/**
 * @file win_shared_memory.c
 * @brief Windows implementation of platform shared memory operations
 */
#include "platform_shared_memory.h"
#include "platform_error.h"

#include <windows.h>
#include <winternl.h>  // For NTSTATUS and other NT API definitions
#include <stdlib.h>
#include <string.h>
#include <stdio.h>  // Add this for printf

struct PlatformSharedMemory {
    HANDLE mapping_handle;
    void* mapped_address;
    size_t size;
    char name[MAX_PATH];
    PlatformSharedMemoryAccess access;
    bool is_owner;
};

PlatformErrorCode platform_shared_memory_open(
    PlatformSharedMemoryHandle* handle,
    const char* name,
    size_t size,
    PlatformSharedMemoryAccess access,
    bool create_new
) {
    printf("platform_shared_memory_open: name=%s, size=%zu, access=%d, create=%d\n", 
           name, size, access, create_new);
    
    if (!handle || !name) {
        printf("Invalid arguments: handle=%p, name=%p\n", (void*)handle, (void*)name);
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    // Allocate the handle structure
    struct PlatformSharedMemory* shm = (struct PlatformSharedMemory*)malloc(sizeof(struct PlatformSharedMemory));
    if (!shm) {
        printf("Failed to allocate memory for shared memory handle\n");
        return PLATFORM_ERROR_MEMORY_ALLOC;
    }

    // Initialize the structure
    memset(shm, 0, sizeof(struct PlatformSharedMemory));
    strncpy(shm->name, name, MAX_PATH - 1);
    shm->size = size;
    shm->access = access;
    shm->is_owner = create_new;

    // Determine access rights
    DWORD desired_access = 0;
    if (access & PLATFORM_SHM_READ) {
        desired_access |= FILE_MAP_READ;
    }
    if (access & PLATFORM_SHM_WRITE) {
        desired_access |= FILE_MAP_WRITE;
    }

    // Determine creation flags
    DWORD protection = PAGE_READONLY;
    if (access & PLATFORM_SHM_WRITE) {
        protection = PAGE_READWRITE;
    }

    printf("Windows shared memory parameters: protection=%u, desired_access=%u\n", 
           protection, desired_access);

    if (create_new) {
        // Create a new shared memory segment
        if (size == 0) {
            printf("Cannot create shared memory with zero size\n");
            free(shm);
            return PLATFORM_ERROR_INVALID_ARGUMENT;
        }

        printf("Creating new shared memory: %s\n", name);
        shm->mapping_handle = CreateFileMappingA(
            INVALID_HANDLE_VALUE,  // Use paging file
            NULL,                  // Default security attributes
            protection,            // Read/write access
            (DWORD)((size >> 32) & 0xFFFFFFFF),  // High-order DWORD of size
            (DWORD)(size & 0xFFFFFFFF),          // Low-order DWORD of size
            name                   // Name of the mapping object
        );
    } else {
        // Open an existing shared memory segment
        printf("Opening existing shared memory: %s\n", name);
        shm->mapping_handle = OpenFileMappingA(
            desired_access,        // Read/write access
            FALSE,                 // Do not inherit the name
            name                   // Name of the mapping object
        );
    }

    if (shm->mapping_handle == NULL) {
        DWORD error = GetLastError();
        printf("Failed to %s shared memory '%s': Windows error %u\n", 
               create_new ? "create" : "open", name, error);
        
        char error_msg[256];
        FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            error_msg,
            sizeof(error_msg),
            NULL
        );
        printf("Windows error message: %s\n", error_msg);
        
        free(shm);
        
        if (error == ERROR_FILE_NOT_FOUND) {
            return PLATFORM_ERROR_FILE_NOT_FOUND;
        }
        return PLATFORM_ERROR_SYSTEM;
    }

    printf("Successfully %s shared memory '%s'\n", 
           create_new ? "created" : "opened", name);
    
    *handle = shm;
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_shared_memory_map(
    PlatformSharedMemoryHandle handle,
    void** data
) {
    if (!handle || !data) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    // Determine access rights
    DWORD desired_access = 0;
    if (handle->access & PLATFORM_SHM_READ) {
        desired_access |= FILE_MAP_READ;
    }
    if (handle->access & PLATFORM_SHM_WRITE) {
        desired_access |= FILE_MAP_WRITE;
    }

    // Map the shared memory
    handle->mapped_address = MapViewOfFile(
        handle->mapping_handle,    // Handle to the mapping object
        desired_access,            // Read/write access
        0,                         // High-order DWORD of offset
        0,                         // Low-order DWORD of offset
        handle->size               // Number of bytes to map (0 = all)
    );

    if (handle->mapped_address == NULL) {
        return PLATFORM_ERROR_SYSTEM;
    }

    *data = handle->mapped_address;
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_shared_memory_unmap(
    PlatformSharedMemoryHandle handle
) {
    if (!handle || !handle->mapped_address) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    if (!UnmapViewOfFile(handle->mapped_address)) {
        return PLATFORM_ERROR_SYSTEM;
    }

    handle->mapped_address = NULL;
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_shared_memory_close(
    PlatformSharedMemoryHandle handle
) {
    if (!handle) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    // Unmap if still mapped
    if (handle->mapped_address) {
        platform_shared_memory_unmap(handle);
    }

    // Close the handle
    if (handle->mapping_handle) {
        CloseHandle(handle->mapping_handle);
    }

    // Free the structure
    free(handle);
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_shared_memory_get_size(
    PlatformSharedMemoryHandle handle,
    size_t* size
) {
    if (!handle || !size) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    // If we already know the size, return it
    if (handle->size > 0) {
        *size = handle->size;
        return PLATFORM_ERROR_SUCCESS;
    }

    // Try to get the size using NtQuerySection if available
    HMODULE ntdll = LoadLibraryA("ntdll.dll");
    if (ntdll) {
        typedef NTSTATUS (NTAPI *NtQuerySectionFn)(
            HANDLE SectionHandle,
            ULONG SectionInformationClass,
            PVOID SectionInformation,
            SIZE_T SectionInformationLength,
            PSIZE_T ReturnLength
        );

        NtQuerySectionFn NtQuerySection = (NtQuerySectionFn)GetProcAddress(ntdll, "NtQuerySection");
        if (NtQuerySection) {
            // SECTION_BASIC_INFORMATION is 0
            struct {
                PVOID BaseAddress;
                ULONG SectionAttributes;
                LARGE_INTEGER SectionSize;
            } sectionInfo;
            
            NTSTATUS status = NtQuerySection(
                handle->mapping_handle,
                0, // SECTION_BASIC_INFORMATION
                &sectionInfo,
                sizeof(sectionInfo),
                NULL
            );
            
            FreeLibrary(ntdll);
            
            if (status == 0) { // STATUS_SUCCESS
                *size = (size_t)sectionInfo.SectionSize.QuadPart;
                handle->size = *size; // Cache the size
                return PLATFORM_ERROR_SUCCESS;
            }
        } else {
            FreeLibrary(ntdll);
        }
    }

    // Fallback: Map the memory and use VirtualQuery to get the size
    void* mapped_address = MapViewOfFile(
        handle->mapping_handle,
        FILE_MAP_READ,
        0, 0, 0  // Map the entire file
    );
    
    if (!mapped_address) {
        DWORD error = GetLastError();
        printf("Failed to map view of file for size detection: %u\n", error);
        return PLATFORM_ERROR_SYSTEM;
    }
    
    MEMORY_BASIC_INFORMATION memInfo;
    if (VirtualQuery(mapped_address, &memInfo, sizeof(memInfo))) {
        *size = memInfo.RegionSize;
        handle->size = *size; // Cache the size
        UnmapViewOfFile(mapped_address);
        return PLATFORM_ERROR_SUCCESS;
    }
    
    UnmapViewOfFile(mapped_address);
    return PLATFORM_ERROR_SYSTEM;
}

PlatformErrorCode platform_shared_memory_get_data(
    PlatformSharedMemoryHandle handle,
    void** data
) {
    if (!handle || !data) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    if (!handle->mapped_address) {
        return PLATFORM_ERROR_NOT_INITIALIZED;
    }

    *data = handle->mapped_address;
    return PLATFORM_ERROR_SUCCESS;
}
