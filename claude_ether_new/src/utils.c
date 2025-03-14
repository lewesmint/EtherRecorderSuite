#include "utils.h"
#include "platform_path.h"
#include "platform_time.h"
#include "platform_console.h"
#include "platform_string.h"

#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>  // Add this include for isspace()

PlatformErrorCode init_console(void) {
    PlatformErrorCode result;
    
    // Initialize console subsystem
    result = platform_console_init();
    if (result != PLATFORM_ERROR_SUCCESS) {
        return result;
    }

    // Enable VT mode for ANSI color support
    result = platform_console_enable_vt_mode(true);
    if (result != PLATFORM_ERROR_SUCCESS) {
        return result;
    }

    // Disable quick edit mode on Windows (prevents console freezing)
    result = platform_console_set_quick_edit(false);
    if (result != PLATFORM_ERROR_SUCCESS) {
        return result;
    }

    return PLATFORM_ERROR_SUCCESS;
}

void strip_directory_path(const char* full_file_path, char* directory_path, size_t size) {
    if (!full_file_path || !directory_path || size == 0) return;

    const char* last_sep = strrchr(full_file_path, PATH_SEPARATOR);
    if (last_sep != NULL) {
        size_t len = (size_t)(last_sep - full_file_path);
        if (len >= size) len = size - 1;
        directory_path[0] = '\0';  // Initialize to empty string
        platform_strcat(directory_path, full_file_path, len + 1);  // +1 for null terminator
        directory_path[len] = '\0';  // Ensure null termination at the correct position
    }
    else {
        directory_path[0] = '\0';
    }
}

int create_directories(const char* path) {
    if (!path || !*path) return 0;

    char tmp[MAX_PATH_LEN];
    snprintf(tmp, sizeof(tmp), "%s", path);

    size_t len = strlen(tmp);
    if (len > 0 && (tmp[len - 1] == '/' || tmp[len - 1] == '\\')) {
        tmp[len - 1] = '\0';
    }

    for (char* p = tmp + 1; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
            if (platform_mkdir(tmp) != 0 && errno != EEXIST) return -1;
            *p = PATH_SEPARATOR;
        }
    }
    return (platform_mkdir(tmp) != 0 && errno != EEXIST) ? -1 : 0;
}

/**
 * @brief Prints a formatted string to a given stream.
 * @param stream Output stream.
 * @param format The format string.
 * @param ... Additional arguments for the format string.
 */
void stream_print(FILE* stream, const char *format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    int n = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (n > 0) {
        platform_write(stream, buffer, (size_t)n);
    }
}

void sleep_seconds(double seconds) {
    unsigned int milliseconds = (unsigned int)(seconds * 1000);
    sleep_ms(milliseconds);
}

void sanitise_path(char* path) {
    if (!path) return;

    size_t len = strlen(path);
    while (len > 0 && isspace((unsigned char)path[len - 1])) {
        path[len - 1] = '\0';
        --len;
    }

    char* start = path;
    while (*start && isspace((unsigned char)*start)) {
        ++start;
    }
    if (start != path) {
        memmove(path, start, strlen(start) + 1);
        len = strlen(path);
    }

    while (len > 0 && (path[len - 1] == '/' || path[len - 1] == '\\')) {
        path[len - 1] = '\0';
        --len;
    }

    for (char* p = path; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            *p = PATH_SEPARATOR;
        }
    }
}

/**
 * @brief Get current time in milliseconds
 * @return Current time in milliseconds
 */
uint32_t get_time_ms(void) {
    uint32_t ticks;
    if (platform_get_tick_count(&ticks) != PLATFORM_ERROR_SUCCESS) {
        return 0; // Return 0 on error
    }
    return ticks;
}
