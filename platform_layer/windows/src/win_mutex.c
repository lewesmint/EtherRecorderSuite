/**
 * @file windows_mutex.c
 * @brief Windows implementation of platform mutex and condition variable primitives
 */
#include "platform_mutex.h"
#include <windows.h>
#include <stdlib.h>

/* Verify storage sizes are adequate */
_Static_assert(sizeof(CRITICAL_SECTION) <= PLATFORM_MUTEX_STORAGE_SIZE,
               "PLATFORM_MUTEX_STORAGE_SIZE is too small for CRITICAL_SECTION");
_Static_assert(sizeof(CONDITION_VARIABLE) <= PLATFORM_COND_STORAGE_SIZE,
               "PLATFORM_COND_STORAGE_SIZE is too small for CONDITION_VARIABLE");

/* Helper functions to obtain pointers to the underlying Windows objects */
static CRITICAL_SECTION* win_mutex(PlatformMutex_T* mutex) {
    return (CRITICAL_SECTION*)mutex->opaque;
}

static CONDITION_VARIABLE* win_cond(PlatformCondition_T* cond) {
    return (CONDITION_VARIABLE*)cond->opaque;
}

PlatformErrorCode platform_mutex_init(PlatformMutex_T* mutex) {
    if (!mutex) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }

    InitializeCriticalSection(win_mutex(mutex));
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_mutex_lock(PlatformMutex_T* mutex) {
    if (!mutex) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    EnterCriticalSection(win_mutex(mutex));
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_mutex_unlock(PlatformMutex_T* mutex) {
    if (!mutex) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    LeaveCriticalSection(win_mutex(mutex));
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_mutex_destroy(PlatformMutex_T* mutex) {
    if (!mutex) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    DeleteCriticalSection(win_mutex(mutex));
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_cond_init(PlatformCondition_T* cond) {
    if (!cond) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    InitializeConditionVariable(win_cond(cond));
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_cond_wait(PlatformCondition_T* cond, PlatformMutex_T* mutex) {
    if (!cond || !mutex) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    if (!SleepConditionVariableCS(win_cond(cond), win_mutex(mutex), INFINITE)) {
        return PLATFORM_ERROR_CONDITION_WAIT;
    }
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_cond_timedwait(PlatformCondition_T* cond, 
                                        PlatformMutex_T* mutex,
                                        uint32_t timeout_ms) {
    if (!cond || !mutex) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    if (!SleepConditionVariableCS(win_cond(cond), win_mutex(mutex), timeout_ms)) {
        DWORD error = GetLastError();
        return (error == ERROR_TIMEOUT) ? PLATFORM_ERROR_TIMEOUT : PLATFORM_ERROR_CONDITION_WAIT;
    }
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_cond_signal(PlatformCondition_T* cond) {
    if (!cond) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    WakeConditionVariable(win_cond(cond));
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_cond_broadcast(PlatformCondition_T* cond) {
    if (!cond) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    WakeAllConditionVariable(win_cond(cond));
    return PLATFORM_ERROR_SUCCESS;
}

PlatformErrorCode platform_cond_destroy(PlatformCondition_T* cond) {
    if (!cond) {
        return PLATFORM_ERROR_INVALID_ARGUMENT;
    }
    
    // Windows condition variables don't need explicit destruction
    return PLATFORM_ERROR_SUCCESS;
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