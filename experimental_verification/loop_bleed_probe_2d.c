#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <mpi.h>

uint64_t read_cgt() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int narr = 512;
    double **w = malloc(narr * sizeof(double*));
    double **Di = malloc(narr * sizeof(double*));
    double **p = malloc(narr * sizeof(double*));
    double **Kx = malloc(narr * sizeof(double*));
    double **Ky = malloc(narr * sizeof(double*));
    
    for (int i=0; i<narr; i++) {
        w[i] = malloc(narr * sizeof(double));
        Di[i] = malloc(narr * sizeof(double));
        p[i] = malloc(narr * sizeof(double));
        Kx[i] = malloc(narr * sizeof(double));
        Ky[i] = malloc(narr * sizeof(double));
        for(int j=0; j<narr; j++){
            w[i][j] = Di[i][j] = p[i][j] = Kx[i][j] = Ky[i][j] = 1.0;
        }
    }
    double rx = 0.1, ry = 0.2;

    uint64_t total_cgt = 0;
    uint64_t total_cgt_fence = 0;
    int ntest = 10;

    // Simulate tl_f90 structure WITHOUT fence
    for (int it = 0; it < ntest; it++) {
        for (uint64_t j = 1; j < narr-1; j++) {
            uint64_t t0 = read_cgt();
            for (uint64_t k = 1; k < narr-1; k ++) {
                w[j][k] = Di[j][k] * p[j][k] - ry * (Ky[j+1][k] * p[j+1][k] + Ky[j][k] * p[j-1][k]) - rx * (Kx[j][k+1] * p[j][k+1] + Kx[j][k] * p[j][k-1]);
            }
            uint64_t t1 = read_cgt();
            total_cgt += (t1 - t0);
        }
    }

    // Simulate tl_f90 structure WITH fence
    for (int it = 0; it < ntest; it++) {
        for (uint64_t j = 1; j < narr-1; j++) {
            __asm__ __volatile__("lfence" ::: "memory");
            uint64_t t0 = read_cgt();
            __asm__ __volatile__("lfence" ::: "memory");
            for (uint64_t k = 1; k < narr-1; k ++) {
                w[j][k] = Di[j][k] * p[j][k] - ry * (Ky[j+1][k] * p[j+1][k] + Ky[j][k] * p[j-1][k]) - rx * (Kx[j][k+1] * p[j][k+1] + Kx[j][k] * p[j][k-1]);
            }
            __asm__ __volatile__("lfence" ::: "memory");
            uint64_t t1 = read_cgt();
            __asm__ __volatile__("lfence" ::: "memory");
            total_cgt_fence += (t1 - t0);
        }
    }

    int iters = ntest * (narr - 2);
    printf("Inner Loop iterations: %d\n", iters);
    printf("CGT without fences (bleeding): %lu ns/iter\n", total_cgt / iters);
    printf("CGT with fences (isolated):    %lu ns/iter\n", total_cgt_fence / iters);

    MPI_Finalize();
    return 0;
}
