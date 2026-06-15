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
    double *w = malloc(narr * sizeof(double));
    double *Di = malloc(narr * sizeof(double));
    double *p = malloc(narr * sizeof(double));
    double *Kx = malloc(narr * sizeof(double));
    double *Ky = malloc(narr * sizeof(double));
    
    for (int i=0; i<narr; i++) {
        w[i] = Di[i] = p[i] = Kx[i] = Ky[i] = 1.0;
    }
    double rx = 0.1, ry = 0.2;

    uint64_t total_cgt_unserialized = 0;
    uint64_t total_cgt_serialized = 0;
    int iters = 1000;

    for (int it = 0; it < iters; it++) {
        uint64_t t0 = read_cgt();
        for (uint64_t k = 1; k < narr-1; k ++) {
            w[k] = Di[k] * p[k] - ry * (Ky[k] * p[k] + Ky[k] * p[k-1]) - rx * (Kx[k+1] * p[k+1] + Kx[k] * p[k-1]);
        }
        uint64_t t1 = read_cgt();
        total_cgt_unserialized += (t1 - t0);
    }

    for (int it = 0; it < iters; it++) {
        uint64_t t0 = read_cgt();
        __asm__ __volatile__("cpuid" ::: "%rax", "%rbx", "%rcx", "%rdx", "memory");
        for (uint64_t k = 1; k < narr-1; k ++) {
            w[k] = Di[k] * p[k] - ry * (Ky[k] * p[k] + Ky[k] * p[k-1]) - rx * (Kx[k+1] * p[k+1] + Kx[k] * p[k-1]);
        }
        __asm__ __volatile__("cpuid" ::: "%rax", "%rbx", "%rcx", "%rdx", "memory");
        uint64_t t1 = read_cgt();
        total_cgt_serialized += (t1 - t0);
    }

    printf("Unserialized CGT: %lu ns/iter\n", total_cgt_unserialized / iters);
    printf("Serialized CGT:   %lu ns/iter\n", total_cgt_serialized / iters);

    MPI_Finalize();
    return 0;
}
