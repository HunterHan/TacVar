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
#ifdef __x86_64__
    TIMER_TSC_ASYM
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

#ifdef __x86_64__
int init_timer_tsc_asym(void);
int64_t tick_tsc_asym(void);
int64_t tock_tsc_asym(void);
int64_t get_stamp_tsc_asym(void);
#endif  

#endif