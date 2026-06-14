/**
 * @file detecing-mpi.0614.c
 * @brief: Use function pointer to set gauge kernel, timer and flush kernel.
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
#include <inttypes.h>
#include "pterr.h"
#include "partes_types.h"
#include "stat.h"

#ifndef __PTM_NOP
#define __PTM_NOP __asm__ __volatile__ ("nop");
#endif

/* Memory fence for different ISAs */
#if defined(__x86_64__) || defined(__i386__)
#define __PTM_MFENCE __asm__ __volatile__ ("mfence" ::: "memory");
#elif defined(__aarch64__) || defined(__arm__)
#define __PTM_MFENCE __asm__ __volatile__ ("dmb sy" ::: "memory");
#elif defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
#define __PTM_MFENCE __asm__ __volatile__ ("sync" ::: "memory");
#elif defined(__riscv)
#define __PTM_MFENCE __asm__ __volatile__ ("fence rw,rw" ::: "memory");
#elif defined(__s390x__)
#define __PTM_MFENCE __asm__ __volatile__ ("bcr 15,0" ::: "memory");
#elif defined(__sparc__)
#define __PTM_MFENCE __asm__ __volatile__ ("membar #Sync" ::: "memory");
#elif defined(__alpha__)
#define __PTM_MFENCE __asm__ __volatile__ ("mb" ::: "memory");
#elif defined(__ia64__)
#define __PTM_MFENCE __asm__ __volatile__ ("mf" ::: "memory");
#else
/* Fallback to compiler barrier */
#define __PTM_MFENCE __asm__ __volatile__ ("" ::: "memory");
#endif


extern int get_tspec(int ntest, pt_timer_func_t *pttimers, pt_timer_spec_t *timer_spec);
extern int parse_ptargs(int argc, char *argv[], pt_opts_t *ptopts, pt_kern_func_t *ptfuncs, pt_timer_func_t *pttimers, pt_gauge_func_t *ptgauges);
extern int exp_fit_gpns(int ntest, int64_t tmax, pt_timer_func_t *pttimers, pt_gauge_func_t *ptgauges, double *gpns);


