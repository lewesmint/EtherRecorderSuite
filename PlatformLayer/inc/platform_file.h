/**
 * @file platform_file.h
 * @brief Platform-agnostic file operations with consistent locking behavior
 */
#ifndef PLATFORM_FILE_H
#define PLATFORM_FILE_H

#include <stddef.h>
#include <stdbool.h>
#include "platform_error.h"

// Opaque file handle
typedef struct PlatformFile* PlatformFileHandle;

// File sharing modes
typedef enum {
    PLATFORM_FILE_SHARE_NONE = 0,      // Exclusive access
    PLATFORM_FILE_SHARE_READ = 1,      // Allow others to read
    PLATFORM_FILE_SHARE_WRITE = 2,     // Allow others to write
    PLATFORM_FILE_SHARE_DELETE = 4     // Allow others to delete/rename
} PlatformFileShare;

// File access modes
typedef enum {
    PLATFORM_FILE_READ = 1,
    PLATFORM_FILE_WRITE = 2,
    PLATFORM_FILE_READWRITE = 3
} PlatformFileAccess;

/**
 * @brief Opens a file with specified sharing and access modes
 * 
 * @param filepath Path to the file
 * @param access Access mode (read/write)
 * @param share Sharing mode flags (can be combined with |)
 * @param error_code Optional pointer to receive error code
 * @return PlatformFileHandle NULL if failed
 */
PlatformFileHandle platform_file_open(
    const char* filepath,
    PlatformFileAccess access,
    PlatformFileShare share,
    PlatformErrorCode* error_code
);

/**
 * @brief Reads from file at current position
 * 
 * @param handle File handle
 * @param buffer Buffer to read into
 * @param size Number of bytes to read
 * @param bytes_read Actual number of bytes read
 * @return PlatformErrorCode
 */
PlatformErrorCode platform_file_read(
    PlatformFileHandle handle,
    void* buffer,
    size_t size,
    size_t* bytes_read
);

/**
 * @brief Gets the size of the file
 * 
 * @param handle File handle
 * @param size Pointer to receive file size
 * @return PlatformErrorCode
 */
PlatformErrorCode platform_file_get_size(
    PlatformFileHandle handle,
    size_t* size
);

/**
 * @brief Closes the file handle
 * 
 * @param handle File handle
 */
void platform_file_close(PlatformFileHandle handle);

#endif // PLATFORM_FILE_H