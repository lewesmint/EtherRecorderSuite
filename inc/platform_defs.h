/**
 * @file platform_defs.h
 * @brief Common platform-agnostic definitions
 */
#ifndef PLATFORM_DEFS_H
#define PLATFORM_DEFS_H

#include <stdint.h>

// Platform-agnostic handle types
#ifdef _WIN32
    #include <windows.h>
    typedef HANDLE PlatformHandle_T;
    typedef HANDLE ThreadHandle_T;
    typedef DWORD ThreadId_T;
#else
    #include <pthread.h>
    typedef intptr_t PlatformHandle_T;
    typedef pthread_t ThreadHandle_T;
    typedef pthread_t ThreadId_T;
#endif

// Platform wait result codes
#define PLATFORM_WAIT_SUCCESS 0
#define PLATFORM_WAIT_TIMEOUT 1
#define PLATFORM_WAIT_ERROR -1
#define PLATFORM_WAIT_INFINITE ((uint32_t)-1)

#endif // PLATFORM_DEFS_H