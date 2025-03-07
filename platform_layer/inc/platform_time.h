/**
 * @file platform_time.h
 * @brief Platform-agnostic time and high-resolution timer functions
 */
#ifndef PLATFORM_TIME_H
#define PLATFORM_TIME_H

#include <stdint.h>
#include <time.h>

#include "platform_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque high-resolution timestamp type
 */
typedef struct {
    uint64_t counter;  // Unified counter type for all platforms
} PlatformHighResTimestamp_T;

/**
 * @brief Time granularity options in nanoseconds
 */
typedef enum {
    PLATFORM_TIME_GRANULARITY_NS     = 1,           // Nanoseconds
    PLATFORM_TIME_GRANULARITY_US     = 1000,        // Microseconds
    PLATFORM_TIME_GRANULARITY_MS     = 1000000,     // Milliseconds
    PLATFORM_TIME_GRANULARITY_SEC    = 1000000000   // Seconds
} PlatformTimeGranularity;

/**
 * @brief Initialize the high-resolution timer system for the current thread
 * @return PlatformErrorCode indicating success or failure
 * @note Must be called from each thread that uses high-resolution timestamps
 */
PlatformErrorCode platform_init_timestamp_system(void);

/**
 * @brief Get current high-resolution timestamp
 * @param[out] timestamp Pointer to store the timestamp
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_get_high_res_timestamp(PlatformHighResTimestamp_T* timestamp);

/**
 * @brief Convert timestamp to calendar time
 * @param[in] timestamp High-resolution timestamp
 * @param[out] seconds Seconds since Unix epoch
 * @param[out] nanoseconds Nanosecond part
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_timestamp_to_calendar_time(
    const PlatformHighResTimestamp_T* timestamp,
    time_t* seconds,
    int64_t* nanoseconds);

/**
 * @brief Calculate elapsed time between two timestamps
 * @param[in] start Start timestamp
 * @param[in] end End timestamp
 * @param[in] granularity Desired time granularity
 * @param[out] elapsed Elapsed time in specified granularity
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_timestamp_elapsed(
    const PlatformHighResTimestamp_T* start,
    const PlatformHighResTimestamp_T* end,
    PlatformTimeGranularity granularity,
    uint64_t* elapsed);

/**
 * @brief Thread-safe localtime function
 * @param[in] timer Time value to convert
 * @param[out] result Where to store the result
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_localtime(const time_t* timer, struct tm* result);

/**
 * @brief Get system tick count
 * @param[out] ticks Current tick count in milliseconds
 * @return PlatformErrorCode indicating success or failure
 * @note Wraps around approximately every 49.7 days
 */
PlatformErrorCode platform_get_tick_count(uint32_t* ticks);

/**
 * @brief Sleep for specified duration
 * @param milliseconds Duration to sleep in milliseconds
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_sleep(uint32_t milliseconds);

/**
 * @brief Get system uptime in specified granularity
 * @param[in] granularity Desired time granularity
 * @param[out] uptime System uptime in specified granularity
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_get_system_uptime(
    PlatformTimeGranularity granularity,
    uint64_t* uptime);

/**
 * @brief Format current time as string using specified format
 * @param[out] buffer Buffer to store formatted time string
 * @param[in] buffer_size Size of buffer
 * @param[in] format strftime format string (NULL for default "%Y-%m-%d %H:%M:%S")
 * @return PlatformErrorCode indicating success or failure
 *         PLATFORM_ERROR_SUCCESS on success
 *         PLATFORM_ERROR_INVALID_ARGUMENT if buffer is NULL or buffer_size is 0
 *         PLATFORM_ERROR_BUFFER_TOO_SMALL if buffer size is insufficient
 *         PLATFORM_ERROR_SYSTEM if system time functions fail
 * 
 * @note If format is NULL, uses default format "%Y-%m-%d %H:%M:%S"
 * @note Always null-terminates the output buffer on success
 * @note Thread-safe implementation
 * 
 * Example usage:
 * @code
 * char timestamp[64];
 * PlatformErrorCode result = platform_format_current_time(timestamp, sizeof(timestamp), NULL);
 * if (result == PLATFORM_ERROR_SUCCESS) {
 *     // Use timestamp string
 * }
 * 
 * // Custom format
 * result = platform_format_current_time(timestamp, sizeof(timestamp), "%H:%M:%S");
 * @endcode
 */
PlatformErrorCode platform_format_current_time(
    char* buffer,
    size_t buffer_size,
    const char* format);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_TIME_H
