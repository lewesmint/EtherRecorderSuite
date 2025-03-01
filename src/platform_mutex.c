#include "platform_mutex.h"
#include "platform_atomic.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <time.h>
#include <errno.h>
#endif

/*
 * Platform-specific mutex implementation.
 */

#ifdef _WIN32
/*
 * Detect MSVC via _MSC_VER.
 */
#if defined(_MSC_VER)
    #define STATIC_ASSERT(cond, msg) typedef char static_assertion_##msg[(cond) ? 1 : -1]
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
    #define STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
    #define STATIC_ASSERT(cond, msg) typedef char static_assertion_##msg[(cond) ? 1 : -1]
#endif

/*
 * Ensure storage sizes are adequate
 */
STATIC_ASSERT(sizeof(CRITICAL_SECTION) <= PLATFORM_MUTEX_STORAGE_SIZE,
              CRITICAL_SECTION_storage_size_too_small);
STATIC_ASSERT(sizeof(CONDITION_VARIABLE) <= PLATFORM_COND_STORAGE_SIZE,
              CONDITION_VARIABLE_storage_size_too_small);

/*
 * Helper functions
 */
static CRITICAL_SECTION *win_mutex(PlatformMutex_T *mutex) {
    return (CRITICAL_SECTION *)mutex->opaque;
}

static CONDITION_VARIABLE *win_cond(PlatformCondition_T *cond) {
    return (CONDITION_VARIABLE *)cond->opaque;
}

