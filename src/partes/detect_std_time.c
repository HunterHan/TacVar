#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdint.h>
#include <inttypes.h>
#include <float.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <mpi.h>
#include "pterr.h"
#include "partes_types.h"
#include "gauges/sub.h"
#include "timers/clock_gettime.h"

#define NUM_IGNORE_TIMING 2 // Ignore the first 2 results by default
#define MET_REPEAT 10
#define FIT_XLEN 25
#define DELTA_TICK 10

extern int calc_sample_var_1d_u64(uint64_t *arr, size_t n, double *var);

static inline int64_t _run_sub(uint64_t nsub);

/**
 * @brief: run ra=nsub, ra-=1 until ra==0.
 */
static inline int64_t 
_run_sub(uint64_t nsub)
{
    int64_t res;
    __timer_init_clock_gettime;
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);
    __timer_tick_clock_gettime;
    __gauge_sub_intrinsic(nsub);
    __timer_tock_clock_gettime(res);
    return res;
}

/** 
 * @brief Exponential guessing to estimate theoretical time per sub-op
 * @param myrank: my rank
 * @param nrank: number of ranks
 * @param gpt_guess: returned guess gauges/tick
 * @param timer_info: timer info
 * @return 0 on success, 1 on failure
 */
int
exponential_guessing(int myrank, int nrank, pt_timer_info_t *timer_info, double *gpt_guess)
{
    const int max_exp = 10; // nsub_arr=[1,10,...,10^10]
    uint64_t nsub_arr[11], tmet_arr[11];
    int err = PTERR_SUCCESS, break_flag = 0;
    int all_break_flags[nrank];
    int global_break_flag = 0;

    // Initialize nsub_arr = [1, 10, 100, ..., 10^10]
    nsub_arr[0] = 1;
    for (int i = 1; i <= max_exp; i++) {
        nsub_arr[i] = nsub_arr[i-1] * 10;
    }
    
    for (int step = 0; step <= max_exp; step++) {
        if (myrank == 0) {
            printf("Rank %d: nsub=%" PRIu64 "\n", myrank, nsub_arr[step]);
        }

        uint64_t nsub = nsub_arr[step], tmet, tmet_min = UINT64_MAX;
        tmet_arr[step] = 0;

        /* Run PT_VAR_START_NSTEP steps */
        for (int i = 0; i < PT_VAR_MAX_NSTEP; i++) {
            tmet = (uint64_t)_run_sub(nsub) / timer_info->tick;
            tmet_min = tmet < tmet_min ? tmet : tmet_min;
        }
        tmet_arr[step] = tmet_min;
        if (myrank == 0) {
            printf("Rank %d: tmet_min=%" PRIu64 " ticks\n", myrank, tmet_arr[step]);
        }
        if (tmet_arr[step] * 10 * timer_info->tick > PT_THRES_GUESS_NSUB_TIME && break_flag == 0) {
            break_flag = 1;
        }
        
        MPI_Gather(&break_flag, 1, MPI_INT, all_break_flags, 1, MPI_INT, 0, MPI_COMM_WORLD);
        if (myrank == 0) {
            for (int i = 0; i < nrank; i++) {
                if (all_break_flags[i] == 1) {
                    global_break_flag = 1;
                    break;
                }
            }
        }
        
        
        MPI_Bcast(&global_break_flag, 1, MPI_INT, 0, MPI_COMM_WORLD);
        
        if (global_break_flag == 1) {
            if (step >= 1) {
                *gpt_guess = ((double)nsub_arr[step] - (double)nsub_arr[step-1]) /
                             ((double)tmet_arr[step] - (double)tmet_arr[step-1]);
            } else {
                *gpt_guess = (double)nsub_arr[step] / ((double)tmet_arr[step] -
                             (double)timer_info->ovh);
            }
            break;
        }
    }

    return err;
}

/*
 * Detect theoretical time per sub-op and its short-term stability.
 * On rank 0 prints results; returns 0 on success.
 */
