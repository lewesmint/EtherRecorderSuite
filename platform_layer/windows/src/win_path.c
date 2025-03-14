/**
 * @file win_path.c
 * @brief Windows implementation of platform path operations
 */
#include "platform_path.h"
#include "platform_error.h"

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <shlwapi.h>
#include <direct.h>

// Link against shlwapi.lib for path functions
#pragma comment(lib, "shlwapi.lib")

// Platform-specific constants
const char PATH_SEPARATOR = '\\';

PlatformErrorCode platform_get_current_dir(char* buffer, size_t size) {
    if (!buffer || size == 0) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    if (_getcwd(buffer, (int)size) == NULL) {
        switch (errno) {
            case ERANGE:
                return PLATFORM_ERROR_BUFFER_TOO_SMALL;
            default:
                return PLATFORM_ERROR_UNKNOWN;
        }
    }

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_set_current_dir(const char* path) {
    if (!path) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    if (_chdir(path) != 0) {
        switch (errno) {
            case ENOENT:
                return PLATFORM_ERROR_DIRECTORY_NOT_FOUND;
            case EACCES:
                return PLATFORM_ERROR_DIRECTORY_ACCESS;
            default:
                return PLATFORM_ERROR_UNKNOWN;
        }
    }

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_path_to_absolute(const char* path, char* abs_path, size_t size) {
    if (!path || !abs_path || size == 0) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    DWORD result = GetFullPathNameA(path, (DWORD)size, abs_path, NULL);
    if (result == 0) {
        switch (GetLastError()) {
            case ERROR_FILE_NOT_FOUND:
                return PLATFORM_ERROR_FILE_NOT_FOUND;
            case ERROR_ACCESS_DENIED:
                return PLATFORM_ERROR_FILE_ACCESS;
            case ERROR_INSUFFICIENT_BUFFER:
                return PLATFORM_ERROR_BUFFER_TOO_SMALL;
            default:
                return PLATFORM_ERROR_UNKNOWN;
        }
    }
    if (result >= size) {
        return PLATFORM_ERROR_BUFFER_TOO_SMALL;
    }

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_path_join(const char* base, const char* part, char* result, size_t size) {
    if (!base || !part || !result || size == 0) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    size_t base_len = strlen(base);
    size_t part_len = strlen(part);
    
    // Calculate required size including potential separator and null terminator
    size_t required_size = base_len + part_len + 2;
    if (required_size > size) {
        return PLATFORM_ERROR_BUFFER_TOO_SMALL;
    }

    strcpy(result, base);
    
    // Add separator if base doesn't end with one and part doesn't start with one
    if (base_len > 0 && base[base_len - 1] != PATH_SEPARATOR && 
        base[base_len - 1] != '/' &&  // Also check for forward slash
        part_len > 0 && part[0] != PATH_SEPARATOR && 
        part[0] != '/') {  // Also check for forward slash
        result[base_len] = PATH_SEPARATOR;
        strcpy(result + base_len + 1, part);
    } else {
        strcpy(result + base_len, part);
    }

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_path_directory(const char* path, char* dir, size_t size) {
    if (!path || !dir || size == 0) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    char* path_copy = _strdup(path);
    if (!path_copy) {
        return PLATFORM_ERROR_MEMORY_ALLOC;
    }

    char drive[_MAX_DRIVE];
    char dir_part[_MAX_DIR];
    _splitpath(path_copy, drive, dir_part, NULL, NULL);

    size_t required_size = strlen(drive) + strlen(dir_part) + 1;
    if (required_size > size) {
        free(path_copy);
        return PLATFORM_ERROR_BUFFER_TOO_SMALL;
    }

    _makepath(dir, drive, dir_part, NULL, NULL);
    free(path_copy);

    // Remove trailing backslash if present (except for root directories)
    size_t len = strlen(dir);
    if (len > 3 && dir[len - 1] == PATH_SEPARATOR) {
        dir[len - 1] = '\0';
    }

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_path_filename(const char* path, char* filename, size_t size) {
    if (!path || !filename || size == 0) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    char* path_copy = _strdup(path);
    if (!path_copy) {
        return PLATFORM_ERROR_MEMORY_ALLOC;
    }

    char fname[_MAX_FNAME];
    char ext[_MAX_EXT];
    _splitpath(path_copy, NULL, NULL, fname, ext);

    size_t required_size = strlen(fname) + strlen(ext) + 1;
    if (required_size > size) {
        free(path_copy);
        return PLATFORM_ERROR_BUFFER_TOO_SMALL;
    }

    _makepath(filename, NULL, NULL, fname, ext);
    free(path_copy);
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_path_extension(const char* path, char* extension, size_t size) {
    if (!path || !extension || size == 0) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    const char* dot = strrchr(path, '.');
    if (!dot || dot == path) {
        extension[0] = '\0';
        return PLATFORM_ERROR_SUCCESS;
    }

    if (strlen(dot) >= size) {
        return PLATFORM_ERROR_BUFFER_TOO_SMALL;
    }

    strcpy(extension, dot);
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_path_normalize(char* path) {
    if (!path) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    char full_path[MAX_PATH];
    DWORD result = GetFullPathNameA(path, MAX_PATH, full_path, NULL);
    if (result == 0 || result >= MAX_PATH) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    // Convert forward slashes to backslashes
    char* p = full_path;
    while (*p) {
        if (*p == '/') {
            *p = PATH_SEPARATOR;
        }
        p++;
    }

    strcpy(path, full_path);
    return PLATFORM_ERROR_SUCCESS;
}

bool platform_path_is_absolute(const char* path) {
    if (!path) {
        return false;
    }
    
    // Check for drive letter (e.g., "C:\")
    if (isalpha(path[0]) && path[1] == ':' && 
        (path[2] == PATH_SEPARATOR || path[2] == '/')) {
        return true;
    }
    
    // Check for UNC path (e.g., "\\server\share")
    if ((path[0] == PATH_SEPARATOR || path[0] == '/') && 
        (path[1] == PATH_SEPARATOR || path[1] == '/')) {
        return true;
    }
    
    return false;
}

PlatformErrorCode platform_path_to_native(char* path) {
    if (!path) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    // Convert forward slashes to backslashes
    char* p = path;
    while (*p) {
        if (*p == '/') {
            *p = PATH_SEPARATOR;
        }
        p++;
    }

    return PLATFORM_ERROR_SUCCESS;
}

size_t platform_write(FILE* stream, const void* buffer, size_t size) {
    return fwrite(buffer, 1, size, stream);
}

int platform_mkdir(const char* path) {
    return _mkdir(path);
}

PlatformErrorCode platform_fopen(FILE** file, const char* filename, const char* mode) {
    if (!file || !filename || !mode) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    errno_t err = fopen_s(file, filename, mode);
    if (err != 0) {
        *file = NULL;
        return PLATFORM_ERROR_FILE_ACCESS;
    }
    
    return PLATFORM_ERROR_SUCCESS;
}
