#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <mpi.h>
#include <x86intrin.h>

static inline void tsc_start_serialized(uint64_t *cycle){
    unsigned ch, cl;
    asm volatile (  "CPUID" "\n\t"
                    "RDTSC" "\n\t"
                    "mov %%edx, %0" "\n\t"
                    "mov %%eax, %1" "\n\t"
                    : "=r" (ch), "=r" (cl) :: "%rax", "%rbx", "%rcx", "%rdx");
    *cycle = ( ((uint64_t)ch << 32) | cl );
}

static inline void tsc_stop_serialized(uint64_t *cycle){
    unsigned ch, cl;
    asm volatile (  "RDTSCP" "\n\t"
                    "mov %%edx, %0" "\n\t"
                    "mov %%eax, %1" "\n\t"
                    "CPUID" "\n\t"
                    : "=r" (ch), "=r" (cl) :: "%rax", "%rbx", "%rcx", "%rdx");
    *cycle = ( ((uint64_t)ch << 32) | cl );
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
        w[i] = malloc(narr*sizeof(double)); Di[i] = malloc(narr*sizeof(double));
        p[i] = malloc(narr*sizeof(double)); Kx[i] = malloc(narr*sizeof(double));
        Ky[i] = malloc(narr*sizeof(double));
        for(int j=0;j<narr;j++) w[i][j]=Di[i][j]=p[i][j]=Kx[i][j]=Ky[i][j]=1.0;
    }
    double rx = 0.1, ry = 0.2;
    uint64_t t_nat = 0, t_ser = 0;
    int ntest = 10;
    
    // UNSERIALIZED (__rdtsc) - like USE_TSC_NATIVE
    for (int it=0; it<ntest; it++) {
        for (uint64_t j=1; j<narr-1; j++) {
            uint64_t c0 = __rdtsc();
            for (uint64_t k=1; k<narr-1; k++) {
                w[j][k] = Di[j][k]*p[j][k] - ry*(Ky[j+1][k]*p[j+1][k]+Ky[j][k]*p[j-1][k]) - rx*(Kx[j][k+1]*p[j][k+1]+Kx[j][k]*p[j][k-1]);
            }
            unsigned aux;
            uint64_t c1 = __rdtscp(&aux);
            t_nat += (c1 - c0);
        }
    }
    
    // SERIALIZED (CPUID) - like USE_TSC
    for (int it=0; it<ntest; it++) {
        for (uint64_t j=1; j<narr-1; j++) {
            uint64_t c0, c1;
            tsc_start_serialized(&c0);
            for (uint64_t k=1; k<narr-1; k++) {
                w[j][k] = Di[j][k]*p[j][k] - ry*(Ky[j+1][k]*p[j+1][k]+Ky[j][k]*p[j-1][k]) - rx*(Kx[j][k+1]*p[j][k+1]+Kx[j][k]*p[j][k-1]);
            }
            tsc_stop_serialized(&c1);
            t_ser += (c1 - c0);
        }
    }
    
    int iters = ntest * (narr - 2);
    printf("USE_TSC_NATIVE (unserialized): %lu cycles/iter (%.1f ns)\n", t_nat/iters, (t_nat/iters)*0.323);
    printf("USE_TSC (serialized):          %lu cycles/iter (%.1f ns)\n", t_ser/iters, (t_ser/iters)*0.323);
    MPI_Finalize();
    return 0;
}
