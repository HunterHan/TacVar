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

#define MET_REPEAT 10

extern int calc_sample_var_1d_u64(uint64_t *arr, size_t n, double *var);

static inline int64_t _run_sub(uint64_t nsub);

/**
 * @brief: run ra=nsub, ra-=1 until ra==0.
 */
static inline int64_t 
_run_sub(uint64_t nsub)
{
    struct timespec tv;
    int64_t ns0, ns1;
    uint64_t t[MET_REPEAT];

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);

    for (int i = 0; i < MET_REPEAT; i++) {
        register uint64_t ra = nsub;
        register uint64_t rb = 1;
        clock_gettime(CLOCK_MONOTONIC, &tv);
        ns0 = (int64_t)tv.tv_sec * 1000000000ULL + (int64_t)tv.tv_nsec;
#if defined(__x86_64__)
        __asm__ __volatile__(
            "1:\n\t"
            "cmp $0, %0\n\t"
            "je 2f\n\t"
            "sub %1, %0\n\t"
            "jmp 1b\n\t"
            "2:\n\t"
            : "+r"(ra)
            : "r"(rb)
            : "cc");
#elif defined(__aarch64__)
        __asm__ __volatile__(
            "1:\n\t"
            "cmp %0, #0\n\t"
            "beq 2f\n\t"
            "sub %0, %0, %1\n\t"
            "b 1b\n\t"
            "2:\n\t"
            : "+r"(ra)
            : "r"(rb)
            : "cc");
#else
        while (ra) { ra -= rb; }
#endif

        clock_gettime(CLOCK_MONOTONIC, &tv);
        ns1 = (int64_t)tv.tv_sec * 1000000000ULL + (int64_t)tv.tv_nsec;
        t[i] = ns1 - ns0;
        // printf("%"PRId64",%"PRId64"\n", ns0, ns1);
    }
    // for (int i = 0; i < MET_REPEAT; i++) {
    //     printf("t[%d]=%" PRId64 " ns\n", i, t[i]);
    // }

    return (int64_t)(t[0]);
}

/*
 * Exponential guessing to estimate theoretical time per sub-op
 * Returns t_guess in ns/op, or 0.0 on failure
 */
int
exponential_guessing(int myrank, uint64_t *t_guess_ns, uint64_t tick_ns)
{
    const int max_exp = 10; // nsub_arr=[1,10,...,10^10]
    uint64_t nsub_arr[11], t_met_avg[11];
    double last_var = DBL_MAX, var = 0.0;
    int valid_steps = 0;
    int err;
    
    // Initialize nsub_arr = [1, 10, 100, ..., 10^10]
    nsub_arr[0] = 1;
    for (int i = 1; i <= max_exp; i++) {
        nsub_arr[i] = nsub_arr[i-1] * 10;
    }
    
    for (int step = 0; step <= max_exp; step++) {
        if (myrank == 0) {
            printf("Rank %d: nsub=%d\n", myrank, nsub_arr[step]);
        }

        int err;
        uint64_t nsub = nsub_arr[step], met_arr[PT_VAR_MAX_NSTEP], sum = 0;
        t_met_avg[step] = 0;

        /* Run PT_VAR_START_NSTEP steps */
        for (int i = 0; i < PT_VAR_START_NSTEP; i++) {
            met_arr[i] = _run_sub(nsub);
            sum += met_arr[i];
        }
        err = calc_sample_var_1d_u64(met_arr, PT_VAR_START_NSTEP, &last_var);
        if (err != PTERR_SUCCESS) {
            return err;
        }

        for (int i = 0; i < PT_VAR_MAX_NSTEP - PT_VAR_START_NSTEP; i ++) {
            met_arr[i + PT_VAR_START_NSTEP] = _run_sub(nsub);
            sum += met_arr[i + PT_VAR_START_NSTEP];
            err = calc_sample_var_1d_u64(met_arr, i + PT_VAR_START_NSTEP + 1, &var);
            if (myrank == 0) {
                printf("Rank %d: step %d, i=%d, var=%.6e\n", myrank, step, i, var);
            }
            if (err != PTERR_SUCCESS) {
                return err;
            }
            if (fabs((var - last_var) / last_var) < PT_THRS_GUESS_VAR || 
                i >= PT_VAR_MAX_NSTEP - PT_VAR_START_NSTEP - 1) {
                t_met_avg[step] = sum / (i + PT_VAR_START_NSTEP + 1);
                valid_steps = step;
                break;
            }
            last_var = var;
        }

        if (t_met_avg[step] * 10 > PT_THRES_GUESS_NSUB_TIME) { 
            break;
        }
    }
    
    // Calculate t_guess using last two valid steps
    if (valid_steps >= 2) {
        uint64_t nsub_diff = nsub_arr[valid_steps] - nsub_arr[valid_steps - 1];
        uint64_t time_diff = t_met_avg[valid_steps] - t_met_avg[valid_steps - 1];
        
        if (time_diff > 0) {
            *t_guess_ns = nsub_diff / time_diff; // ns per operation
            printf("Rank %d: t_guess = %"PRIu64" ns/op, var = %.6e\n", myrank, *t_guess_ns, var);
            return PTERR_SUCCESS;
        } else {
            err = PTERR_TIMER_NEGATIVE;
        }
    }
    
    if (myrank == 0) {
        printf("  Exponential guessing failed to converge\n");
    }
    err = PTERR_TIMER_OVERFLOW;
    *t_guess_ns = 0;
    return err;
}

