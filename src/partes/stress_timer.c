/**
 * @file stress_timer.c
 * @brief: Stress running the timer, determine the time per tick and the lowest overhead.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>
#include "pterr.h"
#include "partes_types.h"

/***
 * @param ntpint: The number of tests per interval
 * @param nint: The number of intervals
 * @param tint: Interval time in seconds
 * @param nwint: The number of warmup intervals
 * @param tick: Time per tick in nanoseconds
 * @param ovh: The lowest overhead in nanoseconds
 * @return: The error code
 */
int 
stress_timer(int ntpint, int nint, int tint, int nwint, pt_timer_info_t *timer_info) 
{
    enum pterr ret = PTERR_SUCCESS;
    struct timespec tv;
    FILE *fp;
    uint64_t *raw = (uint64_t *)malloc(ntpint * sizeof(uint64_t));
    uint64_t *gap = (uint64_t *)malloc(ntpint * sizeof(uint64_t));
    
    long long min_overhead = INT64_MAX;
    long long min_tick = INT64_MAX;
    
    // Loop through intervals
    for (int i = 0; i < nint; i++) {
        // Loop through tests per interval
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
        // Get initial timestamp
        clock_gettime(CLOCK_MONOTONIC, &tv);
        raw[0] = tv.tv_sec * 1000000000ULL + tv.tv_nsec;
        // Run consecutive timing measurements
        for (int j = 1; j < ntpint; j++) {
            clock_gettime(CLOCK_MONOTONIC, &tv);
            raw[j] = tv.tv_sec * 1000000000ULL + tv.tv_nsec;
            gap[j] = raw[j] - raw[j-1];
        }
            
            // Calculate overhead and tick only if not in warmup period
        if (i >= nwint) {
            for (int j = 1; j < ntpint; j++) {
                // Update minimum overhead (any gap >= 0)
                if (gap[j] < min_overhead) {
                    min_overhead = gap[j];
                }
                
                // Update minimum tick (any gap > 0)
                if (gap[j] > 0 && gap[j] < min_tick) {
                    min_tick = gap[j];
                }
            }
        }
        sleep(tint);
    }
    
    timer_info->ovh = min_overhead;
    timer_info->tick = (unsigned long long)min_tick;
    free(raw);
    free(gap);

    if (min_overhead < 0) {
        ret = PTERR_TIMER_NEGATIVE;
    }

    return ret;
}
