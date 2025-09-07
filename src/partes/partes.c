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
#include "./kernels/kernels.h"
#include "gauges/sub.h"



extern int get_tspec(int ntest, pt_timer_spec_t *timer_spec);

extern int parse_ptargs(int argc, char *argv[], pt_opts_t *ptopts, pt_kern_func_t *ptfuncs);
extern void calc_w(int64_t *tm_arr, uint64_t tm_len, int64_t *sim_cdf, int64_t *w_arr, double *wp_arr, double p_zcut);
extern int exp_guess_gauge(int myrank, int nrank, pt_timer_spec_t *timer_spec, double *gpt_guess);

int 
main(int argc, char *argv[]) 
{
    int myrank = 0, nrank = 1;
    enum pterr err;
    pt_opts_t ptopts;
    pt_kern_func_t ptfuncs;
    pt_timer_spec_t timer_spec;
    pt_gauge_info_t gauge_info;
    
    // Initialize MPI
    err = MPI_Init(&argc, &argv);
    if (err != MPI_SUCCESS) {
        printf("Failed to init MPI.\n");
        return 1;
    }
    MPI_Comm_size(MPI_COMM_WORLD, &nrank);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

    err = parse_ptargs(argc, argv, &ptopts, &ptfuncs);

    if (myrank == 0) {
        printf("Repeat %" PRIi64 " runtime measurements, target gauge time: %" PRIi64 
            "ns, %" PRIi64 "ns\n", ptopts.ntests, ptopts.ta, ptopts.tb);
        printf("Front kernel: %d, size: %zu KiB\n", ptopts.fkern, ptopts.fsize);
        printf("Rear kernel: %d, size: %zu KiB\n", ptopts.rkern, ptopts.rsize);
        printf("Timer: %d\n", ptopts.timer);
    }

    /* Initialize kernels */
    size_t fsize_real = 0, rsize_real = 0;
    if (ptfuncs.init_fkern) {
        ptfuncs.init_fkern(ptopts.fsize, PT_CALL_ID_FRONT, &fsize_real);
    }
    
    if (ptfuncs.init_rkern) {
        ptfuncs.init_rkern(ptopts.rsize, PT_CALL_ID_REAR, &rsize_real);
    }

    /* Step 1: Get the minimum overhead and time per tick */
    err = get_tspec(10000, &timer_spec);
    _ptm_handle_error(err, "get_tspec");
    pt_mpi_printf(myrank, nrank, "Timer spec: tick=%" PRIi64 ", ovh=%" PRIi64 "\n", timer_spec.tick, timer_spec.ovh);

    /* Step 2: Detect theoretical time of the gauge kernel */
    gauge_info.cy_per_op = 0;
    gauge_info.gpt = 0.0;
    gauge_info.wtime_per_op = 0.0;
    err = exp_guess_gauge(myrank, nrank, &timer_spec, &gauge_info.gpt);
    _ptm_handle_error(err, "exp_guess_gauge");
    pt_mpi_printf(myrank, nrank, "Gauge info: gpt=%.6f\n", gauge_info.gpt);
    
    
    /* Step 3: Run the timing error sensor */
    MPI_Barrier(MPI_COMM_WORLD);
    
    // Allocate arrays for timing measurements
    int64_t ngs[2] = {0};

    ngs[0] = (int64_t)((double)ptopts.ta / (double)timer_spec.tick) * gauge_info.gpt;
    ngs[1] = (int64_t)((double)ptopts.tb / (double)timer_spec.tick) * gauge_info.gpt;
    
    /* Cleanup kernels */
    if (ptfuncs.cleanup_fkern) {
        ptfuncs.cleanup_fkern(PT_CALL_ID_FRONT);
    }
    
    if (ptfuncs.cleanup_rkern) {
        ptfuncs.cleanup_rkern(PT_CALL_ID_REAR);
    }

    MPI_Finalize();
    return 0;
}