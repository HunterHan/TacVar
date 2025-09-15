/**
 * @file partes-fit.c
 * @brief Standalone utility to detect standard runtime of gauge block
 */
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#define NMEAS 100
#define NTILE 100
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <mpi.h>
#include <math.h>
#include "pterr.h"
#include "partes_types.h"
#include "timers/timers.h"
#include "gauges/sub.h"

extern int fit_sub_time(int myrank, int nrank, pt_timer_func_t *pttimers, pt_timer_spec_t *timer_spec, pt_gauge_info_t *gauge_info, double gpt_guess);
extern int get_tspec(int ntest, pt_timer_func_t *pttimers, pt_timer_spec_t *timer_spec);
extern int exp_guess_gauge(int myrank, int nrank, pt_timer_func_t *pttimers, pt_timer_spec_t *timer_spec, double *gpt_guess);
extern const char *get_pterr_str(enum pterr err);

static int _comp_u64(const void *a, const void *b);
static inline int64_t _run_sub(uint64_t nsub, pt_timer_func_t *pttimers);
/**
 * @brief Test key metrics for MMI and MMD at a given t0, with interval dt*nintv. 
 */
static int _test_ltt_ltd(uint64_t ts, uint64_t dt, uint64_t nintv, 
    pt_timer_func_t *pttimers, pt_timer_spec_t *timer_spec, pt_gauge_info_t *gauge_info);

static int
_comp_u64(const void *a, const void *b)
{
    return (*(const uint64_t *)a > *(const uint64_t *)b) - (*(const uint64_t *)a < *(const uint64_t *)b);
}

static inline int64_t 
_run_sub(uint64_t nsub, pt_timer_func_t *pttimers)
{
    int64_t res;
    _ptm_return_on_error(pttimers->init_timer(), "run_sub");
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);
    register int64_t t0 = pttimers->tick();
    __gauge_sub_intrinsic(nsub);
    res = pttimers->tock() - t0;
    return res;
}

