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

extern int fit_sub_time(int myrank, int nrank, pt_timer_info_t *timer_info, pt_gauge_info_t *gauge_info);
extern int stress_timer(int ntest, int nwait, int isroot, int isverb, pt_timer_info_t *timer_info);

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

    /* Detect standard sub time */
    // TODO: what if cyc_per_op != 1?
    gauge_info.cy_per_op = 1;
    err = fit_sub_time(myrank, nrank, &timer_info, &gauge_info);
    if (err != PTERR_SUCCESS) {
        fprintf(stderr, "[Error] Rank %d: detect_standard_sub_time failed: %d\n", myrank, err);
        MPI_Finalize();
        return err;
    }

    pt_gauge_info_t *p_gauge_info_all = (pt_gauge_info_t *)malloc(nrank * sizeof(pt_gauge_info_t));
    MPI_Gather(&gauge_info, 1, MPI_LONG_LONG, p_gauge_info_all, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);
    if (myrank == 0) {
        for (int i = 0; i < nrank; i++) {
            printf("rank %d: core_freq=%" PRIu64 " Hz, wtime_per_op=%.6f, nop_per_tick=%lld\n", i, p_gauge_info_all[i].core_freq, p_gauge_info_all[i].wtime_per_op, p_gauge_info_all[i].nop_per_tick);
            free(p_gauge_info_all);
            p_gauge_info_all = NULL;
        }
    }
    
    /* Print results on rank 0 */
    if (myrank == 0) {
        printf("wtime_per_op=%.6f\n", gauge_info.wtime_per_op);
        printf("nop_per_tick=%lld\n", gauge_info.nop_per_tick);
    }
    
    MPI_Finalize();
    return 0;
}
