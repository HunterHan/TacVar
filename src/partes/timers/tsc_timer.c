/**
 * @file tsc_timer.c
 * @brief x86 TSC timer matching the stencil TSC read pattern, converted to ns.
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "timers.h"
#include "../pterr.h"

static double g_tsc_cycles_per_ns = 0.0;

static uint64_t
read_tsc_start(void)
{
    unsigned ch, cl;
    __asm__ volatile ("CPUID\n\t"
                      "RDTSC\n\t"
                      "mov %%edx, %0\n\t"
                      "mov %%eax, %1\n\t"
                      : "=r" (ch), "=r" (cl)
                      :
                      : "%rax", "%rbx", "%rcx", "%rdx");
    return (((uint64_t)ch << 32) | cl);
}

static uint64_t
read_tsc_stop(void)
{
    unsigned ch, cl;
    __asm__ volatile ("RDTSCP\n\t"
                      "mov %%edx, %0\n\t"
                      "mov %%eax, %1\n\t"
                      "CPUID\n\t"
                      : "=r" (ch), "=r" (cl)
                      :
                      : "%rax", "%rbx", "%rcx", "%rdx");
    return (((uint64_t)ch << 32) | cl);
}

static int64_t
cycles_to_ns(uint64_t cycles)
{
    return (int64_t)((double)cycles / g_tsc_cycles_per_ns);
}

static double
read_cpuinfo_mhz(void)
{
    FILE *fp = fopen("/proc/cpuinfo", "r");
    char line[256];
    double mhz = 0.0;
    if (!fp) {
        return 0.0;
    }
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "cpu MHz : %lf", &mhz) == 1 ||
            sscanf(line, "cpu MHz\t: %lf", &mhz) == 1) {
            break;
        }
    }
    fclose(fp);
    return mhz;
}

int
init_timer_tsc(void)
{
    const char *env = getenv("PT_TSC_CYCLES_PER_NS");
    if (!env) {
        env = getenv("TSC_CYCLES_PER_NS");
    }
    if (env) {
        g_tsc_cycles_per_ns = atof(env);
    }
    if (!(g_tsc_cycles_per_ns > 0.0)) {
        double mhz = read_cpuinfo_mhz();
        if (mhz > 0.0) {
            g_tsc_cycles_per_ns = mhz / 1000.0;
        }
    }
    return (g_tsc_cycles_per_ns > 0.0) ? PTERR_SUCCESS : PTERR_TIMER_INIT_FAILED;
}

int64_t
tick_tsc(void)
{
    return cycles_to_ns(read_tsc_start());
}

int64_t
tock_tsc(void)
{
    return cycles_to_ns(read_tsc_stop());
}

int64_t
get_stamp_tsc(void)
{
    return cycles_to_ns(read_tsc_start());
}
