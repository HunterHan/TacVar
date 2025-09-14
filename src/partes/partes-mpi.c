/**
 * @file partes.c
 * @brief: The main file for partes - Parallel Timing Error Sensor.
 */
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <mpi.h>
#include <stdint.h>
#include <math.h>
#include <inttypes.h>
#include "pterr.h"
#include "partes_types.h"
#include "timers/clock_gettime.h"
#include "./kernels/kernels.h"
#include "gauges/sub.h"
#include "stat.h"



extern int get_tspec(int ntest, pt_timer_spec_t *timer_spec);
extern int parse_ptargs(int argc, char *argv[], pt_opts_t *ptopts, pt_kern_func_t *ptfuncs);
extern int exp_guess_gauge(int myrank, int nrank, pt_timer_spec_t *timer_spec, double *gpt_guess);

int 
main(int argc, char *argv[]) 
{
    int myrank = 0, nrank = 1, mpi_inited = 0;
    // Measured times and # of gauges
    int64_t **p_tmet = NULL, **p_tmet_all = NULL, ngs[2] = {0}; 
    enum pterr err = PTERR_SUCCESS;
    pt_opts_t ptopts;
    pt_kern_func_t ptfuncs;
    pt_timer_spec_t timer_spec;
    pt_gauge_info_t gauge_info;
    // Initialize MPI
    err = MPI_Init(&argc, &argv);
    if (err != MPI_SUCCESS) {
        printf("Failed to init MPI.\n");
        goto EXIT;
    }
    err = PTERR_SUCCESS;
    mpi_inited = 1;

    MPI_Comm_size(MPI_COMM_WORLD, &nrank);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

    _ptm_exit_on_error(parse_ptargs(argc, argv, &ptopts, &ptfuncs), "parse_ptargs");

    /* Initialize kernels */
    err = ptfuncs.init_fkern(ptopts.fsize_a, PT_CALL_ID_TA_FRONT, &ptopts.fsize_real_a);
    _ptm_exit_on_error(err, "init_fkern_a");
    err = ptfuncs.init_rkern(ptopts.rsize_a, PT_CALL_ID_TA_REAR, &ptopts.rsize_real_a);
    _ptm_exit_on_error(err, "init_rkern_a");
    err = ptfuncs.init_fkern(ptopts.fsize_b, PT_CALL_ID_TB_FRONT, &ptopts.fsize_real_b);
    _ptm_exit_on_error(err, "init_fkern_b");
    err = ptfuncs.init_rkern(ptopts.rsize_b, PT_CALL_ID_TB_REAR, &ptopts.rsize_real_b);
    _ptm_exit_on_error(err, "init_rkern_b");

    if (myrank == 0) {
        printf("Repeat %" PRIi64 " runtime measurements, target gauge time: %" PRIi64 
            "ns, %" PRIi64 "ns\n", ptopts.ntests, ptopts.ta, ptopts.tb);
        printf("Timer: %d\n", ptopts.timer);
        printf("ta flush info:\n");
        printf("Front kernel: %s, size: %zu KiB, real size: %zu KiB\n", 
            ptopts.fkern_name, ptopts.fsize_a, ptopts.fsize_real_a);
        printf("Rear kernel: %s, size: %zu KiB, real size: %zu KiB\n", 
            ptopts.rkern_name, ptopts.rsize_a, ptopts.rsize_real_a);
        printf("tb flush info:\n");
        printf("Front kernel: %s, size: %zu KiB, real size: %zu KiB\n", 
            ptopts.fkern_name, ptopts.fsize_b, ptopts.fsize_real_b);
        printf("Rear kernel: %s, size: %zu KiB, real size: %zu KiB\n", 
            ptopts.rkern_name, ptopts.rsize_b, ptopts.rsize_real_b);
    }

    /* Step 1: Get the minimum overhead and time per tick */
    err = get_tspec(10000, &timer_spec);
    _ptm_exit_on_error(err, "get_tspec");
    pt_mpi_printf(myrank, nrank, "Timer spec: tick=%" PRIi64 ", ovh=%" PRIi64 "\n", timer_spec.tick, timer_spec.ovh);

    /* Step 2: Detect theoretical time of the gauge kernel */
    gauge_info.cy_per_op = 0;
    gauge_info.gpt = 0.0;
    gauge_info.wtime_per_op = 0.0;
    err = exp_guess_gauge(myrank, nrank, &timer_spec, &gauge_info.gpt);
    _ptm_exit_on_error(err, "exp_guess_gauge");
    pt_mpi_printf(myrank, nrank, "Gauge info: gpt=%.6f\n", gauge_info.gpt);
    
        /* Step 3: Run the timing error sensor */
    MPI_Barrier(MPI_COMM_WORLD);
    // Allocate arrays for timing measurements
    p_tmet = (int64_t **)malloc(2 * sizeof(int64_t *));
    if (p_tmet == NULL) {
        err = PTERR_MALLOC_FAILED;
        _ptm_exit_on_error(err, "main:malloc");
    }
    for (int i = 0; i < 2; i++) {
        p_tmet[i] = NULL;
        p_tmet[i] = (int64_t *)malloc(ptopts.ntests * sizeof(int64_t));
        if (p_tmet[i] == NULL) {
            err = PTERR_MALLOC_FAILED;
            _ptm_exit_on_error(err, "main:malloc");
        }
    }
    if (myrank == 0) {
        p_tmet_all = (int64_t **)malloc(2 * sizeof(int64_t *));
        if (p_tmet_all == NULL) {
            err = PTERR_MALLOC_FAILED;
            _ptm_exit_on_error(err, "main:malloc");
        }
        for (int i = 0; i < 2; i++) {
            p_tmet_all[i] = NULL;
            p_tmet_all[i] = (int64_t *)malloc(ptopts.ntests * nrank * sizeof(int64_t));
            if (p_tmet_all[i] == NULL) {
                err = PTERR_MALLOC_FAILED;
                _ptm_exit_on_error(err, "main:malloc");
            }
        }
    }

    ngs[0] = (int64_t)((double)ptopts.ta / (double)timer_spec.tick) * gauge_info.gpt;
    ngs[1] = (int64_t)((double)ptopts.tb / (double)timer_spec.tick) * gauge_info.gpt;

    MPI_Barrier(MPI_COMM_WORLD);
    if (myrank == 0) {
        fflush(stdout);
        printf("t0 = %" PRIi64 ", number of gauges: %" PRIi64 "\n"
            "t1 = %" PRIi64 ", number of gauges: %" PRIi64 "\n", ptopts.ta, ngs[0], ptopts.tb, ngs[1]);
    }

    __timer_init_clock_gettime;
    MPI_Barrier(MPI_COMM_WORLD);
    for (int i = 0; i < ptopts.ntests; i++) {
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
        ptfuncs.run_fkern(PT_CALL_ID_TA_FRONT);
        __timer_tick_clock_gettime;
        __gauge_sub_intrinsic(ngs[0]);
        __timer_tock_clock_gettime(p_tmet[0][i]);
        ptfuncs.run_rkern(PT_CALL_ID_TA_REAR);
        ptfuncs.update_fkern_key(PT_CALL_ID_TA_FRONT);
        ptfuncs.update_rkern_key(PT_CALL_ID_TA_REAR);
    }

    for (int i = 0; i < ptopts.ntests; i++) {
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
        ptfuncs.run_fkern(PT_CALL_ID_TB_FRONT);
        __timer_tick_clock_gettime;
        __gauge_sub_intrinsic(ngs[1]);
        __timer_tock_clock_gettime(p_tmet[1][i]);
        ptfuncs.run_rkern(PT_CALL_ID_TB_REAR);
        ptfuncs.update_fkern_key(PT_CALL_ID_TB_FRONT);
        ptfuncs.update_rkern_key(PT_CALL_ID_TB_REAR);
    }
    double perc_gap_ta_front, perc_gap_ta_rear, perc_gap_tb_front, perc_gap_tb_rear;
    
    ptfuncs.check_fkern_key(PT_CALL_ID_TA_FRONT, ptopts.ntests, &perc_gap_ta_front);
    if (myrank == 0) {
        printf("TA Front kernel percentage gap: %f\n", perc_gap_ta_front);
    }
    ptfuncs.check_rkern_key(PT_CALL_ID_TA_REAR, ptopts.ntests, &perc_gap_ta_rear);
    if (myrank == 0) {
        printf("TA Rear kernel percentage gap: %f\n", perc_gap_ta_rear);
    }
    ptfuncs.check_fkern_key(PT_CALL_ID_TB_FRONT, ptopts.ntests, &perc_gap_tb_front);
    if (myrank == 0) {
        printf("TB Front kernel percentage gap: %f\n", perc_gap_tb_front);
    }
    ptfuncs.check_rkern_key(PT_CALL_ID_TB_REAR, ptopts.ntests, &perc_gap_tb_rear);
    if (myrank == 0) {
        printf("TB Rear kernel percentage gap: %f\n", perc_gap_tb_rear);
    }
    /* Step 4: Calculate Wasserstein distance */
    for (int i = 0; i < 2; i++) {
        if (myrank == 0) {
            MPI_Gather(p_tmet[i], ptopts.ntests, MPI_INT64_T, p_tmet_all[i], 
                ptopts.ntests, MPI_INT64_T, 0, MPI_COMM_WORLD);
        } else {
            MPI_Gather(p_tmet[i], ptopts.ntests, MPI_INT64_T, NULL, 
                0, MPI_INT64_T, 0, MPI_COMM_WORLD);
        }
    }
    if (myrank == 0) {
        int64_t p_cdf[2][ptopts.ntiles];
        double w;

        calc_cdf_i64(p_tmet_all[0], ptopts.ntests * nrank, p_cdf[0], ptopts.ntiles);
        calc_cdf_i64(p_tmet_all[1], ptopts.ntests * nrank, p_cdf[1], ptopts.ntiles);
        calc_w(p_cdf[0], p_cdf[1], ptopts.ntiles, ptopts.cut_p, &w);
        printf("Percentage cut: %f\nTime gap: %" PRIi64 "ns\n", ptopts.cut_p, ptopts.tb - ptopts.ta);
        printf("Percentile, Gap\n");
        printf("0, %" PRIi64 "\n", p_cdf[1][0] - p_cdf[0][0]);
        printf("50, %" PRIi64 "\n", p_cdf[1][(int)(ptopts.ntiles * 0.5)] - p_cdf[0][(int)(ptopts.ntiles * 0.5)]);
        printf("75, %" PRIi64 "\n", p_cdf[1][(int)(ptopts.ntiles * 0.75)] - p_cdf[0][(int)(ptopts.ntiles * 0.75)]);
        printf("90, %" PRIi64 "\n", p_cdf[1][(int)(ptopts.ntiles * 0.9)] - p_cdf[0][(int)(ptopts.ntiles * 0.9)]);
        printf("95, %" PRIi64 "\n", p_cdf[1][(int)(ptopts.ntiles * 0.95)] - p_cdf[0][(int)(ptopts.ntiles * 0.95)]);
        printf("99, %" PRIi64 "\n", p_cdf[1][(int)(ptopts.ntiles * 0.99)] - p_cdf[0][(int)(ptopts.ntiles * 0.99)]);
        printf("100, %" PRIi64 "\n", p_cdf[1][(int)(ptopts.ntiles * 1.0)] - p_cdf[0][(int)(ptopts.ntiles * 1.0)]);
        printf("Wasserstein distance: %f\n", w);
    }
    FILE *fp_a = NULL, *fp_b = NULL;
    char fp_a_name[1024], fp_b_name[1024];
    sprintf(fp_a_name, "partes_ta_r%d.csv", myrank);
    sprintf(fp_b_name, "partes_tb_r%d.csv", myrank);
    fp_a = fopen(fp_a_name, "w");
    if (!fp_a) {
        err = PTERR_FILE_OPEN_FAILED;
        _ptm_exit_on_error(err, "main:fopen");
    }
    fp_b = fopen(fp_b_name, "w");
    if (!fp_b) {
        err = PTERR_FILE_OPEN_FAILED;
        fclose(fp_a);
        fp_a = NULL;
        _ptm_exit_on_error(err, "main:fopen");
    }
    for (int i = 0; i < ptopts.ntests; i++) {
        fprintf(fp_a, "%" PRIi64 "\n", p_tmet[0][i]);
        fprintf(fp_b, "%" PRIi64 "\n", p_tmet[1][i]);
    }
    fclose(fp_a);
    fclose(fp_b);
    fp_a = NULL;
    fp_b = NULL;
    MPI_Barrier(MPI_COMM_WORLD);

EXIT:
    /* Cleanup measured arrays */
    if (p_tmet) {
        for (int i = 0; i < 2; i++) {
            if (p_tmet[i]) {
                free(p_tmet[i]);
                p_tmet[i] = NULL;
            }
        }
        free(p_tmet);
        p_tmet = NULL;
    }
    if (myrank == 0) {
        if (p_tmet_all) {
            for (int i = 0; i < 2; i++) {
                if (p_tmet_all[i]) {
                    free(p_tmet_all[i]);
                    p_tmet_all[i] = NULL;
                }
            }
            free(p_tmet_all);
            p_tmet_all = NULL;
        }
    }

    /* Cleanup kernels */
    ptfuncs.cleanup_fkern(PT_CALL_ID_TA_FRONT);
    ptfuncs.cleanup_rkern(PT_CALL_ID_TA_REAR);
    ptfuncs.cleanup_fkern(PT_CALL_ID_TB_FRONT);
    ptfuncs.cleanup_rkern(PT_CALL_ID_TB_REAR);

    if (mpi_inited) {
        MPI_Finalize();
    }
    return err;
}