/**
 * @file tsc_asym.c
 * @brief: Implementation of asymmetric TSC timer, refer to Gabriele Paoloni's
 *         white paper "How to Benchmark Code Execution Times on Intel IA-32 
 *         and IA-64 Instruction Set Architectures".
 */

#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#include <time.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include "timers.h"
#include "../pterr.h"

#ifdef __x86_64__

static int g_inited = 0;
static int g_use_tsc = 0;
static uint64_t g_tsc0 = 0;
static int64_t g_ns0 = 0;
static double g_cycles_per_ns = 0.0;

static inline int64_t _now_ns(void) {
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    return (int64_t)tv.tv_sec * 1000000000LL + (int64_t)tv.tv_nsec;
}

static inline uint64_t _rdtsc_serialized(void) {
    unsigned hi, lo;
    __asm__ volatile(
        "cpuid\n\t"
        "rdtsc\n\t"
        : "=d"(hi), "=a"(lo)
        :
        : "%rbx", "%rcx");
    return ((uint64_t)hi << 32) | (uint64_t)lo;
}

static inline uint64_t _rdtscp_serialized(void) {
    unsigned hi, lo;
    __asm__ volatile(
        "rdtscp\n\t"
        "mov %%edx, %0\n\t"
        "mov %%eax, %1\n\t"
        "cpuid\n\t"
        : "=r"(hi), "=r"(lo)
        :
        : "%rax", "%rbx", "%rcx", "%rdx");
    return ((uint64_t)hi << 32) | (uint64_t)lo;
}

int
init_timer_tsc_asym(void)
{
    if (g_inited) return PTERR_SUCCESS;

    // Calibrate TSC to ns so this timer returns int64_t nanoseconds like others.
    // If calibration is not trustworthy, fall back to CLOCK_MONOTONIC.
    //
    // NOTE: This assumes a reasonably stable/invariant TSC and minimal cross-core
    // skew during calibration. It's still far safer than returning a bogus
    // "edx*1e9 + eax" value which can overflow int64_t and go non-monotonic.
    int64_t ns1 = _now_ns();
    uint64_t t1 = _rdtsc_serialized();
    // short sleep to get a measurable delta without slowing startup too much
    usleep(50000); // 50ms
    int64_t ns2 = _now_ns();
    uint64_t t2 = _rdtscp_serialized();

    int64_t dns = ns2 - ns1;
    uint64_t dtsc = t2 - t1;
    if (dns > 0 && dtsc > 0) {
        g_cycles_per_ns = (double)dtsc / (double)dns;
        if (g_cycles_per_ns > 0.0) {
            g_tsc0 = t2;
            g_ns0 = ns2;
            g_use_tsc = 1;
            g_inited = 1;
            return PTERR_SUCCESS;
        }
    }

    fprintf(stderr, "[WARN] tsc_asym: calibration failed (dns=%lld dtsc=%llu); fallback to clock_gettime\n",
            (long long)dns, (unsigned long long)dtsc);
    g_use_tsc = 0;
    g_inited = 1;
    return PTERR_SUCCESS;
}

int64_t
tick_tsc_asym(void)
{
    if (!g_inited) (void)init_timer_tsc_asym();
    if (!g_use_tsc) return _now_ns();
    uint64_t t = _rdtsc_serialized();
    double dcycles = (double)(t - g_tsc0);
    return g_ns0 + (int64_t)(dcycles / g_cycles_per_ns);
}

int64_t 
tock_tsc_asym(void)
{
    if (!g_inited) (void)init_timer_tsc_asym();
    if (!g_use_tsc) return _now_ns();
    uint64_t t = _rdtscp_serialized();
    double dcycles = (double)(t - g_tsc0);
    return g_ns0 + (int64_t)(dcycles / g_cycles_per_ns);
}

int64_t
get_stamp_tsc_asym(void)
{
    return tick_tsc_asym();

}

#endif