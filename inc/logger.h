/**
 * @file logger.h
 * @brief Logger interface for logging messages to stderr and/or a file.
 */
#ifndef LOGGER_H
#define LOGGER_H

#include <winsock2.h>
#include <windows.h>
#include <stdbool.h>
#include <stdint.h>



#define LOG_MSG_BUFFER_SIZE 1024 // Buffer size for log messages
#define THREAD_LABEL_SIZE 64 // Buffer size for thread labels

#ifdef _DEBUG
/*
    * Global runtime flag to control whether all log messages should
    * include file and line information.
    *
*/
 extern bool g_trace_all;
#endif // _DEBUG

/* Define log levels. */
typedef enum LogLevel {
    LOG_TRACE,    /**< Trace level: Very detailed debugging information */
    LOG_DEBUG,    /**< Debug level: General debugging information */
    LOG_INFO,     /**< Info level: General operational messages */
    LOG_NOTICE,   /**< Notice level: Normal but significant events */
    LOG_WARN,     /**< Warning level: Potential issues to investigate */
    LOG_ERROR,    /**< Error level: Errors that do not stop the program */
    LOG_CRITICAL, /**< Critical level: Severe errors needing prompt attention */
    LOG_FATAL     /**< Fatal level: Errors causing premature program termination */
} LogLevel;

/**
 * @brief Logs a message with the specified log level.
 *
 * @param format The format string for the message.
 * @param ... The arguments for the format string.
 */
void _logger_log(LogLevel level, const char* format, ...);

#ifdef _DEBUG
/*
 * In debug builds, if the log level is LOG_TRACE (or if g_trace_all is true),
 * prepend the file and line information to the log message.
 */

#define _logger_log_helper(level, fmt, ...) _logger_log(level, fmt, ##__VA_ARGS__)

#define logger_log(level, fmt, ...)                                \
    do {                                                           \
        if ((level) == LOG_TRACE || g_trace_all) {                 \
            _logger_log_helper(level, "[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__); \
        } else {                                                   \
            _logger_log_helper(level, fmt, ##__VA_ARGS__);           \
        }                                                          \
    } while (0)
#else
/*
 * In non-debug builds, always use the standard logging function without
 * file/line info.
 */
#define logger_log(level, fmt, ...) \
        _logger_log(level, fmt, ##__VA_ARGS__)
#endif

/**
 * @enum LogOutput
 * @brief Defines the possible output destinations for logs.
 */
typedef enum LogOutput {
    LOG_OUTPUT_SCREEN,  // Only screen (stderr or stdout)
    LOG_OUTPUT_FILE,    // Only file/stderr
    LOG_OUTPUT_BOTH     // Both screen/stderr and file
} LogOutput;

/**
 * @brief Structure representing a log entry.
 */

typedef struct LogEntry_T {
    uint64_t index;
    LogLevel level;
    LARGE_INTEGER timestamp; // Use LARGE_INTEGER for high-resolution timestamp
    char message[LOG_MSG_BUFFER_SIZE];
    char thread_label[THREAD_LABEL_SIZE]; // Add thread label field
} LogEntry_T;

/**
 * @brief Initialises the logger.
 */
bool init_logger_from_config(char *logger_init_result);

void init_thread_timestamp_system(void);

void create_log_entry(LogEntry_T* entry, LogLevel level, const char* message);

/**
 * @brief Sets a thread-specific log file from configuration.
 *
 * @param thread_name The name of the thread.
 */
void set_thread_log_file_from_config(const char *thread_name);

/**
 * @brief Converts the log level to a string.
 *
 * @param level The log level to convert.
 * @return The string representation of the log level.
 */
const char* log_level_to_string(LogLevel level);

/**
 * @brief Logs a message now to file and console, avoiding the queue.
 */
void log_now(const LogEntry_T *entry);

/**
 * @brief Closes the logger and releases any resources.
 */
void logger_close(void);

#endif // LOGGER_H
