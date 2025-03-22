/**
 * @file windows_atomic.c
 * @brief Windows implementation of platform atomic operations
 */
#ifdef _WIN32

#include "platform_atomic.h"
#include <windows.h>
#include <stdint.h>

// Initialize operations
// 8-bit initialization
void platform_atomic_init_int8(PlatformAtomicInt8* atomic, int8_t value) {
    atomic->value = value;
}

void platform_atomic_init_uint8(PlatformAtomicUInt8* atomic, uint8_t value) {
    atomic->value = value;
}

// 16-bit initialization
void platform_atomic_init_int16(PlatformAtomicInt16* atomic, int16_t value) {
    atomic->value = value;
}

void platform_atomic_init_uint16(PlatformAtomicUInt16* atomic, uint16_t value) {
    atomic->value = value;
}

// 32-bit initialization
void platform_atomic_init_int32(PlatformAtomicInt32* atomic, int32_t value) {
    atomic->value = value;
}

void platform_atomic_init_uint32(PlatformAtomicUInt32* atomic, uint32_t value) {
    atomic->value = value;
}

// 64-bit initialization
void platform_atomic_init_int64(PlatformAtomicInt64* atomic, int64_t value) {
    atomic->value = value;
}

void platform_atomic_init_uint64(PlatformAtomicUInt64* atomic, uint64_t value) {
    atomic->value = value;
}

// Other initialization
void platform_atomic_init_bool(PlatformAtomicBool* atomic, bool value) {
    atomic->value = value;
}

void platform_atomic_init_ptr(PlatformAtomicPtr* atomic, void* value) {
    atomic->value = value;
}

// Store operations
// 8-bit store
void platform_atomic_store_int8(PlatformAtomicInt8* atomic, int8_t value) {
    InterlockedExchange8((volatile char*)&atomic->value, (char)value);
}

void platform_atomic_store_uint8(PlatformAtomicUInt8* atomic, uint8_t value) {
    InterlockedExchange8((volatile char*)&atomic->value, (char)value);
}

// 16-bit store
void platform_atomic_store_int16(PlatformAtomicInt16* atomic, int16_t value) {
    InterlockedExchange16((volatile SHORT*)&atomic->value, (SHORT)value);
}

void platform_atomic_store_uint16(PlatformAtomicUInt16* atomic, uint16_t value) {
    InterlockedExchange16((volatile SHORT*)&atomic->value, (SHORT)value);
}

// 32-bit store
void platform_atomic_store_int32(PlatformAtomicInt32* atomic, int32_t value) {
    InterlockedExchange((volatile LONG*)&atomic->value, (LONG)value);
}

void platform_atomic_store_uint32(PlatformAtomicUInt32* atomic, uint32_t value) {
    InterlockedExchange((volatile LONG*)&atomic->value, (LONG)value);
}

// 64-bit store
void platform_atomic_store_int64(PlatformAtomicInt64* atomic, int64_t value) {
    InterlockedExchange64((volatile LONGLONG*)&atomic->value, (LONGLONG)value);
}

void platform_atomic_store_uint64(PlatformAtomicUInt64* atomic, uint64_t value) {
    InterlockedExchange64((volatile LONGLONG*)&atomic->value, (LONGLONG)value);
}

// Other store
void platform_atomic_store_bool(PlatformAtomicBool* atomic, bool value) {
    InterlockedExchange8((volatile char*)&atomic->value, (char)value);
}

void platform_atomic_store_ptr(PlatformAtomicPtr* atomic, void* value) {
    InterlockedExchangePointer((volatile PVOID*)&atomic->value, value);
}

// Load operations
// 8-bit load
int8_t platform_atomic_load_int8(const PlatformAtomicInt8* atomic) {
    return (int8_t)atomic->value; // volatile read is sufficient for 8-bit
}

uint8_t platform_atomic_load_uint8(const PlatformAtomicUInt8* atomic) {
    return (uint8_t)atomic->value; // volatile read is sufficient for 8-bit
}

// 16-bit load
int16_t platform_atomic_load_int16(const PlatformAtomicInt16* atomic) {
    return (int16_t)atomic->value; // volatile read is sufficient for 16-bit
}

uint16_t platform_atomic_load_uint16(const PlatformAtomicUInt16* atomic) {
    return (uint16_t)atomic->value; // volatile read is sufficient for 16-bit
}

// 32-bit load
int32_t platform_atomic_load_int32(const PlatformAtomicInt32* atomic) {
    return (int32_t)atomic->value; // volatile read is sufficient for 32-bit
}

uint32_t platform_atomic_load_uint32(const PlatformAtomicUInt32* atomic) {
    return (uint32_t)atomic->value; // volatile read is sufficient for 32-bit
}

// 64-bit load
int64_t platform_atomic_load_int64(const PlatformAtomicInt64* atomic) {
#ifdef _WIN64
    return (int64_t)atomic->value; // volatile read is sufficient on x64
#else
    // Use compare-exchange on 32-bit platforms to ensure atomic read
    return (int64_t)InterlockedCompareExchange64((volatile LONGLONG*)&atomic->value, 0, 0);
#endif
}

