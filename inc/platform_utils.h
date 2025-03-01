/**
* @file platform_utils.h
* @brief Platform-specific utility functions.
* 
*/
#ifndef PLATFORM_UTILS_H
#define PLATFORM_UTILS_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef _WIN32
    #include <windows.h>
#else // !_WIN32
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/random.h>
    #include <unistd.h> // For getcwd on non-Windows platforms
#endif // _WIN32

#include "platform_mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
    #define sleep(x) Sleep(1000 * (x))
    #ifndef PATH_MAX
        #define PATH_MAX MAX_PATH
    #endif // PATH_MAX
#else // !_WIN32
    #ifndef MAX_PATH
        #define MAX_PATH PATH_MAX
    #endif // MAX_PATH
#endif // _WIN32

/*
 * Extern declaration of a platform-specific path separator.
 * The definition will be in platform_utils.c, where we handle _WIN32.
 */
extern const char PATH_SEPARATOR;

/**
 * @brief Converts a string to an unsigned 64-bit integer.
 * @param str The null-terminated string to convert.
 * @param endptr If non-null, receives the address of the first invalid character.
 * @param base The numeric base to interpret (e.g. 10).
 * @return The converted value, or 0 on failure.
 */
uint64_t platform_strtoull(const char *str, char **endptr, int base);

/**
 * @brief Gets the current time as a formatted string (YYYY-mm-dd HH:MM:SS).
 * @param buffer Pointer to the buffer where the formatted string is placed.
 * @param buffer_size Size of @p buffer.
 */
void get_current_time(char *buffer, size_t buffer_size);

/**
 * @brief Initialises a mutex.
 * @param mutex Pointer to the mutex structure.
 */
int init_mutex(PlatformMutex_T *mutex);

/**
 * @brief Locks a mutex.
 * @param mutex Pointer to the mutex structure.
 */
int lock_mutex(PlatformMutex_T *mutex);

/**
 * @brief Unlocks a mutex.
 * @param mutex Pointer to the mutex structure.
 */
int unlock_mutex(PlatformMutex_T *mutex);

/**
 * @brief Prints a formatted string to a given stream.
 * @param stream Output stream.
 * @param format The format string.
 * @param ... Additional arguments for the format string.
 */
void stream_print(FILE* stream, const char* format, ...);

/**
 * @brief Sleeps for a specified number of milliseconds.
 * @param milliseconds Number of milliseconds to sleep.
 */
void sleep_ms(unsigned int milliseconds);

/**
 * @brief Sleeps for a specified number of seconds (as a double).
 * @param seconds Number of seconds to sleep.
 */
void sleep_seconds(double seconds);

/**
 * @brief Gets the last platform error code.
 *
 * @return The error code.
 */
int platform_get_last_error(void);

/**
 * @brief Gets a string describing the last error.
 *
 * @param buffer Buffer to store the error message.
 * @param buffer_size Size of the buffer.
 * @return The buffer containing the error message.
 */
char* platform_get_error_message(char* buffer, size_t buffer_size);

/**
 * @brief Sleep for the specified number of milliseconds.
 *
 * @param ms Number of milliseconds to sleep.
 */
void sleep_ms(uint32_t ms);

// /**
//  * @brief Generates a random number.
//  * @return A random number. Protected by a mutex.
//  */
// int platform_rand();

/**
 * @brief Sanitises a file path by trimming spaces, removing trailing slashes
 *        and converting path separators to the platform default.
 * @param path The path to sanitise.
 */
void sanitise_path(char* path);

/**
 * @brief Extracts the directory portion of a file path.
 * @param full_file_path The full path to examine.
 * @param directory_path Buffer to receive the directory portion.
 * @param size The size of the @p directory_path buffer.
 */
void strip_directory_path(const char* full_file_path, char* directory_path, size_t size);

/**
 * @brief Recursively creates directories for the given path.
 * @param path The directory path to create.
 * @return 0 on success, -1 on failure.
 */
int create_directories(const char* path);

/**
 * @brief Initialises the console (e.g. disables Quick Edit mode on Windows).
 */
void init_console();

/**
 * @brief Resolves the absolute (full) path of a given file/directory.
 * @param filename The file or directory name to resolve.
 * @param full_path Buffer to hold the resulting absolute path.
 * @param size The size of @p full_path.
 * @return true on success, false on failure.
 */
bool resolve_full_path(const char* filename, char* full_path, size_t size);

/**
 * @brief Provides a case-insensitive string comparison.
 * A cross-platform equivalent of `strcasecmp()`, implemented in `platform_utils.c`.
 * @param s1 First string.
 * @param s2 Second string.
 * @return 0 if the strings are equal (ignoring case), a negative value if s1 < s2, 
 *         and a positive value if s1 > s2.
 */
int str_cmp_nocase(const char *s1, const char *s2);

uint32_t platform_random();
uint32_t platform_random_range(uint32_t min, uint32_t max);

char* get_cwd(char* buffer, int max_length);
/**
 * @brief Function type for shutdown callbacks
 */
typedef void (*ShutdownCallback_T)(void);

/**
 * @brief Register handler for application termination signals 
 * (Ctrl+C on Windows, SIGINT/SIGTERM on POSIX)
 * 
 * @param callback Function to call when termination signal is received
 * @return bool true on success, false on failure
 */
bool platform_register_shutdown_handler(ShutdownCallback_T callback);

/**
 * @brief Thread-safe string tokenization function
 * @param str String to tokenize (NULL to continue tokenizing previous string)
 * @param delimiters String containing delimiter characters
 * @param saveptr Pointer used to store state between calls
 * @return Pointer to next token or NULL if no more tokens
 */
char* platform_strtok(char* str, const char* delimiters, char** saveptr);

#ifdef __cplusplus
}
#endif // __cplusplus


#endif // PLATFORM_UTILS_H