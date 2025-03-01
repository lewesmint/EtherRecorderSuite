/**
 * @file platform_defs.h
 * @brief Common platform-agnostic definitions
 */
#ifndef PLATFORM_DEFS_H
#define PLATFORM_DEFS_H

// Platform wait result codes
#define PLATFORM_WAIT_SUCCESS 0
#define PLATFORM_WAIT_TIMEOUT 1
#define PLATFORM_WAIT_ERROR -1
#define PLATFORM_WAIT_INFINITE ((uint32_t)-1)

#endif // PLATFORM_DEFS_H