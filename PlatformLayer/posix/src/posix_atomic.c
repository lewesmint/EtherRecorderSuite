/**
 * @file posix_atomic.c
 * @brief POSIX implementation of platform atomic operations using C11 atomics
 */

#ifndef _WIN32

#include <stdatomic.h>
#include "platform_atomic.h"

// Initialize operations
// 8-bit initialization
void platform_atomic_init_int8(PlatformAtomicInt8* atomic, int8_t value) {
    atomic_init((_Atomic int32_t*)&atomic->_pad, (int32_t)value);
}

void platform_atomic_init_uint8(PlatformAtomicUInt8* atomic, uint8_t value) {
    atomic_init((_Atomic uint32_t*)&atomic->_pad, (uint32_t)value);
}

// 16-bit initialization
void platform_atomic_init_int16(PlatformAtomicInt16* atomic, int16_t value) {
    atomic_init((_Atomic int32_t*)&atomic->_pad, (int32_t)value);
}

void platform_atomic_init_uint16(PlatformAtomicUInt16* atomic, uint16_t value) {
    atomic_init((_Atomic uint32_t*)&atomic->_pad, (uint32_t)value);
}

// 32-bit initialization (direct value access)
void platform_atomic_init_int32(PlatformAtomicInt32* atomic, int32_t value) {
    atomic_init((_Atomic int32_t*)&atomic->value, value);
}

void platform_atomic_init_uint32(PlatformAtomicUInt32* atomic, uint32_t value) {
    atomic_init((_Atomic uint32_t*)&atomic->value, value);
}

// 64-bit initialization (direct value access)
void platform_atomic_init_int64(PlatformAtomicInt64* atomic, int64_t value) {
    atomic_init((_Atomic int64_t*)&atomic->value, value);
}

void platform_atomic_init_uint64(PlatformAtomicUInt64* atomic, uint64_t value) {
    atomic_init((_Atomic uint64_t*)&atomic->value, value);
}

// Bool initialization
void platform_atomic_init_bool(PlatformAtomicBool* atomic, bool value) {
    atomic_init((_Atomic uint32_t*)&atomic->_pad, (uint32_t)value);
}

// Pointer initialization (direct value access)
void platform_atomic_init_ptr(PlatformAtomicPtr* atomic, void* value) {
    atomic_init((_Atomic void**)&atomic->value, value);
}

// Store operations
// 8-bit store
void platform_atomic_store_int8(PlatformAtomicInt8* atomic, int8_t value) {
    atomic_store((_Atomic int32_t*)&atomic->_pad, (int32_t)value);
}

void platform_atomic_store_uint8(PlatformAtomicUInt8* atomic, uint8_t value) {
    atomic_store((_Atomic uint32_t*)&atomic->_pad, (uint32_t)value);
}

// 16-bit store
void platform_atomic_store_int16(PlatformAtomicInt16* atomic, int16_t value) {
    atomic_store((_Atomic int32_t*)&atomic->_pad, (int32_t)value);
}

void platform_atomic_store_uint16(PlatformAtomicUInt16* atomic, uint16_t value) {
    atomic_store((_Atomic uint32_t*)&atomic->_pad, (uint32_t)value);
}

// 32-bit store (direct value access)
void platform_atomic_store_int32(PlatformAtomicInt32* atomic, int32_t value) {
    atomic_store((_Atomic int32_t*)&atomic->value, value);
}

void platform_atomic_store_uint32(PlatformAtomicUInt32* atomic, uint32_t value) {
    atomic_store((_Atomic uint32_t*)&atomic->value, value);
}

// Load operations
// 8-bit load
int8_t platform_atomic_load_int8(const PlatformAtomicInt8* atomic) {
    return (int8_t)atomic_load((_Atomic int32_t*)&atomic->_pad);
}

uint8_t platform_atomic_load_uint8(const PlatformAtomicUInt8* atomic) {
    return (uint8_t)atomic_load((_Atomic uint32_t*)&atomic->_pad);
}

// 16-bit load
int16_t platform_atomic_load_int16(const PlatformAtomicInt16* atomic) {
    return (int16_t)atomic_load((_Atomic int32_t*)&atomic->_pad);
}

uint16_t platform_atomic_load_uint16(const PlatformAtomicUInt16* atomic) {
    return (uint16_t)atomic_load((_Atomic uint32_t*)&atomic->_pad);
}

// 32-bit load
int32_t platform_atomic_load_int32(const PlatformAtomicInt32* atomic) {
    return atomic_load((_Atomic int32_t*)&atomic->value);
}

