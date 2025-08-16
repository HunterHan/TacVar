/**
 * @file run_nsub_mpi_with_w.c
 * @brief Test the change of Wasserstein distance during increasing tick.
 * 
 * This program runs nsub kernels for different tick values and calculates the 1D
 * Wasserstein Distance between measured timing CDF and theoretical timing CDF.
 * 
 * Usage: mpirun -np <nprocs> ./run_nsub_mpi_with_w.x <gpt> <ticks> <ticke> <interval> <ntiles> <nspt> <cut_tile>
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <math.h>
#include <mpi.h>
#include "../pterr.h"

/* Comparison function for qsort */
static int compare_int64(const void *a, const void *b)
{
    int64_t x = *(const int64_t *)a;
    int64_t y = *(const int64_t *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

#define NREPEAT 100  /* Number of repetitions for each tick measurement */

static inline int64_t run_sub_kernel(uint64_t nsub);
static void calc_cdf(int64_t *times, uint64_t n, int64_t *cdf, uint64_t ntiles);
static int calc_w(int64_t *cdf_met, int64_t std_time, 
                 uint64_t ntiles, double cut_tile, double *w_abs, double *w_rel);

/**
 * @brief Run subtraction kernel: ra=nsub, ra-=1 until ra==0
 */
static inline int64_t 
run_sub_kernel(uint64_t nsub)
{
    struct timespec tv;
    int64_t ns0, ns1;
    
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
    
    return ns1 - ns0;
}

/**
 * @brief Calculate CDF from timing measurements
 */
static void 
calc_cdf(int64_t *times, uint64_t n, int64_t *cdf, uint64_t ntiles)
{
    /* Sort times for CDF calculation using qsort */
    qsort(times, n, sizeof(int64_t), compare_int64);
    
    /* Calculate CDF at ntiles points */
    for (uint64_t i = 0; i < ntiles; i++) {
        uint64_t idx = (uint64_t)((double)i / (double)(ntiles - 1) * (double)(n - 1));
        if (idx >= n) idx = n - 1;
        cdf[i] = times[idx];
    }
}

/**
 * @brief Calculate 1D Wasserstein distance between measured and theoretical CDFs
 */
static int 
calc_w(int64_t *cdf_met, int64_t std_time, 
       uint64_t ntiles, double cut_tile, double *w_abs, double *w_rel)
{
    *w_abs = 0.0;
    *w_rel = 0.0;
    
    /* Calculate effective number of tiles to use based on cut_tile */
    uint64_t effective_tiles = (uint64_t)(cut_tile * (double)ntiles);
    if (effective_tiles == 0) effective_tiles = 1;
    if (effective_tiles > ntiles) effective_tiles = ntiles;
    
    /* Calculate Wasserstein distance only for the effective range (lower quantiles) */
    for (uint64_t i = 0; i < effective_tiles; i++) {
        *w_abs += fabs((double)(cdf_met[i] - std_time));
    }
    
    *w_abs /= (double)effective_tiles;
    *w_rel = *w_abs / (double)std_time;
    
    return 0;
}

int
main(int argc, char *argv[])
{
    int myrank, nrank;
    double gpt, nspt, cut_tile;
    uint64_t ticks, ticke, interval, ntiles;
    int64_t *met_times = NULL;
    int64_t *cdf_met = NULL;
    int64_t *cdf_std = NULL;
    
    /* Check args */
    if (argc != 8) {
        fprintf(stderr, "Usage: %s <gpt> <ticks> <ticke> <interval> <ntiles> <nspt> <cut_tile>\n", argv[0]);
        fprintf(stderr, "  gpt: theoretical time per tick (ns) - for reference\n");
        fprintf(stderr, "  ticks: starting tick value\n");
        fprintf(stderr, "  ticke: ending tick value\n");
        fprintf(stderr, "  interval: tick increment\n");
        fprintf(stderr, "  ntiles: number of tiles for CDF calculation\n");
        fprintf(stderr, "  nspt: nanoseconds per tick - used for standard time calculation\n");
        fprintf(stderr, "  cut_tile: fraction of lower quantiles to use (0.0-1.0)\n");
        return 1;
    }
    
    gpt = strtod(argv[1], NULL);
    ticks = strtoull(argv[2], NULL, 10);
    ticke = strtoull(argv[3], NULL, 10);
    interval = strtoull(argv[4], NULL, 10);
    ntiles = strtoull(argv[5], NULL, 10);
    nspt = strtod(argv[6], NULL);
    cut_tile = strtod(argv[7], NULL);
    
    if (gpt <= 0.0 || ticks == 0 || ticke == 0 || interval == 0 || ntiles == 0 || nspt <= 0.0) {
        fprintf(stderr, "Error: all parameters must be > 0\n");
        return 1;
    }
    
    if (cut_tile < 0.0 || cut_tile > 1.0) {
        fprintf(stderr, "Error: cut_tile must be between 0.0 and 1.0\n");
        return 1;
    }
    
    if (ticks > ticke) {
        fprintf(stderr, "Error: ticks must be <= ticke\n");
        return 1;
    }
    
    /* Init MPI */
    if (MPI_Init(&argc, &argv) != MPI_SUCCESS) {
        fprintf(stderr, "MPI_Init failed\n");
        return 1;
    }
    
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    MPI_Comm_size(MPI_COMM_WORLD, &nrank);
    
    /* Allocate memory for timing measurements and CDFs */
    met_times = (int64_t *)malloc(NREPEAT * sizeof(int64_t));
    cdf_met = (int64_t *)malloc(ntiles * sizeof(int64_t));
    
    if (!met_times || !cdf_met) {
        fprintf(stderr, "Rank %d: memory allocation failed\n", myrank);
        free(met_times);
        free(cdf_met);
        MPI_Finalize();
        return 1;
    }
    
    /* Print header on rank 0 */
    if (myrank == 0) {
        printf("Running nsub kernel measurements with Wasserstein distance calculation\n");
        printf("Parameters: gpt=%.6f, ticks=%" PRIu64 ", ticke=%" PRIu64 ", interval=%" PRIu64 ", ntiles=%" PRIu64 ", nspt=%.6f, cut_tile=%.3f\n",
               gpt, ticks, ticke, interval, ntiles, nspt, cut_tile);
        printf("Format: tick,core_id,std_time_ns,w_abs_ns,w_rel\n");
    }
    
    /* Run measurements for each tick value */
    for (uint64_t tick = ticks; tick <= ticke; tick += interval) {
        /* Calculate nsub = tick * gpt */
        uint64_t nsub = (uint64_t)(tick * gpt);
        
        /* Run NREPEAT measurements for this tick */
        for (uint64_t i = 0; i < NREPEAT; i++) {
            /* Barrier sync before each measurement */
            MPI_Barrier(MPI_COMM_WORLD);
            MPI_Barrier(MPI_COMM_WORLD);
            
            /* Run subtraction kernel */
            met_times[i] = run_sub_kernel(nsub);
        }
        
        /* Calculate measured CDF */
        calc_cdf(met_times, NREPEAT, cdf_met, ntiles);
        
        /* Calculate standard CDF */
        int64_t std_time = (int64_t)(nspt * (double)tick);
        
        /* Calculate absolute Wasserstein distance */
        double w_abs = 0.0, w_rel = 0.0;
        calc_w(cdf_met, std_time, ntiles, cut_tile, &w_abs, &w_rel);
        
        
        /* Gather results from all ranks */
        double *all_w_abs = NULL;
        double *all_w_rel = NULL;
        
        if (myrank == 0) {
            all_w_abs = (double *)malloc(nrank * sizeof(double));
            all_w_rel = (double *)malloc(nrank * sizeof(double));
        }
        
        MPI_Gather(&w_abs, 1, MPI_DOUBLE, all_w_abs, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        MPI_Gather(&w_rel, 1, MPI_DOUBLE, all_w_rel, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        
        /* Print results for each core on rank 0 */
        if (myrank == 0) {
            for (int i = 0; i < nrank; i++) {
                printf("%" PRIu64 ",%d,%.2f,%.2f,%.6f\n", 
                       tick, i, (double)std_time, all_w_abs[i], all_w_rel[i]);
            }
            free(all_w_abs);
            free(all_w_rel);
        }
    }
    MPI_Barrier(MPI_COMM_WORLD);
    
    /* Cleanup */
    free(met_times);
    free(cdf_met);
    free(cdf_std);
    
    MPI_Finalize();
    return 0;
}