/* Windows implementation functions */
int platform_mutex_init(PlatformMutex_T *mutex) {
    if (!mutex)
        return -1;
    __try {
        InitializeCriticalSection(win_mutex(mutex));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
    return 0;
}

int platform_mutex_lock(PlatformMutex_T *mutex) {
    if (!mutex)
        return -1;
    EnterCriticalSection(win_mutex(mutex));
    return 0;
}

int platform_mutex_unlock(PlatformMutex_T *mutex) {
    if (!mutex)
        return -1;
    LeaveCriticalSection(win_mutex(mutex));
    return 0;
}

int platform_mutex_destroy(PlatformMutex_T *mutex) {
    if (!mutex)
        return -1;
    DeleteCriticalSection(win_mutex(mutex));
    return 0;
}

int platform_cond_init(PlatformCondition_T *cond) {
    if (!cond)
        return -1;
    InitializeConditionVariable(win_cond(cond));
    return 0;
}

int platform_cond_wait(PlatformCondition_T *cond, PlatformMutex_T *mutex) {
    if (!cond || !mutex)
        return -1;
    return SleepConditionVariableCS(win_cond(cond), win_mutex(mutex), INFINITE) ? 0 : -1;
}

int platform_cond_signal(PlatformCondition_T *cond) {
    if (!cond)
        return -1;
    WakeConditionVariable(win_cond(cond));
    return 0;
}

int platform_cond_broadcast(PlatformCondition_T *cond) {
    if (!cond)
        return -1;
    WakeAllConditionVariable(win_cond(cond));
    return 0;
}

int platform_cond_timedwait(PlatformCondition_T *cond, PlatformMutex_T *mutex, uint32_t timeout_ms) {
    if (!cond || !mutex)
        return PLATFORM_WAIT_ERROR;
    
    BOOL result = SleepConditionVariableCS(win_cond(cond), win_mutex(mutex), timeout_ms);
    if (result) {
        return PLATFORM_WAIT_SUCCESS;
    } else if (GetLastError() == ERROR_TIMEOUT) {
        return PLATFORM_WAIT_TIMEOUT;
    } else {
        return PLATFORM_WAIT_ERROR;
    }
}

int platform_cond_destroy(PlatformCondition_T *cond) {
    /* Windows condition variables do not require explicit destruction. */
    return (cond ? 0 : -1);
}

bool platform_event_create(PlatformEvent_T* event, bool manual_reset, bool initial_state) {
    if (!event) {
        return false;
    }
    
#ifdef _WIN32
    *event = CreateEventA(NULL, manual_reset, initial_state, NULL);
    if (*event == NULL) {
        return false;
    }
#else
    // Initialize mutex
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
    
    if (pthread_mutex_init(&event->mutex, &mutex_attr) != 0) {
        pthread_mutexattr_destroy(&mutex_attr);
        return false;
    }
    
    pthread_mutexattr_destroy(&mutex_attr);
    
    // Initialize condition variable
    if (pthread_cond_init(&event->cond, NULL) != 0) {
        pthread_mutex_destroy(&event->mutex);
        return false;
    }
    
    event->signaled = initial_state;
    event->manual_reset = manual_reset;
#endif
    
    return true;
}

void platform_event_destroy(PlatformEvent_T* event) {
    if (!event) {
        return;
    }
    
#ifdef _WIN32
    if (*event) {
        CloseHandle(*event);
        *event = NULL;
    }
#else
    pthread_cond_destroy(&event->cond);
    pthread_mutex_destroy(&event->mutex);
#endif
}

bool platform_event_set(PlatformEvent_T* event) {
    if (!event) {
        return false;
    }
    
#ifdef _WIN32
    if (!*event) {
        return false;
    }
    
    return SetEvent(*event) != 0;
#else
    pthread_mutex_lock(&event->mutex);
    event->signaled = true;
    
    if (event->manual_reset) {
        pthread_cond_broadcast(&event->cond);
    } else {
        pthread_cond_signal(&event->cond);
    }
    
    pthread_mutex_unlock(&event->mutex);
    return true;
#endif
}

bool platform_event_reset(PlatformEvent_T* event) {
    if (!event) {
        return false;
    }
    
#ifdef _WIN32
    if (!*event) {
        return false;
    }
    
    return ResetEvent(*event) != 0;
#else
    pthread_mutex_lock(&event->mutex);
    event->signaled = false;
    pthread_mutex_unlock(&event->mutex);
    
    return true;
#endif
}

int platform_event_wait(PlatformEvent_T* event, uint32_t timeout_ms) {
    if (!event) {
        return PLATFORM_WAIT_ERROR;
    }
    
#ifdef _WIN32
    if (!*event) {
        return PLATFORM_WAIT_ERROR;
    }
    
    DWORD result = WaitForSingleObject(*event, timeout_ms);
    switch (result) {
        case WAIT_OBJECT_0:
            return PLATFORM_WAIT_SUCCESS;
        case WAIT_TIMEOUT:
            return PLATFORM_WAIT_TIMEOUT;
        default:
            return PLATFORM_WAIT_ERROR;
    }
#else
    pthread_mutex_lock(&event->mutex);
    
    int result = PLATFORM_WAIT_ERROR;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    
    while (!event->signaled) {
        if (timeout_ms == PLATFORM_WAIT_INFINITE) {
            pthread_cond_wait(&event->cond, &event->mutex);
        } else {
            if (pthread_cond_timedwait(&event->cond, &event->mutex, &ts) == ETIMEDOUT) {
                result = PLATFORM_WAIT_TIMEOUT;
                break;
            }
        }
    }
    
    if (event->signaled) {
        if (!event->manual_reset) {
            event->signaled = false;
        }
        result = PLATFORM_WAIT_SUCCESS;
    }
    
    pthread_mutex_unlock(&event->mutex);
    return result;
#endif
}

#else // !_WIN32
/* ---------------------- POSIX Implementation ---------------------- */

/* For POSIX systems (Linux, macOS, etc.), we can rely on _Static_assert. */
_Static_assert(sizeof(pthread_mutex_t) <= PLATFORM_MUTEX_STORAGE_SIZE,
               "PLATFORM_MUTEX_STORAGE_SIZE is too small for pthread_mutex_t");
_Static_assert(sizeof(pthread_cond_t) <= PLATFORM_COND_STORAGE_SIZE,
               "PLATFORM_COND_STORAGE_SIZE is too small for pthread_cond_t");

/*
 * Helper functions to obtain pointers to the underlying POSIX objects.
 */
static pthread_mutex_t *posix_mutex(PlatformMutex_T *mutex) {
    return (pthread_mutex_t *)mutex->opaque;
}

static pthread_cond_t *posix_cond(PlatformCondition_T *cond) {
    return (pthread_cond_t *)cond->opaque;
}

int platform_mutex_init(PlatformMutex_T *mutex) {
    if (!mutex)
        return -1;
    return pthread_mutex_init(posix_mutex(mutex), NULL);
}

int platform_mutex_lock(PlatformMutex_T *mutex) {
    if (!mutex)
        return -1;
    return pthread_mutex_lock(posix_mutex(mutex));
}

int platform_mutex_unlock(PlatformMutex_T *mutex) {
    if (!mutex)
        return -1;
    return pthread_mutex_unlock(posix_mutex(mutex));
}

int platform_mutex_destroy(PlatformMutex_T *mutex) {
    if (!mutex)
        return -1;
    return pthread_mutex_destroy(posix_mutex(mutex));
}

int platform_cond_init(PlatformCondition_T *cond) {
    if (!cond)
        return -1;
    return pthread_cond_init(posix_cond(cond), NULL);
}

int platform_cond_wait(PlatformCondition_T *cond, PlatformMutex_T *mutex) {
    if (!cond || !mutex)
        return -1;
    return pthread_cond_wait(posix_cond(cond), posix_mutex(mutex));
}

int platform_cond_signal(PlatformCondition_T *cond) {
    if (!cond)
        return -1;
    return pthread_cond_signal(posix_cond(cond));
}

int platform_cond_broadcast(PlatformCondition_T *cond) {
    if (!cond)
        return -1;
    return pthread_cond_broadcast(posix_cond(cond));
}

int platform_cond_timedwait(PlatformCondition_T *cond, PlatformMutex_T *mutex, uint32_t timeout_ms) {
    if (!cond || !mutex)
        return PLATFORM_WAIT_ERROR;
    
    struct timespec ts;
    if (timeout_ms != PLATFORM_WAIT_INFINITE) {
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000;
        // Handle nanosecond overflow
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000;
        }
        
        int result = pthread_cond_timedwait(posix_cond(cond), posix_mutex(mutex), &ts);
        if (result == 0) {
            return PLATFORM_WAIT_SUCCESS;
        } else if (result == ETIMEDOUT) {
            return PLATFORM_WAIT_TIMEOUT;
        } else {
            return PLATFORM_WAIT_ERROR;
        }
    } else {
        int result = pthread_cond_wait(posix_cond(cond), posix_mutex(mutex));
        return (result == 0) ? PLATFORM_WAIT_SUCCESS : PLATFORM_WAIT_ERROR;
    }
}

