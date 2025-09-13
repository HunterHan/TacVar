/**
 * @file clock_gettime.h
 * @brief: Macros encapsulating POSIX clock_gettime() function.
 */
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <time.h>

#define __timer_init_clock_gettime  \
    struct timespec __tv;             \
    int64_t __ns0, __ns1;

#define __timer_tick_clock_gettime          \
    clock_gettime(CLOCK_MONOTONIC, &__tv);    \
    __ns0 = (int64_t)__tv.tv_sec * 1000000000ULL + (int64_t)__tv.tv_nsec;

#define __timer_tock_clock_gettime(res)     \
    clock_gettime(CLOCK_MONOTONIC, &__tv);    \
    __ns1 = (int64_t)__tv.tv_sec * 1000000000ULL + (int64_t)__tv.tv_nsec; \
    (res) = __ns1 - __ns0;

#define __timer_stamp_clock_gettime(res)     \
    clock_gettime(CLOCK_MONOTONIC, &__tv);    \
    __ns1 = (int64_t)__tv.tv_sec * 1000000000ULL + (int64_t)__tv.tv_nsec; \
    (res) = __ns1;
