#include "platform_file.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <errno.h>

struct PlatformFile {
    int fd;
    bool is_valid;
};

static int get_posix_flags(PlatformFileAccess access, PlatformFileShare share) {
    int flags = 0;
    
    // Access mode
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

    // If no sharing is requested, we'll use flock to enforce exclusive access
    if (share == PLATFORM_FILE_SHARE_NONE) {
        flags |= O_NONBLOCK;  // For flock behavior
    }

    return flags;
}

PlatformFileHandle platform_file_open(
    const char* filepath,
    PlatformFileAccess access,
    PlatformFileShare share,
    PlatformErrorCode* error_code
) {
    PlatformFileHandle file = malloc(sizeof(struct PlatformFile));
    if (!file) {
        if (error_code) *error_code = PLATFORM_ERROR_OUT_OF_MEMORY;
        return NULL;
    }

    int flags = get_posix_flags(access, share);
    file->fd = open(filepath, flags);
    
    if (file->fd == -1) {
        free(file);
        if (error_code) *error_code = PLATFORM_ERROR_FILE_OPEN;
        return NULL;
    }

    // If exclusive access requested, try to acquire lock
    if (share == PLATFORM_FILE_SHARE_NONE) {
        if (flock(file->fd, LOCK_EX | LOCK_NB) == -1) {
            close(file->fd);
            free(file);
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
    if (handle && handle->is_valid) {
        // Release any exclusive lock if it exists
        flock(handle->fd, LOCK_UN);
        close(handle->fd);
        handle->is_valid = false;
        free(handle);
    }
}