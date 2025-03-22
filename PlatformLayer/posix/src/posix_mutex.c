/**
 * @file platform_mutex.c
 * @brief POSIX implementation of platform mutex, condition variable, and event primitives
 */
#include "platform_mutex.h"

#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

/* Verify storage sizes are adequate */
_Static_assert(sizeof(pthread_mutex_t) <= PLATFORM_MUTEX_STORAGE_SIZE,
               "PLATFORM_MUTEX_STORAGE_SIZE is too small for pthread_mutex_t");
_Static_assert(sizeof(pthread_cond_t) <= PLATFORM_COND_STORAGE_SIZE,
               "PLATFORM_COND_STORAGE_SIZE is too small for pthread_cond_t");

/* Helper functions to obtain pointers to the underlying POSIX objects */
static pthread_mutex_t* posix_mutex(PlatformMutex_T* mutex) {
    return (pthread_mutex_t*)mutex->opaque;
}

static pthread_cond_t* posix_cond(PlatformCondition_T* cond) {
    return (pthread_cond_t*)cond->opaque;
}

/* Event implementation structure */
struct platform_event {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool signaled;
    bool manual_reset;
};

PlatformErrorCode platform_mutex_init(PlatformMutex_T* mutex) {
    if (!mutex) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    pthread_mutexattr_t attr;
    int result = pthread_mutexattr_init(&attr);
    if (result != 0) {
        return PLATFORM_ERROR_MUTEX_INIT;
    }

    result = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    if (result != 0) {
        pthread_mutexattr_destroy(&attr);
        return PLATFORM_ERROR_MUTEX_INIT;
    }

    result = pthread_mutex_init(posix_mutex(mutex), &attr);
    pthread_mutexattr_destroy(&attr);

    return (result == 0) ? PLATFORM_ERROR_SUCCESS : PLATFORM_ERROR_MUTEX_INIT;
}

PlatformErrorCode platform_mutex_lock(PlatformMutex_T* mutex) {
    if (!mutex) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    return pthread_mutex_lock(posix_mutex(mutex)) == 0 ? 
           PLATFORM_ERROR_SUCCESS : PLATFORM_ERROR_MUTEX_LOCK;
}

PlatformErrorCode platform_mutex_unlock(PlatformMutex_T* mutex) {
    if (!mutex) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    return pthread_mutex_unlock(posix_mutex(mutex)) == 0 ? 
           PLATFORM_ERROR_SUCCESS : PLATFORM_ERROR_MUTEX_UNLOCK;
}

PlatformErrorCode platform_mutex_destroy(PlatformMutex_T* mutex) {
    if (!mutex) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    return pthread_mutex_destroy(posix_mutex(mutex)) == 0 ? 
           PLATFORM_ERROR_SUCCESS : PLATFORM_ERROR_MUTEX_UNLOCK;
}

PlatformErrorCode platform_cond_init(PlatformCondition_T* cond) {
    if (!cond) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    return pthread_cond_init(posix_cond(cond), NULL) == 0 ? 
           PLATFORM_ERROR_SUCCESS : PLATFORM_ERROR_CONDITION_INIT;
}

PlatformErrorCode platform_cond_wait(PlatformCondition_T* cond, PlatformMutex_T* mutex) {
    if (!cond || !mutex) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    return pthread_cond_wait(posix_cond(cond), posix_mutex(mutex)) == 0 ? 
           PLATFORM_ERROR_SUCCESS : PLATFORM_ERROR_CONDITION_WAIT;
}

PlatformErrorCode platform_cond_timedwait(PlatformCondition_T* cond, 
                                        PlatformMutex_T* mutex,
                                        uint32_t timeout_ms) {
    if (!cond || !mutex) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return PLATFORM_ERROR_UNKNOWN;
    }

    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000;
    }

    int result = pthread_cond_timedwait(posix_cond(cond), posix_mutex(mutex), &ts);
    if (result == 0) {
        return PLATFORM_ERROR_SUCCESS;
    }
    return (result == ETIMEDOUT) ? PLATFORM_ERROR_TIMEOUT : PLATFORM_ERROR_CONDITION_WAIT;
}

PlatformErrorCode platform_cond_signal(PlatformCondition_T* cond) {
    if (!cond) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    return pthread_cond_signal(posix_cond(cond)) == 0 ? 
           PLATFORM_ERROR_SUCCESS : PLATFORM_ERROR_CONDITION_SIGNAL;
}

PlatformErrorCode platform_cond_broadcast(PlatformCondition_T* cond) {
    if (!cond) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    return pthread_cond_broadcast(posix_cond(cond)) == 0 ? 
           PLATFORM_ERROR_SUCCESS : PLATFORM_ERROR_CONDITION_SIGNAL;
}

PlatformErrorCode platform_cond_destroy(PlatformCondition_T* cond) {
    if (!cond) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    return pthread_cond_destroy(posix_cond(cond)) == 0 ? 
           PLATFORM_ERROR_SUCCESS : PLATFORM_ERROR_CONDITION_SIGNAL;
}

int init_mutex(PlatformMutex_T *mutex) {
    return platform_mutex_init(mutex);
}

int lock_mutex(PlatformMutex_T *mutex) {
    return platform_mutex_lock(mutex);
}

int unlock_mutex(PlatformMutex_T *mutex) {
    return platform_mutex_unlock(mutex);
}

