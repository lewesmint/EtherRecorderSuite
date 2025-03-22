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
// 8-bit types padded to 32 bits
typedef union {
    int8_t value;
    int32_t _pad;
} PlatformAtomicInt8;

typedef union {
    uint8_t value;
    uint32_t _pad;
} PlatformAtomicUInt8;

// 16-bit types padded to 32 bits
typedef union {
    int16_t value;
    int32_t _pad;
} PlatformAtomicInt16;

typedef union {
    uint16_t value;
    uint32_t _pad;
} PlatformAtomicUInt16;

// 32-bit types (no padding needed)
typedef struct {
    int32_t value;
} PlatformAtomicInt32;

typedef struct {
    uint32_t value;
} PlatformAtomicUInt32;

// 64-bit types (no padding needed)
typedef struct {
    int64_t value;
} PlatformAtomicInt64;

typedef struct {
    uint64_t value;
} PlatformAtomicUInt64;

// Bool padded to 32 bits
typedef union {
    bool value;
    uint32_t _pad;
} PlatformAtomicBool;

// Pointer (architecture dependent size)
typedef struct {
    void* value;
} PlatformAtomicPtr;

/**
 * @brief Initialize atomic types
 */
// 8-bit initialization
void platform_atomic_init_int8(PlatformAtomicInt8* atomic, int8_t value);
void platform_atomic_init_uint8(PlatformAtomicUInt8* atomic, uint8_t value);

// 16-bit initialization
void platform_atomic_init_int16(PlatformAtomicInt16* atomic, int16_t value);
void platform_atomic_init_uint16(PlatformAtomicUInt16* atomic, uint16_t value);

// 32-bit initialization
void platform_atomic_init_int32(PlatformAtomicInt32* atomic, int32_t value);
void platform_atomic_init_uint32(PlatformAtomicUInt32* atomic, uint32_t value);

// 64-bit initialization
void platform_atomic_init_int64(PlatformAtomicInt64* atomic, int64_t value);
void platform_atomic_init_uint64(PlatformAtomicUInt64* atomic, uint64_t value);

// Bool initialization
void platform_atomic_init_bool(PlatformAtomicBool* atomic, bool value);

// Pointer initialization
void platform_atomic_init_ptr(PlatformAtomicPtr* atomic, void* value);

/**
 * @brief Store operations
 */
// 8-bit store
void platform_atomic_store_int8(PlatformAtomicInt8* atomic, int8_t value);
void platform_atomic_store_uint8(PlatformAtomicUInt8* atomic, uint8_t value);

// 16-bit store
void platform_atomic_store_int16(PlatformAtomicInt16* atomic, int16_t value);
void platform_atomic_store_uint16(PlatformAtomicUInt16* atomic, uint16_t value);

// 32-bit store
void platform_atomic_store_int32(PlatformAtomicInt32* atomic, int32_t value);
void platform_atomic_store_uint32(PlatformAtomicUInt32* atomic, uint32_t value);

// 64-bit store
void platform_atomic_store_int64(PlatformAtomicInt64* atomic, int64_t value);
void platform_atomic_store_uint64(PlatformAtomicUInt64* atomic, uint64_t value);

// Bool store
void platform_atomic_store_bool(PlatformAtomicBool* atomic, bool value);

// Pointer store
void platform_atomic_store_ptr(PlatformAtomicPtr* atomic, void* value);

/**
 * @brief Load operations
 */
// 8-bit load
int8_t  platform_atomic_load_int8(const PlatformAtomicInt8* atomic);
uint8_t platform_atomic_load_uint8(const PlatformAtomicUInt8* atomic);

// 16-bit load
int16_t  platform_atomic_load_int16(const PlatformAtomicInt16* atomic);
uint16_t platform_atomic_load_uint16(const PlatformAtomicUInt16* atomic);

// 32-bit load
int32_t  platform_atomic_load_int32(const PlatformAtomicInt32* atomic);
uint32_t platform_atomic_load_uint32(const PlatformAtomicUInt32* atomic);

// 64-bit load
int64_t  platform_atomic_load_int64(const PlatformAtomicInt64* atomic);
uint64_t platform_atomic_load_uint64(const PlatformAtomicUInt64* atomic);

