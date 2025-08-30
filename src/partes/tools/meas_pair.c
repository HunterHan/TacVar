/**
 * @file meas_pair.c
 * @brief Calculate statistical measures between CDFs of two nsub kernel measurements
 * 
 * This program runs two different nsub kernels for nrepeat times and calculates the 1D
 * Wasserstein Distance and Pearson correlation coefficient between their timing CDFs.
 * Raw measurement results are saved to CSV files.
 * 
 * Usage: mpirun -np <nprocs> ./meas_pair.x <nsub1> <nsub2> <nrepeat> <ntiles> <cut_tile>
 */
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <math.h>
#include <mpi.h>
#include "../pterr.h"
#include "../gauges/sub.h"

/* Comparison function for qsort */
static int 
compare_int64(const void *a, const void *b)
{
    int64_t x = *(const int64_t *)a;
    int64_t y = *(const int64_t *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

static inline int64_t run_sub_kernel(uint64_t nsub);
static void calc_cdf(int64_t *times, uint64_t n, int64_t *cdf, uint64_t ntiles);
static int calc_wasserstein_distance(int64_t *cdf1, int64_t *cdf2, 
                                    uint64_t ntiles, double cut_tile, double *w_distance);
static int calc_pearson_correlation(int64_t *cdf1, int64_t *cdf2, 
                                   uint64_t ntiles, double cut_tile, double *pearson_r);
static int write_raw_measurements(int64_t *times, uint64_t nrepeat, 
                                 int rank, const char *suffix);

/**
 * @brief Run subtraction kernel: ra=nsub, ra-=1 until ra==0
 */
static inline int64_t 
run_sub_kernel(uint64_t nsub)
{
    struct timespec tv;
    int64_t ns0, ns1;
    
    clock_gettime(CLOCK_MONOTONIC, &tv);
    ns0 = (int64_t)tv.tv_sec * 1000000000ULL + (int64_t)tv.tv_nsec;
    
    __gauge_sub_intrinsic(nsub);
    
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
 * @brief Calculate 1D Wasserstein distance between two CDFs
 */
static int 
calc_wasserstein_distance(int64_t *cdf1, int64_t *cdf2, 
                         uint64_t ntiles, double cut_tile, double *w_distance)
{
    *w_distance = 0.0;
    
    /* Calculate effective number of tiles to use based on cut_tile */
    uint64_t effective_tiles = (uint64_t)(cut_tile * (double)ntiles);
    if (effective_tiles == 0) effective_tiles = 1;
    if (effective_tiles > ntiles) effective_tiles = ntiles;
    
    /* Calculate Wasserstein distance for the effective range (lower quantiles) */
    for (uint64_t i = 0; i < effective_tiles; i++) {
        *w_distance += fabs((double)(cdf1[i] - cdf2[i]));
    }
    
    *w_distance /= (double)effective_tiles;
    
    return 0;
}

/**
 * @brief Calculate Pearson correlation coefficient between two CDFs
 */
static int 
calc_pearson_correlation(int64_t *cdf1, int64_t *cdf2, 
                        uint64_t ntiles, double cut_tile, double *pearson_r)
{
    *pearson_r = 0.0;
    
    /* Calculate effective number of tiles to use based on cut_tile */
    uint64_t effective_tiles = (uint64_t)(cut_tile * (double)ntiles);
    if (effective_tiles == 0) effective_tiles = 1;
    if (effective_tiles > ntiles) effective_tiles = ntiles;
    
    /* Calculate means */
    double mean1 = 0.0, mean2 = 0.0;
    for (uint64_t i = 0; i < effective_tiles; i++) {
        mean1 += (double)cdf1[i];
        mean2 += (double)cdf2[i];
    }
    mean1 /= (double)effective_tiles;
    mean2 /= (double)effective_tiles;
    
    /* Calculate Pearson correlation coefficient */
    double numerator = 0.0;
    double sum_sq1 = 0.0, sum_sq2 = 0.0;
    
    for (uint64_t i = 0; i < effective_tiles; i++) {
        double diff1 = (double)cdf1[i] - mean1;
        double diff2 = (double)cdf2[i] - mean2;
        
        numerator += diff1 * diff2;
        sum_sq1 += diff1 * diff1;
        sum_sq2 += diff2 * diff2;
    }
    
    /* Avoid division by zero */
    double denominator = sqrt(sum_sq1 * sum_sq2);
    if (denominator > 0.0) {
        *pearson_r = numerator / denominator;
    } else {
        *pearson_r = 0.0;  /* No correlation if no variance */
    }
    
    return 0;
}

/**
 * @brief Write raw measurement results to CSV file
 */
static int 
write_raw_measurements(int64_t *times, uint64_t nrepeat, 
                      int rank, const char *suffix)
{
    char filename[256];
    FILE *fp;
    
    snprintf(filename, sizeof(filename), "meas_pair_r%d_ng%s.csv", rank, suffix);
    
    fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Rank %d: failed to open %s for writing\n", rank, filename);
        return -1;
    }
    
    /* Write raw measurements, one per line, no header */
    for (uint64_t i = 0; i < nrepeat; i++) {
        fprintf(fp, "%" PRId64 "\n", times[i]);
    }
    
    fclose(fp);
    return 0;
}

int
main(int argc, char *argv[])
{
    int myrank, nrank;
    uint64_t nsub1, nsub2, nrepeat, ntiles;
    double cut_tile;
    int64_t *times1 = NULL;
    int64_t *times2 = NULL;
    int64_t *cdf1 = NULL;
    int64_t *cdf2 = NULL;
    char nsub1_str[32], nsub2_str[32];
    
    /* Check args */
    if (argc != 6) {
        fprintf(stderr, "Usage: %s <nsub1> <nsub2> <nrepeat> <ntiles> <cut_tile>\n", argv[0]);
        fprintf(stderr, "  nsub1: number of subtractions for first kernel\n");
        fprintf(stderr, "  nsub2: number of subtractions for second kernel\n");
        fprintf(stderr, "  nrepeat: number of repetitions for each kernel\n");
        fprintf(stderr, "  ntiles: number of tiles for CDF calculation\n");
        fprintf(stderr, "  cut_tile: fraction of lower quantiles to use (0.0-1.0]\n");
        return 1;
    }
    
    nsub1 = strtoull(argv[1], NULL, 10);
    nsub2 = strtoull(argv[2], NULL, 10);
    nrepeat = strtoull(argv[3], NULL, 10);
    ntiles = strtoull(argv[4], NULL, 10);
    cut_tile = strtod(argv[5], NULL);
    
    /* Store nsub values as strings for filename generation */
    snprintf(nsub1_str, sizeof(nsub1_str), "%" PRIu64, nsub1);
    snprintf(nsub2_str, sizeof(nsub2_str), "%" PRIu64, nsub2);
    
    if (nsub1 == 0 || nsub2 == 0 || nrepeat == 0 || ntiles == 0) {
        fprintf(stderr, "Error: nsub1, nsub2, nrepeat, and ntiles must be > 0\n");
        return 1;
    }
    
    if (cut_tile <= 0.0 || cut_tile > 1.0) {
        fprintf(stderr, "Error: cut_tile must be between 0.0 and 1.0 (exclusive of 0.0, inclusive of 1.0)\n");
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
    times1 = (int64_t *)malloc(nrepeat * sizeof(int64_t));
    times2 = (int64_t *)malloc(nrepeat * sizeof(int64_t));
    cdf1 = (int64_t *)malloc(ntiles * sizeof(int64_t));
    cdf2 = (int64_t *)malloc(ntiles * sizeof(int64_t));
    
    if (!times1 || !times2 || !cdf1 || !cdf2) {
        fprintf(stderr, "Rank %d: memory allocation failed\n", myrank);
        free(times1);
        free(times2);
        free(cdf1);
        free(cdf2);
        MPI_Finalize();
        return 1;
    }
    
    /* Print header on rank 0 */
    if (myrank == 0) {
        printf("Running nsub pair kernel measurements with statistical analysis\n");
        printf("Parameters: nsub1=%" PRIu64 ", nsub2=%" PRIu64 ", nrepeat=%" PRIu64 ", ntiles=%" PRIu64 ", cut_tile=%.3f\n",
               nsub1, nsub2, nrepeat, ntiles, cut_tile);
        printf("Format: core_id,w_distance_ns,pearson_r\n");
    }
    
    /* Run measurements for nsub1 */
    for (uint64_t i = 0; i < nrepeat; i++) {
        /* Barrier sync before each measurement */
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
        
        /* Run subtraction kernel with nsub1 */
        times1[i] = run_sub_kernel(nsub1);
    }
    
    /* Run measurements for nsub2 */
    for (uint64_t i = 0; i < nrepeat; i++) {
        /* Barrier sync before each measurement */
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
        
        /* Run subtraction kernel with nsub2 */
        times2[i] = run_sub_kernel(nsub2);
    }
    
    /* Write raw measurements to CSV files */
    if (write_raw_measurements(times1, nrepeat, myrank, nsub1_str) != 0) {
        fprintf(stderr, "Rank %d: failed to write nsub1 measurements\n", myrank);
    }
    
    if (write_raw_measurements(times2, nrepeat, myrank, nsub2_str) != 0) {
        fprintf(stderr, "Rank %d: failed to write nsub2 measurements\n", myrank);
    }
    
    /* Calculate CDFs for both datasets */
    calc_cdf(times1, nrepeat, cdf1, ntiles);
    calc_cdf(times2, nrepeat, cdf2, ntiles);
    
    /* Calculate Wasserstein distance between the two CDFs */
    double w_distance = 0.0;
    calc_wasserstein_distance(cdf1, cdf2, ntiles, cut_tile, &w_distance);
    
    /* Calculate Pearson correlation coefficient between the two CDFs */
    double pearson_r = 0.0;
    calc_pearson_correlation(cdf1, cdf2, ntiles, cut_tile, &pearson_r);
    
    /* Gather results from all ranks */
    double *all_w_distances = NULL;
    double *all_pearson_r = NULL;
    
    if (myrank == 0) {
        all_w_distances = (double *)malloc(nrank * sizeof(double));
        all_pearson_r = (double *)malloc(nrank * sizeof(double));
        if (!all_w_distances || !all_pearson_r) {
            fprintf(stderr, "Rank 0: memory allocation failed for gathering results\n");
            free(all_w_distances);
            free(all_pearson_r);
            MPI_Finalize();
            return 1;
        }
    }
    
    MPI_Gather(&w_distance, 1, MPI_DOUBLE, all_w_distances, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Gather(&pearson_r, 1, MPI_DOUBLE, all_pearson_r, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    
    /* Print results for each core on rank 0 */
    if (myrank == 0) {
        for (int i = 0; i < nrank; i++) {
            printf("%d,%.2f,%.6f\n", i, all_w_distances[i], all_pearson_r[i]);
        }
        free(all_w_distances);
        free(all_pearson_r);
        
        printf("Raw measurements written to meas_pair_r<rank>_ng<nsub>.csv files\n");
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    /* Cleanup */
    free(times1);
    free(times2);
    free(cdf1);
    free(cdf2);
    
    MPI_Finalize();
    return 0;
}
