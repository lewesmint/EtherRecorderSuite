/**
 * @file platform_path.c
 * @brief POSIX implementation of platform path operations
 */
#include "platform_path.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>
#include <stdbool.h>
#include <sys/stat.h>

// Platform-specific constants
const char PATH_SEPARATOR = '/';

PlatformErrorCode platform_get_current_dir(char* buffer, size_t size) {
    if (!buffer || size == 0) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    if (getcwd(buffer, size) == NULL) {
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

    if (chdir(path) != 0) {
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

    char* resolved = realpath(path, abs_path);
    if (!resolved) {
        switch (errno) {
            case ENOENT:
                return PLATFORM_ERROR_FILE_NOT_FOUND;
            case EACCES:
                return PLATFORM_ERROR_FILE_ACCESS;
            case ENAMETOOLONG:
                return PLATFORM_ERROR_BUFFER_TOO_SMALL;
            default:
                return PLATFORM_ERROR_UNKNOWN;
        }
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
        part_len > 0 && part[0] != PATH_SEPARATOR) {
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

    // Create a modifiable copy since dirname may modify the string
    char* path_copy = strdup(path);
    if (!path_copy) {
        return PLATFORM_ERROR_MEMORY_ALLOC;
    }

    char* dir_part = dirname(path_copy);
    if (strlen(dir_part) >= size) {
        free(path_copy);
        return PLATFORM_ERROR_BUFFER_TOO_SMALL;
    }

    strcpy(dir, dir_part);
    free(path_copy);
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_path_filename(const char* path, char* filename, size_t size) {
    if (!path || !filename || size == 0) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    // Create a modifiable copy since basename may modify the string
    char* path_copy = strdup(path);
    if (!path_copy) {
        return PLATFORM_ERROR_MEMORY_ALLOC;
    }

    char* base_part = basename(path_copy);
    if (strlen(base_part) >= size) {
        free(path_copy);
        return PLATFORM_ERROR_BUFFER_TOO_SMALL;
    }

    strcpy(filename, base_part);
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

    char* src = path;
    char* dst = path;
    char* last_slash = NULL;
    bool skip_slash = false;

    while (*src) {
        if (*src == PATH_SEPARATOR) {
            if (!skip_slash) {
                last_slash = dst;
                *dst++ = *src;
                skip_slash = true;
            }
        } else if (*src == '.') {
            if (*(src + 1) == PATH_SEPARATOR || *(src + 1) == '\0') {
                src++;  // Skip '.' and move to separator or end
                continue;
            } else if (*(src + 1) == '.' && 
                      (*(src + 2) == PATH_SEPARATOR || *(src + 2) == '\0')) {
                if (last_slash) {
                    dst = last_slash;  // Remove last component
                    if (dst > path) {
                        // Find the last separator before dst
                        char* p = dst - 1;
                        while (p >= path) {
                            if (*p == PATH_SEPARATOR) {
                                last_slash = p;
                                break;
                            }
                            p--;
                        }
                    } else {
                        last_slash = NULL;
                    }
                }
                src += 2;  // Skip '..' and move to separator or end
                continue;
            }
            skip_slash = false;
            *dst++ = *src;
        } else {
            skip_slash = false;
            *dst++ = *src;
        }
        src++;
    }

    *dst = '\0';
    return PLATFORM_ERROR_SUCCESS;
}

bool platform_path_is_absolute(const char* path) {
    if (!path) {
        return false;
    }
    return path[0] == PATH_SEPARATOR;
}

PlatformErrorCode platform_path_to_native(char* path) {
    if (!path) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    // On POSIX systems, convert backslashes to forward slashes
    char* p = path;
    while (*p) {
        if (*p == '\\') {
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
    return mkdir(path, 0755);
}

PlatformErrorCode platform_fopen(FILE** file, const char* filename, const char* mode) {
    if (!file || !filename || !mode) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    *file = fopen(filename, mode);
    if (*file == NULL) {
        return PLATFORM_ERROR_FILE_OPEN;
    }
    
    return PLATFORM_ERROR_SUCCESS;
}
