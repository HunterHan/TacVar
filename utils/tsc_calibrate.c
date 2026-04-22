/*
 * TSC cycles/ns for stencil (divide RDTSC delta by printed value).
 * Usage: tsc_calibrate [calibrate|coarse]
 *   calibrate - MONOTONIC[_RAW] + CPUID/RDTSC/RDTSCP, median of short sleeps (default)
 *   coarse    - bare RDTSC + sleep(1) wall (ballpark only)
 */
#define _GNU_SOURCE
#include <errno.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if !defined(__x86_64__) && !defined(__i386__)
#error "tsc_calibrate.c requires x86_64 or i386"
#endif

static void tsc_start(uint64_t *cycle) {
    unsigned ch, cl;
    __asm__ __volatile__("CPUID\n\t"
                         "RDTSC\n\t"
                         "mov %%edx, %0\n\t"
                         "mov %%eax, %1\n\t"
                         : "=r"(ch), "=r"(cl)
                         :
                         : "%rax", "%rbx", "%rcx", "%rdx");
    *cycle = ((uint64_t)ch << 32) | cl;
}

static void tsc_stop(uint64_t *cycle) {
    unsigned ch, cl;
    __asm__ __volatile__("RDTSCP\n\t"
                         "mov %%edx, %0\n\t"
                         "mov %%eax, %1\n\t"
                         "CPUID\n\t"
                         : "=r"(ch), "=r"(cl)
                         :
                         : "%rax", "%rbx", "%rcx", "%rdx");
    *cycle = ((uint64_t)ch << 32) | cl;
}

static uint64_t rdtsc_loose(void) {
    unsigned lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static uint64_t mono_ns(clockid_t clk) {
    struct timespec ts;
    if (clock_gettime(clk, &ts) != 0) {
        perror("clock_gettime");
        exit(1);
    }
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static clockid_t pick_monotonic_clock(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) == 0)
        return CLOCK_MONOTONIC_RAW;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
        return CLOCK_MONOTONIC;
    perror("clock_gettime");
    exit(1);
}

static int cmp_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static void pin_cpu0(void) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0)
        fprintf(stderr, "tsc_calibrate: sched_setaffinity(cpu0): %s\n", strerror(errno));
}

static int run_fine(void) {
    enum { NSAMP = 48, SLEEP_NS = 10000000 };
    struct timespec sl = {0, SLEEP_NS};
    double ratios[NSAMP];
    clockid_t mclk = pick_monotonic_clock();

    pin_cpu0();

    for (int i = 0; i < NSAMP;) {
        uint64_t t0 = mono_ns(mclk);
        uint64_t c0;
        tsc_start(&c0);
        if (nanosleep(&sl, NULL) != 0) {
            perror("nanosleep");
            return 1;
        }
        uint64_t c1;
        tsc_stop(&c1);
        uint64_t t1 = mono_ns(mclk);
        if (t1 <= t0 || c1 <= c0)
            continue;
        ratios[i++] = (double)(c1 - c0) / (double)(t1 - t0);
    }

    qsort(ratios, NSAMP, sizeof(ratios[0]), cmp_double);
    double med = (ratios[NSAMP / 2 - 1] + ratios[NSAMP / 2]) * 0.5;
    printf("%.*f\n", 9, med);
    return 0;
}

static int run_coarse(void) {
    uint64_t t0 = mono_ns(CLOCK_MONOTONIC);
    uint64_t c0 = rdtsc_loose();
    sleep(1);
    uint64_t c1 = rdtsc_loose();
    uint64_t t1 = mono_ns(CLOCK_MONOTONIC);
    if (t1 <= t0 || c1 <= c0) {
        fprintf(stderr, "tsc_calibrate: coarse sample invalid\n");
        return 1;
    }
    uint64_t dt = t1 - t0;
    double tsc_ns = (double)(c1 - c0) / (double)dt;
    fprintf(stderr, "coarse: wall_s=%.6f (~%.3f MHz)\n", (double)dt / 1e9, tsc_ns * 1000.0);
    printf("%.9f\n", tsc_ns);
    return 0;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "usage: %s [calibrate|coarse]\n", argv[0]);
        return 2;
    }
    if (argc == 1 || strcmp(argv[1], "calibrate") == 0)
        return run_fine();
    if (strcmp(argv[1], "coarse") == 0)
        return run_coarse();
    fprintf(stderr, "tsc_calibrate: unknown mode %s\n", argv[1]);
    return 2;
}
