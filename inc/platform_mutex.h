/** 
* @file platform_mutex.h
* @brief Platform-specific mutex and condition variable functions.
*/
#ifndef PLATFORM_MUTEX_H
#define PLATFORM_MUTEX_H

#ifdef __cplusplus
extern "C" {
#endif

/*
* Define fixed storage sizes for the opaque types.
* These sizes should be large enough to hold the underlying platform-specific
* synchronisation objects. For Windows and most POSIX systems (including macOS)
* 64 bytes is often sufficient.
*/
#ifndef PLATFORM_MUTEX_STORAGE_SIZE
#define PLATFORM_MUTEX_STORAGE_SIZE 64
#endif

#ifndef PLATFORM_COND_STORAGE_SIZE
#define PLATFORM_COND_STORAGE_SIZE 64
#endif

/**
* @brief Opaque type for a mutex.
*
* Memory is reserved automatically for the underlying platform-specific data.
*/
typedef struct PlatformMutex {
    unsigned char opaque[PLATFORM_MUTEX_STORAGE_SIZE];
} PlatformMutex_T;

/**
* @brief Opaque type for a condition variable.
*
* Memory is reserved automatically for the underlying platform-specific data.
*/
typedef struct PlatformCondition {
    unsigned char opaque[PLATFORM_COND_STORAGE_SIZE];
} PlatformCondition_T;

/**
* @brief Initialises the given mutex.
*
* @param mutex Pointer to a PlatformMutex_T variable.
* @return int 0 on success, -1 on failure.
*/
int platform_mutex_init(PlatformMutex_T* mutex);

/**
* @brief Locks the given mutex.
*
* @param mutex Pointer to the mutex.
* @return int 0 on success, -1 on failure.
*/
int platform_mutex_lock(PlatformMutex_T* mutex);

/**
* @brief Unlocks the given mutex.
*
* @param mutex Pointer to the mutex.
* @return int 0 on success, -1 on failure.
*/
int platform_mutex_unlock(PlatformMutex_T* mutex);

/**
* @brief Destroys the given mutex.
*
* @param mutex Pointer to the mutex.
* @return int 0 on success, -1 on failure.
*/
int platform_mutex_destroy(PlatformMutex_T* mutex);

/**
* @brief Initialises the given condition variable.
*
* @param cond Pointer to a PlatformCondition_T variable.
* @return int 0 on success, -1 on failure.
*/
int platform_cond_init(PlatformCondition_T* cond);

/**
* @brief Waits on the condition variable using the given mutex.
*
* @param cond Pointer to the condition variable.
* @param mutex Pointer to the mutex.
* @return int 0 on success, -1 on failure.
*/
int platform_cond_wait(PlatformCondition_T* cond, PlatformMutex_T* mutex);

/**
* @brief Signals the given condition variable.
*
* @param cond Pointer to the condition variable.
* @return int 0 on success, -1 on failure.
*/
int platform_cond_signal(PlatformCondition_T* cond);

/**
* @brief Destroys the given condition variable.
*
* @param cond Pointer to the condition variable.
* @return int 0 on success, -1 on failure.
*/
int platform_cond_destroy(PlatformCondition_T* cond);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_MUTEX_H */