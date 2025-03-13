/**
 * @file logger.h
 * @brief Logger interface for logging messages to stderr and/or a file.
 */
#ifndef LOGGER_H
#define LOGGER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>  // Add this for variable arguments

#include "platform_time.h"
#include "app_thread.h"  // Add this include for ThreadConfig
#include "logger_macros.h"


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
    PlatformHighResTimestamp_T timestamp;
    char message[LOG_MSG_BUFFER_SIZE];
    char thread_label[THREAD_LABEL_SIZE];
} LogEntry_T;

/**
 * @brief Initialises the logger.
 */
bool init_logger_from_config(char *logger_init_result);

/**
 * @brief Initializes the thread timestamp system.
 */
void init_thread_timestamp_system(void);

/**
 * @brief Creates a log entry with the given level and message.
 */
void create_log_entry(LogEntry_T* entry, LogLevel level, const char* message);

/**
 * @brief Sets a thread-specific log file from configuration.
 */
void set_thread_log_file_from_config(const char *thread_name);

/**
 * @brief Converts the log level to a string.
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

/**
 * @brief Get the current log level.
 */
LogLevel logger_get_level(void);

/**
 * @brief Get the name of a log level.
 */
const char* get_level_name(LogLevel level);

/**
 * @brief Sets the log level.
 */
void logger_set_level(LogLevel level);

/**
 * @brief Internal logging function - use logger_log macro instead.
 * @note This function should not be called directly. Use the logger_log macro.
 */
void _logger_log(LogLevel level, const char* format, ...);

/**
 * @brief Get the logger thread configuration.
 */
ThreadConfig* get_logger_thread(void);

#endif // LOGGER_H