/*
 * Detect theoretical time per sub-op and its short-term stability.
 * On rank 0 prints results; returns 0 on success.
 */
int
fit_sub_time(int myrank, int nrank, pt_timer_info_t *timer_info, pt_gauge_info_t *gauge_info, uint64_t t_guess)
{
    uint64_t hi_hz, lo_hz;
    
    // Use t_guess to set initial bounds if available
    if (t_guess > 0) {
        uint64_t freq_guess = t_guess; // Hz
        lo_hz = freq_guess / 2;  // 0.5 * t_guess
        hi_hz = freq_guess * 2;  // 2.0 * t_guess
        
        // Clamp to reasonable bounds
        if (lo_hz < MIN_TRY_HZ) lo_hz = MIN_TRY_HZ;
        if (hi_hz > MAX_TRY_HZ) hi_hz = MAX_TRY_HZ;
        
        if (myrank == 0) {
            printf("Using t_guess=%" PRIu64 " ns/op, freq_guess=%" PRIu64 " Hz, lo_hz=%" PRIu64 ", hi_hz=%" PRIu64 "\n",
                   t_guess, freq_guess, lo_hz, hi_hz);
        }
    } else {
        hi_hz = MAX_TRY_HZ;
        lo_hz = MIN_TRY_HZ;
        if (myrank == 0) {
            printf("No t_guess available, using default bounds: lo_hz=%" PRIu64 ", hi_hz=%" PRIu64 "\n",
                   lo_hz, hi_hz);
        }
    }
    uint64_t dt = timer_info->tick * 10; // dx=10ticks
    uint64_t *pmet = NULL;
    uint64_t xlen = NUM_IGNORE_TIMING + 10;
    int64_t delta, delta2;  // Gap between measured and actual time gap of dx.
    uint64_t f, dx, nsub_min;
    uint64_t conv_me = 0, conv_other = 0, conv_target = 0, conv_now = 0;

    pmet = (uint64_t *)malloc(xlen * sizeof(uint64_t));
    if (pmet == NULL) {
        return PTERR_MALLOC_FAILED;
    }

    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

    gauge_info->wtime_per_op = 0;
    gauge_info->nop_per_tick = 0;
    gauge_info->core_freq = 0;

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
            f = 0.5 * (hi_hz + lo_hz);
            double nop_per_ns = f * gauge_info->cy_per_op / 1e9;
            dx = (uint64_t)(nop_per_ns * dt);
            nsub_min = (timer_info->ovh + dt * NUM_IGNORE_TIMING) * (f / 1e9) * gauge_info->cy_per_op;

            delta = 0;
            delta2 = 0;
            printf("Rank %d: Trying frequency %" PRIu64 " Hz, dx=%" PRIu64 " ticks, dt=%" PRIu64 " ns, nsub_min=%" PRIu64 "\n", myrank, f, dx, dt, nsub_min);
        }
        for (uint64_t i = 0; i < xlen; i++) {
            _run_sub(nsub_min + i * dx);
            for (int j = 0; j < MET_REPEAT; j++) {
                pmet[i] += _run_sub(nsub_min + i * dx);
            }
            pmet[i] /= MET_REPEAT;
        }
        for (uint64_t i = NUM_IGNORE_TIMING; i < xlen; i++) {
            delta += pmet[i] - pmet[i-1] - dt;
            delta2 += (pmet[i] - pmet[i-1] - dt) * (pmet[i] - pmet[i-1] - dt);
        }

        if (delta == 0 || hi_hz <= lo_hz) {
            gauge_info->core_freq = f;
            gauge_info->wtime_per_op = 1.0e9 / f;
            gauge_info->nop_per_tick = (uint64_t)(f * timer_info->tick / 1e9 / gauge_info->cy_per_op);
            conv_me = 1;
        } else if (delta < 0) {
            lo_hz = f;
        } else if (delta > 0) {
            hi_hz = f;
        }
        if (conv_me != 1) {
            printf("Rank %d: delta=%" PRId64 ", delta2=%" PRId64 
                ",hi_hz=%" PRId64 "lo_hz=%" PRId64 "\n", 
                myrank, delta, delta2, hi_hz, lo_hz);
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