uint64_t platform_atomic_load_uint64(const PlatformAtomicUInt64* atomic) {
#ifdef _WIN64
    return (uint64_t)atomic->value; // volatile read is sufficient on x64
#else
    // Use compare-exchange on 32-bit platforms to ensure atomic read
    return (uint64_t)InterlockedCompareExchange64((volatile LONGLONG*)&atomic->value, 0, 0);
#endif
}

// Other load
bool platform_atomic_load_bool(const PlatformAtomicBool* atomic) {
    return (bool)atomic->value; // volatile read is sufficient for 8-bit
}

void* platform_atomic_load_ptr(const PlatformAtomicPtr* atomic) {
    return (void*)atomic->value; // volatile read is sufficient for pointer-sized values
}

// Exchange operations
// 8-bit exchange
int8_t platform_atomic_exchange_int8(PlatformAtomicInt8* atomic, int8_t value) {
    return (int8_t)InterlockedExchange8((volatile char*)&atomic->value, (char)value);
}

uint8_t platform_atomic_exchange_uint8(PlatformAtomicUInt8* atomic, uint8_t value) {
    return (uint8_t)InterlockedExchange8((volatile char*)&atomic->value, (char)value);
}

// 16-bit exchange
int16_t platform_atomic_exchange_int16(PlatformAtomicInt16* atomic, int16_t value) {
    return (int16_t)InterlockedExchange16((volatile SHORT*)&atomic->value, (SHORT)value);
}

uint16_t platform_atomic_exchange_uint16(PlatformAtomicUInt16* atomic, uint16_t value) {
    return (uint16_t)InterlockedExchange16((volatile SHORT*)&atomic->value, (SHORT)value);
}

// 32-bit exchange
int32_t platform_atomic_exchange_int32(PlatformAtomicInt32* atomic, int32_t value) {
    return (int32_t)InterlockedExchange((volatile LONG*)&atomic->value, (LONG)value);
}

uint32_t platform_atomic_exchange_uint32(PlatformAtomicUInt32* atomic, uint32_t value) {
    return (uint32_t)InterlockedExchange((volatile LONG*)&atomic->value, (LONG)value);
}

// 64-bit exchange
int64_t platform_atomic_exchange_int64(PlatformAtomicInt64* atomic, int64_t value) {
    return (int64_t)InterlockedExchange64((volatile LONGLONG*)&atomic->value, (LONGLONG)value);
}

uint64_t platform_atomic_exchange_uint64(PlatformAtomicUInt64* atomic, uint64_t value) {
    return (uint64_t)InterlockedExchange64((volatile LONGLONG*)&atomic->value, (LONGLONG)value);
}

// Other exchange
bool platform_atomic_exchange_bool(PlatformAtomicBool* atomic, bool value) {
    return (bool)InterlockedExchange8((volatile char*)&atomic->value, (char)value);
}

void* platform_atomic_exchange_ptr(PlatformAtomicPtr* atomic, void* value) {
    return InterlockedExchangePointer((volatile PVOID*)&atomic->value, value);
}

// Compare-exchange operations
// 8-bit compare-exchange
bool platform_atomic_compare_exchange_int8(PlatformAtomicInt8* atomic, int8_t* expected, int8_t desired) {
    LONG expectedVal = (LONG)*expected;
    LONG result = InterlockedCompareExchange((volatile LONG*)&atomic->value, (LONG)desired, expectedVal);
    if (result == expectedVal) return true;
    *expected = (int8_t)result;
    return false;
}

bool platform_atomic_compare_exchange_uint8(PlatformAtomicUInt8* atomic, uint8_t* expected, uint8_t desired) {
    LONG expectedVal = (LONG)*expected;
    LONG result = InterlockedCompareExchange((volatile LONG*)&atomic->value, (LONG)desired, expectedVal);
    if (result == expectedVal) return true;
    *expected = (uint8_t)result;
    return false;
}

// 16-bit compare-exchange
bool platform_atomic_compare_exchange_int16(PlatformAtomicInt16* atomic, int16_t* expected, int16_t desired) {
    SHORT expectedVal = (SHORT)*expected;
    SHORT result = InterlockedCompareExchange16((volatile SHORT*)&atomic->value, (SHORT)desired, expectedVal);
    if (result == expectedVal) return true;
    *expected = (int16_t)result;
    return false;
}

bool platform_atomic_compare_exchange_uint16(PlatformAtomicUInt16* atomic, uint16_t* expected, uint16_t desired) {
    SHORT expectedVal = (SHORT)*expected;
    SHORT result = InterlockedCompareExchange16((volatile SHORT*)&atomic->value, (SHORT)desired, expectedVal);
    if (result == expectedVal) return true;
    *expected = (uint16_t)result;
    return false;
}

