/**
 * @file platform_path.h
 * @brief Platform-agnostic path manipulation and filesystem path operations
 */
#ifndef PLATFORM_PATH_H
#define PLATFORM_PATH_H

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <limits.h>

#include "platform_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MAX_PATH_LEN
#ifdef MAX_PATH
#define MAX_PATH_LEN MAX_PATH
#else
#define MAX_PATH_LEN PATH_MAX
#endif
#endif


/**
 * @brief Platform-specific path separator character
 */
extern const char PATH_SEPARATOR;

/**
 * @brief Get the current working directory
 * @param[out] buffer Buffer to store the path
 * @param[in] size Size of the buffer
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_get_current_dir(char* buffer, size_t size);

/**
 * @brief Set the current working directory
 * @param[in] path Directory path to set as current
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_set_current_dir(const char* path);

/**
 * @brief Convert a path to absolute form
 * @param[in] path Path to convert
 * @param[out] abs_path Buffer to store absolute path
 * @param[in] size Size of abs_path buffer
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_path_to_absolute(
    const char* path,
    char* abs_path,
    size_t size);

/**
 * @brief Join two path components
 * @param[in] base First path component
 * @param[in] part Second path component
 * @param[out] result Buffer to store joined path
 * @param[in] size Size of result buffer
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_path_join(
    const char* base,
    const char* part,
    char* result,
    size_t size);

/**
 * @brief Get the directory component of a path
 * @param[in] path Full path
 * @param[out] dir Buffer to store directory path
 * @param[in] size Size of dir buffer
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_path_directory(
    const char* path,
    char* dir,
    size_t size);

/**
 * @brief Get the filename component of a path
 * @param[in] path Full path
 * @param[out] filename Buffer to store filename
 * @param[in] size Size of filename buffer
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_path_filename(
    const char* path,
    char* filename,
    size_t size);

/**
 * @brief Get the file extension from a path
 * @param[in] path Full path
 * @param[out] extension Buffer to store extension (including dot)
 * @param[in] size Size of extension buffer
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_path_extension(
    const char* path,
    char* extension,
    size_t size);

/**
 * @brief Normalize a path (remove ./, ../, duplicate separators)
 * @param[in,out] path Path to normalize in place
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_path_normalize(char* path);

/**
 * @brief Check if a path is absolute
 * @param[in] path Path to check
 * @return true if path is absolute, false if relative
 */
bool platform_path_is_absolute(const char* path);

/**
 * @brief Convert path separators to platform-native format
 * @param[in,out] path Path to convert in place
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_path_to_native(char* path);

/**
 * @brief Writes data to a stream
 * @param stream The output stream
 * @param buffer The data buffer to write
 * @param size Number of bytes to write
 * @return Number of bytes written, or -1 on error
 */
size_t platform_write(FILE* stream, const void* buffer, size_t size);

int platform_mkdir(const char* path);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_PATH_H
