/**
 * @file win_string.c
 * @brief Windows implementation of platform string operations
 */
#include "platform_string.h"
#include "platform_error.h"

#include <windows.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>

// Ensure vsnprintf is properly declared for Windows
#if defined(_MSC_VER) && _MSC_VER < 1900
#define vsnprintf _vsnprintf
#endif

size_t platform_strcat(char* dest, const char* src, size_t size) {
    if (!dest || !src || size == 0) {
        return 0;
    }

    size_t dest_len = strlen(dest);
    size_t src_len = strlen(src);
    size_t to_copy = (dest_len + src_len < size - 1) ? src_len : (size - dest_len - 1);

    if (to_copy > 0) {
        memcpy(dest + dest_len, src, to_copy);
        dest[dest_len + to_copy] = '\0';
    }

    return dest_len + to_copy;
}

int strcmp_nocase(const char* str1, const char* str2) {
    if (!str1 || !str2) {
        return str1 ? 1 : (str2 ? -1 : 0);
    }
    return _stricmp(str1, str2);
}

int strncmp_nocase(const char* str1, const char* str2, size_t n) {
    if (!str1 || !str2) {
        return str1 ? 1 : (str2 ? -1 : 0);
    }
    return _strnicmp(str1, str2, n);
}

char* platform_strtok(char* str, const char* delim, char** saveptr) {
    if (!delim || !saveptr) {
        return NULL;
    }
    
    if (str) {
        *saveptr = str;
    }
    
    if (!*saveptr) {
        return NULL;
    }

    // Skip leading delimiters
    *saveptr += strspn(*saveptr, delim);
    if (**saveptr == '\0') {
        *saveptr = NULL;
        return NULL;
    }

    // Find end of token
    char* token = *saveptr;
    *saveptr += strcspn(*saveptr, delim);
    
    if (**saveptr != '\0') {
        **saveptr = '\0';
        (*saveptr)++;
    } else {
        *saveptr = NULL;
    }

    return token;
}

void platform_strlower(char* str) {
    if (!str) {
        return;
    }
    
    while (*str) {
        *str = (char)tolower(*str);
        str++;
    }
}

void platform_strupper(char* str) {
    if (!str) {
        return;
    }
    
    while (*str) {
        *str = (char)toupper(*str);
        str++;
    }
}

const char* platform_stristr(const char* str, const char* substr) {
    if (!str || !substr) {
        return NULL;
    }

    size_t str_len = strlen(str);
    size_t substr_len = strlen(substr);

    if (substr_len == 0) {
        return str;
    }

    if (substr_len > str_len) {
        return NULL;
    }

    for (size_t i = 0; i <= str_len - substr_len; i++) {
        if (_strnicmp(str + i, substr, substr_len) == 0) {
            return str + i;
        }
    }

    return NULL;
}

size_t platform_str_split(char* str, char delim, char** parts, size_t max_parts) {
    if (!str || !parts || max_parts == 0) {
        return 0;
    }

    size_t count = 0;
    char* start = str;
    char* end;

    while (count < max_parts && *start) {
        // Skip leading delimiters
        while (*start == delim) {
            start++;
        }
        
        if (!*start) {
            break;
        }

        // Find end of current part
        end = start;
        while (*end && *end != delim) {
            end++;
        }

        // Store the part
        if (end > start) {
            if (*end) {
                *end = '\0';
                end++;
            }
            parts[count++] = start;
            start = end;
        }
    }

    return count;
}

size_t platform_strformat(char* dest, size_t size, const char* format, ...) {
    if (!dest || !format || size == 0) {
        return 0;
    }

    va_list args;
    va_start(args, format);
    int result = vsnprintf(dest, size, format, args);
    va_end(args);

    if (result < 0) {
        dest[0] = '\0';
        return 0;
    }

    // Return what would have been written if buffer was large enough
    return (size_t)result;
}

bool platform_str_starts_with(const char* str, const char* prefix, bool case_sensitive) {
    if (!str || !prefix) {
        return false;
    }
    
    return case_sensitive ?
        strncmp(str, prefix, strlen(prefix)) == 0 :
        _strnicmp(str, prefix, strlen(prefix)) == 0;
}

bool platform_str_ends_with(const char* str, const char* suffix, bool case_sensitive) {
    if (!str || !suffix) {
        return false;
    }
    
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    
    if (suffix_len > str_len) {
        return false;
    }
    
    return case_sensitive ?
        strcmp(str + str_len - suffix_len, suffix) == 0 :
        _stricmp(str + str_len - suffix_len, suffix) == 0;
}

void platform_str_trim(char* str) {
    if (!str) {
        return;
    }

    // Trim trailing whitespace
    char* end = str + strlen(str) - 1;
    while (end >= str && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }

    // Trim leading whitespace
    char* start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}
