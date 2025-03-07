#ifndef PLATFORM_TIME_H
#define PLATFORM_TIME_H

// 1. System includes
#include <stdint.h>
#include <time.h>

// 2. Platform-specific includes
#ifdef _WIN32
    #include <windows.h>
    // Platform-specific timestamp type
    typedef struct {
        LARGE_INTEGER counter;
    } PlatformHighResTimestamp_T;
#else
    // Platform-specific timestamp type
    typedef struct {
        int64_t counter;
    } PlatformHighResTimestamp_T;
#endif

// 3. Project includes
#include "platform_threads.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise the high-resolution timer for the current thread
 */
void platform_init_timestamp_system(void);

/**
 * @brief Get current high-resolution timestamp
 * @param timestamp Pointer to store the timestamp
 */
void platform_get_high_res_timestamp(PlatformHighResTimestamp_T* timestamp);

/**
 * @brief Convert timestamp to calendar time
 * @param timestamp High-resolution timestamp
 * @param seconds Output pointer for seconds since Unix epoch
 * @param nanoseconds Output pointer for nanosecond part
 */
void platform_timestamp_to_calendar_time(
    const PlatformHighResTimestamp_T* timestamp,
    time_t* seconds,
    int64_t* nanoseconds);

/**
 * @brief Thread-safe localtime function
 * @param timer Time value to convert
 * @param result Where to store the result
 * @return 0 on success, error code on failure
 */
int platform_localtime(const time_t* timer, struct tm* result);

/**
 * @brief Atomic increment for 64-bit values
 * @param value Pointer to value to increment
 * @return The incremented value
 */
uint64_t platform_atomic_increment_u64(volatile uint64_t* value);

/**
 * @brief Get system tick count in milliseconds
 *
 * @return uint32_t Current tick count
 */
uint32_t platform_get_tick_count(void);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_TIME_H
