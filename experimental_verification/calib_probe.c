#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <mpi.h>
#include <x86intrin.h>

static inline void tsc_start(uint64_t *cycle){
    unsigned ch, cl;
    asm volatile (  "CPUID" "\n\t"
                    "RDTSC" "\n\t"
                    "mov %%edx, %0" "\n\t"
                    "mov %%eax, %1" "\n\t"
                    : "=r" (ch), "=r" (cl) :: "%rax", "%rbx", "%rcx", "%rdx");
    *cycle = ( ((uint64_t)ch << 32) | cl );
}

static inline void tsc_stop(uint64_t *cycle){
    unsigned ch, cl;
    asm volatile (  "RDTSCP" "\n\t"
                    "mov %%edx, %0" "\n\t"
                    "mov %%eax, %1" "\n\t"
                    "CPUID" "\n\t"
                    : "=r" (ch), "=r" (cl) :: "%rax", "%rbx", "%rcx", "%rdx");
    *cycle = ( ((uint64_t)ch << 32) | cl );
}

static inline uint64_t nsec_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    struct timespec req = {
        .tv_sec = 0,
        .tv_nsec = 200000000,
    };

    uint64_t c0, c1;
    uint64_t n0 = nsec_now();
    tsc_start(&c0);
    nanosleep(&req, NULL);
    tsc_stop(&c1);
    uint64_t n1 = nsec_now();

    double tsc_ns = (double)(n1 - n0) / (double)(c1 - c0);
    printf("Calibrated tsc_ns = %.9f (Freq: %.3f GHz)\n", tsc_ns, 1.0/tsc_ns);
    
    // Test MONOTONIC vs MONOTONIC_RAW
    struct timespec tm, tm_raw;
    clock_gettime(CLOCK_MONOTONIC, &tm);
    clock_gettime(CLOCK_MONOTONIC_RAW, &tm_raw);
    printf("MONOTONIC: %lu s, %lu ns\n", tm.tv_sec, tm.tv_nsec);
    printf("MONOTONIC_RAW: %lu s, %lu ns\n", tm_raw.tv_sec, tm_raw.tv_nsec);

    MPI_Finalize();
    return 0;
}