// 32-bit compare-exchange
bool platform_atomic_compare_exchange_int32(PlatformAtomicInt32* atomic, int32_t* expected, int32_t desired) {
    LONG expectedVal = (LONG)*expected;
    LONG result = InterlockedCompareExchange((volatile LONG*)&atomic->value, (LONG)desired, expectedVal);
    if (result == expectedVal) return true;
    *expected = (int32_t)result;
    return false;
}

bool platform_atomic_compare_exchange_uint32(PlatformAtomicUInt32* atomic, uint32_t* expected, uint32_t desired) {
    LONG expectedVal = (LONG)*expected;
    LONG result = InterlockedCompareExchange((volatile LONG*)&atomic->value, (LONG)desired, expectedVal);
    if (result == expectedVal) return true;
    *expected = (uint32_t)result;
    return false;
}

// 64-bit compare-exchange
bool platform_atomic_compare_exchange_int64(PlatformAtomicInt64* atomic, int64_t* expected, int64_t desired) {
    LONGLONG expectedVal = (LONGLONG)*expected;
    LONGLONG result = InterlockedCompareExchange64((volatile LONGLONG*)&atomic->value, (LONGLONG)desired, expectedVal);
    if (result == expectedVal) return true;
    *expected = (int64_t)result;
    return false;
}

bool platform_atomic_compare_exchange_uint64(PlatformAtomicUInt64* atomic, uint64_t* expected, uint64_t desired) {
    LONGLONG expectedVal = (LONGLONG)*expected;
    LONGLONG result = InterlockedCompareExchange64((volatile LONGLONG*)&atomic->value, (LONGLONG)desired, expectedVal);
    if (result == expectedVal) return true;
    *expected = (uint64_t)result;
    return false;
}

// Other compare-exchange
bool platform_atomic_compare_exchange_bool(PlatformAtomicBool* atomic, bool* expected, bool desired) {
    LONG expectedVal = (LONG)*expected;
    LONG result = InterlockedCompareExchange((volatile LONG*)&atomic->value, (LONG)desired, expectedVal);
    if (result == expectedVal) return true;
    *expected = (bool)result;
    return false;
}

bool platform_atomic_compare_exchange_ptr(PlatformAtomicPtr* atomic, void** expected, void* desired) {
    void* expectedVal = *expected;
    void* result = InterlockedCompareExchangePointer((volatile PVOID*)&atomic->value, desired, expectedVal);
    if (result == expectedVal) return true;
    *expected = result;
    return false;
}

// Fetch-add operations
// 8-bit fetch-add
int8_t platform_atomic_fetch_add_int8(PlatformAtomicInt8* atomic, int8_t value) {
    return (int8_t)InterlockedExchangeAdd8((volatile char*)&atomic->value, (char)value);
}

uint8_t platform_atomic_fetch_add_uint8(PlatformAtomicUInt8* atomic, uint8_t value) {
    return (uint8_t)InterlockedExchangeAdd8((volatile char*)&atomic->value, (char)value);
}

// 16-bit fetch-add
int16_t platform_atomic_fetch_add_int16(PlatformAtomicInt16* atomic, int16_t value) {
    return (int16_t)InterlockedExchangeAdd16((volatile SHORT*)&atomic->value, (SHORT)value);
}

uint16_t platform_atomic_fetch_add_uint16(PlatformAtomicUInt16* atomic, uint16_t value) {
    return (uint16_t)InterlockedExchangeAdd16((volatile SHORT*)&atomic->value, (SHORT)value);
}

// 32-bit fetch-add
int32_t platform_atomic_fetch_add_int32(PlatformAtomicInt32* atomic, int32_t value) {
    return (int32_t)InterlockedExchangeAdd((volatile LONG*)&atomic->value, (LONG)value);
}

uint32_t platform_atomic_fetch_add_uint32(PlatformAtomicUInt32* atomic, uint32_t value) {
    return (uint32_t)InterlockedExchangeAdd((volatile LONG*)&atomic->value, (LONG)value);
}

// 64-bit fetch-add
int64_t platform_atomic_fetch_add_int64(PlatformAtomicInt64* atomic, int64_t value) {
    return (int64_t)InterlockedExchangeAdd64((volatile LONGLONG*)&atomic->value, (LONGLONG)value);
}

uint64_t platform_atomic_fetch_add_uint64(PlatformAtomicUInt64* atomic, uint64_t value) {
    return (uint64_t)InterlockedExchangeAdd64((volatile LONGLONG*)&atomic->value, (LONGLONG)value);
}

// Memory fence operation
void platform_atomic_thread_fence(PlatformMemoryOrder order) {
    switch (order) {
        case PLATFORM_MEMORY_ORDER_RELAXED:
            // No fence needed
            break;
        case PLATFORM_MEMORY_ORDER_CONSUME:
        case PLATFORM_MEMORY_ORDER_ACQUIRE:
            _ReadBarrier();
            break;
        case PLATFORM_MEMORY_ORDER_RELEASE:
            _WriteBarrier();
            break;
        case PLATFORM_MEMORY_ORDER_ACQ_REL:
        case PLATFORM_MEMORY_ORDER_SEQ_CST:
            MemoryBarrier();
            break;
    }
}

#endif // _WIN32
