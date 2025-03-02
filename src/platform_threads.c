#include "platform_threads.h"
#include "platform_utils.h"
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#endif

int platform_thread_create(PlatformThread_T *thread, ThreadFunc_T func, void *arg) {
#ifdef _WIN32
    *thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0, NULL);
    return (*thread != NULL) ? 0 : -1;
#else
    return pthread_create(thread, NULL, func, arg);
#endif
}

int platform_thread_join(PlatformThread_T thread, void **retval) {
#ifdef _WIN32
    DWORD result = WaitForSingleObject(thread, INFINITE);
    if (result == WAIT_OBJECT_0) {
        if (retval) {
            DWORD exit_code;
            GetExitCodeThread(thread, &exit_code);
            *retval = (void*)(uintptr_t)exit_code;
        }
        return 0;
    }
    return -1;
#else
    return pthread_join(thread, retval);
#endif
}

int platform_thread_close(PlatformThread_T *thread) {
#ifdef _WIN32
    if (*thread != NULL) {
        CloseHandle(*thread);  // Use Windows API directly
        *thread = NULL;
        return 0;
    }
    return -1;
#else
    // In POSIX, pthread_join already releases resources
    *thread = 0;
    return 0;
#endif
}

int platform_thread_wait_single(PlatformThread_T thread, uint32_t timeout_ms) {
#ifdef _WIN32
    DWORD result = WaitForSingleObject(thread, timeout_ms);
    if (result == WAIT_OBJECT_0) {
        return PLATFORM_WAIT_SUCCESS;
    } else if (result == WAIT_TIMEOUT) {
        return PLATFORM_WAIT_TIMEOUT;
    }
    return PLATFORM_WAIT_ERROR;
#else
    if (timeout_ms == PLATFORM_WAIT_INFINITE) {
        if (pthread_join(thread, NULL) == 0) {
            return PLATFORM_WAIT_SUCCESS;
        }
        return PLATFORM_WAIT_ERROR;
    } else {
        struct timespec ts;
        struct timeval tv;
        gettimeofday(&tv, NULL);
        
        ts.tv_sec = tv.tv_sec + (timeout_ms / 1000);
        ts.tv_nsec = (tv.tv_usec + (timeout_ms % 1000) * 1000) * 1000;
        
        // Normalize nanoseconds
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000;
        }
        
        // Add conditional compilation:
#if defined(__GLIBC__) && defined(_GNU_SOURCE)
        int result = pthread_timedjoin_np(thread, NULL, &ts);
        if (result == 0) {
            return PLATFORM_WAIT_SUCCESS;
        } else if (result == ETIMEDOUT) {
            return PLATFORM_WAIT_TIMEOUT;
        }
        return PLATFORM_WAIT_ERROR;
#else
        // Fall back for macOS and other systems without GNU extensions
        struct timespec sleep_ts = {0, 1000000}; // Start with 1ms
        unsigned int backoff = 1; // For exponential backoff
        uint64_t deadline_us = ((uint64_t)ts.tv_sec * 1000000) + (ts.tv_nsec / 1000);

        while (1) {
            // Try to join with zero timeout
            int join_result = pthread_join(thread, NULL);
            if (join_result == 0) {
                return PLATFORM_WAIT_SUCCESS;
            } else if (join_result != EBUSY) {
                // Thread is likely already joined or invalid
                return PLATFORM_WAIT_ERROR;
            }
            
            // Check timeout
            struct timeval now;
            gettimeofday(&now, NULL);
            uint64_t now_us = ((uint64_t)now.tv_sec * 1000000) + now.tv_usec;
            
            if (now_us >= deadline_us) {
                return PLATFORM_WAIT_TIMEOUT;
            }
            
            // Adaptive sleep with exponential backoff (max 10ms)
            sleep_ts.tv_nsec = (1000000 * backoff);
            if (sleep_ts.tv_nsec > 10000000) sleep_ts.tv_nsec = 10000000;
            backoff *= 2;
            
            nanosleep(&sleep_ts, NULL);
        }
#endif
    }
#endif
}

int platform_thread_wait_multiple(PlatformThread_T *threads, uint32_t count, bool wait_all, uint32_t timeout_ms) {
#ifdef _WIN32
    DWORD result = WaitForMultipleObjects((DWORD)count, threads, wait_all ? TRUE : FALSE, timeout_ms);
    
    if (result >= WAIT_OBJECT_0 && result < WAIT_OBJECT_0 + count) {
        return PLATFORM_WAIT_SUCCESS;
    } else if (result == WAIT_TIMEOUT) {
        return PLATFORM_WAIT_TIMEOUT;
    }
    return PLATFORM_WAIT_ERROR;
#else
    // POSIX doesn't have a direct equivalent of WaitForMultipleObjects
    // We need to implement this manually
    if (count == 0) {
        return PLATFORM_WAIT_SUCCESS;
    }
    
    uint32_t time_left = timeout_ms;
    struct timespec start, end;
    
    if (timeout_ms != PLATFORM_WAIT_INFINITE) {
        clock_gettime(CLOCK_MONOTONIC, &start);
    }
    
    bool* thread_done = (bool*)calloc(count, sizeof(bool));
    if (!thread_done) {
        return PLATFORM_WAIT_ERROR;
    }
    
    size_t done_count = 0;
    
    while (done_count < (wait_all ? count : 1)) {
        if (timeout_ms != PLATFORM_WAIT_INFINITE && time_left == 0) {
            free(thread_done);
            return PLATFORM_WAIT_TIMEOUT;
        }
        
        for (size_t i = 0; i < count; i++) {
            if (!thread_done[i]) {
                int result = platform_thread_wait_single(threads[i], wait_all ? 0 : time_left);
                
                if (result == PLATFORM_WAIT_SUCCESS) {
                    thread_done[i] = true;
                    done_count++;
                    
                    if (!wait_all) {
                        free(thread_done);
                        return PLATFORM_WAIT_SUCCESS;
                    }
                } else if (!wait_all && result == PLATFORM_WAIT_TIMEOUT) {
                    free(thread_done);
                    return PLATFORM_WAIT_TIMEOUT;
                }
            }
        }
        
        // If we're waiting for all threads and not all are done, sleep briefly
        if (wait_all && done_count < count) {
            sleep_ms(10);  // Short sleep to avoid CPU spinning
            
            if (timeout_ms != PLATFORM_WAIT_INFINITE) {
                clock_gettime(CLOCK_MONOTONIC, &end);
                uint32_t elapsed = (end.tv_sec - start.tv_sec) * 1000 + 
                                  (end.tv_nsec - start.tv_nsec) / 1000000;
                
                if (elapsed >= time_left) {
                    free(thread_done);
                    return PLATFORM_WAIT_TIMEOUT;
                }
                
                time_left = timeout_ms - elapsed;
            }
        }
    }
    
    free(thread_done);
    return PLATFORM_WAIT_SUCCESS;
#endif
}

bool platform_thread_equal(PlatformThread_T thread1, PlatformThread_T thread2) {
#ifdef _WIN32
    return (thread1 == thread2);
#else
    return pthread_equal(thread1, thread2);
#endif
}

bool platform_thread_is_active(PlatformThread_T thread) {
#ifdef _WIN32
    if (thread == NULL) {
        return false;
    }
    
    DWORD exit_code;
    if (!GetExitCodeThread(thread, &exit_code)) {
        return false;
    }
    
    return (exit_code == STILL_ACTIVE);
#else
    // In POSIX, we can try to send signal 0 which doesn't actually send a signal
    // but checks if the thread exists and we have permission to signal it
    return (pthread_kill(thread, 0) == 0);
#endif
}
