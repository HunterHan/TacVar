/**
 * @file timers.h
 * @brief: Header file for all kernel functions (external interface)
 */
#ifndef TIMERS_H
#define TIMERS_H

#include <stddef.h>
#include <stdint.h>

enum timer_name {
    TIMER_CLOCK_GETTIME = 0,
    TIMER_MPI_WTIME,
#ifdef USE_PAPI
    TIMER_PAPI,
    TIMER_PAPIX6,
#endif
#ifdef USE_LIKWID
    TIMER_LIKWID,
#endif
#ifdef __x86_64__
    TIMER_TSC,
    TIMER_TSC_ASYM,
#endif
#ifdef __aarch64__
    TIMER_CNTVCT,
    TIMER_CNTVCT_FENCE,
    TIMER_CNTVCTO,
#endif
};

int init_timer_clock_gettime(void);
int64_t tick_clock_gettime(void);
int64_t tock_clock_gettime(void);
int64_t get_stamp_clock_gettime(void);

int init_timer_mpi_wtime(void);
int64_t tick_mpi_wtime(void);
int64_t tock_mpi_wtime(void);
int64_t get_stamp_mpi_wtime(void);

#ifdef USE_PAPI
int init_timer_papi(void);
int64_t tick_papi(void);
int64_t tock_papi(void);
int64_t get_stamp_papi(void);

int init_timer_papix6(void);
int64_t tick_papix6(void);
int64_t tock_papix6(void);
int64_t get_stamp_papix6(void);
#endif

#ifdef USE_LIKWID
int init_timer_likwid(void);
int64_t tick_likwid(void);
int64_t tock_likwid(void);
int64_t get_stamp_likwid(void);
#endif

#ifdef __x86_64__
int init_timer_tsc(void);
int64_t tick_tsc(void);
int64_t tock_tsc(void);
int64_t get_stamp_tsc(void);

int init_timer_tsc_asym(void);
int64_t tick_tsc_asym(void);
int64_t tock_tsc_asym(void);
int64_t get_stamp_tsc_asym(void);
#endif

#ifdef __aarch64__
int init_timer_cntvct(void);
int64_t tick_cntvct(void);
int64_t tock_cntvct(void);
int64_t get_stamp_cntvct(void);

int init_timer_cntvct_fence(void);
int64_t tick_cntvct_fence(void);
int64_t tock_cntvct_fence(void);
int64_t get_stamp_cntvct_fence(void);

int init_timer_cntvcto(void);
int64_t tick_cntvcto(void);
int64_t tock_cntvcto(void);
int64_t get_stamp_cntvcto(void);
#endif

#endif
