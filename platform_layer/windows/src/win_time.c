/**
 * @file windows_time.c
 * @brief Windows implementation of platform time functions using high-performance APIs
 */
#include "platform_time.h"
#include "platform_error.h"

#include <windows.h>
#include <timeapi.h>
#include <stdbool.h>

// Thread-local storage for timestamp reference
static __declspec(thread) struct {
    LARGE_INTEGER qpc_reference;
    FILETIME sys_time_reference;
    double qpc_frequency_ms;
    bool initialized;
} g_timestamp_context = {0};

PlatformErrorCode platform_init_timestamp_system(void) {
    LARGE_INTEGER freq;
    if (!QueryPerformanceFrequency(&freq)) {
        return PLATFORM_ERROR_UNKNOWN;
    }
    
    // Get QPC frequency in milliseconds
    g_timestamp_context.qpc_frequency_ms = 1000.0 / freq.QuadPart;
    
    // Get current QPC value
    if (!QueryPerformanceCounter(&g_timestamp_context.qpc_reference)) {
        return PLATFORM_ERROR_UNKNOWN;
    }
    
    // Get current system time
    GetSystemTimeAsFileTime(&g_timestamp_context.sys_time_reference);
    
    g_timestamp_context.initialized = true;
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
    
    LARGE_INTEGER qpc;
    if (!QueryPerformanceCounter(&qpc)) {
        return PLATFORM_ERROR_UNKNOWN;
    }
    
    // Store QPC value directly - we'll convert when needed
    timestamp->counter = qpc.QuadPart;
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

    // Calculate elapsed QPC ticks
    int64_t elapsed_ticks = timestamp->counter - g_timestamp_context.qpc_reference.QuadPart;
    
    // Convert to 100-nanosecond intervals (FILETIME resolution)
    ULARGE_INTEGER ref_ft;
    ref_ft.LowPart = g_timestamp_context.sys_time_reference.dwLowDateTime;
    ref_ft.HighPart = g_timestamp_context.sys_time_reference.dwHighDateTime;
    
    // Calculate elapsed time in 100ns units
    double elapsed_100ns = elapsed_ticks * (g_timestamp_context.qpc_frequency_ms * 10000.0 / 1000.0);
    
    // Add to reference time
    ULARGE_INTEGER result_ft;
    result_ft.QuadPart = ref_ft.QuadPart + (ULONGLONG)elapsed_100ns;
    
    // Convert to Unix epoch (FILETIME starts at 1601-01-01)
    // Number of 100-nanosecond intervals between 1601-01-01 and 1970-01-01
    const uint64_t EPOCH_DIFFERENCE = 116444736000000000ULL;
    
    result_ft.QuadPart -= EPOCH_DIFFERENCE;
    
    // Convert to seconds and nanoseconds
    *seconds = (time_t)(result_ft.QuadPart / 10000000ULL);
    *nanoseconds = (result_ft.QuadPart % 10000000ULL) * 100;
    
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

    if (!g_timestamp_context.initialized) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    uint64_t diff_ticks = end->counter - start->counter;
    double diff_ns = diff_ticks * (g_timestamp_context.qpc_frequency_ms * 1000000.0);
    *elapsed = (uint64_t)(diff_ns / granularity);
    
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_localtime(const time_t* timer, struct tm* result) {
    if (!timer || !result) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    errno_t err = localtime_s(result, timer);
    if (err != 0) {
        return PLATFORM_ERROR_UNKNOWN;
    }
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_get_tick_count(uint32_t* ticks) {
    if (!ticks) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    *ticks = GetTickCount();
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_sleep(uint32_t milliseconds) {
    // Use high-resolution sleep if available
    HANDLE timer = CreateWaitableTimer(NULL, TRUE, NULL);
    if (timer) {
        LARGE_INTEGER due_time;
        due_time.QuadPart = -((LONGLONG)milliseconds * 10000LL); // Convert to 100ns intervals
        
        if (SetWaitableTimer(timer, &due_time, 0, NULL, NULL, FALSE)) {
            WaitForSingleObject(timer, INFINITE);
            CloseHandle(timer);
            return PLATFORM_ERROR_SUCCESS;
        }
        CloseHandle(timer);
    }
    
    // Fallback to Sleep
    Sleep(milliseconds);
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_get_system_uptime(
    PlatformTimeGranularity granularity,
    uint64_t* uptime)
{
    if (!uptime) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    ULONGLONG ms = GetTickCount64();
    uint64_t ns = ms * 1000000ULL;
    *uptime = ns / granularity;
    
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

    SYSTEMTIME st;
    GetLocalTime(&st);
    
    const char* time_format = format ? format : "%Y-%m-%d %H:%M:%S";
    
    struct tm tm_time = {
        .tm_sec = st.wSecond,
        .tm_min = st.wMinute,
        .tm_hour = st.wHour,
        .tm_mday = st.wDay,
        .tm_mon = st.wMonth - 1,
        .tm_year = st.wYear - 1900,
        .tm_wday = st.wDayOfWeek,
        .tm_isdst = -1
    };
    
    size_t result = strftime(buffer, buffer_size, time_format, &tm_time);
    if (result == 0) {
        return PLATFORM_ERROR_BUFFER_TOO_SMALL;
    }
    
    return PLATFORM_ERROR_SUCCESS;
}

void sleep_ms(uint32_t ms) {
    platform_sleep(ms);
}