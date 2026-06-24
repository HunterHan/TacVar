#define _GNU_SOURCE
#define _ISOC11_SOURCE
#include <mpi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef USE_PAPI
#include "papi.h"
#endif

#ifndef TIMER_NAME
#define TIMER_NAME unknown_timer
#endif
#ifndef KERNEL_NAME
#define KERNEL_NAME unknown_kernel
#endif

#define STR2(x) #x
#define STR(x) STR2(x)

static const double alpha = 1.5;
static const double beta = 1.2;
static volatile double checksum_sink = 0.0;

static void *xmalloc(size_t n) {
    void *p = NULL;
    if (posix_memalign(&p, 64, n) != 0 || !p) {
        fprintf(stderr, "allocation failed: %zu\n", n);
        MPI_Abort(MPI_COMM_WORLD, 2);
    }
    return p;
}

static inline uint64_t ns_from_ts(const struct timespec *ts) {
    return (uint64_t)ts->tv_sec * 1000000000ull + (uint64_t)ts->tv_nsec;
}

static inline uint64_t cgt_now(void) {
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    return ns_from_ts(&tv);
}

#if defined(__x86_64__)
static inline void tsc_start(uint64_t *cycle) {
    unsigned ch, cl;
    __asm__ volatile("CPUID\n\tRDTSC\n\tmov %%edx,%0\n\tmov %%eax,%1\n\t"
                     : "=r"(ch), "=r"(cl)
                     :
                     : "%rax", "%rbx", "%rcx", "%rdx");
    *cycle = (((uint64_t)ch << 32) | cl);
}

static inline void tsc_stop(uint64_t *cycle) {
    unsigned ch, cl;
    __asm__ volatile("RDTSCP\n\tmov %%edx,%0\n\tmov %%eax,%1\n\tCPUID\n\t"
                     : "=r"(ch), "=r"(cl)
                     :
                     : "%rax", "%rbx", "%rcx", "%rdx");
    *cycle = (((uint64_t)ch << 32) | cl);
}

static double ns_per_tick(void) {
    struct timespec req = {.tv_sec = 0, .tv_nsec = 200000000};
    uint64_t c0, c1;
    uint64_t n0 = cgt_now();
    tsc_start(&c0);
    nanosleep(&req, NULL);
    tsc_stop(&c1);
    uint64_t n1 = cgt_now();
    return (double)(n1 - n0) / (double)(c1 - c0);
}
#else
#error "This diagnostic is intentionally x86-only; use TSC reference."
#endif

static inline uint64_t timer_begin(void) {
#if defined(USE_TSC)
    uint64_t t;
    tsc_start(&t);
    return t;
#elif defined(USE_CGT)
    return cgt_now();
#elif defined(USE_PAPI)
    return (uint64_t)PAPI_get_real_nsec();
#elif defined(USE_WTIME)
    return (uint64_t)(MPI_Wtime() * 1e9);
#else
#error "Select exactly one timer macro."
#endif
}

static inline uint64_t timer_end(void) {
#if defined(USE_TSC)
    uint64_t t;
    tsc_stop(&t);
    return t;
#elif defined(USE_CGT)
    return cgt_now();
#elif defined(USE_PAPI)
    return (uint64_t)PAPI_get_real_nsec();
#elif defined(USE_WTIME)
    return (uint64_t)(MPI_Wtime() * 1e9);
#else
#error "Select exactly one timer macro."
#endif
}

static inline uint64_t elapsed_ns(uint64_t a, uint64_t b, double nspv) {
#if defined(USE_TSC)
    return (uint64_t)((double)(b - a) * nspv);
#else
    (void)nspv;
    return b - a;
#endif
}

static void init_arrays(uint64_t n, double *A, double *B, double *C) {
    for (uint64_t i = 0; i < n; i++) {
        for (uint64_t j = 0; j < n; j++) {
            A[i * n + j] = ((double)i * j) / (double)n;
            B[i * n + j] = ((double)i * j + 1.0) / (double)n;
            C[i * n + j] = ((double)i * j + 2.0) / (double)n;
        }
    }
}

static inline void reset_c_row(uint64_t n, uint64_t i, double *C) {
    for (uint64_t j = 0; j < n; j++) C[i * n + j] = ((double)i * j + 2.0) / (double)n;
}

