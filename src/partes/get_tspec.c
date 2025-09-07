/**
 * @file get_timer_spec.c
 * @brief: Stress running the timer, determine the time per tick and the lowest overhead.
 */
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>
#include "pterr.h"
#include "partes_types.h"
#include "timers/clock_gettime.h"

/**
 * @brief Stress timing to get timer_spec (ovh and tick).
 * @param ntest: The number of timing
 * @param timer_spec: The timer specification
 * @return: The error code
 */
int 
get_tspec(int ntest, pt_timer_spec_t *timer_spec) 
{
    enum pterr ret = PTERR_SUCCESS;
    int64_t *ptstamp = (int64_t *)malloc(ntest * sizeof(int64_t));
    __timer_init_clock_gettime;
    
    int64_t ovh = INT64_MAX;
    int64_t tick = INT64_MAX;
    
    // Loop through intervals
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);
    for (int i = 0; i < ntest; i++) {
        __timer_tick_clock_gettime;
        __timer_tock_clock_gettime(ptstamp[i]);
    }
    for (int i = 0; i < ntest; i++) {
        ovh = ptstamp[i] < ovh ? ptstamp[i] : ovh;
    }
    for (int i = 0; i < ntest; i++) {
        __timer_stamp_clock_gettime(ptstamp[i]);
    } 
    for (int i = 1; i < ntest; i++) {
        if (ptstamp[i] - ptstamp[i-1] > 0) {
            tick = ptstamp[i] - ptstamp[i-1] < tick ? ptstamp[i] - ptstamp[i-1] : tick;
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    timer_spec->ovh = ovh;
    timer_spec->tick = tick;
    free(ptstamp);

    if (ovh < 0) {
        ret = PTERR_TIMER_NEGATIVE;
    }

    return ret;
}
