/**
 * @file run_nsub_mpi.c
 * @brief Simple MPI utility to run subtraction kernel and write timing results to CSV
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <mpi.h>

static inline int64_t run_sub_kernel(uint64_t nsub);

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

int
main(int argc, char *argv[])
{
    int myrank, nrank;
    uint64_t nsub, nrepeat;
    FILE *fp = NULL;
    char fname[256];
    
    /* Check args */
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <nsub> <nrepeat>\n", argv[0]);
        return 1;
    }
    
    nsub = strtoull(argv[1], NULL, 10);
    nrepeat = strtoull(argv[2], NULL, 10);
    
    if (nsub == 0 || nrepeat == 0) {
        fprintf(stderr, "Error: nsub and nrepeat must be > 0\n");
        return 1;
    }
    
    /* Init MPI */
    if (MPI_Init(&argc, &argv) != MPI_SUCCESS) {
        fprintf(stderr, "MPI_Init failed\n");
        return 1;
    }
    
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    MPI_Comm_size(MPI_COMM_WORLD, &nrank);
    
    /* Open CSV file for this rank */
    snprintf(fname, sizeof(fname), "meas_r%d_ng%" PRIu64 ".csv", myrank, nsub);
    fp = fopen(fname, "w");
    if (!fp) {
        fprintf(stderr, "Rank %d: failed to open %s\n", myrank, fname);
        MPI_Finalize();
        return 1;
    }
    
    /* Run measurements with MPI barrier sync */
    for (uint64_t i = 0; i < nrepeat; i++) {
        int64_t measured_ns;
        
        /* Barrier sync before each measurement */
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
        
        /* Run subtraction kernel */
        measured_ns = run_sub_kernel(nsub);
        
        /* Write result to CSV */
        fprintf(fp, "%" PRIu64 "\n", measured_ns);
        fflush(fp);
    }
    
    fclose(fp);
    
    if (myrank == 0) {
        printf("Completed %" PRIu64 " measurements of nsub=%" PRIu64 " on %d ranks\n", 
               nrepeat, nsub, nrank);
        printf("Results written to meas_r<rank>_ng<nsub>.csv files\n");
    }
    
    MPI_Finalize();
    return 0;
}
