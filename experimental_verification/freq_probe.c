#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <mpi.h>
#include <x86intrin.h>

uint64_t read_cgt() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

uint64_t read_tsc() {
    unsigned int aux;
    return __rdtscp(&aux);
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    uint64_t c0 = read_tsc();
    uint64_t t0 = read_cgt();
    
    sleep(1);
    
    uint64_t c1 = read_tsc();
    uint64_t t1 = read_cgt();
    
    uint64_t delta_c = c1 - c0;
    uint64_t delta_t = t1 - t0;
    
    printf("1 sec sleep test:\n");
    printf("TSC Delta: %lu cycles\n", delta_c);
    printf("CGT Delta: %lu ns\n", delta_t);
    printf("Implied TSC Freq: %.3f GHz\n", (double)delta_c / (double)delta_t);

    MPI_Finalize();
    return 0;
}
