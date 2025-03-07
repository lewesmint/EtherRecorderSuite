/**
 * @file platform_atomic.c
 * @brief POSIX implementation of platform atomic operations using C11 atomics
 */
#include "platform_atomic.h"

#include <stdatomic.h>

void platform_atomic_init_int32(PlatformAtomicInt32* atomic, int32_t value) {
    atomic_init((atomic_int*)&atomic->value, value);
}

void platform_atomic_init_uint32(PlatformAtomicUInt32* atomic, uint32_t value) {
    atomic_init((atomic_uint*)&atomic->value, value);
}

void platform_atomic_init_int64(PlatformAtomicInt64* atomic, int64_t value) {
    atomic_init((atomic_long*)&atomic->value, value);
}

void platform_atomic_init_uint64(PlatformAtomicUInt64* atomic, uint64_t value) {
    atomic_init((atomic_ulong*)&atomic->value, value);
}

void platform_atomic_init_bool(PlatformAtomicBool* atomic, bool value) {
    atomic_init((atomic_bool*)&atomic->value, value);
}

void platform_atomic_init_ptr(PlatformAtomicPtr* atomic, void* value) {
    atomic_init((atomic_uintptr_t*)&atomic->value, (uintptr_t)value);
}

// Store operations
void platform_atomic_store_int32(PlatformAtomicInt32* atomic, int32_t value, PlatformMemoryOrder order) {
    atomic_store_explicit((atomic_int*)&atomic->value, value, order);
}

void platform_atomic_store_uint32(PlatformAtomicUInt32* atomic, uint32_t value, PlatformMemoryOrder order) {
    atomic_store_explicit((atomic_uint*)&atomic->value, value, order);
}

void platform_atomic_store_int64(PlatformAtomicInt64* atomic, int64_t value, PlatformMemoryOrder order) {
    atomic_store_explicit((atomic_long*)&atomic->value, value, order);
}

void platform_atomic_store_uint64(PlatformAtomicUInt64* atomic, uint64_t value, PlatformMemoryOrder order) {
    atomic_store_explicit((atomic_ulong*)&atomic->value, value, order);
}

void platform_atomic_store_bool(PlatformAtomicBool* atomic, bool value, PlatformMemoryOrder order) {
    atomic_store_explicit((atomic_bool*)&atomic->value, value, order);
}

void platform_atomic_store_ptr(PlatformAtomicPtr* atomic, void* value, PlatformMemoryOrder order) {
    atomic_store_explicit((atomic_uintptr_t*)&atomic->value, (uintptr_t)value, order);
}

// Load operations
int32_t platform_atomic_load_int32(const PlatformAtomicInt32* atomic, PlatformMemoryOrder order) {
    return atomic_load_explicit((atomic_int*)&atomic->value, order);
}

uint32_t platform_atomic_load_uint32(const PlatformAtomicUInt32* atomic, PlatformMemoryOrder order) {
    return atomic_load_explicit((atomic_uint*)&atomic->value, order);
}

int64_t platform_atomic_load_int64(const PlatformAtomicInt64* atomic, PlatformMemoryOrder order) {
    return atomic_load_explicit((atomic_long*)&atomic->value, order);
}

uint64_t platform_atomic_load_uint64(const PlatformAtomicUInt64* atomic, PlatformMemoryOrder order) {
    return atomic_load_explicit((atomic_ulong*)&atomic->value, order);
}

bool platform_atomic_load_bool(const PlatformAtomicBool* atomic, PlatformMemoryOrder order) {
    return atomic_load_explicit((atomic_bool*)&atomic->value, order);
}

void* platform_atomic_load_ptr(const PlatformAtomicPtr* atomic, PlatformMemoryOrder order) {
    return (void*)atomic_load_explicit((atomic_uintptr_t*)&atomic->value, order);
}

// Exchange operations
int32_t platform_atomic_exchange_int32(PlatformAtomicInt32* atomic, int32_t value, PlatformMemoryOrder order) {
    return atomic_exchange_explicit((atomic_int*)&atomic->value, value, order);
}

uint32_t platform_atomic_exchange_uint32(PlatformAtomicUInt32* atomic, uint32_t value, PlatformMemoryOrder order) {
    return atomic_exchange_explicit((atomic_uint*)&atomic->value, value, order);
}

