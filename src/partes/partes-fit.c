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

extern int fit_sub_time(int myrank, int nrank, pt_timer_info_t *timer_info, pt_gauge_info_t *gauge_info, double gpt_guess);
extern int stress_timer(int ntest, int nwait, int isroot, int isverb, pt_timer_info_t *timer_info);
extern int exponential_guessing(int myrank, int nrank, pt_timer_info_t *timer_info, double *gpt_guess);

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
    uint64_t *ptick_all = NULL, *povh_all = NULL;
    double gpt_guess = 0, gpt_guess_all[nrank];
    
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
        ptick_all = (uint64_t *)malloc(nrank * sizeof(uint64_t));
        povh_all = (uint64_t *)malloc(nrank * sizeof(uint64_t));
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
    MPI_Gather(&timer_info.tick, 1, MPI_UINT64_T, ptick_all, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);
    MPI_Gather(&timer_info.ovh, 1, MPI_UINT64_T, povh_all, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);
    if (myrank == 0) {
        for (int i = 0; i < nrank; i++) {
            printf("rank %d: tick=%" PRIu64 ", ovh=%" PRIu64 "\n", i, ptick_all[i], povh_all[i]);
        }
    }
    
    /* Exponential guessing */
    if (myrank == 0) {
        printf("=== Exponential Guessing ===\n");
    }
    err = exponential_guessing(myrank, nrank, &timer_info, &gpt_guess);
    if (err != PTERR_SUCCESS) {
        fprintf(stderr, "[Error] Rank %d: exponential_guessing failed: %d\n", myrank, err);
        MPI_Finalize();
        return err;
    }
    if (myrank == 0) {
        gpt_guess_all[0] = gpt_guess;
        for (int i = 1; i < nrank; i++) {
            MPI_Recv(&gpt_guess_all[i], 1, MPI_DOUBLE, i, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    } else {
        MPI_Send(&gpt_guess, 1, MPI_DOUBLE, 0, myrank, MPI_COMM_WORLD);
    }
    if (myrank == 0) {
        for (int i = 0; i < nrank; i++) {
            printf("rank %d: Guess gauges/tick=%lf\n", i, gpt_guess_all[i]);
        }
    }


    /* Detect standard sub time */
    gauge_info.cy_per_op = 1; // TODO: what if cyc_per_op != 1?
    gauge_info.gpt = 0;
    gauge_info.wtime_per_op = 0;
    err = fit_sub_time(myrank, nrank, &timer_info, &gauge_info, gpt_guess);
    if (err != PTERR_SUCCESS) {
        fprintf(stderr, "[Error] Rank %d: fit_sub_time failed: %d\n", myrank, err);
        MPI_Finalize();
        return err;
    }

    /* Gather gauge info */
    double *wtime_per_op_all = NULL;
    double *gpt_all = NULL;
    
    if (myrank == 0) {
        wtime_per_op_all = (double *)malloc(nrank * sizeof(double));
        gpt_all = (double *)malloc(nrank * sizeof(double));
        if (!wtime_per_op_all || !gpt_all) {
            fprintf(stderr, "[Error] malloc failed for gauge info arrays\n");
            if (wtime_per_op_all) free(wtime_per_op_all);
            if (gpt_all) free(gpt_all);
            MPI_Finalize();
            return 1;
        }
    }
    
    MPI_Gather(&gauge_info.wtime_per_op, 1, MPI_DOUBLE, wtime_per_op_all, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Gather(&gauge_info.gpt, 1, MPI_DOUBLE, gpt_all, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    
    if (myrank == 0) {
        for (int i = 0; i < nrank; i++) {
            printf("rank %d: gpt=%.6f, wtime_per_op=%.6f\n", 
                   i, gpt_all[i], wtime_per_op_all[i]);
        }
        free(wtime_per_op_all);
        free(gpt_all);
        wtime_per_op_all = NULL;
        gpt_all = NULL;
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    return 0;
}
