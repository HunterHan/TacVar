/**
 * @file likwid_timer.c
 * @brief LIKWID timer: return ns (clock_gettime), but require LIKWID init to succeed.
 *
 * This keeps the timer output compatible with other timers (ns),
 * while still exercising LIKWID as a dependency (soft-disable via USE_LIKWID).
 */

#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "timers.h"
#include "../pterr.h"

#ifdef USE_LIKWID

#include <likwid.h>

#ifdef PTOPT_USE_MPI
#include <mpi.h>
static int _pt_rank(void) {
    int r = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &r);
    return r;
}
#else
static int _pt_rank(void) { return 0; }
#endif

static int g_inited = 0;

int init_timer_likwid(void) {
    if (g_inited) return PTERR_SUCCESS;

    // NOTE: In LIKWID 5.5.x, likwid_markerInit() returns void.
    // We still call it to ensure LIKWID is initialized when this timer is selected.
    likwid_markerInit();
    likwid_markerThreadInit();

    g_inited = 1;
    return PTERR_SUCCESS;
}

static inline int64_t _now_ns(void) {
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    return (int64_t)tv.tv_sec * 1000000000LL + (int64_t)tv.tv_nsec;
}

int64_t tick_likwid(void) { return _now_ns(); }
int64_t tock_likwid(void) { return _now_ns(); }
int64_t get_stamp_likwid(void) { return _now_ns(); }

#endif  // USE_LIKWID