static inline void gemm_row(uint64_t n, uint64_t i, double *A, double *B, double *C) {
    for (uint64_t j = 0; j < n; j++) C[i * n + j] *= beta;
    for (uint64_t k = 0; k < n; k++) {
        double aik = A[i * n + k];
        for (uint64_t j = 0; j < n; j++) C[i * n + j] += alpha * aik * B[k * n + j];
    }
}

static inline void dsub_loop(uint64_t n) {
    uint64_t ra = n * n * 16 + 1024;
    const uint64_t rb = 7;
    __asm__ volatile(
        "1:\n\t"
        "subq %[rb], %[ra]\n\t"
        "subq %[rb], %[ra]\n\t"
        "cmpq %[rb], %[ra]\n\t"
        "ja 1b\n\t"
        : [ra] "+&r"(ra)
        : [rb] "r"(rb)
        : "cc", "memory");
    checksum_sink += (double)ra;
}

static inline void run_kernel(uint64_t n, uint64_t i, double *A, double *B, double *C) {
#if defined(KERNEL_GEMM)
    gemm_row(n, i % n, A, B, C);
    checksum_sink += C[(i % n) * n + ((i + 1) % n)] * 1e-30;
    reset_c_row(n, i % n, C);
#elif defined(KERNEL_DSUB)
    (void)i;
    (void)A;
    (void)B;
    (void)C;
    dsub_loop(n);
#else
#error "Select exactly one kernel macro."
#endif
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank = 0, nrank = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nrank);

#ifdef USE_PAPI
    if (PAPI_library_init(PAPI_VER_CURRENT) != PAPI_VER_CURRENT) {
        fprintf(stderr, "PAPI_library_init failed\n");
        MPI_Abort(MPI_COMM_WORLD, 3);
    }
#endif

    const uint64_t n = argc > 1 ? strtoull(argv[1], NULL, 10) : 64;
    const uint64_t nsamp = argc > 2 ? strtoull(argv[2], NULL, 10) : 500;
    const char *outdir = argc > 3 ? argv[3] : ".";
    const double nspv = ns_per_tick();

    double *A = (double *)xmalloc(sizeof(double) * n * n);
    double *B = (double *)xmalloc(sizeof(double) * n * n);
    double *C = (double *)xmalloc(sizeof(double) * n * n);
    init_arrays(n, A, B, C);

    for (uint64_t w = 0; w < n; w++) run_kernel(n, w, A, B, C);
    init_arrays(n, A, B, C);
    MPI_Barrier(MPI_COMM_WORLD);

    char path[1024];
    snprintf(path, sizeof(path), "%s/%s_%s_rank%05d.csv", outdir, STR(TIMER_NAME), STR(KERNEL_NAME), rank);
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "failed to open output: %s\n", path);
        MPI_Abort(MPI_COMM_WORLD, 4);
    }
    fprintf(fp, "sample,rank,nrank,size,timer,kernel,elapsed_ns,nspv,checksum\n");
    for (uint64_t s = 0; s < nsamp; s++) {
        uint64_t i = (s + (uint64_t)rank) % n;
        uint64_t t0 = timer_begin();
        run_kernel(n, i, A, B, C);
        uint64_t t1 = timer_end();
        uint64_t e = elapsed_ns(t0, t1, nspv);
        fprintf(fp, "%llu,%d,%d,%llu,%s,%s,%llu,%.12g,%.17g\n",
                (unsigned long long)s, rank, nrank, (unsigned long long)n,
                STR(TIMER_NAME), STR(KERNEL_NAME), (unsigned long long)e, nspv, checksum_sink);
    }
    fclose(fp);

    if (rank == 0) {
        snprintf(path, sizeof(path), "%s/%s_%s_meta.txt", outdir, STR(TIMER_NAME), STR(KERNEL_NAME));
        FILE *mf = fopen(path, "w");
        if (mf) {
            fprintf(mf, "timer=%s\nkernel=%s\nsize=%llu\nnsamp=%llu\nnrank=%d\nnspv=%.12g\n",
                    STR(TIMER_NAME), STR(KERNEL_NAME), (unsigned long long)n, (unsigned long long)nsamp, nrank, nspv);
            fclose(mf);
        }
    }

    free(A);
    free(B);
    free(C);
#ifdef USE_PAPI
    PAPI_shutdown();
#endif
    MPI_Finalize();
    return 0;
}