int64_t platform_atomic_exchange_int64(PlatformAtomicInt64* atomic, int64_t value, PlatformMemoryOrder order) {
    return atomic_exchange_explicit((atomic_long*)&atomic->value, value, order);
}

uint64_t platform_atomic_exchange_uint64(PlatformAtomicUInt64* atomic, uint64_t value, PlatformMemoryOrder order) {
    return atomic_exchange_explicit((atomic_ulong*)&atomic->value, value, order);
}

bool platform_atomic_exchange_bool(PlatformAtomicBool* atomic, bool value, PlatformMemoryOrder order) {
    return atomic_exchange_explicit((atomic_bool*)&atomic->value, value, order);
}

void* platform_atomic_exchange_ptr(PlatformAtomicPtr* atomic, void* value, PlatformMemoryOrder order) {
    return (void*)atomic_exchange_explicit((atomic_uintptr_t*)&atomic->value, (uintptr_t)value, order);
}

// Compare-exchange operations
bool platform_atomic_compare_exchange_int32(PlatformAtomicInt32* atomic, int32_t* expected, int32_t desired,
                                          PlatformMemoryOrder success_order, PlatformMemoryOrder failure_order) {
    return atomic_compare_exchange_strong_explicit(
        (atomic_int*)&atomic->value, expected, desired, success_order, failure_order);
}

bool platform_atomic_compare_exchange_uint32(PlatformAtomicUInt32* atomic, uint32_t* expected, uint32_t desired,
                                           PlatformMemoryOrder success_order, PlatformMemoryOrder failure_order) {
    return atomic_compare_exchange_strong_explicit(
        (atomic_uint*)&atomic->value, expected, desired, success_order, failure_order);
}

bool platform_atomic_compare_exchange_int64(PlatformAtomicInt64* atomic, long* expected, long desired,
                                          PlatformMemoryOrder success_order, PlatformMemoryOrder failure_order) {
    return atomic_compare_exchange_strong_explicit(
        (atomic_long*)&atomic->value, expected, desired, success_order, failure_order);
}

bool platform_atomic_compare_exchange_uint64(PlatformAtomicUInt64* atomic, unsigned long* expected, unsigned long desired,
                                           PlatformMemoryOrder success_order, PlatformMemoryOrder failure_order) {
    return atomic_compare_exchange_strong_explicit(
        (atomic_ulong*)&atomic->value, expected, desired, success_order, failure_order);
}

bool platform_atomic_compare_exchange_bool(PlatformAtomicBool* atomic, bool* expected, bool desired,
                                         PlatformMemoryOrder success_order, PlatformMemoryOrder failure_order) {
    return atomic_compare_exchange_strong_explicit(
        (atomic_bool*)&atomic->value, expected, desired, success_order, failure_order);
}

bool platform_atomic_compare_exchange_ptr(PlatformAtomicPtr* atomic, void** expected, void* desired,
                                        PlatformMemoryOrder success_order, PlatformMemoryOrder failure_order) {
    return atomic_compare_exchange_strong_explicit(
        (atomic_uintptr_t*)&atomic->value, (uintptr_t*)expected, (uintptr_t)desired,
        success_order, failure_order);
}

// Fetch-and-add operations
int32_t platform_atomic_fetch_add_int32(PlatformAtomicInt32* atomic, int32_t value, PlatformMemoryOrder order) {
    return atomic_fetch_add_explicit((atomic_int*)&atomic->value, value, order);
}

uint32_t platform_atomic_fetch_add_uint32(PlatformAtomicUInt32* atomic, uint32_t value, PlatformMemoryOrder order) {
    return atomic_fetch_add_explicit((atomic_uint*)&atomic->value, value, order);
}

int64_t platform_atomic_fetch_add_int64(PlatformAtomicInt64* atomic, int64_t value, PlatformMemoryOrder order) {
    return atomic_fetch_add_explicit((atomic_long*)&atomic->value, value, order);
}

uint64_t platform_atomic_fetch_add_uint64(PlatformAtomicUInt64* atomic, uint64_t value, PlatformMemoryOrder order) {
    return atomic_fetch_add_explicit((atomic_ulong*)&atomic->value, value, order);
}

void platform_atomic_thread_fence(PlatformMemoryOrder order) {
    atomic_thread_fence(order);
}
