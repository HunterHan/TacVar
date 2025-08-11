/**
 * @file partes-fit.c
 * @brief Standalone utility to detect standard runtime of gauge block
 */
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <mpi.h>
#include "pterr.h"
#include "partes_types.h"

extern int fit_sub_time(int myrank, int nrank, pt_timer_info_t *timer_info, pt_gauge_info_t *gauge_info, uint64_t t_guess);
extern int stress_timer(int ntest, int nwait, int isroot, int isverb, pt_timer_info_t *timer_info);
extern int exponential_guessing(int myrank, uint64_t *t_guess_ns, uint64_t tick_ns);

#define _ptm_handle_error(err, msg) do { \
    if (err != 0) { \
        fprintf(stderr, "[Error] %s: %s\n", msg, get_pterr_str(err)); \
        MPI_Finalize(); \
        exit(err); \
    } \
} while(0)

extern const char *get_pterr_str(enum pterr err);

int
main(int argc, char *argv[])
{
    int myrank, nrank, err;
    pt_gauge_info_t gauge_info;
    pt_timer_info_t timer_info;
    long long *ptick_all = NULL, *povh_all = NULL;
    
    /* Init MPI */
    err = MPI_Init(&argc, &argv);
    if (err != MPI_SUCCESS) {
        fprintf(stderr, "MPI_Init failed\n");
        return 1;
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    MPI_Comm_size(MPI_COMM_WORLD, &nrank);
    
    err = stress_timer(10000, 2, 1, 1, &timer_info);
    _ptm_handle_error(err, "stress_timer");
    if (myrank == 0) {
        ptick_all = (long long *)malloc(nrank * sizeof(long long));
        povh_all = (long long *)malloc(nrank * sizeof(long long));
        if (!ptick_all || !povh_all) {
            fprintf(stderr, "[Error] malloc failed\n");
            if (ptick_all) {
                free(ptick_all);
                ptick_all = NULL;
            }
            if (povh_all) {
                free(povh_all);
                povh_all = NULL;
            }
            MPI_Finalize();
            return 1;
        }
    }
    MPI_Gather(&timer_info.tick, 1, MPI_LONG_LONG, ptick_all, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);
    MPI_Gather(&timer_info.ovh, 1, MPI_LONG_LONG, povh_all, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);
    if (myrank == 0) {
        for (int i = 0; i < nrank; i++) {
            printf("rank %d: tick=%lld, ovh=%lld\n", i, ptick_all[i], povh_all[i]);
        }
    }
    
    /* Exponential guessing */
    if (myrank == 0) {
        printf("=== Exponential Guessing ===\n");
    }
    uint64_t t_guess = 0;
    err = exponential_guessing(myrank, &t_guess, timer_info.tick);
    if (err != PTERR_SUCCESS) {
        fprintf(stderr, "[Error] Rank %d: exponential_guessing failed: %d\n", myrank, err);
        MPI_Finalize();
        return err;
    }


    /* Detect standard sub time */
    // TODO: what if cyc_per_op != 1?
    gauge_info.cy_per_op = 1;
    err = fit_sub_time(myrank, nrank, &timer_info, &gauge_info, t_guess);
    if (err != PTERR_SUCCESS) {
        fprintf(stderr, "[Error] Rank %d: fit_sub_time failed: %d\n", myrank, err);
        MPI_Finalize();
        return err;
    }

    /* Gather gauge info */
    double *wtime_per_op_all = NULL;
    int64_t *nop_per_tick_all = NULL;
    int64_t *core_freq_all = NULL;
    
    if (myrank == 0) {
        wtime_per_op_all = (double *)malloc(nrank * sizeof(double));
        nop_per_tick_all = (int64_t *)malloc(nrank * sizeof(int64_t));
        core_freq_all = (int64_t *)malloc(nrank * sizeof(int64_t));
        if (!wtime_per_op_all || !nop_per_tick_all || !core_freq_all) {
            fprintf(stderr, "[Error] malloc failed for gauge info arrays\n");
            if (wtime_per_op_all) free(wtime_per_op_all);
            if (nop_per_tick_all) free(nop_per_tick_all);
            if (core_freq_all) free(core_freq_all);
            MPI_Finalize();
            return 1;
        }
    }
    
    MPI_Gather(&gauge_info.wtime_per_op, 1, MPI_DOUBLE, wtime_per_op_all, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Gather(&gauge_info.nop_per_tick, 1, MPI_LONG_LONG, nop_per_tick_all, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);
    MPI_Gather(&gauge_info.core_freq, 1, MPI_LONG_LONG, core_freq_all, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);
    
    if (myrank == 0) {
        for (int i = 0; i < nrank; i++) {
            printf("rank %d: core_freq=%" PRId64 " Hz, wtime_per_op=%.6f, nop_per_tick=%" PRId64 "\n", 
                   i, core_freq_all[i], wtime_per_op_all[i], nop_per_tick_all[i]);
        }
        free(wtime_per_op_all);
        free(nop_per_tick_all);
        free(core_freq_all);
        wtime_per_op_all = NULL;
        nop_per_tick_all = NULL;
        core_freq_all = NULL;
    }
    
    /* Print results on rank 0 */
    if (myrank == 0) {
        printf("wtime_per_op=%.6f\n", gauge_info.wtime_per_op);
        printf("nop_per_tick=%lld\n", gauge_info.nop_per_tick);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    return 0;
}
