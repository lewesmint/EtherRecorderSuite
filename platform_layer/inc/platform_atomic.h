/**
 * @file platform_atomic.h
 * @brief Platform-agnostic atomic operations interface
 */
#ifndef PLATFORM_ATOMIC_H
#define PLATFORM_ATOMIC_H

#include <stdint.h>
#include <stdbool.h>

#include "platform_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Memory ordering for atomic operations
 */
typedef enum {
    PLATFORM_MEMORY_ORDER_RELAXED,     ///< No synchronization/ordering constraints
    PLATFORM_MEMORY_ORDER_CONSUME,     ///< Data dependency ordering
    PLATFORM_MEMORY_ORDER_ACQUIRE,     ///< Acquire operation: no reads/writes can be reordered before
    PLATFORM_MEMORY_ORDER_RELEASE,     ///< Release operation: no reads/writes can be reordered after
    PLATFORM_MEMORY_ORDER_ACQ_REL,     ///< Both acquire and release barriers
    PLATFORM_MEMORY_ORDER_SEQ_CST      ///< Sequential consistency: total ordering
} PlatformMemoryOrder;

/**
 * @brief Atomic integer types
 */
typedef struct PlatformAtomicInt32  { int32_t  value; } PlatformAtomicInt32;
typedef struct PlatformAtomicUInt32 { uint32_t value; } PlatformAtomicUInt32;
typedef struct PlatformAtomicInt64  { int64_t  value; } PlatformAtomicInt64;
typedef struct PlatformAtomicUInt64 { uint64_t value; } PlatformAtomicUInt64;
typedef struct PlatformAtomicBool   { bool     value; } PlatformAtomicBool;
typedef struct PlatformAtomicPtr    { void*    value; } PlatformAtomicPtr;

/**
 * @brief Initialize atomic types
 */
void platform_atomic_init_int32(PlatformAtomicInt32* atomic, int32_t value);
void platform_atomic_init_uint32(PlatformAtomicUInt32* atomic, uint32_t value);
void platform_atomic_init_int64(PlatformAtomicInt64* atomic, int64_t value);
void platform_atomic_init_uint64(PlatformAtomicUInt64* atomic, uint64_t value);
void platform_atomic_init_bool(PlatformAtomicBool* atomic, bool value);
void platform_atomic_init_ptr(PlatformAtomicPtr* atomic, void* value);

/**
 * @brief Store operations
 */
void platform_atomic_store_int32(PlatformAtomicInt32* atomic, int32_t value, PlatformMemoryOrder order);
void platform_atomic_store_uint32(PlatformAtomicUInt32* atomic, uint32_t value, PlatformMemoryOrder order);
void platform_atomic_store_int64(PlatformAtomicInt64* atomic, int64_t value, PlatformMemoryOrder order);
void platform_atomic_store_uint64(PlatformAtomicUInt64* atomic, uint64_t value, PlatformMemoryOrder order);
void platform_atomic_store_bool(PlatformAtomicBool* atomic, bool value, PlatformMemoryOrder order);
void platform_atomic_store_ptr(PlatformAtomicPtr* atomic, void* value, PlatformMemoryOrder order);

/**
 * @brief Load operations
 */
int32_t  platform_atomic_load_int32(const PlatformAtomicInt32* atomic, PlatformMemoryOrder order);
uint32_t platform_atomic_load_uint32(const PlatformAtomicUInt32* atomic, PlatformMemoryOrder order);
int64_t  platform_atomic_load_int64(const PlatformAtomicInt64* atomic, PlatformMemoryOrder order);
uint64_t platform_atomic_load_uint64(const PlatformAtomicUInt64* atomic, PlatformMemoryOrder order);
bool     platform_atomic_load_bool(const PlatformAtomicBool* atomic, PlatformMemoryOrder order);
void*    platform_atomic_load_ptr(const PlatformAtomicPtr* atomic, PlatformMemoryOrder order);

/**
 * @brief Exchange operations
 */
int32_t  platform_atomic_exchange_int32(PlatformAtomicInt32* atomic, int32_t value, PlatformMemoryOrder order);
uint32_t platform_atomic_exchange_uint32(PlatformAtomicUInt32* atomic, uint32_t value, PlatformMemoryOrder order);
int64_t  platform_atomic_exchange_int64(PlatformAtomicInt64* atomic, int64_t value, PlatformMemoryOrder order);
uint64_t platform_atomic_exchange_uint64(PlatformAtomicUInt64* atomic, uint64_t value, PlatformMemoryOrder order);
bool     platform_atomic_exchange_bool(PlatformAtomicBool* atomic, bool value, PlatformMemoryOrder order);
void*    platform_atomic_exchange_ptr(PlatformAtomicPtr* atomic, void* value, PlatformMemoryOrder order);

/**
 * @brief Compare-and-exchange operations
 */
bool platform_atomic_compare_exchange_int32(PlatformAtomicInt32* atomic, int32_t* expected, int32_t desired, 
                                          PlatformMemoryOrder success_order, PlatformMemoryOrder failure_order);
bool platform_atomic_compare_exchange_uint32(PlatformAtomicUInt32* atomic, uint32_t* expected, uint32_t desired,
                                           PlatformMemoryOrder success_order, PlatformMemoryOrder failure_order);
bool platform_atomic_compare_exchange_int64(PlatformAtomicInt64* atomic, long* expected, long desired,
                                          PlatformMemoryOrder success_order, PlatformMemoryOrder failure_order);
bool platform_atomic_compare_exchange_uint64(PlatformAtomicUInt64* atomic, unsigned long* expected, unsigned long desired,
                                           PlatformMemoryOrder success_order, PlatformMemoryOrder failure_order);
bool platform_atomic_compare_exchange_bool(PlatformAtomicBool* atomic, bool* expected, bool desired,
                                         PlatformMemoryOrder success_order, PlatformMemoryOrder failure_order);
bool platform_atomic_compare_exchange_ptr(PlatformAtomicPtr* atomic, void** expected, void* desired,
                                        PlatformMemoryOrder success_order, PlatformMemoryOrder failure_order);

/**
 * @brief Fetch-and-add operations
 */
int32_t  platform_atomic_fetch_add_int32(PlatformAtomicInt32* atomic, int32_t value, PlatformMemoryOrder order);
uint32_t platform_atomic_fetch_add_uint32(PlatformAtomicUInt32* atomic, uint32_t value, PlatformMemoryOrder order);
int64_t  platform_atomic_fetch_add_int64(PlatformAtomicInt64* atomic, int64_t value, PlatformMemoryOrder order);
uint64_t platform_atomic_fetch_add_uint64(PlatformAtomicUInt64* atomic, uint64_t value, PlatformMemoryOrder order);

/**
 * @brief Memory fence operation
 */
void platform_atomic_thread_fence(PlatformMemoryOrder order);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_ATOMIC_H
