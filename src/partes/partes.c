/**
 * @file partes.c
 * @brief: The main file for partes - Parallel Timing Error Sensor.
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <mpi.h>
#include <stdint.h>
#include <math.h>
#include "pterr.h"
#include "partes_types.h"
#include "./kernels/kernels.h"

#define _ptm_handle_error(err, fname) \
    if (err != PTERR_SUCCESS) { \
        printf("In %s: %s\n", fname, get_pterr_str(err)); \
        return 0; \
    }

extern int stress_timer(int ntpint, int nint, int tint, int nwint, long long *tick, long long *ovh);

extern int parse_ptargs(int argc, char *argv[], pt_test_options_t *ptopts, pt_kern_func_t *ptfuncs);

// Function to set kernel function pointers based on kernel types
void 
set_kernel_functions(pt_test_options_t *ptopts, pt_kern_func_t *ptfuncs) 
{
    // Initialize all function pointers to NULL
    ptfuncs->init_fkern = NULL;
    ptfuncs->run_fkern = NULL;
    ptfuncs->cleanup_fkern = NULL;
    ptfuncs->init_rkern = NULL;
    ptfuncs->run_rkern = NULL;
    ptfuncs->cleanup_rkern = NULL;

    // Set front kernel functions
    switch (ptopts->fkern) {
        case KERN_NONE:
            ptfuncs->init_fkern = init_kern_none;
            ptfuncs->run_fkern = run_kern_none;
            ptfuncs->cleanup_fkern = cleanup_kern_none;
            break;
        case KERN_TRIAD:
            ptfuncs->init_fkern = init_kern_triad;
            ptfuncs->run_fkern = run_kern_triad;
            ptfuncs->cleanup_fkern = cleanup_kern_triad;
            break;
        case KERN_SCALE:
            ptfuncs->init_fkern = init_kern_scale;
            ptfuncs->run_fkern = run_kern_scale;
            ptfuncs->cleanup_fkern = cleanup_kern_scale;
            break;
        case KERN_COPY:
            ptfuncs->init_fkern = init_kern_copy;
            ptfuncs->run_fkern = run_kern_copy;
            ptfuncs->cleanup_fkern = cleanup_kern_copy;
            break;
        case KERN_ADD:
            ptfuncs->init_fkern = init_kern_add;
            ptfuncs->run_fkern = run_kern_add;
            ptfuncs->cleanup_fkern = cleanup_kern_add;
            break;
        case KERN_POW:
            ptfuncs->init_fkern = init_kern_pow;
            ptfuncs->run_fkern = run_kern_pow;
            ptfuncs->cleanup_fkern = cleanup_kern_pow;
            break;
        case KERN_DGEMM:
            ptfuncs->init_fkern = init_kern_dgemm;
            ptfuncs->run_fkern = run_kern_dgemm;
            ptfuncs->cleanup_fkern = cleanup_kern_dgemm;
            break;
        case KERN_MPI_BCAST:
            ptfuncs->init_fkern = init_kern_bcast;
            ptfuncs->run_fkern = run_kern_bcast;
            ptfuncs->cleanup_fkern = cleanup_kern_bcast;
            break;
    }

    // Set rear kernel functions
    switch (ptopts->rkern) {
        case KERN_NONE:
            ptfuncs->init_rkern = init_kern_none;
            ptfuncs->run_rkern = run_kern_none;
            ptfuncs->cleanup_rkern = cleanup_kern_none;
            break;
        case KERN_TRIAD:
            ptfuncs->init_rkern = init_kern_triad;
            ptfuncs->run_rkern = run_kern_triad;
            ptfuncs->cleanup_rkern = cleanup_kern_triad;
            break;
        case KERN_SCALE:
            ptfuncs->init_rkern = init_kern_scale;
            ptfuncs->run_rkern = run_kern_scale;
            ptfuncs->cleanup_rkern = cleanup_kern_scale;
            break;
        case KERN_COPY:
            ptfuncs->init_rkern = init_kern_copy;
            ptfuncs->run_rkern = run_kern_copy;
            ptfuncs->cleanup_rkern = cleanup_kern_copy;
            break;
        case KERN_ADD:
            ptfuncs->init_rkern = init_kern_add;
            ptfuncs->run_rkern = run_kern_add;
            ptfuncs->cleanup_rkern = cleanup_kern_add;
            break;
        case KERN_POW:
            ptfuncs->init_rkern = init_kern_pow;
            ptfuncs->run_rkern = run_kern_pow;
            ptfuncs->cleanup_rkern = cleanup_kern_pow;
            break;
        case KERN_DGEMM:
            ptfuncs->init_rkern = init_kern_dgemm;
            ptfuncs->run_rkern = run_kern_dgemm;
            ptfuncs->cleanup_rkern = cleanup_kern_dgemm;
            break;
        case KERN_MPI_BCAST:
            ptfuncs->init_rkern = init_kern_bcast;
            ptfuncs->run_rkern = run_kern_bcast;
            ptfuncs->cleanup_rkern = cleanup_kern_bcast;
            break;
    }
}

// Wasserstein distance calculation function
void 
calc_w(int64_t *tm_arr, uint64_t tm_len, int64_t *sim_cdf, int64_t *w_arr, double *wp_arr, double p_zcut) 
{
    double w = 0, wm = 0;
    int wtile = (int)((1 - p_zcut) * 100.0); // Using 100 as NTILE
    for (size_t i = 0; i < 100; i++) {
        size_t itm = (size_t)((double)i / 100.0 * tm_len);
        w_arr[i] = sim_cdf[i] - tm_arr[itm];
        wp_arr[i] = (double)w_arr[i] / tm_arr[itm];
    }
    for (size_t i = 0; i < wtile; i++) {
        size_t itm = (size_t)((double)i / 100.0 * tm_len);
        w += abs(w_arr[i]);
        wm += tm_arr[itm];
    }
    w = w / (double)wtile;
    wm = wm / (double)wtile;
    double er = w / wm;

    printf(" W-Distance=%f  ", w);
    FILE *fp = fopen("wd.out", "w");
    if (fp) {
        fprintf(fp, "%f", w);
        fclose(fp);
    }
}

int 
main(int argc, char *argv[]) 
{
    enum pterr err;
    pt_test_options_t ptopts;
    pt_kern_func_t ptfuncs;
    int myrank = 0, nrank = 1;
    
    // Initialize MPI
    err = MPI_Init(&argc, &argv);
    if (err != MPI_SUCCESS) {
        printf("Failed to init MPI.\n");
        return 1;
    }
    MPI_Comm_size(MPI_COMM_WORLD, &nrank);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

    ptopts.fsize = 0;
    ptopts.rsize = 0;
    ptopts.fkern = KERN_NONE;
    ptopts.rkern = KERN_NONE;
    ptopts.ntests = 0;
    ptopts.nsub = 0;
    ptopts.timer = -1; // No timer set.
    if (myrank == 0) {
        err = parse_ptargs(argc, argv, &ptopts, &ptfuncs);
    }
    MPI_Bcast(&ptopts, sizeof(pt_test_options_t), MPI_BYTE, 0, MPI_COMM_WORLD);
    if (ptopts.timer == -1) {
        MPI_Finalize();
        return 0; // Early exit
    }
    
    // Set function pointers based on broadcasted options (on all ranks)
    set_kernel_functions(&ptopts, &ptfuncs);

    if (myrank == 0) {
        printf("Front kernel: %d, size: %zu KiB\n", ptopts.fkern, ptopts.fsize);
        printf("Rear kernel: %d, size: %zu KiB\n", ptopts.rkern, ptopts.rsize);
        printf("Timer: %d\n", ptopts.timer);
        printf("Tests: %d, Subtractions per test: %d\n", ptopts.ntests, ptopts.nsub);
    }

    /* Initialize kernels */
    if (ptfuncs.init_fkern) {
        ptfuncs.init_fkern(ptopts.fsize);
    }
    
    if (ptfuncs.init_rkern) {
        ptfuncs.init_rkern(ptopts.rsize);
    }

    /* Step 1: Get the minimum overhead and time per tick */
    long long tick, ovh;
    err = stress_timer(10000, 2, 1, 1, &tick, &ovh);
    _ptm_handle_error(err, "stress_timer");
    
    if (myrank == 0) {
        printf("Tick: %lld, Overhead: %lld\n", tick, ovh);
    }

    /* Step 2: Run the timing error sensor */
    MPI_Barrier(MPI_COMM_WORLD);
    
    // Allocate arrays for timing measurements
    int64_t *gauge = malloc(ptopts.ntests * sizeof(int64_t));
    int64_t *measured_times = malloc(ptopts.ntests * sizeof(int64_t));
    int64_t *theoretical_times = malloc(ptopts.ntests * sizeof(int64_t));
    
    if (!gauge || !measured_times || !theoretical_times) {
        fprintf(stderr, "Failed to allocate memory for timing arrays\n");
        MPI_Finalize();
        return 1;
    }
    
    // Initialize gauge array with nsub (no random distribution for now)
    for (int i = 0; i < ptopts.ntests; i++) {
        gauge[i] = ptopts.nsub;
    }
    
    if (myrank == 0) {
        printf("Starting timing error sensor with %d tests, %d subtractions each\n", 
               ptopts.ntests, ptopts.nsub);
    }
    
    // Run timing measurements
    for (int test = 0; test < ptopts.ntests; test++) {
        register uint64_t ra = gauge[test];
        register uint64_t rb = 1;
        struct timespec tv;
        uint64_t ns0, ns1;
        
        // Run front kernel
        if (ptfuncs.run_fkern) {
            ptfuncs.run_fkern();
        }
        
        // Start timing
        clock_gettime(CLOCK_MONOTONIC, &tv);
        ns0 = tv.tv_sec * 1e9 + tv.tv_nsec;
        
        // Run subtraction kernel (gauge block)
#ifdef __x86_64__
        __asm__ __volatile__ (
            "1:\n\t"
            "cmp $0, %0\n\t"
            "je 2f\n\t"
            "sub %1, %0\n\t"
            "jmp 1b\n\t"
            "2:\n\t"
            : "+r" (ra)
            : "r" (rb)
            : "cc"
        );
#elif defined(__aarch64__)
        __asm__ __volatile__ (
            "1:\n\t"
            "cmp %0, #0\n\t"
            "beq 2f\n\t"
            "sub %0, %0, %1\n\t"
            "b 1b\n\t"
            "2:\n\t"
            : "+r" (ra)
            : "r" (rb)
            : "cc"
        );
#endif
        
        // Stop timing
        clock_gettime(CLOCK_MONOTONIC, &tv);
        ns1 = tv.tv_sec * 1e9 + tv.tv_nsec;
        
        measured_times[test] = ns1 - ns0;
        theoretical_times[test] = gauge[test] * tick; // Theoretical time based on tick
        
        // Run rear kernel
        if (ptfuncs.run_rkern) {
            ptfuncs.run_rkern();
        }
        printf("%d\n", test);
        
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
    }
    
    // Gather all results to rank 0
    int64_t *all_measured_times = NULL;
    int64_t *all_theoretical_times = NULL;
    
    if (myrank == 0) {
        all_measured_times = malloc(nrank * ptopts.ntests * sizeof(int64_t));
        all_theoretical_times = malloc(nrank * ptopts.ntests * sizeof(int64_t));
    }
    
    MPI_Gather(measured_times, ptopts.ntests, MPI_INT64_T, 
               all_measured_times, ptopts.ntests, MPI_INT64_T, 0, MPI_COMM_WORLD);
    MPI_Gather(theoretical_times, ptopts.ntests, MPI_INT64_T, 
               all_theoretical_times, ptopts.ntests, MPI_INT64_T, 0, MPI_COMM_WORLD);
    
    // Calculate and print wasserstein distance on rank 0
    if (myrank == 0) {
        int64_t *w_arr = malloc(100 * sizeof(int64_t));
        double *wp_arr = malloc(100 * sizeof(double));
        int64_t *sim_cdf = malloc(100 * sizeof(int64_t));
        
        if (w_arr && wp_arr && sim_cdf) {
            // For now, use a simple CDF (can be enhanced later)
            for (int i = 0; i < 100; i++) {
                sim_cdf[i] = i * 10; // Simple linear CDF
            }
            
            calc_w(all_measured_times, nrank * ptopts.ntests, sim_cdf, w_arr, wp_arr, 0.1);
            
            free(w_arr);
            free(wp_arr);
            free(sim_cdf);
        }
        
        free(all_measured_times);
        free(all_theoretical_times);
    }
    
    free(gauge);
    free(measured_times);
    free(theoretical_times);

    /* Cleanup kernels */
    if (ptfuncs.cleanup_fkern) {
        ptfuncs.cleanup_fkern();
    }
    
    if (ptfuncs.cleanup_rkern) {
        ptfuncs.cleanup_rkern();
    }

    MPI_Finalize();
    return 0;
}