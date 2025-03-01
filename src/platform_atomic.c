#include "platform_atomic.h"

#ifdef _WIN32
#include <windows.h>
#elif defined(__GNUC__)
// GCC built-in atomic operations
#else
#include <stdatomic.h>
#endif

long platform_atomic_compare_exchange(volatile long* dest, long exchange, long comparand) {
#ifdef _WIN32
    return InterlockedCompareExchange(dest, exchange, comparand);
#elif defined(__GNUC__)
    return __sync_val_compare_and_swap(dest, comparand, exchange);
#else
    return atomic_compare_exchange_strong((atomic_long*)dest, &comparand, exchange) ? comparand : *dest;
#endif
}

long platform_atomic_exchange(volatile long* dest, long exchange) {
#ifdef _WIN32
    return InterlockedExchange(dest, exchange);
#elif defined(__GNUC__)
    return __sync_lock_test_and_set(dest, exchange);
#else
    return atomic_exchange((atomic_long*)dest, exchange);
#endif
}

long platform_atomic_increment(volatile long* dest) {
#ifdef _WIN32
    return InterlockedIncrement(dest);
#elif defined(__GNUC__)
    return __sync_add_and_fetch(dest, 1);
#else
    return atomic_fetch_add((atomic_long*)dest, 1) + 1;
#endif
}

long platform_atomic_decrement(volatile long* dest) {
#ifdef _WIN32
    return InterlockedDecrement(dest);
#elif defined(__GNUC__)
    return __sync_sub_and_fetch(dest, 1);
#else
    return atomic_fetch_sub((atomic_long*)dest, 1) - 1;
#endif
}