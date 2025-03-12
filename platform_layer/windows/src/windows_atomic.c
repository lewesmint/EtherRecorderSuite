/**
 * @file windows_atomic.c
 * @brief Windows implementation of platform atomic operations using Interlocked functions
 */

#ifdef _WIN32

#include "platform_atomic.h"

#include <windows.h>

// Initialize operations
void platform_atomic_init_int32(PlatformAtomicInt32* atomic, int32_t value) {
    atomic->value = value;
}

void platform_atomic_init_uint32(PlatformAtomicUInt32* atomic, uint32_t value) {
    atomic->value = value;
}

void platform_atomic_init_int64(PlatformAtomicInt64* atomic, int64_t value) {
    atomic->value = value;
}

void platform_atomic_init_uint64(PlatformAtomicUInt64* atomic, uint64_t value) {
    atomic->value = value;
}

void platform_atomic_init_bool(PlatformAtomicBool* atomic, bool value) {
    atomic->value = value;
}

void platform_atomic_init_ptr(PlatformAtomicPtr* atomic, void* value) {
    atomic->value = value;
}

// Load operations
int32_t platform_atomic_load_int32(const PlatformAtomicInt32* atomic) {
    return InterlockedCompareExchange((volatile LONG*)&atomic->value, 0, 0);
}

uint32_t platform_atomic_load_uint32(const PlatformAtomicUInt32* atomic) {
    return InterlockedCompareExchange((volatile LONG*)&atomic->value, 0, 0);
}

int64_t platform_atomic_load_int64(const PlatformAtomicInt64* atomic) {
    return InterlockedCompareExchange64((volatile LONG64*)&atomic->value, 0, 0);
}

uint64_t platform_atomic_load_uint64(const PlatformAtomicUInt64* atomic) {
    return InterlockedCompareExchange64((volatile LONG64*)&atomic->value, 0, 0);
}

bool platform_atomic_load_bool(const PlatformAtomicBool* atomic) {
    return InterlockedCompareExchange((volatile LONG*)&atomic->value, 0, 0) != 0;
}

void* platform_atomic_load_ptr(const PlatformAtomicPtr* atomic) {
    return InterlockedCompareExchangePointer((volatile PVOID*)&atomic->value, NULL, NULL);
}

// Store operations
void platform_atomic_store_int32(PlatformAtomicInt32* atomic, int32_t value) {
    InterlockedExchange((volatile LONG*)&atomic->value, (LONG)value);
}

void platform_atomic_store_uint32(PlatformAtomicUInt32* atomic, uint32_t value) {
    InterlockedExchange((volatile LONG*)&atomic->value, (LONG)value);
}

void platform_atomic_store_int64(PlatformAtomicInt64* atomic, int64_t value) {
    InterlockedExchange64((volatile LONG64*)&atomic->value, (LONG64)value);
}

void platform_atomic_store_uint64(PlatformAtomicUInt64* atomic, uint64_t value) {
    InterlockedExchange64((volatile LONG64*)&atomic->value, (LONG64)value);
}

void platform_atomic_store_bool(PlatformAtomicBool* atomic, bool value) {
    InterlockedExchange((volatile LONG*)&atomic->value, value ? 1 : 0);
}

void platform_atomic_store_ptr(PlatformAtomicPtr* atomic, void* value) {
    InterlockedExchangePointer((volatile PVOID*)&atomic->value, value);
}

// Compare-exchange operations
bool platform_atomic_compare_exchange_int32(PlatformAtomicInt32* atomic, int32_t* expected, int32_t desired) {
    LONG original = InterlockedCompareExchange(
        (volatile LONG*)&atomic->value,
        (LONG)desired,
        (LONG)*expected
    );
    if (original == *expected) {
        return true;
    } else {
        *expected = original;
        return false;
    }
}

bool platform_atomic_compare_exchange_uint32(PlatformAtomicUInt32* atomic, uint32_t* expected, uint32_t desired) {
    LONG original = InterlockedCompareExchange(
        (volatile LONG*)&atomic->value,
        (LONG)desired,
        (LONG)*expected
    );
    if (original == *expected) {
        return true;
    } else {
        *expected = original;
        return false;
    }
}

bool platform_atomic_compare_exchange_int64(PlatformAtomicInt64* atomic, int64_t* expected, int64_t desired) {
    LONG64 original = InterlockedCompareExchange64(
        (volatile LONG64*)&atomic->value,
        (LONG64)desired,
        (LONG64)*expected
    );
    if (original == *expected) {
        return true;
    } else {
        *expected = original;
        return false;
    }
}

bool platform_atomic_compare_exchange_uint64(PlatformAtomicUInt64* atomic, uint64_t* expected, uint64_t desired) {
    LONG64 original = InterlockedCompareExchange64(
        (volatile LONG64*)&atomic->value,
        (LONG64)desired,
        (LONG64)*expected
    );
    if (original == *expected) {
        return true;
    } else {
        *expected = original;
        return false;
    }
}

bool platform_atomic_compare_exchange_bool(PlatformAtomicBool* atomic, bool* expected, bool desired) {
    LONG exp = *expected ? 1 : 0;
    LONG des = desired ? 1 : 0;
    LONG original = InterlockedCompareExchange(
        (volatile LONG*)&atomic->value,
        des,
        exp
    );
    if ((original != 0) == *expected) {
        return true;
    } else {
        *expected = original != 0;
        return false;
    }
}

bool platform_atomic_compare_exchange_ptr(PlatformAtomicPtr* atomic, void** expected, void* desired) {
    PVOID original = InterlockedCompareExchangePointer(
        (volatile PVOID*)&atomic->value,
        desired,
        *expected
    );
    if (original == *expected) {
        return true;
    } else {
        *expected = original;
        return false;
    }
}

// Fetch-add operations
int32_t platform_atomic_fetch_add_int32(PlatformAtomicInt32* atomic, int32_t value) {
    return InterlockedExchangeAdd((volatile LONG*)&atomic->value, (LONG)value);
}

uint32_t platform_atomic_fetch_add_uint32(PlatformAtomicUInt32* atomic, uint32_t value) {
    return InterlockedExchangeAdd((volatile LONG*)&atomic->value, (LONG)value);
}

int64_t platform_atomic_fetch_add_int64(PlatformAtomicInt64* atomic, int64_t value) {
    return InterlockedExchangeAdd64((volatile LONG64*)&atomic->value, (LONG64)value);
}

uint64_t platform_atomic_fetch_add_uint64(PlatformAtomicUInt64* atomic, uint64_t value) {
    return InterlockedExchangeAdd64((volatile LONG64*)&atomic->value, (LONG64)value);
}

// Thread fence operation
void platform_atomic_thread_fence(void) {
    MemoryBarrier();
}

#endif // _WIN32
