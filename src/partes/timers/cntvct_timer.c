/**
 * @file cntvct_timer.c
 * @brief ARM CNTVCT timer variants converted to ns.
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include "timers.h"
#include "../pterr.h"

static uint64_t g_cntfrq = 0;

static inline uint64_t
cntvct_to_ns(uint64_t ticks)
{
    return (uint64_t)(((__uint128_t)ticks * 1000000000ULL) / g_cntfrq);
}

static inline uint64_t
read_cntvct(void)
{
    uint64_t ticks;
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(ticks));
    return ticks;
}

static inline uint64_t
read_cntvct_fence(void)
{
    uint64_t ticks;
    __asm__ __volatile__("isb; mrs %0, cntvct_el0" : "=r"(ticks) :: "memory");
    return ticks;
}

static inline uint64_t
read_cntvcto_start(void)
{
    uint64_t ticks;
    __asm__ __volatile__("isb\n\t"
                         "mrs %0, cntvct_el0\n\t"
                         "isb"
                         : "=r"(ticks)
                         :
                         : "memory");
    return ticks;
}

static inline uint64_t
read_cntvcto_stop(void)
{
    uint64_t ticks;
    __asm__ __volatile__("mrs %0, cntvct_el0\n\t"
                         : "=r"(ticks)
                         :
                         : "memory");
    return ticks;
}

static int
init_cntvct_freq(void)
{
    __asm__ __volatile__("mrs %0, cntfrq_el0" : "=r"(g_cntfrq));
    return g_cntfrq ? PTERR_SUCCESS : PTERR_TIMER_INIT_FAILED;
}

int init_timer_cntvct(void) { return init_cntvct_freq(); }
int64_t tick_cntvct(void) { return (int64_t)cntvct_to_ns(read_cntvct()); }
int64_t tock_cntvct(void) { return (int64_t)cntvct_to_ns(read_cntvct()); }
int64_t get_stamp_cntvct(void) { return (int64_t)cntvct_to_ns(read_cntvct()); }

int init_timer_cntvct_fence(void) { return init_cntvct_freq(); }
int64_t tick_cntvct_fence(void) { return (int64_t)cntvct_to_ns(read_cntvct_fence()); }
int64_t tock_cntvct_fence(void) { return (int64_t)cntvct_to_ns(read_cntvct_fence()); }
int64_t get_stamp_cntvct_fence(void) { return (int64_t)cntvct_to_ns(read_cntvct_fence()); }

int init_timer_cntvcto(void) { return init_cntvct_freq(); }
int64_t tick_cntvcto(void) { return (int64_t)cntvct_to_ns(read_cntvcto_start()); }
int64_t tock_cntvcto(void) { return (int64_t)cntvct_to_ns(read_cntvcto_stop()); }
int64_t get_stamp_cntvcto(void) { return (int64_t)cntvct_to_ns(read_cntvcto_start()); }