uint32_t platform_atomic_load_uint32(const PlatformAtomicUInt32* atomic) {
    return atomic_load((_Atomic uint32_t*)&atomic->value);
}

// 64-bit load
int64_t platform_atomic_load_int64(const PlatformAtomicInt64* atomic) {
    return atomic_load((_Atomic int64_t*)&atomic->value);
}

uint64_t platform_atomic_load_uint64(const PlatformAtomicUInt64* atomic) {
    return atomic_load((_Atomic uint64_t*)&atomic->value);
}

// Other load
bool platform_atomic_load_bool(const PlatformAtomicBool* atomic) {
    return (bool)atomic_load((_Atomic uint32_t*)&atomic->_pad);
}

void* platform_atomic_load_ptr(const PlatformAtomicPtr* atomic) {
    return atomic_load((_Atomic void**)&atomic->value);
}

// Exchange operations
// 8-bit exchange
int8_t platform_atomic_exchange_int8(PlatformAtomicInt8* atomic, int8_t value) {
    return atomic_exchange((_Atomic int8_t*)&atomic->value, value);
}

uint8_t platform_atomic_exchange_uint8(PlatformAtomicUInt8* atomic, uint8_t value) {
    return atomic_exchange((_Atomic uint8_t*)&atomic->value, value);
}

// 16-bit exchange
int16_t platform_atomic_exchange_int16(PlatformAtomicInt16* atomic, int16_t value) {
    return atomic_exchange((_Atomic int16_t*)&atomic->value, value);
}

uint16_t platform_atomic_exchange_uint16(PlatformAtomicUInt16* atomic, uint16_t value) {
    return atomic_exchange((_Atomic uint16_t*)&atomic->value, value);
}

// 32-bit exchange
int32_t platform_atomic_exchange_int32(PlatformAtomicInt32* atomic, int32_t value) {
    return atomic_exchange((_Atomic int32_t*)&atomic->value, value);
}

uint32_t platform_atomic_exchange_uint32(PlatformAtomicUInt32* atomic, uint32_t value) {
    return atomic_exchange((_Atomic uint32_t*)&atomic->value, value);
}

// 64-bit exchange
int64_t platform_atomic_exchange_int64(PlatformAtomicInt64* atomic, int64_t value) {
    return atomic_exchange((_Atomic int64_t*)&atomic->value, value);
}

uint64_t platform_atomic_exchange_uint64(PlatformAtomicUInt64* atomic, uint64_t value) {
    return atomic_exchange((_Atomic uint64_t*)&atomic->value, value);
}

// Other exchange
bool platform_atomic_exchange_bool(PlatformAtomicBool* atomic, bool value) {
    return atomic_exchange((_Atomic bool*)&atomic->value, value);
}

void* platform_atomic_exchange_ptr(PlatformAtomicPtr* atomic, void* value) {
    return (void*)atomic_exchange((_Atomic uintptr_t*)&atomic->value, (uintptr_t)value);
}

// Compare-exchange operations
// 8-bit compare-exchange
bool platform_atomic_compare_exchange_int8(PlatformAtomicInt8* atomic, int8_t* expected, int8_t desired) {
    int32_t exp = (int32_t)*expected;
    bool result = atomic_compare_exchange_strong(
        (_Atomic int32_t*)&atomic->_pad,
        &exp,
        (int32_t)desired
    );
    *expected = (int8_t)exp;
    return result;
}

bool platform_atomic_compare_exchange_uint8(PlatformAtomicUInt8* atomic, uint8_t* expected, uint8_t desired) {
    uint32_t exp = (uint32_t)*expected;
    bool result = atomic_compare_exchange_strong(
        (_Atomic uint32_t*)&atomic->_pad,
        &exp,
        (uint32_t)desired
    );
    *expected = (uint8_t)exp;
    return result;
}

// 16-bit compare-exchange
bool platform_atomic_compare_exchange_int16(PlatformAtomicInt16* atomic, int16_t* expected, int16_t desired) {
    if (!atomic || !expected) return false;
    return atomic_compare_exchange_strong((_Atomic int16_t*)&atomic->value, expected, desired);
}

bool platform_atomic_compare_exchange_uint16(PlatformAtomicUInt16* atomic, uint16_t* expected, uint16_t desired) {
    if (!atomic || !expected) return false;
    return atomic_compare_exchange_strong((_Atomic uint16_t*)&atomic->value, expected, desired);
}

