/**
 * @file logger_macros.h
 * @brief Macro definitions for the logging system.
 */
#ifndef LOGGER_MACROS_H
#define LOGGER_MACROS_H

#include "logger.h"

#ifdef _DEBUG
/*
 * In debug builds, if the log level is LOG_TRACE (or if g_trace_all is true),
 * prepend the file and line information to the log message.
 */

// Helper macros for readability
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#endif

#define _logger_log_with_file_line(level, fmt, ...) \
    _logger_log(level, "[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)

#define _logger_log_without_file_line(level, fmt, ...) \
    _logger_log(level, fmt, ##__VA_ARGS__)

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

// The main logger_log macro definitions
#if defined(_MSC_VER)
  // MSVC - use different pragma syntax
  #define logger_log(level, fmt, ...) \
      __pragma(warning(push)) \
      __pragma(warning(disable:4003)) \
      ((level) == LOG_TRACE || g_trace_all ? \
        _logger_log_with_file_line(level, fmt, ##__VA_ARGS__) : \
        _logger_log_without_file_line(level, fmt, ##__VA_ARGS__)) \
      __pragma(warning(pop))
#elif defined(__clang__)
  // Clang approach
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
  #define logger_log(level, fmt, ...) \
      ((level) == LOG_TRACE || g_trace_all ? \
        _logger_log_with_file_line(level, fmt, ##__VA_ARGS__) : \
        _logger_log_without_file_line(level, fmt, ##__VA_ARGS__))
  #pragma clang diagnostic pop
#else
  // GCC and others
  #define logger_log(level, fmt, ...) \
      ((level) == LOG_TRACE || g_trace_all ? \
        _logger_log_with_file_line(level, fmt, ##__VA_ARGS__) : \
        _logger_log_without_file_line(level, fmt, ##__VA_ARGS__))
#endif

#else
/*
 * In non-debug builds, always use the standard logging function without
 * file/line info.
 */
#if defined(_MSC_VER)
  #define logger_log(level, fmt, ...) \
      __pragma(warning(push)) \
      __pragma(warning(disable:4003)) \
      _logger_log(level, fmt, ##__VA_ARGS__) \
      __pragma(warning(pop))
#elif defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
  #define logger_log(level, fmt, ...) \
      _logger_log(level, fmt, ##__VA_ARGS__)
  #pragma clang diagnostic pop
#else
  #define logger_log(level, fmt, ...) \
      _logger_log(level, fmt, ##__VA_ARGS__)
#endif
#endif

#endif // LOGGER_MACROS_H