// Bool load
bool     platform_atomic_load_bool(const PlatformAtomicBool* atomic);

// Pointer load
void*    platform_atomic_load_ptr(const PlatformAtomicPtr* atomic);

/**
 * @brief Exchange operations
 */
// 8-bit exchange
int8_t  platform_atomic_exchange_int8(PlatformAtomicInt8* atomic, int8_t value);
uint8_t platform_atomic_exchange_uint8(PlatformAtomicUInt8* atomic, uint8_t value);

// 16-bit exchange
int16_t  platform_atomic_exchange_int16(PlatformAtomicInt16* atomic, int16_t value);
uint16_t platform_atomic_exchange_uint16(PlatformAtomicUInt16* atomic, uint16_t value);

// 32-bit exchange
int32_t  platform_atomic_exchange_int32(PlatformAtomicInt32* atomic, int32_t value);
uint32_t platform_atomic_exchange_uint32(PlatformAtomicUInt32* atomic, uint32_t value);

// 64-bit exchange
int64_t  platform_atomic_exchange_int64(PlatformAtomicInt64* atomic, int64_t value);
uint64_t platform_atomic_exchange_uint64(PlatformAtomicUInt64* atomic, uint64_t value);

// Bool exchange
bool     platform_atomic_exchange_bool(PlatformAtomicBool* atomic, bool value);

// Pointer exchange
void*    platform_atomic_exchange_ptr(PlatformAtomicPtr* atomic, void* value);

/**
 * @brief Compare-and-exchange operations
 */
// 8-bit compare-exchange
bool platform_atomic_compare_exchange_int8(PlatformAtomicInt8* atomic, int8_t* expected, int8_t desired);
bool platform_atomic_compare_exchange_uint8(PlatformAtomicUInt8* atomic, uint8_t* expected, uint8_t desired);

// 16-bit compare-exchange
bool platform_atomic_compare_exchange_int16(PlatformAtomicInt16* atomic, int16_t* expected, int16_t desired);
bool platform_atomic_compare_exchange_uint16(PlatformAtomicUInt16* atomic, uint16_t* expected, uint16_t desired);

// 32-bit compare-exchange
bool platform_atomic_compare_exchange_int32(PlatformAtomicInt32* atomic, int32_t* expected, int32_t desired);
bool platform_atomic_compare_exchange_uint32(PlatformAtomicUInt32* atomic, uint32_t* expected, uint32_t desired);

// 64-bit compare-exchange
bool platform_atomic_compare_exchange_int64(PlatformAtomicInt64* atomic, int64_t* expected, int64_t desired);
bool platform_atomic_compare_exchange_uint64(PlatformAtomicUInt64* atomic, uint64_t* expected, uint64_t desired);

// Bool compare-exchange
bool platform_atomic_compare_exchange_bool(PlatformAtomicBool* atomic, bool* expected, bool desired);

// Pointer compare-exchange
bool platform_atomic_compare_exchange_ptr(PlatformAtomicPtr* atomic, void** expected, void* desired);

/**
 * @brief Fetch-and-add operations
 */
// 8-bit fetch-add
int8_t  platform_atomic_fetch_add_int8(PlatformAtomicInt8* atomic, int8_t value);
uint8_t platform_atomic_fetch_add_uint8(PlatformAtomicUInt8* atomic, uint8_t value);

// 16-bit fetch-add
int16_t  platform_atomic_fetch_add_int16(PlatformAtomicInt16* atomic, int16_t value);
uint16_t platform_atomic_fetch_add_uint16(PlatformAtomicUInt16* atomic, uint16_t value);

// 32-bit fetch-add
int32_t  platform_atomic_fetch_add_int32(PlatformAtomicInt32* atomic, int32_t value);
uint32_t platform_atomic_fetch_add_uint32(PlatformAtomicUInt32* atomic, uint32_t value);

// 64-bit fetch-add
int64_t  platform_atomic_fetch_add_int64(PlatformAtomicInt64* atomic, int64_t value);
uint64_t platform_atomic_fetch_add_uint64(PlatformAtomicUInt64* atomic, uint64_t value);

/**
 * @brief Memory fence operation
 */
void platform_atomic_thread_fence(PlatformMemoryOrder order);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_ATOMIC_H