int
fit_sub_time(int myrank, int nrank, pt_timer_info_t *timer_info, pt_gauge_info_t *gauge_info, double gpt_guess)
{
    double hi_gpt, lo_gpt, lo_gpt_bound, hi_gpt_bound, gpt;
    uint64_t dt = DELTA_TICK; // dx=10ticks
    uint64_t *pmet = NULL;
    uint64_t xlen = NUM_IGNORE_TIMING + FIT_XLEN; // # of the nsub measured each try
    int64_t delta = 0, delta2 = 0;  // Gap between measured and actual time gap of dx.
    uint64_t dx, nsub_min;
    uint64_t conv_me = 0, conv_other = 0, conv_target = 0, conv_now = 0;

  
    // Use t_guess to set initial bounds if available
    gpt = gpt_guess;
    lo_gpt = gpt_guess / 2;  // 0.5 * t_guess
    hi_gpt = gpt_guess * 2;  // 2.0 * t_guess
    lo_gpt_bound =  timer_info->tick * 
                    ((double)MIN_TRY_HZ / gauge_info->cy_per_op / (double)1e9);
    hi_gpt_bound =  timer_info->tick * 
                    ((double)MAX_TRY_HZ / gauge_info->cy_per_op / (double)1e9);
    
    // Clamp to reasonable bounds
    // gauge_per_ns = freq_hz / 1e9
    lo_gpt = lo_gpt < lo_gpt_bound ? lo_gpt_bound : lo_gpt;
    hi_gpt = hi_gpt > hi_gpt_bound ? hi_gpt_bound : hi_gpt;
    pmet = (uint64_t *)malloc(xlen * sizeof(uint64_t));
    if (pmet == NULL) {
        return PTERR_MALLOC_FAILED;
    }

    gauge_info->wtime_per_op = 0;
    gauge_info->gpt = 0;

    // Try f to minimize:
    // delta = sigma_(i=NUM_IGNORE_TIMING+1)^(xlen-1) (pmet[i] - pmet[i-1] - dt)
    // delta2 = sigma_(i=NUM_IGNORE_TIMING+1)^(xlen-1) delta^2
    // delta<0: increase f; delta>0: decrease f
    // exit when delta2 >= delta2_old
    for (int i = 0; i < nrank; i ++) {
        conv_target |= (1ULL << i);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    while (conv_now != conv_target) {
        if (conv_me == 0) {
            dx = (uint64_t)(gpt * dt);
            nsub_min = (uint64_t)(((double)timer_info->ovh / (double)timer_info->tick) + 1 + dt) * gpt;

            delta = 0;
            delta2 = 0;
            printf("Rank %d: Trying %f G/tick, dx=%" PRIu64 ", dt=%" PRIu64 
                " ticks, nsub_min=%" PRIu64 "\n", myrank, gpt, dx, dt, nsub_min);
        }
        for (uint64_t i = 0; i < xlen; i++) {
            pmet[i] = (uint64_t)_run_sub(nsub_min + i * dx);
            for (int j = 0; j < MET_REPEAT; j++) {
                register uint64_t tmet = (uint64_t)_run_sub(nsub_min + i * dx);
                pmet[i] = tmet < pmet[i] ? tmet : pmet[i];
            }
        }
        for (uint64_t i = NUM_IGNORE_TIMING; i < xlen; i++) {
            delta = delta + ((int64_t)pmet[i] - (int64_t)pmet[i-1]) / (int64_t)timer_info->tick - (int64_t)dt;
            delta2 = delta2 + (((int64_t)pmet[i] - (int64_t)pmet[i-1]) / 
                    (int64_t)timer_info->tick - (int64_t)dt) * (((int64_t)pmet[i] -
                    (int64_t)pmet[i-1]) / (int64_t)timer_info->tick - (int64_t)dt);
        }

        if (delta == 0 || fabs(hi_gpt - lo_gpt) < 0.01*gpt) {
            gauge_info->gpt = gpt;
            gauge_info->wtime_per_op = timer_info->tick / gpt;
            conv_me = 1;
        } else if (delta < 0) {
            lo_gpt = gpt;
        } else if (delta > 0) {
            hi_gpt = gpt;
        }
        if (conv_me != 1) {
            gpt = 0.5 * (hi_gpt + lo_gpt);
            printf("Rank %d: delta=%" PRId64 ", delta2=%" PRId64 ","
                "Next gpt=%.6f, hi_gpt=%.6f, lo_gpt=%.6f\n", 
                myrank, delta, delta2, gpt, hi_gpt, lo_gpt);
        }
        if (myrank == 0) {
            conv_now |= conv_me;
            for (int r = 1; r < nrank; r ++) {
                MPI_Recv(&conv_other, 1, MPI_UINT64_T, r, r, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                conv_now |= conv_other << r;
            }
            MPI_Bcast(&conv_now, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);
        } else {
            MPI_Send(&conv_me, 1, MPI_UINT64_T, 0, myrank, MPI_COMM_WORLD);
            MPI_Bcast(&conv_now, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);
        }
    }

    free(pmet);
    pmet = NULL;

    return PTERR_SUCCESS;
}
