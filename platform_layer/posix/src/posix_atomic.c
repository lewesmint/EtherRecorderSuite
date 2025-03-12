/**
 * @file posix_atomic.c
 * @brief POSIX implementation of platform atomic operations using C11 atomics
 */

#ifndef _WIN32

#include <stdatomic.h>
#include "platform_atomic.h"

// Initialize operations
void platform_atomic_init_int32(PlatformAtomicInt32* atomic, int32_t value) {
    atomic_init((_Atomic int32_t*)&atomic->value, value);
}

void platform_atomic_init_uint32(PlatformAtomicUInt32* atomic, uint32_t value) {
    atomic_init((_Atomic uint32_t*)&atomic->value, value);
}

void platform_atomic_init_int64(PlatformAtomicInt64* atomic, int64_t value) {
    atomic_init((_Atomic int64_t*)&atomic->value, value);
}

void platform_atomic_init_uint64(PlatformAtomicUInt64* atomic, uint64_t value) {
    atomic_init((_Atomic uint64_t*)&atomic->value, value);
}

void platform_atomic_init_bool(PlatformAtomicBool* atomic, bool value) {
    atomic_init((_Atomic bool*)&atomic->value, value);
}

void platform_atomic_init_ptr(PlatformAtomicPtr* atomic, void* value) {
    atomic_init((_Atomic uintptr_t*)&atomic->value, (uintptr_t)value);
}

// Load operations
int32_t platform_atomic_load_int32(const PlatformAtomicInt32* atomic) {
    return atomic_load((_Atomic int32_t*)&atomic->value);
}

uint32_t platform_atomic_load_uint32(const PlatformAtomicUInt32* atomic) {
    return atomic_load((_Atomic uint32_t*)&atomic->value);
}

int64_t platform_atomic_load_int64(const PlatformAtomicInt64* atomic) {
    return atomic_load((_Atomic int64_t*)&atomic->value);
}

uint64_t platform_atomic_load_uint64(const PlatformAtomicUInt64* atomic) {
    return atomic_load((_Atomic uint64_t*)&atomic->value);
}

bool platform_atomic_load_bool(const PlatformAtomicBool* atomic) {
    return atomic_load((_Atomic bool*)&atomic->value);
}

void* platform_atomic_load_ptr(const PlatformAtomicPtr* atomic) {
    return (void*)atomic_load((_Atomic uintptr_t*)&atomic->value);
}

// Exchange operations
int32_t platform_atomic_exchange_int32(PlatformAtomicInt32* atomic, int32_t value) {
    return atomic_exchange((_Atomic int32_t*)&atomic->value, value);
}

uint32_t platform_atomic_exchange_uint32(PlatformAtomicUInt32* atomic, uint32_t value) {
    return atomic_exchange((_Atomic uint32_t*)&atomic->value, value);
}

int64_t platform_atomic_exchange_int64(PlatformAtomicInt64* atomic, int64_t value) {
    return atomic_exchange((_Atomic int64_t*)&atomic->value, value);
}

uint64_t platform_atomic_exchange_uint64(PlatformAtomicUInt64* atomic, uint64_t value) {
    return atomic_exchange((_Atomic uint64_t*)&atomic->value, value);
}

bool platform_atomic_exchange_bool(PlatformAtomicBool* atomic, bool value) {
    return atomic_exchange((_Atomic bool*)&atomic->value, value);
}

void* platform_atomic_exchange_ptr(PlatformAtomicPtr* atomic, void* value) {
    return (void*)atomic_exchange((_Atomic uintptr_t*)&atomic->value, (uintptr_t)value);
}

// Compare-exchange operations
bool platform_atomic_compare_exchange_int32(PlatformAtomicInt32* atomic, int32_t* expected, int32_t desired) {
    if (!atomic || !expected) return false;
    return atomic_compare_exchange_strong((_Atomic int32_t*)&atomic->value, expected, desired);
}

bool platform_atomic_compare_exchange_uint32(PlatformAtomicUInt32* atomic, uint32_t* expected, uint32_t desired) {
    if (!atomic || !expected) return false;
    return atomic_compare_exchange_strong((_Atomic uint32_t*)&atomic->value, expected, desired);
}

bool platform_atomic_compare_exchange_int64(PlatformAtomicInt64* atomic, int64_t* expected, int64_t desired) {
    if (!atomic || !expected) return false;
    return atomic_compare_exchange_strong((_Atomic int64_t*)&atomic->value, expected, desired);
}

bool platform_atomic_compare_exchange_uint64(PlatformAtomicUInt64* atomic, uint64_t* expected, uint64_t desired) {
    if (!atomic || !expected) return false;
    return atomic_compare_exchange_strong((_Atomic uint64_t*)&atomic->value, expected, desired);
}

bool platform_atomic_compare_exchange_bool(PlatformAtomicBool* atomic, bool* expected, bool desired) {
    if (!atomic || !expected) return false;
    return atomic_compare_exchange_strong((_Atomic bool*)&atomic->value, expected, desired);
}

bool platform_atomic_compare_exchange_ptr(PlatformAtomicPtr* atomic, void** expected, void* desired) {
    if (!atomic || !expected) return false;
    return atomic_compare_exchange_strong(
        (_Atomic uintptr_t*)&atomic->value,
        (uintptr_t*)expected,
        (uintptr_t)desired
    );
}

// Fetch-add operations
int32_t platform_atomic_fetch_add_int32(PlatformAtomicInt32* atomic, int32_t value) {
    return atomic_fetch_add((_Atomic int32_t*)&atomic->value, value);
}

uint32_t platform_atomic_fetch_add_uint32(PlatformAtomicUInt32* atomic, uint32_t value) {
    return atomic_fetch_add((_Atomic uint32_t*)&atomic->value, value);
}

int64_t platform_atomic_fetch_add_int64(PlatformAtomicInt64* atomic, int64_t value) {
    return atomic_fetch_add((_Atomic int64_t*)&atomic->value, value);
}

uint64_t platform_atomic_fetch_add_uint64(PlatformAtomicUInt64* atomic, uint64_t value) {
    return atomic_fetch_add((_Atomic uint64_t*)&atomic->value, value);
}

// Thread fence operation
void platform_atomic_thread_fence(PlatformMemoryOrder order) {
    atomic_thread_fence(order);
}

#endif // !_WIN32