int platform_cond_destroy(PlatformCondition_T *cond) {
    if (!cond)
        return -1;
    return pthread_cond_destroy(posix_cond(cond));
}

bool platform_event_create(PlatformEvent_T* event, bool manual_reset, bool initial_state) {
    if (!event) {
        return false;
    }
    
#ifdef _WIN32
    *event = CreateEventA(NULL, manual_reset, initial_state, NULL);
    if (*event == NULL) {
        return false;
    }
#else
    // Initialize mutex
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
    
    if (pthread_mutex_init(&event->mutex, &mutex_attr) != 0) {
        pthread_mutexattr_destroy(&mutex_attr);
        return false;
    }
    
    pthread_mutexattr_destroy(&mutex_attr);
    
    // Initialize condition variable
    if (pthread_cond_init(&event->cond, NULL) != 0) {
        pthread_mutex_destroy(&event->mutex);
        return false;
    }
    
    event->signaled = initial_state;
    event->manual_reset = manual_reset;
#endif
    
    return true;
}

void platform_event_destroy(PlatformEvent_T* event) {
    if (!event) {
        return;
    }
    
#ifdef _WIN32
    if (*event) {
        CloseHandle(*event);
        *event = NULL;
    }
#else
    pthread_cond_destroy(&event->cond);
    pthread_mutex_destroy(&event->mutex);
#endif
}

bool platform_event_set(PlatformEvent_T* event) {
    if (!event) {
        return false;
    }
    
#ifdef _WIN32
    if (!*event) {
        return false;
    }
    
    return SetEvent(*event) != 0;
#else
    pthread_mutex_lock(&event->mutex);
    event->signaled = true;
    
    if (event->manual_reset) {
        pthread_cond_broadcast(&event->cond);
    } else {
        pthread_cond_signal(&event->cond);
    }
    
    pthread_mutex_unlock(&event->mutex);
    return true;
#endif
}

bool platform_event_reset(PlatformEvent_T* event) {
    if (!event) {
        return false;
    }
    
#ifdef _WIN32
    if (!*event) {
        return false;
    }
    
    return ResetEvent(*event) != 0;
#else
    pthread_mutex_lock(&event->mutex);
    event->signaled = false;
    pthread_mutex_unlock(&event->mutex);
    
    return true;
#endif
}

int platform_event_wait(PlatformEvent_T* event, uint32_t timeout_ms) {
    if (!event) {
        return PLATFORM_WAIT_ERROR;
    }
    
#ifdef _WIN32
    if (!*event) {
        return PLATFORM_WAIT_ERROR;
    }
    
    DWORD result = WaitForSingleObject(*event, timeout_ms);
    switch (result) {
        case WAIT_OBJECT_0:
            return PLATFORM_WAIT_SUCCESS;
        case WAIT_TIMEOUT:
            return PLATFORM_WAIT_TIMEOUT;
        default:
            return PLATFORM_WAIT_ERROR;
    }
#else
    pthread_mutex_lock(&event->mutex);
    
    int result = PLATFORM_WAIT_ERROR;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    
    while (!event->signaled) {
        if (timeout_ms == PLATFORM_WAIT_INFINITE) {
            pthread_cond_wait(&event->cond, &event->mutex);
        } else {
            if (pthread_cond_timedwait(&event->cond, &event->mutex, &ts) == ETIMEDOUT) {
                result = PLATFORM_WAIT_TIMEOUT;
                break;
            }
        }
    }
    
    if (event->signaled) {
        if (!event->manual_reset) {
            event->signaled = false;
        }
        result = PLATFORM_WAIT_SUCCESS;
    }
    
    pthread_mutex_unlock(&event->mutex);
    return result;
#endif
}

#endif // _WIN32
