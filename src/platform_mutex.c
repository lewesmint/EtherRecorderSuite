#include "platform_mutex.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

/*
 * Platform-specific mutex implementation.
 */

#ifdef _WIN32
/*
 * Detect MSVC via _MSC_VER. Even if MSVC sets __STDC_VERSION__ to 201112L,
 * it may not correctly handle _Static_assert(cond, "string") in C mode.
 * We'll fallback to a negative array-size trick to force a compile-time error
 * if the condition is false.
 */
#if defined(_MSC_VER)
    #define STATIC_ASSERT(cond, msg) typedef char static_assertion_##msg[(cond) ? 1 : -1]
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
    #define STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else // Fallback to negative array-size trick
    #define STATIC_ASSERT(cond, msg) typedef char static_assertion_##msg[(cond) ? 1 : -1]
#endif // _MSC_VER

/*
 * Ensure the fixed-size buffers in platform_mutex.h are big enough to hold
 * CRITICAL_SECTION and CONDITION_VARIABLE.
 */
STATIC_ASSERT(sizeof(CRITICAL_SECTION) <= PLATFORM_MUTEX_STORAGE_SIZE,
              CRITICAL_SECTION_storage_size_too_small);
STATIC_ASSERT(sizeof(CONDITION_VARIABLE) <= PLATFORM_COND_STORAGE_SIZE,
              CONDITION_VARIABLE_storage_size_too_small);

/*
 * Helper functions to obtain pointers to the underlying Windows objects.
 */
static CRITICAL_SECTION *win_mutex(PlatformMutex_T *mutex) {
    return (CRITICAL_SECTION *)mutex->opaque;
}

static CONDITION_VARIABLE *win_cond(PlatformCondition_T *cond) {
    return (CONDITION_VARIABLE *)cond->opaque;
}

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

int platform_cond_destroy(PlatformCondition_T *cond) {
    /* Windows condition variables do not require explicit destruction. */
    return (cond ? 0 : -1);
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

int platform_cond_destroy(PlatformCondition_T *cond) {
    if (!cond)
        return -1;
    return pthread_cond_destroy(posix_cond(cond));
}

#endif // _WIN32