int
main(int argc, char *argv[])
{
    int myrank = 0, nrank = 1, mpi_inited = 0;
    int64_t *p_tmet = NULL, *p_tmet_all = NULL, ngs = 0;
    enum pterr err = PTERR_SUCCESS;
    pt_opts_t ptopts;
    pt_kern_func_t ptfuncs;
    pt_timer_func_t pttimers;
    pt_gauge_func_t ptgauges;
    pt_gauge_info_t gauge_info;

    err = MPI_Init(&argc, &argv);
    if (err != MPI_SUCCESS) {
        printf("Failed to init MPI.\n");
        goto EXIT;
    }
    err = PTERR_SUCCESS;
    mpi_inited = 1;

    MPI_Comm_size(MPI_COMM_WORLD, &nrank);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

    _ptm_exit_on_error(parse_ptargs(argc, argv, &ptopts, &ptfuncs, &pttimers, &ptgauges), "parse_ptargs");

    err = ptfuncs.init_fkern_a(ptopts.fsize_a, PT_CALL_ID_TA_FRONT, &ptopts.fsize_real_a);
    _ptm_exit_on_error(err, "init_fkern_a");
    err = ptfuncs.init_rkern_a(ptopts.rsize_a, PT_CALL_ID_TA_REAR, &ptopts.rsize_real_a);
    _ptm_exit_on_error(err, "init_rkern_a");

    _ptm_exit_on_error(ptgauges.init_gauge(), "init_gauge");

    if (myrank == 0) {
        printf("Repeat %" PRIi64 " runtime measurements, target gauge time: %" PRIi64 "ns\n",
            ptopts.ntests, ptopts.ta);
        printf("Timer: %s\n", ptopts.timer_name);
        printf("Gauge: %s\n", ptopts.gauge_name);
        printf("ta flush info:\n");
        printf("Front kernel: %s, size: %zu KiB, real size: %zu KiB\n",
            ptopts.fkern_a_name, ptopts.fsize_a, ptopts.fsize_real_a);
        printf("Rear kernel: %s, size: %zu KiB, real size: %zu KiB\n",
            ptopts.rkern_a_name, ptopts.rsize_a, ptopts.rsize_real_a);
        fflush(stdout);
    }
    MPI_Barrier(MPI_COMM_WORLD);

    gauge_info.cy_per_op = 0;
    gauge_info.gpt = 0.0;
    gauge_info.gpns = 0.0;
    gauge_info.wtime_per_op = 0.0;
    _ptm_exit_on_error(pttimers.init_timer(), "init_timer");
    err = exp_fit_gpns(100, 100000LL, &pttimers, &ptgauges, &gauge_info.gpns);
    _ptm_exit_on_error(err, "exp_fit_gpns");
    pt_mpi_printf(myrank, nrank, "Gauge info: gpns=%f\n", gauge_info.gpns);
    if (myrank == 0) {
        printf("Estimated gauge time per operation: %f ns\n", gauge_info.gpns);
        fflush(stdout);
    }
    if (!(gauge_info.gpns > 0.0)) {
        fprintf(stderr, "[ERROR][Rank %d] Invalid gpns=%f; abort to avoid hang.\n",
                myrank, gauge_info.gpns);
        fflush(stderr);
        MPI_Abort(MPI_COMM_WORLD, PTERR_TIMER_INIT_FAILED);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    p_tmet = (int64_t *)malloc(ptopts.ntests * sizeof(int64_t));
    if (p_tmet == NULL) {
        err = PTERR_MALLOC_FAILED;
        _ptm_exit_on_error(err, "main:malloc");
    }
    if (myrank == 0) {
        p_tmet_all = (int64_t *)malloc(ptopts.ntests * nrank * sizeof(int64_t));
        if (p_tmet_all == NULL) {
            err = PTERR_MALLOC_FAILED;
            _ptm_exit_on_error(err, "main:malloc");
        }
    }

    ngs = (int64_t)((double)ptopts.ta * gauge_info.gpns);
    if (ngs <= 0) {
        fprintf(stderr,
                "[ERROR][Rank %d] Invalid ngs: gpns=%f ta=%" PRIi64
                " => ngs=%" PRIi64 " ; abort to avoid hang.\n",
                myrank, gauge_info.gpns, ptopts.ta, ngs);
        fflush(stderr);
        MPI_Abort(MPI_COMM_WORLD, PTERR_INVALID_ARGUMENT);
    }

    if (myrank == 0) {
        fflush(stdout);
        printf("ta = %" PRIi64 ", number of gauges: %" PRIi64 "\n", ptopts.ta, ngs);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    for (int i = 0; i < ptopts.ntests; i++) {
        __PTM_NOP;
        MPI_Barrier(MPI_COMM_WORLD);
        __PTM_MFENCE;
        MPI_Barrier(MPI_COMM_WORLD);
        ptfuncs.run_fkern_a(PT_CALL_ID_TA_FRONT);
        register int64_t t0 = pttimers.tick();
        ptgauges.run_gauge(ngs);
        p_tmet[i] = pttimers.tock() - t0;
        ptfuncs.run_rkern_a(PT_CALL_ID_TA_REAR);
        ptfuncs.update_fkern_a_key(PT_CALL_ID_TA_FRONT);
        ptfuncs.update_rkern_a_key(PT_CALL_ID_TA_REAR);
    }

    double perc_gap_ta_front, perc_gap_ta_rear;

    ptfuncs.check_fkern_a_key(PT_CALL_ID_TA_FRONT, ptopts.ntests, &perc_gap_ta_front);
    if (myrank == 0) {
        printf("TA Front kernel percentage gap: %f%%\n", perc_gap_ta_front);
    }
    ptfuncs.check_rkern_a_key(PT_CALL_ID_TA_REAR, ptopts.ntests, &perc_gap_ta_rear);
    if (myrank == 0) {
        printf("TA Rear kernel percentage gap: %f%%\n", perc_gap_ta_rear);
    }

    if (myrank == 0) {
        MPI_Gather(p_tmet, ptopts.ntests, MPI_INT64_T, p_tmet_all,
            ptopts.ntests, MPI_INT64_T, 0, MPI_COMM_WORLD);
    } else {
        MPI_Gather(p_tmet, ptopts.ntests, MPI_INT64_T, NULL,
            0, MPI_INT64_T, 0, MPI_COMM_WORLD);
    }
    if (myrank == 0) {
        int64_t p_cdf[ptopts.ntiles];
        FILE *fp_ta_cdf = NULL;

        calc_cdf_i64(p_tmet_all, ptopts.ntests * nrank, p_cdf, ptopts.ntiles);
        printf("Percentage cut: %f\n", ptopts.cut_p);
        printf("Quantile, W(Ta)\n");
        printf("0, %" PRIi64 "\n", p_cdf[0]);
        printf("50, %" PRIi64 "\n", p_cdf[(int)(ptopts.ntiles * 0.5)]);
        printf("75, %" PRIi64 "\n", p_cdf[(int)(ptopts.ntiles * 0.75)]);
        printf("90, %" PRIi64 "\n", p_cdf[(int)(ptopts.ntiles * 0.9)]);
        printf("95, %" PRIi64 "\n", p_cdf[(int)(ptopts.ntiles * 0.95)]);
        printf("99, %" PRIi64 "\n", p_cdf[(int)(ptopts.ntiles * 0.99)]);
        printf("100, %" PRIi64 "\n", p_cdf[ptopts.ntiles - 1]);

        fp_ta_cdf = fopen("partes_ta_cdf.csv", "w");
        if (!fp_ta_cdf) {
            err = PTERR_FILE_OPEN_FAILED;
            _ptm_exit_on_error(err, "main:fopen");
        }
        for (int i = 0; i < ptopts.ntiles; i++) {
            fprintf(fp_ta_cdf, "%" PRIi64 "\n", p_cdf[i]);
        }
        fclose(fp_ta_cdf);
        fp_ta_cdf = NULL;
    }

    FILE *fp_a = NULL;
    char fp_a_name[1024];
    sprintf(fp_a_name, "partes_ta_r%d.csv", myrank);
    fp_a = fopen(fp_a_name, "w");
    if (!fp_a) {
        err = PTERR_FILE_OPEN_FAILED;
        _ptm_exit_on_error(err, "main:fopen");
    }
    for (int i = 0; i < ptopts.ntests; i++) {
        fprintf(fp_a, "%" PRIi64 "\n", p_tmet[i]);
    }
    fclose(fp_a);
    fp_a = NULL;
    MPI_Barrier(MPI_COMM_WORLD);

EXIT:
    if (p_tmet) {
        free(p_tmet);
        p_tmet = NULL;
    }
    if (myrank == 0) {
        if (p_tmet_all) {
            free(p_tmet_all);
            p_tmet_all = NULL;
        }
    }

    ptfuncs.cleanup_fkern_a(PT_CALL_ID_TA_FRONT);
    ptfuncs.cleanup_rkern_a(PT_CALL_ID_TA_REAR);
    ptgauges.cleanup_gauge();

    if (mpi_inited) {
        MPI_Finalize();
    }
    return err;
}
