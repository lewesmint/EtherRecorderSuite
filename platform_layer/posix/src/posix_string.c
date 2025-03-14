/**
 * @file platform_string.c
 * @brief POSIX implementation of platform string operations
 */
#include "../../inc/platform_string.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>    // for vsnprintf
#include <ctype.h>
#include <stdlib.h>   // for strtoull
#include <stdbool.h>  // for bool type and false constant

size_t platform_strcat(char* dest, const char* src, size_t size) {
    if (!dest || !src || size == 0) {
        return 0;
    }

    size_t dest_len = strlen(dest);
    if (dest_len >= size) {
        return dest_len;
    }

    size_t remaining = size - dest_len;
    size_t src_len = strlen(src);
    size_t to_copy = (src_len < remaining) ? src_len : (remaining - 1);

    memcpy(dest + dest_len, src, to_copy);
    dest[dest_len + to_copy] = '\0';

    return dest_len + to_copy;
}

int strcmp_nocase(const char* str1, const char* str2) {
    if (!str1 || !str2) {
        return str1 ? 1 : (str2 ? -1 : 0);
    }
    return strcasecmp(str1, str2);
}

int strncmp_nocase(const char* str1, const char* str2, size_t n) {
    if (!str1 || !str2) {
        return str1 ? 1 : (str2 ? -1 : 0);
    }
    return strncasecmp(str1, str2, n);
}

char* platform_strtok(char* str, const char* delim, char** saveptr) {
    if (!delim || !saveptr) {
        return NULL;
    }
    return strtok_r(str, delim, saveptr);
}

size_t platform_str_split(char* str, char delim, char** parts, size_t max_parts) {
    if (!str || !parts || max_parts == 0) {
        return 0;
    }

    size_t count = 0;
    char* start = str;
    char* end;

    while (count < max_parts && *start != '\0') {
        // Skip leading delimiters
        while (*start == delim) {
            start++;
        }
        if (*start == '\0') {
            break;
        }

        // Find end of current part
        end = start;
        while (*end != '\0' && *end != delim) {
            end++;
        }

        // Store the part
        parts[count++] = start;

        if (*end == '\0') {
            break;
        }

        // Null-terminate this part
        *end = '\0';
        start = end + 1;
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

uint64_t platform_strtoull(const char* str, char** endptr, int base) {
    if (!str || (base != 0 && (base < 2 || base > 36))) {
        if (endptr) {
            *endptr = (char*)str;
        }
        return 0;
    }
    return strtoull(str, endptr, base);
}

bool platform_str_starts_with(const char* str, const char* prefix, bool case_sensitive) {
    if (!str || !prefix) {
        return false;
    }
    
    size_t str_len = strlen(str);
    size_t prefix_len = strlen(prefix);
    
    if (prefix_len > str_len) {
        return false;
    }
    
    return case_sensitive ? 
        strncmp(str, prefix, prefix_len) == 0 :
        strncasecmp(str, prefix, prefix_len) == 0;
}

#ifdef __GNUC__
bool platform_str_ends_with(const char* str, const char* suffix, bool case_sensitive)
    __attribute__((unused));
#endif
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
        strcasecmp(str + str_len - suffix_len, suffix) == 0;
}

size_t platform_strlen(const char* str) {
    if (!str) {
        return 0;
    }
    
    #if defined(__GLIBC__) && defined(_GNU_SOURCE)
        return strnlen(str, SIZE_MAX);
    #else
        return strlen(str);
    #endif
}
