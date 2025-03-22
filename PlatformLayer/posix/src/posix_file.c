#include "platform_file.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <errno.h>
#include <stdbool.h>

#define MAX_FILE_HANDLES 256

struct PlatformFile {
    int fd;
    bool is_valid;
};

static struct PlatformFile g_file_pool[MAX_FILE_HANDLES];
static bool g_file_pool_used[MAX_FILE_HANDLES];

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

static int get_posix_flags(PlatformFileAccess access, PlatformFileShare share) {
    int flags = 0;
    
    switch (access) {
        case PLATFORM_FILE_READ:
            flags |= O_RDONLY;
            break;
        case PLATFORM_FILE_WRITE:
            flags |= O_WRONLY;
            break;
        case PLATFORM_FILE_READWRITE:
            flags |= O_RDWR;
            break;
    }

    if (share == PLATFORM_FILE_SHARE_NONE) {
        flags |= O_NONBLOCK;
    }

    return flags;
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
    
    int flags = get_posix_flags(access, share);
    file->fd = open(filepath, flags);
    
    if (file->fd == -1) {
        g_file_pool_used[slot] = false;
        if (error_code) *error_code = PLATFORM_ERROR_FILE_OPEN;
        return NULL;
    }

    if (share == PLATFORM_FILE_SHARE_NONE) {
        if (flock(file->fd, LOCK_EX | LOCK_NB) == -1) {
            close(file->fd);
            g_file_pool_used[slot] = false;
            if (error_code) *error_code = PLATFORM_ERROR_FILE_LOCKED;
            return NULL;
        }
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

    ssize_t result = read(handle->fd, buffer, size);
    if (result == -1) {
        return PLATFORM_ERROR_FILE_READ;
    }

    *bytes_read = (size_t)result;
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_file_get_size(
    PlatformFileHandle handle,
    size_t* size
) {
    if (!handle || !handle->is_valid || !size) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    off_t current = lseek(handle->fd, 0, SEEK_CUR);
    if (current == -1) {
        return PLATFORM_ERROR_FILE_READ;
    }

    off_t end = lseek(handle->fd, 0, SEEK_END);
    if (end == -1) {
        return PLATFORM_ERROR_FILE_READ;
    }

    // Restore original position
    if (lseek(handle->fd, current, SEEK_SET) == -1) {
        return PLATFORM_ERROR_FILE_READ;
    }

    *size = (size_t)end;
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
        flock(handle->fd, LOCK_UN);
        close(handle->fd);
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