static int
_test_ltt_ltd(uint64_t ts, uint64_t dt, uint64_t nintv, pt_timer_func_t *pttimers, pt_timer_spec_t *timer_spec, pt_gauge_info_t *gauge_info)
{
    int ret = PTERR_SUCCESS;
    uint64_t ng;
    uint64_t *pt0=NULL, *pt1=NULL, *ptmp=NULL;
    uint64_t *pt0_cdf=NULL, *pt1_cdf=NULL;
    double *wabs_all=NULL, *wrel_all=NULL, *wabs2min_all=NULL;
    int myrank=0, nrank=0;
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    MPI_Comm_size(MPI_COMM_WORLD, &nrank);

    pt0 = (uint64_t *)malloc(NMEAS * sizeof(uint64_t));
    pt1 = (uint64_t *)malloc(NMEAS * sizeof(uint64_t));
    pt0_cdf = (uint64_t *)malloc(NTILE * sizeof(uint64_t));
    pt1_cdf = (uint64_t *)malloc(NTILE * sizeof(uint64_t));
    wabs_all = (double *)malloc(nrank * sizeof(double));
    wrel_all = (double *)malloc(nrank * sizeof(double));
    wabs2min_all = (double *)malloc(nrank * sizeof(double));
    if (!pt0 || !pt1 || !pt0_cdf || !pt1_cdf || !wabs_all || !wrel_all) {
        ret = PTERR_MALLOC_FAILED;
        fprintf(stderr, "[Error] malloc failed\n");
        if (pt0) free(pt0);
        if (pt1) free(pt1);
        if (pt0_cdf) free(pt0_cdf);
        if (pt1_cdf) free(pt1_cdf);
        if (wabs_all) free(wabs_all);
        if (wrel_all) free(wrel_all);
        if (wabs2min_all) free(wabs2min_all);
        return ret;
    }

    ng = (uint64_t)((double)ts / (double)timer_spec->tick * (double)gauge_info->gpt);
    for (int i = 0; i < NMEAS; i++) {
        pt0[i] = (uint64_t)(_run_sub(ng, pttimers) - timer_spec->ovh);
    }
    qsort(pt0, NMEAS, sizeof(uint64_t), _comp_u64);
    for (int i = 0; i < NTILE; i++) {
        size_t tid = (size_t)((double)i / (double)NTILE * (double)NMEAS);
        pt0_cdf[i] = pt0[tid];
    }

    for (uint64_t n = 0; n < nintv; n++) {
        uint64_t t = ts + n * dt;
        ng = (uint64_t)((double)t / (double)timer_spec->tick * (double)gauge_info->gpt);
        // wabs: W 2 theo; wrel: W between 2 timesteps; wabs2min: wabs-min_measured_t
        double wabs = 0, wrel = 0, wabs2min=0; 

        for (int i = 0; i < NMEAS; i++) {
            pt1[i] = (uint64_t)(_run_sub(ng, pttimers) - timer_spec->ovh);
        }
        qsort(pt1, NMEAS, sizeof(uint64_t), _comp_u64);
        for (int i = 0; i < NTILE; i++) {
            size_t tid = (size_t)((double)i / (double)NTILE * (double)NMEAS);
            pt1_cdf[i] = pt1[tid];
            wabs += fabs((double)pt1_cdf[i] - (double)t);
            wabs2min += fabs((double)pt1_cdf[i] - (double)pt1_cdf[0]);
            wrel += fabs((double)pt1_cdf[i] - (double)pt0_cdf[i]);
        }
        wabs = (double)wabs / (double)NTILE / (double)t;
        wabs2min = (double)wabs2min / (double)NTILE / (double)t;
        wrel = (double)wrel / (double)NTILE / dt;
        MPI_Gather(&wabs, 1, MPI_DOUBLE, wabs_all, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        MPI_Gather(&wabs2min, 1, MPI_DOUBLE, wabs2min_all, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        MPI_Gather(&wrel, 1, MPI_DOUBLE, wrel_all, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        if (myrank == 0) {
            for (int i = 0; i < nrank; i++) {
                printf("rank %d: t=%" PRIu64 ", ng=%" PRIu64 ", wabs=%f, wabs2min=%f, wrel=%f\n", i, t, ng, wabs_all[i], wabs2min_all[i], wrel_all[i]);
            }
        }
        ptmp = pt0;
        pt0 = pt1;
        pt1 = ptmp;
        ptmp = pt0_cdf;
        pt0_cdf = pt1_cdf;
        pt1_cdf = ptmp;
        ptmp = NULL;
    }

    free(pt0);
    free(pt1);
    free(pt0_cdf);
    free(pt1_cdf);
    free(wabs_all);
    free(wabs2min_all);
    free(wrel_all);

    return ret;
}


int
main(int argc, char *argv[])
{
    int myrank, nrank, err;
    pt_gauge_info_t gauge_info;
    pt_timer_spec_t timer_spec;
    pt_timer_func_t pttimers;
    int64_t *ptick_all = NULL, *povh_all = NULL;
    double gpt_guess = 0, *pgpt_guess_all = NULL;

    pttimers.init_timer = init_timer_clock_gettime;
    pttimers.tick = tick_clock_gettime;
    pttimers.tock = tock_clock_gettime;
    pttimers.get_stamp = get_stamp_clock_gettime;
    
    /* Init MPI */
    err = MPI_Init(&argc, &argv);
    if (err != MPI_SUCCESS) {
        fprintf(stderr, "MPI_Init failed\n");
        return 1;
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    MPI_Comm_size(MPI_COMM_WORLD, &nrank);
    
    err = get_tspec(10000, &pttimers, &timer_spec);
    _ptm_exit_on_error(err, "get_tspec");
    if (myrank == 0) {
        ptick_all = (int64_t *)malloc(nrank * sizeof(int64_t));
        povh_all = (int64_t *)malloc(nrank * sizeof(int64_t));
        pgpt_guess_all = (double *)malloc(nrank * sizeof(double));
        if (!ptick_all || !povh_all || !pgpt_guess_all) {
            fprintf(stderr, "[Error] malloc failed\n");
            if (ptick_all) {
                free(ptick_all);
                ptick_all = NULL;
            }
            if (povh_all) {
                free(povh_all);
                povh_all = NULL;
            }
            if (pgpt_guess_all) {
                free(pgpt_guess_all);
                pgpt_guess_all = NULL;
            }
            MPI_Finalize();
            return 1;
        }
    }
    MPI_Gather(&timer_spec.tick, 1, MPI_INT64_T, ptick_all, 1, MPI_INT64_T, 0, MPI_COMM_WORLD);
    MPI_Gather(&timer_spec.ovh, 1, MPI_INT64_T, povh_all, 1, MPI_INT64_T, 0, MPI_COMM_WORLD);
    if (myrank == 0) {
        for (int i = 0; i < nrank; i++) {
            printf("rank %d: tick=%" PRIi64 ", ovh=%" PRIi64 "\n", i, ptick_all[i], povh_all[i]);
        }
        free(ptick_all);
        free(povh_all);
        ptick_all = NULL;
        povh_all = NULL;
    }
    
    /* Exponential guessing */
    if (myrank == 0) {
        printf("=== Exponential Guessing ===\n");
    }
    err = exp_guess_gauge(myrank, nrank, &pttimers, &timer_spec, &gpt_guess);
    if (err != PTERR_SUCCESS) {
        fprintf(stderr, "[Error] Rank %d: exp_guess_gauge failed: %d\n", myrank, err);
        MPI_Finalize();
        return err;
    }
    MPI_Gather(&gpt_guess, 1, MPI_DOUBLE, pgpt_guess_all, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    if (myrank == 0) {
        for (int i = 0; i < nrank; i++) {
            printf("rank %d: Guess gauges/tick=%lf\n", i, pgpt_guess_all[i]);
        }
        free(pgpt_guess_all);
        pgpt_guess_all = NULL;
    }


    /* Detect standard sub time */
    gauge_info.cy_per_op = 1; // TODO: what if cyc_per_op != 1?
    gauge_info.gpt = 0;
    gauge_info.wtime_per_op = 0;
    err = fit_sub_time(myrank, nrank, &pttimers, &timer_spec, &gauge_info, gpt_guess);
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

    err = _test_ltt_ltd(1e8, 1e7, 10, &pttimers, &timer_spec, &gauge_info);
    if (err != PTERR_SUCCESS) {
        fprintf(stderr, "[Error] Rank %d: _test_ltt_ltd failed: %d\n", myrank, err);
        MPI_Finalize();
        return err;
    }
EXIT:
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    return err;
}
