/**
 * @file platform_random.c
 * @brief POSIX implementation of platform random number generation
 */
#include <platform_random.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/random.h>
#include <time.h>
#include <pthread.h>

#include <platform_error.h>

// Thread-local storage for random state
static __thread struct {
    int initialized;
    int urandom_fd;
} g_random_context = {0};

PlatformErrorCode platform_random_init(void) {
    if (g_random_context.initialized) {
        return PLATFORM_ERROR_SUCCESS;
    }

    // Try to open /dev/urandom with non-blocking flag
    g_random_context.urandom_fd = open("/dev/urandom", O_RDONLY | O_NONBLOCK);
    if (g_random_context.urandom_fd == -1) {
        return PLATFORM_ERROR_NOT_INITIALIZED;
    }

    g_random_context.initialized = 1;
    return PLATFORM_ERROR_SUCCESS;
}

void platform_random_cleanup(void) {
    if (g_random_context.initialized && g_random_context.urandom_fd != -1) {
        close(g_random_context.urandom_fd);
        g_random_context.urandom_fd = -1;
    }
    g_random_context.initialized = 0;
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

    // Try getrandom() first (most secure, non-blocking)
    #ifdef __linux__
    if (getrandom(buffer, length, GRND_NONBLOCK) == (ssize_t)length) {
        return PLATFORM_ERROR_SUCCESS;
    }
    #endif

    // Fallback to /dev/urandom
    if (read(g_random_context.urandom_fd, buffer, length) == (ssize_t)length) {
        return PLATFORM_ERROR_SUCCESS;
    }

    // Last resort: use high-resolution time mixed with process/thread IDs
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    pid_t pid = getpid();
    pthread_t tid = pthread_self();
    
    unsigned char* bytes = (unsigned char*)buffer;
    for (size_t i = 0; i < length; i++) {
        uint64_t entropy = (uint64_t)ts.tv_nsec + i;
        entropy ^= (uint64_t)pid << 32;
        entropy ^= (uint64_t)(uintptr_t)tid;
        entropy ^= (uint64_t)(uintptr_t)&entropy;
        
        bytes[i] = (unsigned char)(entropy ^ (entropy >> 32));
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
    uint32_t rand_val;
    if (platform_random_bytes(&rand_val, sizeof(rand_val)) != PLATFORM_ERROR_SUCCESS) {
        return min;  // Return min on error
    }

    return min + (rand_val % range);
}
