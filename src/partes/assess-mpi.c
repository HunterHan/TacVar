/**
 * @file assess-mpi.c
 * @brief Gauge-based ta-only timer fluctuation assessment.
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
#include <string.h>
#include "pterr.h"
#include "partes_types.h"
#include "stat.h"

#ifndef __PTM_NOP
#define __PTM_NOP __asm__ __volatile__ ("nop");
#endif

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
#define __PTM_MFENCE __asm__ __volatile__ ("" ::: "memory");
#endif

extern int parse_ptargs(int argc, char *argv[], pt_opts_t *ptopts,
    pt_kern_func_t *ptfuncs, pt_timer_func_t *pttimers, pt_gauge_func_t *ptgauges);
extern int exp_fit_gpns(int ntest, int64_t tmax,
    pt_timer_func_t *pttimers, pt_gauge_func_t *ptgauges, double *gpns);

static int
build_assess_args(int argc, char **argv, int *assess_argc, char ***assess_argv)
{
    int has_ta = 0, has_tb = 0;
    char *ta_value = NULL;

    *assess_argc = argc;
    *assess_argv = argv;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--ta") == 0 && i + 1 < argc) {
            has_ta = 1;
            ta_value = argv[i + 1];
            i++;
        } else if (strcmp(argv[i], "--tb") == 0) {
            has_tb = 1;
            if (i + 1 < argc) {
                i++;
            }
        }
    }

    if (!has_ta || has_tb) {
        return PTERR_SUCCESS;
    }

    *assess_argv = (char **)malloc((argc + 3) * sizeof(char *));
    if (!*assess_argv) {
        return PTERR_MALLOC_FAILED;
    }
    for (int i = 0; i < argc; i++) {
        (*assess_argv)[i] = argv[i];
    }
    (*assess_argv)[argc] = "--tb";
    (*assess_argv)[argc + 1] = ta_value;
    (*assess_argv)[argc + 2] = NULL;
    *assess_argc = argc + 2;
    return PTERR_SUCCESS;
}

static void
write_rank_csv(const char *prefix, int rank, int64_t *data, int64_t ntests, enum pterr *err)
{
    char fname[1024];
    snprintf(fname, sizeof(fname), "%s_r%d.csv", prefix, rank);
    FILE *fp = fopen(fname, "w");
    if (!fp) {
        *err = PTERR_FILE_OPEN_FAILED;
        return;
    }
    for (int64_t i = 0; i < ntests; i++) {
        fprintf(fp, "%" PRIi64 "\n", data[i]);
    }
    fclose(fp);
}

int
main(int argc, char *argv[])
{
    int myrank = 0, nrank = 1, mpi_inited = 0;
    int64_t *tmet = NULL, *tmet_all = NULL, ng = 0;
    int assess_argc = argc;
    char **assess_argv = argv;
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

    _ptm_exit_on_error(build_assess_args(argc, argv, &assess_argc, &assess_argv), "build_assess_args");
    _ptm_exit_on_error(parse_ptargs(assess_argc, assess_argv, &ptopts, &ptfuncs, &pttimers, &ptgauges), "parse_ptargs");

    err = ptfuncs.init_fkern_a(ptopts.fsize_a, PT_CALL_ID_TA_FRONT, &ptopts.fsize_real_a);
    _ptm_exit_on_error(err, "init_fkern_a");
    err = ptfuncs.init_rkern_a(ptopts.rsize_a, PT_CALL_ID_TA_REAR, &ptopts.rsize_real_a);
    _ptm_exit_on_error(err, "init_rkern_a");
    _ptm_exit_on_error(ptgauges.init_gauge(), "init_gauge");

    if (myrank == 0) {
        printf("Assess ta-only runtime measurements: ta=%" PRIi64 "ns, ntests=%" PRIi64 "\n",
            ptopts.ta, ptopts.ntests);
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

    gauge_info.gpns = 0.0;
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

    tmet = (int64_t *)malloc(ptopts.ntests * sizeof(int64_t));
    if (!tmet) {
        err = PTERR_MALLOC_FAILED;
        _ptm_exit_on_error(err, "main:malloc");
    }
    if (myrank == 0) {
        tmet_all = (int64_t *)malloc(ptopts.ntests * nrank * sizeof(int64_t));
        if (!tmet_all) {
            err = PTERR_MALLOC_FAILED;
            _ptm_exit_on_error(err, "main:malloc");
        }
    }

    ng = (int64_t)((double)ptopts.ta * gauge_info.gpns);
    if (ng <= 0) {
        fprintf(stderr,
            "[ERROR][Rank %d] Invalid ng: gpns=%f ta=%" PRIi64 " => ng=%" PRIi64 "\n",
            myrank, gauge_info.gpns, ptopts.ta, ng);
        fflush(stderr);
        MPI_Abort(MPI_COMM_WORLD, PTERR_INVALID_ARGUMENT);
    }

    if (myrank == 0) {
        printf("ta = %" PRIi64 ", number of gauges: %" PRIi64 "\n", ptopts.ta, ng);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    for (int64_t i = 0; i < ptopts.ntests; i++) {
        __PTM_NOP;
        MPI_Barrier(MPI_COMM_WORLD);
        __PTM_MFENCE;
        MPI_Barrier(MPI_COMM_WORLD);
        ptfuncs.run_fkern_a(PT_CALL_ID_TA_FRONT);
        int64_t t0 = pttimers.tick();
        ptgauges.run_gauge(ng);
        tmet[i] = pttimers.tock() - t0;
        ptfuncs.run_rkern_a(PT_CALL_ID_TA_REAR);
        ptfuncs.update_fkern_a_key(PT_CALL_ID_TA_FRONT);
        ptfuncs.update_rkern_a_key(PT_CALL_ID_TA_REAR);
    }

    double perc_gap_ta_front = 0.0, perc_gap_ta_rear = 0.0;
    ptfuncs.check_fkern_a_key(PT_CALL_ID_TA_FRONT, ptopts.ntests, &perc_gap_ta_front);
    ptfuncs.check_rkern_a_key(PT_CALL_ID_TA_REAR, ptopts.ntests, &perc_gap_ta_rear);
    if (myrank == 0) {
        printf("TA Front kernel percentage gap: %.6f%%\n", perc_gap_ta_front);
        printf("TA Rear kernel percentage gap: %.6f%%\n", perc_gap_ta_rear);
    }

    MPI_Gather(tmet, ptopts.ntests, MPI_INT64_T,
        myrank == 0 ? tmet_all : NULL, ptopts.ntests, MPI_INT64_T,
        0, MPI_COMM_WORLD);

    if (myrank == 0) {
        int64_t *cdf = (int64_t *)malloc(ptopts.ntiles * sizeof(int64_t));
        FILE *fp = NULL;
        if (!cdf) {
            err = PTERR_MALLOC_FAILED;
            _ptm_exit_on_error(err, "main:malloc");
        }
        calc_cdf_i64(tmet_all, ptopts.ntests * nrank, cdf, ptopts.ntiles);
        printf("Quantile, Assess(Ta), Assess(Ta)-Ta\n");
        printf("0, %" PRIi64 ", %" PRIi64 "\n", cdf[0], cdf[0] - ptopts.ta);
        printf("50, %" PRIi64 ", %" PRIi64 "\n",
            cdf[(int)(ptopts.ntiles * 0.5)], cdf[(int)(ptopts.ntiles * 0.5)] - ptopts.ta);
        printf("75, %" PRIi64 ", %" PRIi64 "\n",
            cdf[(int)(ptopts.ntiles * 0.75)], cdf[(int)(ptopts.ntiles * 0.75)] - ptopts.ta);
        printf("90, %" PRIi64 ", %" PRIi64 "\n",
            cdf[(int)(ptopts.ntiles * 0.9)], cdf[(int)(ptopts.ntiles * 0.9)] - ptopts.ta);
        printf("95, %" PRIi64 ", %" PRIi64 "\n",
            cdf[(int)(ptopts.ntiles * 0.95)], cdf[(int)(ptopts.ntiles * 0.95)] - ptopts.ta);
        printf("99, %" PRIi64 ", %" PRIi64 "\n",
            cdf[(int)(ptopts.ntiles * 0.99)], cdf[(int)(ptopts.ntiles * 0.99)] - ptopts.ta);
        printf("100, %" PRIi64 ", %" PRIi64 "\n",
            cdf[ptopts.ntiles - 1], cdf[ptopts.ntiles - 1] - ptopts.ta);

        fp = fopen("assess_ta_cdf.csv", "w");
        if (!fp) {
            free(cdf);
            err = PTERR_FILE_OPEN_FAILED;
            _ptm_exit_on_error(err, "main:fopen");
        }
        for (int i = 0; i < ptopts.ntiles; i++) {
            fprintf(fp, "%" PRIi64 "\n", cdf[i]);
        }
        fclose(fp);

        fp = fopen("partes_ta_cdf.csv", "w");
        if (fp) {
            for (int i = 0; i < ptopts.ntiles; i++) {
                fprintf(fp, "%" PRIi64 "\n", cdf[i]);
            }
            fclose(fp);
        }
        free(cdf);
    }

    write_rank_csv("assess_ta", myrank, tmet, ptopts.ntests, &err);
    _ptm_exit_on_error(err, "write_rank_csv");
    write_rank_csv("partes_ta", myrank, tmet, ptopts.ntests, &err);
    _ptm_exit_on_error(err, "write_rank_csv");

    MPI_Barrier(MPI_COMM_WORLD);

EXIT:
    if (tmet) {
        free(tmet);
        tmet = NULL;
    }
    if (myrank == 0 && tmet_all) {
        free(tmet_all);
        tmet_all = NULL;
    }

    ptfuncs.cleanup_fkern_a(PT_CALL_ID_TA_FRONT);
    ptfuncs.cleanup_rkern_a(PT_CALL_ID_TA_REAR);
    ptgauges.cleanup_gauge();

    if (mpi_inited) {
        MPI_Finalize();
    }
    if (assess_argv != argv) {
        free(assess_argv);
    }
    return err;
}
