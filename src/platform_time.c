#include "platform_time.h"
#include <string.h>

#ifdef _WIN32
#include <windows.h>

// Thread-local storage for timestamp reference values
THREAD_LOCAL static ULARGE_INTEGER g_filetime_reference_ularge;
THREAD_LOCAL static LARGE_INTEGER g_qpc_reference;
THREAD_LOCAL static LARGE_INTEGER g_qpc_frequency;
THREAD_LOCAL static int g_timestamp_initialised = 0;

void platform_init_timestamp_system(void) {
    /* Get the frequency once */
    QueryPerformanceFrequency(&g_qpc_frequency);

    /* Capture the high-resolution counter for baseline */
    QueryPerformanceCounter(&g_qpc_reference);

    /* Immediately capture the system time as FILETIME to sync with high-resolution timer */
    FILETIME file_time;
    GetSystemTimeAsFileTime(&file_time);
    g_filetime_reference_ularge.LowPart = file_time.dwLowDateTime;
    g_filetime_reference_ularge.HighPart = file_time.dwHighDateTime;

    g_timestamp_initialised = 1;
}

void platform_get_high_res_timestamp(PlatformHighResTimestamp_T* timestamp) {
    if (!g_timestamp_initialised) {
        platform_init_timestamp_system();
    }
    
    QueryPerformanceCounter(&timestamp->counter);
}

void platform_timestamp_to_calendar_time(
    const PlatformHighResTimestamp_T* timestamp,
    time_t* seconds,
    int64_t* nanoseconds)
{
    // Calculate elapsed ticks since reference
    int64_t elapsed_ticks = timestamp->counter.QuadPart - g_qpc_reference.QuadPart;
    int64_t elapsed_seconds = elapsed_ticks / g_qpc_frequency.QuadPart;
    
    // Convert stored FILETIME reference to time_t (seconds since Unix epoch)
    *seconds = (time_t)((g_filetime_reference_ularge.QuadPart / 10000000ULL) - 11644473600ULL);
    *seconds += elapsed_seconds;
    
    // Calculate nanoseconds part
    *nanoseconds = (elapsed_ticks % g_qpc_frequency.QuadPart) * 
                  (1000000000 / g_qpc_frequency.QuadPart);
}

int platform_localtime(const time_t* timer, struct tm* result) {
    return localtime_s(result, timer);
}

uint64_t platform_atomic_increment_u64(volatile uint64_t* value) {
    return InterlockedIncrement64((volatile LONG64*)value);
}

/**
 * @brief Get system tick count in milliseconds
 *
 * @return uint32_t Current tick count
 */
uint32_t platform_get_tick_count(void) {
    return GetTickCount();
}

#else
// POSIX implementation of the same functions
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

// Thread-local storage for timestamp reference
static __thread struct {
    struct timespec sys_time_reference;
    struct timespec monotonic_reference;
    int initialised;
} g_timestamp_context = {0};

void platform_init_timestamp_system(void) {
    // Get monotonic clock baseline (for precise duration measurement)
    clock_gettime(CLOCK_MONOTONIC, &g_timestamp_context.monotonic_reference);
    
    // Get system clock baseline (for calendar time)
    clock_gettime(CLOCK_REALTIME, &g_timestamp_context.sys_time_reference);
    
    g_timestamp_context.initialised = 1;
}

void platform_get_high_res_timestamp(PlatformHighResTimestamp_T* timestamp) {
    if (!g_timestamp_context.initialised) {
        platform_init_timestamp_system();
    }
    
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    
    // Store nanoseconds since epoch in counter
    timestamp->counter = (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

void platform_timestamp_to_calendar_time(
    const PlatformHighResTimestamp_T* timestamp,
    time_t* seconds,
    int64_t* nanoseconds)
{
    // Calculate elapsed time from monotonic reference
    int64_t elapsed_ns = timestamp->counter - 
                        (g_timestamp_context.monotonic_reference.tv_sec * 1000000000LL + 
                         g_timestamp_context.monotonic_reference.tv_nsec);
    
    // Add to system time reference
    *seconds = g_timestamp_context.sys_time_reference.tv_sec + (elapsed_ns / 1000000000LL);
    *nanoseconds = g_timestamp_context.sys_time_reference.tv_nsec + (elapsed_ns % 1000000000LL);
    
    // Normalize nanoseconds
    if (*nanoseconds >= 1000000000LL) {
        *nanoseconds -= 1000000000LL;
        (*seconds)++;
    }
}

int platform_localtime(const time_t* timer, struct tm* result) {
    return (localtime_r(timer, result) == NULL) ? -1 : 0;
}

uint64_t platform_atomic_increment_u64(volatile uint64_t* value) {
    return __sync_add_and_fetch(value, 1);
}

/**
 * @brief Get system tick count in milliseconds
 *
 * @return uint32_t Current tick count
 */
uint32_t platform_get_tick_count(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    
    // Convert to milliseconds
    uint64_t ms = (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
    
    return (uint32_t)ms;
}
#endif
