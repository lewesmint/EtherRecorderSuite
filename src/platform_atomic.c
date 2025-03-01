#include "platform_atomic.h"

#ifdef _WIN32
#include <windows.h>

long platform_atomic_compare_exchange(volatile long* dest, long exchange, long comparand) {
    return InterlockedCompareExchange(dest, exchange, comparand);
}

long platform_atomic_exchange(volatile long* dest, long exchange) {
    return InterlockedExchange(dest, exchange);
}

long platform_atomic_increment(volatile long* dest) {
    return InterlockedIncrement(dest);
}

long platform_atomic_decrement(volatile long* dest) {
    return InterlockedDecrement(dest);
}

#else /* POSIX implementation */
#include <stdatomic.h>

long platform_atomic_compare_exchange(volatile long* dest, long exchange, long comparand) {
    long expected = comparand;
    atomic_compare_exchange_strong((atomic_long*)dest, &expected, exchange);
    return expected;
}

long platform_atomic_exchange(volatile long* dest, long exchange) {
    return atomic_exchange((atomic_long*)dest, exchange);
}

long platform_atomic_increment(volatile long* dest) {
    return atomic_fetch_add((atomic_long*)dest, 1) + 1;
}

long platform_atomic_decrement(volatile long* dest) {
    return atomic_fetch_sub((atomic_long*)dest, 1) - 1;
}
#endif