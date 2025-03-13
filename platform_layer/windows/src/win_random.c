/**
 * @file windows_random.c
 * @brief Windows implementation of platform random number generation using CNG
 */
#include "platform_random.h"
#include "platform_error.h"

#include <windows.h>
#include <bcrypt.h>
#include <stdbool.h>

// Define RtlGenRandom (aka SystemFunction036)
DECLSPEC_IMPORT BOOLEAN WINAPI SystemFunction036(PVOID RandomBuffer, ULONG RandomBufferLength);
#define RtlGenRandom SystemFunction036

// Link against bcrypt.lib
#pragma comment(lib, "bcrypt.lib")

// Thread-local storage for random state
static __declspec(thread) struct {
    BCRYPT_ALG_HANDLE provider;
    bool initialized;
} g_random_context = {0};

PlatformErrorCode platform_random_init(void) {
    if (g_random_context.initialized) {
        return PLATFORM_ERROR_SUCCESS;
    }

    NTSTATUS status = BCryptOpenAlgorithmProvider(
        &g_random_context.provider,
        BCRYPT_RNG_ALGORITHM,
        NULL,
        0);

    if (!BCRYPT_SUCCESS(status)) {
        return PLATFORM_ERROR_NOT_INITIALIZED;
    }

    g_random_context.initialized = true;
    return PLATFORM_ERROR_SUCCESS;
}

void platform_random_cleanup(void) {
    if (g_random_context.initialized) {
        BCryptCloseAlgorithmProvider(g_random_context.provider, 0);
        g_random_context.provider = NULL;
        g_random_context.initialized = false;
    }
}

PlatformErrorCode platform_random_bytes(void* buffer, size_t length) {
    if (!buffer || length == 0) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    if (!g_random_context.initialized) {
        PlatformErrorCode result = platform_random_init();
        if (result != PLATFORM_ERROR_SUCCESS) {
            return result;
        }
    }

    NTSTATUS status = BCryptGenRandom(
        g_random_context.provider,
        (PUCHAR)buffer,
        (ULONG)length,
        0);

    if (!BCRYPT_SUCCESS(status)) {
        // Fallback to system-provided RNG if CNG fails
        if (!RtlGenRandom(buffer, (ULONG)length)) {
            // Last resort: use high-resolution time and other entropy sources
            LARGE_INTEGER perf_counter;
            QueryPerformanceCounter(&perf_counter);
            FILETIME ft;
            GetSystemTimeAsFileTime(&ft);

            unsigned char* bytes = (unsigned char*)buffer;
            for (size_t i = 0; i < length; i++) {
                ULONGLONG entropy = ((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
                entropy ^= perf_counter.QuadPart + i;
                entropy ^= (ULONGLONG)GetCurrentProcessId() << 32;
                entropy ^= (ULONGLONG)GetCurrentThreadId();
                entropy ^= (ULONGLONG)GetTickCount64();
                
                bytes[i] = (unsigned char)(entropy ^ (entropy >> 32));
            }
        }
    }

    return PLATFORM_ERROR_SUCCESS;
}

uint32_t platform_random_uint32(void) {
    uint32_t value;
    if (platform_random_bytes(&value, sizeof(value)) != PLATFORM_ERROR_SUCCESS) {
        return 0;  // Return 0 on error
    }
    return value;
}

uint64_t platform_random_uint64(void) {
    uint64_t value;
    if (platform_random_bytes(&value, sizeof(value)) != PLATFORM_ERROR_SUCCESS) {
        return 0;  // Return 0 on error
    }
    return value;
}

double platform_random_double(void) {
    uint64_t rand64 = platform_random_uint64();
    // Convert to double in range [0.0, 1.0)
    return (double)(rand64 >> 11) * (1.0 / (double)(1ULL << 53));
}

uint32_t platform_random_range(uint32_t min, uint32_t max) {
    if (min > max) {
        return min;  // Return min on error
    }

    uint32_t range = max - min + 1;
    uint32_t threshold = (0xFFFFFFFF - (0xFFFFFFFF % range));
    uint32_t rand_val;

    // Generate random numbers until we get one in a range that's evenly divisible
    // This eliminates modulo bias
    do {
        if (platform_random_bytes(&rand_val, sizeof(rand_val)) != PLATFORM_ERROR_SUCCESS) {
            return min;  // Return min on error
        }
    } while (rand_val >= threshold);

    return min + (rand_val % range);
}

PlatformErrorCode platform_random_self_test(void) {
    // Test initialization
    PlatformErrorCode result = platform_random_init();
    if (result != PLATFORM_ERROR_SUCCESS) {
        return result;
    }

    // Test basic random generation
    uint32_t test_value;
    result = platform_random_bytes(&test_value, sizeof(test_value));
    if (result != PLATFORM_ERROR_SUCCESS) {
        platform_random_cleanup();
        return result;
    }

    // Test distribution (very basic)
    uint32_t zeros = 0;
    uint32_t ones = 0;
    for (int i = 0; i < 1000; i++) {
        uint32_t bit = platform_random_range(0, 1);
        if (bit == 0) zeros++;
        else ones++;
    }

    // Check if distribution is reasonably balanced
    // (should be roughly 50/50 with some margin for randomness)
    if (zeros < 400 || zeros > 600) {
        platform_random_cleanup();
        return PLATFORM_ERROR_UNKNOWN;
    }

    platform_random_cleanup();
    return PLATFORM_ERROR_SUCCESS;
}

// Legacy compatibility functions
uint32_t platform_random(void) {
    return platform_random_uint32();
}

int random_bytes(void* buffer, size_t length) {
    return platform_random_bytes(buffer, length) == PLATFORM_ERROR_SUCCESS ? 0 : -1;
}
