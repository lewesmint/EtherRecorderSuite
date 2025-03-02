#ifndef PLATFORM_ATOMIC_H
#define PLATFORM_ATOMIC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Atomically compares and exchanges a value.
 *
 * @param dest Pointer to the destination value.
 * @param exchange Value to set if comparison succeeds.
 * @param comparand Value to compare against.
 * @return The original value at dest.
 */
long platform_atomic_compare_exchange(volatile long* dest, long exchange, long comparand);

/**
 * @brief Atomically exchanges a value.
 *
 * @param dest Pointer to the destination value.
 * @param exchange Value to set.
 * @return The original value at dest.
 */
long platform_atomic_exchange(volatile long* dest, long exchange);

/**
 * @brief Atomically increments a value.
 *
 * @param dest Pointer to the destination value.
 * @return The incremented value.
 */
long platform_atomic_increment(volatile long* dest);

/**
 * @brief Atomically decrements a value.
 *
 * @param dest Pointer to the destination value.
 * @return The decremented value.
 */
long platform_atomic_decrement(volatile long* dest);

#ifdef __cplusplus
}
#endif
#endif /* PLATFORM_ATOMIC_H */
