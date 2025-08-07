/**
 * @file timer.c
 * @brief: Different timer APIs being tested.
 */
#include <time.h>

unsigned long long
_ptget_clock_gettime_ns(void)
{
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    return tv.tv_sec * 1000000000ULL + tv.tv_nsec;
}