/**
 * @file platform_time.c
 * @brief POSIX implementation of platform time functions
 */
#include "platform_time.h"

#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>

#include "platform_error.h"


// Thread-local storage for timestamp reference
static __thread struct {
    struct timespec sys_time_reference;
    struct timespec monotonic_reference;
    int initialized;
} g_timestamp_context = {0};

PlatformErrorCode platform_init_timestamp_system(void) {
    if (clock_gettime(CLOCK_MONOTONIC, &g_timestamp_context.monotonic_reference) != 0) {
        return PLATFORM_ERROR_UNKNOWN;
    }
    
    if (clock_gettime(CLOCK_REALTIME, &g_timestamp_context.sys_time_reference) != 0) {
        return PLATFORM_ERROR_UNKNOWN;
    }
    
    g_timestamp_context.initialized = 1;
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_get_high_res_timestamp(PlatformHighResTimestamp_T* timestamp) {
    if (!timestamp) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    if (!g_timestamp_context.initialized) {
        PlatformErrorCode result = platform_init_timestamp_system();
        if (result != PLATFORM_ERROR_SUCCESS) {
            return result;
        }
    }
    
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return PLATFORM_ERROR_UNKNOWN;
    }
    
    // Store nanoseconds since epoch in counter
    timestamp->counter = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_timestamp_to_calendar_time(
    const PlatformHighResTimestamp_T* timestamp,
    time_t* seconds,
    int64_t* nanoseconds) 
{
    if (!timestamp || !seconds || !nanoseconds) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    if (!g_timestamp_context.initialized) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    // Calculate elapsed time from monotonic reference
    uint64_t elapsed_ns = timestamp->counter - 
        ((uint64_t)g_timestamp_context.monotonic_reference.tv_sec * 1000000000ULL + 
         g_timestamp_context.monotonic_reference.tv_nsec);

    // Add elapsed time to system time reference
    *seconds = g_timestamp_context.sys_time_reference.tv_sec + (elapsed_ns / 1000000000ULL);
    *nanoseconds = g_timestamp_context.sys_time_reference.tv_nsec + (elapsed_ns % 1000000000ULL);

    // Handle nanosecond overflow
    if (*nanoseconds >= 1000000000LL) {
        (*seconds)++;
        *nanoseconds -= 1000000000LL;
    }

    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_timestamp_elapsed(
    const PlatformHighResTimestamp_T* start,
    const PlatformHighResTimestamp_T* end,
    PlatformTimeGranularity granularity,
    uint64_t* elapsed)
{
    if (!start || !end || !elapsed) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    uint64_t diff_ns = end->counter - start->counter;
    *elapsed = diff_ns / granularity;
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_localtime(const time_t* timer, struct tm* result) {
    if (!timer || !result) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    if (localtime_r(timer, result) == NULL) {
        return PLATFORM_ERROR_UNKNOWN;
    }
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_get_tick_count(uint32_t* ticks) {
    if (!ticks) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    *ticks = (uint32_t)((ts.tv_sec * 1000) + (ts.tv_nsec / 1000000));
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_sleep(uint32_t milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;

    while (nanosleep(&ts, &ts) == -1) {
        if (errno != EINTR) {
            return PLATFORM_ERROR_UNKNOWN;
        }
    }
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_get_system_uptime(
    PlatformTimeGranularity granularity,
    uint64_t* uptime)
{
    if (!uptime) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    struct timespec ts;
    #ifdef CLOCK_BOOTTIME
        // Linux-specific: includes time system was suspended
        if (clock_gettime(CLOCK_BOOTTIME, &ts) != 0) {
            // Fallback to CLOCK_MONOTONIC if CLOCK_BOOTTIME fails
            if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
                return PLATFORM_ERROR_UNKNOWN;
            }
        }
    #else
        // Other POSIX systems: use CLOCK_MONOTONIC
        if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
            return PLATFORM_ERROR_UNKNOWN;
        }
    #endif

    uint64_t uptime_ns = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    *uptime = uptime_ns / granularity;
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_format_current_time(
    char* buffer,
    size_t buffer_size,
    const char* format)
{
    if (!buffer || buffer_size == 0) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    time_t current_time;
    struct tm time_info;

    if (time(&current_time) == -1) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    if (localtime_r(&current_time, &time_info) == NULL) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    const char* time_format = format ? format : "%Y-%m-%d %H:%M:%S";
    size_t result = strftime(buffer, buffer_size, time_format, &time_info);

    if (result == 0) {
        return PLATFORM_ERROR_BUFFER_TOO_SMALL;
    }

    return PLATFORM_ERROR_SUCCESS;
}
