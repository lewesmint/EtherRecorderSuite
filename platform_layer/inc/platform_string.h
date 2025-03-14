/**
 * @file platform_string.h
 * @brief Platform-agnostic string manipulation functions
 */
#ifndef PLATFORM_STRING_H
#define PLATFORM_STRING_H

#include <stddef.h>
#include <stdbool.h>

#include "platform_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Safe string concatenation with size limit
 * @param[in,out] dest Destination buffer
 * @param[in] src Source string
 * @param[in] size Total size of destination buffer
 * @return Number of characters in resulting string (excluding null terminator)
 * @note Always null terminates if size > 0
 */
size_t platform_strcat(char* dest, const char* src, size_t size);

/**
 * @brief Case-insensitive string comparison
 * @param[in] str1 First string
 * @param[in] str2 Second string
 * @return 0 if equal, <0 if str1 < str2, >0 if str1 > str2
 */
int strcmp_nocase(const char* str1, const char* str2);

/**
 * @brief Case-insensitive string comparison with length limit
 * @param[in] str1 First string
 * @param[in] str2 Second string
 * @param[in] n Maximum number of characters to compare
 * @return 0 if equal, <0 if str1 < str2, >0 if str1 > str2
 */
int strncmp_nocase(const char* str1, const char* str2, size_t n);

/**
 * @brief Thread-safe tokenization function (strtok replacement)
 * @param[in,out] str String to tokenize (NULL to continue with previous string)
 * @param[in] delim Delimiter characters
 * @param[in,out] saveptr Pointer to char* used internally to maintain state
 * @return Pointer to next token or NULL if no more tokens
 */
char* platform_strtok(char* str, const char* delim, char** saveptr);

/**
 * @brief Convert string to lowercase in-place
 * @param[in,out] str String to convert
 */
void platform_strlower(char* str);

/**
 * @brief Convert string to uppercase in-place
 * @param[in,out] str String to convert
 */
void platform_strupper(char* str);

/**
 * @brief Trim whitespace from start and end of string in-place
 * @param[in,out] str String to trim
 * @return Pointer to trimmed string (same as str)
 */
char* platform_strtrim(char* str);

/**
 * @brief Check if string starts with prefix
 * @param[in] str String to check
 * @param[in] prefix Prefix to look for
 * @param[in] case_sensitive Whether comparison is case-sensitive
 * @return true if str starts with prefix
 */
bool platform_str_starts_with(const char* str, const char* prefix, bool case_sensitive);

/**
 * @brief Check if string ends with suffix
 * @param[in] str String to check
 * @param[in] suffix Suffix to look for
 * @param[in] case_sensitive Whether comparison is case-sensitive
 * @return true if str ends with suffix
 */
bool platform_str_ends_with(const char* str, const char* suffix, bool case_sensitive);

/**
 * @brief Find first occurrence of substring (case-insensitive)
 * @param[in] str String to search in
 * @param[in] substr Substring to search for
 * @return Pointer to first occurrence or NULL if not found
 */
const char* platform_stristr(const char* str, const char* substr);

/**
 * @brief Split string into parts based on delimiter
 * @param[in] str String to split
 * @param[in] delim Delimiter character
 * @param[out] parts Array to store pointers to parts
 * @param[in] max_parts Maximum number of parts to split into
 * @return Number of parts found
 * @note Modifies original string, placing null terminators between parts
 */
size_t platform_str_split(char* str, char delim, char** parts, size_t max_parts);

/**
 * @brief Format string with size limit (snprintf replacement)
 * @param[out] dest Destination buffer
 * @param[in] size Size of destination buffer
 * @param[in] format Format string
 * @param[in] ... Format arguments
 * @return Number of characters that would have been written if size was unlimited
 * @note Always null terminates if size > 0
 */
size_t platform_strformat(char* dest, size_t size, const char* format, ...);

/**
 * @brief Get the length of a string
 * @param[in] str String to measure
 * @return Length of string in characters (excluding null terminator)
 * @note Returns 0 if str is NULL
 */
size_t platform_strlen(const char* str);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_STRING_H
