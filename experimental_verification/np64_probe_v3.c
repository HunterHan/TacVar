#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <mpi.h>
#include <x86intrin.h>

uint64_t read_cgt() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

int main(int argc, char **argv) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int narr = 512;
    double **w = malloc(narr * sizeof(double*));
    double **Di = malloc(narr * sizeof(double*));
    double **p = malloc(narr * sizeof(double*));
    double **Kx = malloc(narr * sizeof(double*));
    double **Ky = malloc(narr * sizeof(double*));
    for (int i=0; i<narr; i++) {
        w[i] = malloc(narr*sizeof(double)); Di[i] = malloc(narr*sizeof(double));
        p[i] = malloc(narr*sizeof(double)); Kx[i] = malloc(narr*sizeof(double));
        Ky[i] = malloc(narr*sizeof(double));
        for(int j=0;j<narr;j++) w[i][j]=Di[i][j]=p[i][j]=Kx[i][j]=Ky[i][j]=1.0;
    }
    double rx = 0.1, ry = 0.2;

    uint64_t total_cgt_unser = 0;
    uint64_t total_cgt_ser = 0;
    int ntest = 20;

    MPI_Barrier(MPI_COMM_WORLD);

    // UNSERIALIZED (Baseline: Just loop + gettime)
    for (int it = 0; it < ntest; it++) {
        for (uint64_t j=1; j<narr-1; j++) {
            uint64_t t0 = read_cgt();
            for (uint64_t k=1; k<narr-1; k++) {
                w[j][k] = Di[j][k]*p[j][k] - ry*(Ky[j+1][k]*p[j+1][k]+Ky[j][k]*p[j-1][k]) - rx*(Kx[j][k+1]*p[j][k+1]+Kx[j][k]*p[j][k-1]);
            }
            uint64_t t1 = read_cgt();
            total_cgt_unser += (t1 - t0);
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);

    // PAPI Simulation: Delay function to pace the loop
    for (int it = 0; it < ntest; it++) {
        for (uint64_t j=1; j<narr-1; j++) {
            uint64_t t0 = read_cgt();
            // Simulate 600ns PAPI_read delay using a small loop
            int delay = 0;
            for(int d=0; d<200; d++) { __asm__ __volatile__("" : "+r"(delay) :: "memory"); }
            
            for (uint64_t k=1; k<narr-1; k++) {
                w[j][k] = Di[j][k]*p[j][k] - ry*(Ky[j+1][k]*p[j+1][k]+Ky[j][k]*p[j-1][k]) - rx*(Kx[j][k+1]*p[j][k+1]+Kx[j][k]*p[j][k-1]);
            }
            
            uint64_t t1 = read_cgt();
            total_cgt_ser += (t1 - t0);
        }
    }

    uint64_t avg_unser = total_cgt_unser / (ntest * (narr-2));
    uint64_t avg_ser = total_cgt_ser / (ntest * (narr-2));

    uint64_t global_unser, global_ser;
    MPI_Reduce(&avg_unser, &global_unser, 1, MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&avg_ser, &global_ser, 1, MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("--- NP=%d PAPI Pacing Test ---\n", size);
        printf("Unpaced Kernel (CGT style): %lu ns/iter\n", global_unser / size);
        printf("Paced Kernel (PAPI style delay): %lu ns/iter\n", global_ser / size);
    }

    MPI_Finalize();
    return 0;
}
