/**
 * @file platform_random.h
 * @brief Platform-agnostic cryptographically secure random number generation
 */
#ifndef PLATFORM_RANDOM_H
#define PLATFORM_RANDOM_H

#include <stdint.h>
#include <stddef.h>

#include "platform_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the random number generator
 * @return PlatformErrorCode indicating success or failure
 * @note Must be called before any other platform_random functions
 */
PlatformErrorCode platform_random_init(void);

/**
 * @brief Clean up the random number generator
 */
void platform_random_cleanup(void);

/**
 * @brief Generate cryptographically secure random bytes
 * @param[out] buffer Buffer to fill with random bytes
 * @param[in] length Number of bytes to generate
 * @return PlatformErrorCode indicating success or failure
 */
PlatformErrorCode platform_random_bytes(void* buffer, size_t length);

/**
 * @brief Generate a random 32-bit unsigned integer
 * @return A random uint32_t value
 */
uint32_t platform_random_uint32(void);

/**
 * @brief Generate a random 64-bit unsigned integer
 * @return A random uint64_t value
 */
uint64_t platform_random_uint64(void);

/**
 * @brief Generate a random number within a specified range [min, max]
 * @param[in] min Minimum value (inclusive)
 * @param[in] max Maximum value (inclusive)
 * @return A random number in the range [min, max]
 * @note Handles the modulo bias internally for uniform distribution
 */
uint32_t platform_random_range(uint32_t min, uint32_t max);

/**
 * @brief Generate a random double between 0.0 and 1.0
 * @return A random double in the range [0.0, 1.0)
 */
double platform_random_double(void);

/**
 * @brief Check if the random number generator is available and functioning
 * @return PlatformErrorCode indicating if RNG is working properly
 */
PlatformErrorCode platform_random_self_test(void);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_RANDOM_H
