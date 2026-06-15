#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <papi.h>
#include <mpi.h>

#if defined(__x86_64__)
#include <x86intrin.h>
#endif

// Helper to get real ns from TSC
double tsc_ns_factor = 1.0;

uint64_t read_tsc() {
    unsigned int aux;
    return __rdtscp(&aux);
}

uint64_t read_cgt() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

void calibrate_tsc() {
    struct timespec sl = {0, 100000000}; // 100ms
    uint64_t t0 = read_cgt();
    uint64_t c0 = read_tsc();
    nanosleep(&sl, NULL);
    uint64_t t1 = read_cgt();
    uint64_t c1 = read_tsc();
    tsc_ns_factor = (double)(t1 - t0) / (double)(c1 - c0);
    printf("Calibrated TSC ns factor: %.9f (~%.3f GHz)\n", tsc_ns_factor, 1.0/tsc_ns_factor);
}

// Synthetic workload
__attribute__((noinline)) void dummy_work(uint64_t nsamp) {
    uint64_t ra = nsamp, rb = 1, lower = 0;
    while (ra > lower) {
        ra -= rb;
        __asm__ __volatile__("" ::: "memory");
    }
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int eventset = PAPI_NULL;
    PAPI_library_init(PAPI_VER_CURRENT);
    PAPI_create_eventset(&eventset);
    PAPI_add_named_event(eventset, "cpu-cycles");
    PAPI_start(eventset);

    calibrate_tsc();

    uint64_t nsamp_list[] = {0, 1000, 5000};
    const char *names[] = {"Zero-work", "Short-work", "Medium-work"};

    for (int i = 0; i < 3; i++) {
        uint64_t nsamp = nsamp_list[i];
        printf("\n--- Test: %s (nsamp=%lu) ---\n", names[i], nsamp);

        // 1. CGT
        uint64_t t0 = read_cgt();
        dummy_work(nsamp);
        uint64_t t1 = read_cgt();
        printf("CGT:        %8lu ns\n", t1 - t0);

        // 2. Wtime
        double w0 = MPI_Wtime();
        dummy_work(nsamp);
        double w1 = MPI_Wtime();
        printf("Wtime:      %8lu ns\n", (uint64_t)((w1 - w0) * 1e9));

        // 3. TSC
        uint64_t c0 = read_tsc();
        dummy_work(nsamp);
        uint64_t c1 = read_tsc();
        printf("TSC:        %8lu ns (scaled)\n", (uint64_t)((c1 - c0) * tsc_ns_factor));

        // 4. PAPI
        long long v0, v1;
        uint64_t p0 = PAPI_get_real_nsec();
        dummy_work(nsamp);
        uint64_t p1 = PAPI_get_real_nsec();
        printf("PAPI:       %8lu ns\n", p1 - p0);

        // 5. PAPIX6 (simulated by adding read)
        p0 = PAPI_get_real_nsec();
        PAPI_read(eventset, &v0);
        dummy_work(nsamp);
        PAPI_read(eventset, &v1);
        p1 = PAPI_get_real_nsec();
        printf("PAPI+Read:  %8lu ns (includes read overhead)\n", p1 - p0);
    }

    PAPI_stop(eventset, NULL);
    MPI_Finalize();
    return 0;
}
