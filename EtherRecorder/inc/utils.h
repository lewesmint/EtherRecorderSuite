#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "platform_error.h"

/**
 * @brief Initializes the console with appropriate settings
 * @return PLATFORM_ERROR_SUCCESS on success, error code on failure
 */
PlatformErrorCode init_console(void);

/**
 * @brief Recursively creates directories for the given path.
 * @param path The directory path to create.
 * @return 0 on success, -1 on failure.
 */
void strip_directory_path(const char* full_file_path, char* directory_path, size_t size);

/**
 * @brief Recursively creates directories for the given path.
 * @param path The directory path to create.
 * @return 0 on success, -1 on failure.
 */
int create_directories(const char* path);

/**
 * @brief Prints a formatted string to a given stream.
 * @param stream Output stream.
 * @param format The format string.
 * @param ... Additional arguments for the format string.
 */
void stream_print(FILE* stream, const char* format, ...);

/**
 * @brief Sanitises a file path by trimming spaces, removing trailing slashes
 *        and converting path separators to the platform default.
 * @param path The path to sanitise.
 */
void sanitise_path(char* path);

void sleep_seconds(double seconds);

void strip_directory_path(const char* full_file_path, char* directory_path, size_t size);

int create_directories(const char* path);

/**
 * @brief Get current time in milliseconds
 * @return Current time in milliseconds
 */
uint32_t get_time_ms(void);
#endif // UTILS_H
