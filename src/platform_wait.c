/**
 * @file platform_wait.c
 * @brief Implementation of platform-agnostic wait functions
 */

 #include "platform_wait.h"

 #ifdef _WIN32
 #include <windows.h>
 #else
 #include <pthread.h>
 #include <time.h>
 #include <errno.h>
 #include <unistd.h>
 #include <sys/time.h>
 #endif
 
 /**
  * @brief Translates platform-specific wait result to platform-agnostic wait result
  */
 int platform_translate_wait_result(int result, uint32_t object_count) {
#ifdef _WIN32
    // Windows: Translate WAIT_OBJECT_0, WAIT_TIMEOUT, etc.
    // Use uint32_t to match DWORD without using Windows-specific types
    if ((uint32_t)result >= (uint32_t)WAIT_OBJECT_0 && 
        (uint32_t)result < (uint32_t)(WAIT_OBJECT_0 + object_count)) {
        return PLATFORM_WAIT_SUCCESS;
    } else if (result == WAIT_TIMEOUT) {
        return PLATFORM_WAIT_TIMEOUT;
    } else {
        return PLATFORM_WAIT_ERROR;
    }
#else
    // POSIX: Usually returns 0 for success, ETIMEDOUT for timeout, and other values for errors
    if (result == 0) {
        return PLATFORM_WAIT_SUCCESS;
    } else if (result == ETIMEDOUT) {
        return PLATFORM_WAIT_TIMEOUT;
    } else {
        return PLATFORM_WAIT_ERROR;
    }
#endif
}
 
 /**
  * @brief Waits for a single object with timeout
  */
 int platform_wait_single(PlatformHandle_T handle, uint32_t timeout_ms) {
 #ifdef _WIN32
     DWORD result = WaitForSingleObject(handle, timeout_ms);
     return platform_translate_wait_result(result, 1);
 #else
     // POSIX implementation depends on the object type
     // This is a simplified version that assumes handle is a pthread mutex
     // For a real implementation, you'd need to check the object type
     
     pthread_mutex_t* mutex = (pthread_mutex_t*)handle;
     int result = 0;
     
     if (timeout_ms == PLATFORM_WAIT_INFINITE) {
         result = pthread_mutex_lock(mutex);
         if (result == 0) {
             pthread_mutex_unlock(mutex);
         }
     } else {
         struct timespec ts;
         struct timeval tv;
         gettimeofday(&tv, NULL);
         
         ts.tv_sec = tv.tv_sec + (timeout_ms / 1000);
         ts.tv_nsec = (tv.tv_usec + (timeout_ms % 1000) * 1000) * 1000;
         
         // Normalize the nanoseconds
         if (ts.tv_nsec >= 1000000000) {
             ts.tv_sec += 1;
             ts.tv_nsec -= 1000000000;
         }
         
         // Use trylock with a sleep loop to simulate timeout
         uint32_t start_time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
         uint32_t current_time;
         
         while (1) {
             result = pthread_mutex_trylock(mutex);
             if (result == 0) {
                 pthread_mutex_unlock(mutex);
                 break;
             } else if (result != EBUSY) {
                 break;
             }
             
             // Check timeout
             gettimeofday(&tv, NULL);
             current_time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
             if (current_time - start_time >= timeout_ms) {
                 result = ETIMEDOUT;
                 break;
             }
             
             // Sleep a small amount before trying again
             usleep(1000);
         }
     }
     
     return platform_translate_wait_result(result, 1);
 #endif
 }
 
 /**
  * @brief Waits for multiple objects with timeout
  */
 int platform_wait_multiple(PlatformHandle_T* handles, uint32_t count, bool wait_all, uint32_t timeout_ms) {
 #ifdef _WIN32
     DWORD result = WaitForMultipleObjects(count, handles, wait_all ? TRUE : FALSE, timeout_ms);
     return platform_translate_wait_result(result, count);
 #else
     // POSIX has no direct equivalent of WaitForMultipleObjects
     // This is a simplified implementation that uses a polling approach
     
     if (count == 0) {
         return PLATFORM_WAIT_SUCCESS;
     }
     
     struct timeval tv_start, tv_current;
     gettimeofday(&tv_start, NULL);
     uint32_t start_time = tv_start.tv_sec * 1000 + tv_start.tv_usec / 1000;
     uint32_t current_time;
     
     while (1) {
         uint32_t ready_count = 0;
         
         // Check each object
         for (uint32_t i = 0; i < count; i++) {
             int result = platform_wait_single(handles[i], 0);
             if (result == PLATFORM_WAIT_SUCCESS) {
                 ready_count++;
                 
                 // If we're not waiting for all, return on first success
                 if (!wait_all) {
                     return PLATFORM_WAIT_SUCCESS;
                 }
             } else if (result == PLATFORM_WAIT_ERROR) {
                 return PLATFORM_WAIT_ERROR;
             }
         }
         
         // If waiting for all and all are ready, return success
         if (wait_all && ready_count == count) {
             return PLATFORM_WAIT_SUCCESS;
         }
         
         // Check timeout if not infinite
         if (timeout_ms != PLATFORM_WAIT_INFINITE) {
             gettimeofday(&tv_current, NULL);
             current_time = tv_current.tv_sec * 1000 + tv_current.tv_usec / 1000;
             if (current_time - start_time >= timeout_ms) {
                 return PLATFORM_WAIT_TIMEOUT;
             }
         }
         
         // Sleep a small amount before trying again
         usleep(1000);
     }
 #endif
 }