// 32-bit compare-exchange
bool platform_atomic_compare_exchange_int32(PlatformAtomicInt32* atomic, int32_t* expected, int32_t desired) {
    if (!atomic || !expected) return false;
    return atomic_compare_exchange_strong((_Atomic int32_t*)&atomic->value, expected, desired);
}

bool platform_atomic_compare_exchange_uint32(PlatformAtomicUInt32* atomic, uint32_t* expected, uint32_t desired) {
    if (!atomic || !expected) return false;
    return atomic_compare_exchange_strong((_Atomic uint32_t*)&atomic->value, expected, desired);
}

// 64-bit compare-exchange
bool platform_atomic_compare_exchange_int64(PlatformAtomicInt64* atomic, int64_t* expected, int64_t desired) {
    if (!atomic || !expected) return false;
    return atomic_compare_exchange_strong((_Atomic int64_t*)&atomic->value, expected, desired);
}

bool platform_atomic_compare_exchange_uint64(PlatformAtomicUInt64* atomic, uint64_t* expected, uint64_t desired) {
    if (!atomic || !expected) return false;
    return atomic_compare_exchange_strong((_Atomic uint64_t*)&atomic->value, expected, desired);
}

// Bool operations
void platform_atomic_store_bool(PlatformAtomicBool* atomic, bool value) {
    atomic_store((_Atomic uint32_t*)&atomic->_pad, (uint32_t)value);
}

bool platform_atomic_load_bool(const PlatformAtomicBool* atomic) {
    return (bool)atomic_load((_Atomic uint32_t*)&atomic->_pad);
}

bool platform_atomic_compare_exchange_bool(PlatformAtomicBool* atomic, bool* expected, bool desired) {
    uint32_t exp = (uint32_t)*expected;
    bool result = atomic_compare_exchange_strong(
        (_Atomic uint32_t*)&atomic->_pad,
        &exp,
        (uint32_t)desired
    );
    *expected = (bool)exp;
    return result;
}

// Pointer operations (direct value access)
void platform_atomic_store_ptr(PlatformAtomicPtr* atomic, void* value) {
    atomic_store((_Atomic void**)&atomic->value, value);
}

void* platform_atomic_load_ptr(const PlatformAtomicPtr* atomic) {
    return atomic_load((_Atomic void**)&atomic->value);
}

bool platform_atomic_compare_exchange_ptr(PlatformAtomicPtr* atomic, void** expected, void* desired) {
    return atomic_compare_exchange_strong((_Atomic void**)&atomic->value, expected, desired);
}

// Fetch-add operations
// 8-bit fetch-add
int8_t platform_atomic_fetch_add_int8(PlatformAtomicInt8* atomic, int8_t value) {
    return atomic_fetch_add((_Atomic int8_t*)&atomic->value, value);
}

uint8_t platform_atomic_fetch_add_uint8(PlatformAtomicUInt8* atomic, uint8_t value) {
    return atomic_fetch_add((_Atomic uint8_t*)&atomic->value, value);
}

// 16-bit fetch-add
int16_t platform_atomic_fetch_add_int16(PlatformAtomicInt16* atomic, int16_t value) {
    return atomic_fetch_add((_Atomic int16_t*)&atomic->value, value);
}

uint16_t platform_atomic_fetch_add_uint16(PlatformAtomicUInt16* atomic, uint16_t value) {
    return atomic_fetch_add((_Atomic uint16_t*)&atomic->value, value);
}

// 32-bit fetch-add
int32_t platform_atomic_fetch_add_int32(PlatformAtomicInt32* atomic, int32_t value) {
    return atomic_fetch_add((_Atomic int32_t*)&atomic->value, value);
}

uint32_t platform_atomic_fetch_add_uint32(PlatformAtomicUInt32* atomic, uint32_t value) {
    return atomic_fetch_add((_Atomic uint32_t*)&atomic->value, value);
}

// 64-bit fetch-add
int64_t platform_atomic_fetch_add_int64(PlatformAtomicInt64* atomic, int64_t value) {
    return atomic_fetch_add((_Atomic int64_t*)&atomic->value, value);
}

uint64_t platform_atomic_fetch_add_uint64(PlatformAtomicUInt64* atomic, uint64_t value) {
    return atomic_fetch_add((_Atomic uint64_t*)&atomic->value, value);
}

// Memory fence operation
void platform_atomic_thread_fence(PlatformMemoryOrder order) {
    atomic_thread_fence(order);
}

#endif // !_WIN32
