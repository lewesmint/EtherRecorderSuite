#include "platform_file.h"
#include "platform_error.h"
#include <windows.h>

#define MAX_FILE_HANDLES 256

struct PlatformFile {
    HANDLE handle;
    bool is_valid;
};

static struct PlatformFile g_file_pool[MAX_FILE_HANDLES];
static bool g_file_pool_used[MAX_FILE_HANDLES];

static DWORD get_windows_access(PlatformFileAccess access) {
    switch (access) {
        case PLATFORM_FILE_READ:
            return GENERIC_READ;
        case PLATFORM_FILE_WRITE:
            return GENERIC_WRITE;
        case PLATFORM_FILE_READWRITE:
            return GENERIC_READ | GENERIC_WRITE;
        default:
            return 0;
    }
}

static DWORD get_windows_share(PlatformFileShare share) {
    DWORD result = 0;
    if (share & PLATFORM_FILE_SHARE_READ)   result |= FILE_SHARE_READ;
    if (share & PLATFORM_FILE_SHARE_WRITE)  result |= FILE_SHARE_WRITE;
    if (share & PLATFORM_FILE_SHARE_DELETE) result |= FILE_SHARE_DELETE;
    return result;
}

static int acquire_file_slot(void) {
    for (int i = 0; i < MAX_FILE_HANDLES; i++) {
        if (!g_file_pool_used[i]) {
            g_file_pool_used[i] = true;
            g_file_pool[i].is_valid = false;
            return i;
        }
    }
    return -1;
}

PlatformFileHandle platform_file_open(
    const char* filepath,
    PlatformFileAccess access,
    PlatformFileShare share,
    PlatformErrorCode* error_code
) {
    if (!filepath) {
        if (error_code) *error_code = PLATFORM_ERROR_INVALID_ARGUMENT;
        return NULL;
    }

    int slot = acquire_file_slot();
    if (slot == -1) {
        if (error_code) *error_code = PLATFORM_ERROR_OUT_OF_MEMORY;
        return NULL;
    }

    struct PlatformFile* file = &g_file_pool[slot];
    
    file->handle = CreateFileA(
        filepath,
        get_windows_access(access),
        get_windows_share(share),
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (file->handle == INVALID_HANDLE_VALUE) {
        g_file_pool_used[slot] = false;
        if (error_code) *error_code = PLATFORM_ERROR_FILE_OPEN;
        return NULL;
    }

    file->is_valid = true;
    if (error_code) *error_code = PLATFORM_ERROR_SUCCESS;
    return file;
}

PlatformErrorCode platform_file_read(
    PlatformFileHandle handle,
    void* buffer,
    size_t size,
    size_t* bytes_read
) {
    if (!handle || !handle->is_valid || !buffer || !bytes_read) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    DWORD read_bytes;
    if (!ReadFile(handle->handle, buffer, (DWORD)size, &read_bytes, NULL)) {
        return PLATFORM_ERROR_FILE_READ;
    }

    *bytes_read = read_bytes;
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_file_get_size(
    PlatformFileHandle handle,
    size_t* size
) {
    if (!handle || !handle->is_valid || !size) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(handle->handle, &file_size)) {
        return PLATFORM_ERROR_FILE_READ;
    }

    *size = (size_t)file_size.QuadPart;
    return PLATFORM_ERROR_SUCCESS;
}

void platform_file_close(PlatformFileHandle handle) {
    if (!handle || !handle->is_valid) {
        return;
    }

    // Calculate the index from the handle pointer
    ptrdiff_t index = handle - g_file_pool;
    
    // Validate the handle is actually from our pool
    if (index >= 0 && index < MAX_FILE_HANDLES) {
        CloseHandle(handle->handle);
        handle->is_valid = false;
        g_file_pool_used[index] = false;
    }
}

#ifdef _DEBUG
// Debug helper to check for file handle leaks
size_t platform_file_get_open_count(void) {
    size_t count = 0;
    for (int i = 0; i < MAX_FILE_HANDLES; i++) {
        if (g_file_pool_used[i]) {
            count++;
        }
    }
    return count;
}
#endif
