/**
 * @file platform_defs.h
 * @brief Common platform-agnostic definitions and types
 * 
 * Provides platform-independent type definitions and constants used across
 * the application. Handles Windows and POSIX platform differences.
 */
#ifndef PLATFORM_DEFS_H
#define PLATFORM_DEFS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Platform-agnostic handle types
 */
#ifdef _WIN32
    #include <windows.h>
    typedef HANDLE PlatformHandle_T;    ///< Generic platform handle
    typedef HANDLE ThreadHandle_T;       ///< Thread handle
    typedef DWORD ThreadId_T;           ///< Thread identifier
#else
    #include <pthread.h>
    typedef intptr_t PlatformHandle_T;  ///< Generic platform handle
    typedef pthread_t ThreadHandle_T;    ///< Thread handle
    typedef pthread_t ThreadId_T;       ///< Thread identifier
#endif

/**
 * @brief Platform wait result codes
 */
#define PLATFORM_WAIT_SUCCESS  0      ///< Wait operation completed successfully
#define PLATFORM_WAIT_TIMEOUT  1      ///< Wait operation timed out
#define PLATFORM_WAIT_ERROR   -1      ///< Wait operation failed
#define PLATFORM_WAIT_INFINITE ((uint32_t)-1)  ///< Wait indefinitely

/**
 * @brief Platform-agnostic warning control macros
 */
#ifdef _MSC_VER
    #define DISABLE_FPRINTF_WARNING __pragma(warning(push)) __pragma(warning(disable:4996))
    #define ENABLE_FPRINTF_WARNING  __pragma(warning(pop))
#else
    #define DISABLE_FPRINTF_WARNING _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
    #define ENABLE_FPRINTF_WARNING  _Pragma("GCC diagnostic pop")
#endif

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_DEFS_H
