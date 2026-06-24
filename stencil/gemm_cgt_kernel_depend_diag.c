#define _GNU_SOURCE
#define _ISOC11_SOURCE
#include <mpi.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#ifdef USE_PAPI
#include "papi.h"
#endif

#if defined(__x86_64__)
#include <x86intrin.h>
#endif

#ifndef NSAMP_DEFAULT
#define NSAMP_DEFAULT 1000
#endif

#ifndef TIMER_NAME
#define TIMER_NAME unknown
#endif
#ifndef DIAG_INNER_LIMIT
#define DIAG_INNER_LIMIT 64
#endif
#define STR2(x) #x
#define STR(x) STR2(x)

static double alpha = 1.5;
static double beta = 1.2;
static volatile double checksum_sink = 0.0;

static void *xmalloc(size_t n) {
    void *p = NULL;
    if (posix_memalign(&p, 64, n) != 0 || !p) {
        fprintf(stderr, "alloc failed %zu\n", n);
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

static inline uint64_t cgt_call_only(struct timespec *tv) {
    clock_gettime(CLOCK_MONOTONIC, tv);
    return (uint64_t)tv->tv_nsec;
}

#if defined(__x86_64__)
static inline void tick_start(uint64_t *cycle) {
    unsigned ch, cl;
    __asm__ volatile ("CPUID\n\tRDTSC\n\tmov %%edx,%0\n\tmov %%eax,%1\n\t"
                      : "=r"(ch), "=r"(cl) :: "%rax", "%rbx", "%rcx", "%rdx");
    *cycle = (((uint64_t)ch << 32) | cl);
}
static inline void tick_stop(uint64_t *cycle) {
    unsigned ch, cl;
    __asm__ volatile ("RDTSCP\n\tmov %%edx,%0\n\tmov %%eax,%1\n\tCPUID\n\t"
                      : "=r"(ch), "=r"(cl) :: "%rax", "%rbx", "%rcx", "%rdx");
    *cycle = (((uint64_t)ch << 32) | cl);
}
static inline uint64_t raw_tick(void) {
    unsigned hi, lo;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
static double ns_per_tick(void) {
    struct timespec req = {.tv_sec = 0, .tv_nsec = 200000000};
    uint64_t c0, c1, n0, n1;
    n0 = cgt_now();
    tick_start(&c0);
    nanosleep(&req, NULL);
    tick_stop(&c1);
    n1 = cgt_now();
    return (double)(n1 - n0) / (double)(c1 - c0);
}
#elif defined(__aarch64__)
static uint64_t g_cntfrq = 0;
static inline uint64_t cntvcto(void) { uint64_t v; __asm__ volatile("mrs %0, cntvct_el0" : "=r"(v)); return v; }
static inline void tick_start(uint64_t *cycle) { __asm__ volatile("isb" ::: "memory"); *cycle = cntvcto(); }
static inline void tick_stop(uint64_t *cycle) { *cycle = cntvcto(); __asm__ volatile("isb" ::: "memory"); }
static inline uint64_t raw_tick(void) { return cntvcto(); }
static double ns_per_tick(void) { if (!g_cntfrq) __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(g_cntfrq)); return 1e9 / (double)g_cntfrq; }
#else
#error unsupported arch
#endif

static inline uint64_t timer_begin(void) {
#if defined(VAR_CGT_CURRENT)
    return cgt_now();
#elif defined(VAR_WTIME_CURRENT)
    return (uint64_t)(MPI_Wtime() * 1e9);
#elif defined(VAR_PAPI_TIME_ONLY)
    return (uint64_t)PAPI_get_real_nsec();
#else
    uint64_t t; tick_start(&t); return t;
#endif
}

static inline uint64_t timer_end(void) {
#if defined(VAR_CGT_CURRENT)
    return cgt_now();
#elif defined(VAR_WTIME_CURRENT)
    return (uint64_t)(MPI_Wtime() * 1e9);
#elif defined(VAR_PAPI_TIME_ONLY)
    return (uint64_t)PAPI_get_real_nsec();
#else
    uint64_t t; tick_stop(&t); return t;
#endif
}

static inline uint64_t elapsed_ns(uint64_t a, uint64_t b, double nspv) {
#if defined(VAR_CGT_CURRENT) || defined(VAR_WTIME_CURRENT) || defined(VAR_PAPI_TIME_ONLY)
    (void)nspv;
    return b - a;
#else
    return (uint64_t)((double)(b - a) * nspv);
#endif
}

static void init_arrays(uint64_t n, double *A, double *B, double *C) {
    for (uint64_t i = 0; i < n; i++) {
        for (uint64_t j = 0; j < n; j++) {
            A[i*n+j] = ((double)i * j) / (double)n;
            B[i*n+j] = ((double)i * j + 1.0) / (double)n;
            C[i*n+j] = ((double)i * j + 2.0) / (double)n;
        }
    }
}

static inline void reset_c_row(uint64_t n, uint64_t i, double *C) {
    for (uint64_t j = 0; j < n; j++) C[i*n+j] = ((double)i * j + 2.0) / (double)n;
}

static inline void gemm_row(uint64_t n, uint64_t i, double *A, double *B, double *C) {
    for (uint64_t j = 0; j < n; j++) C[i*n+j] *= beta;
    for (uint64_t k = 0; k < n; k++) {
        double aik = A[i*n+k];
        for (uint64_t j = 0; j < n; j++) C[i*n+j] += alpha * aik * B[k*n+j];
    }
}

static inline void dsub_loop(uint64_t n) {
#if defined(__x86_64__)
    uint64_t ra = n * n * 16 + 1024;
    uint64_t rb = 7;
    __asm__ volatile(
        "1:\n\t"
        "subq %[rb], %[ra]\n\t"
        "subq %[rb], %[ra]\n\t"
        "cmpq %[rb], %[ra]\n\t"
        "ja 1b\n\t"
        : [ra] "+&r"(ra) : [rb] "r"(rb) : "cc", "memory");
    checksum_sink += (double)ra;
#else
    volatile uint64_t ra = n * n * 16 + 1024;
    uint64_t rb = 7;
    while (ra > rb) { ra -= rb; ra -= rb; }
    checksum_sink += (double)ra;
#endif
}

static inline uint64_t diag_lim(uint64_t n) { return n < DIAG_INNER_LIMIT ? n : DIAG_INNER_LIMIT; }

static inline void gemm_like(uint64_t n, double *A, double *B, double *C, uint64_t i) {
    uint64_t row = i % n;
    uint64_t lim = diag_lim(n);
    for (uint64_t j = 0; j < lim; j++) C[row*n+j] = C[row*n+j] * beta + alpha * A[row*n+j] * B[row*n+j];
    for (uint64_t k = 0; k < lim; k++) {
        double aik = A[row*n+k];
        for (uint64_t j = 0; j < lim; j++) C[row*n+j] += alpha * aik * B[k*n+j] * 0.0000001;
    }
}

static inline void gemm_row_bounded(uint64_t n, uint64_t i, double *A, double *B, double *C) {
    uint64_t row = i % n;
    uint64_t lim = diag_lim(n);
    for (uint64_t j = 0; j < lim; j++) C[row*n+j] *= beta;
    for (uint64_t k = 0; k < lim; k++) {
        double aik = A[row*n+k];
        for (uint64_t j = 0; j < lim; j++) C[row*n+j] += alpha * aik * B[k*n+j];
    }
}

typedef void (*kernel_fn)(uint64_t, uint64_t, double*, double*, double*);

static void run_gemm_row(uint64_t n, uint64_t i, double *A, double *B, double *C) { gemm_row(n, i % n, A, B, C); }
static void run_gemm_like(uint64_t n, uint64_t i, double *A, double *B, double *C) { gemm_like(n, A, B, C, i); }
static void run_gemm_row_bounded(uint64_t n, uint64_t i, double *A, double *B, double *C) { gemm_row_bounded(n, i % n, A, B, C); }
static void run_dsub(uint64_t n, uint64_t i, double *A, double *B, double *C) { (void)i; (void)A; (void)B; (void)C; dsub_loop(n); }

static inline void reset_if_mutating(kernel_fn fn, uint64_t n, uint64_t i, double *C) {
    if (fn == run_gemm_row || fn == run_gemm_like || fn == run_gemm_row_bounded) reset_c_row(n, i % n, C);
}

static uint64_t measure_plain(kernel_fn fn, uint64_t n, uint64_t i, double *A, double *B, double *C, double nspv) {
    uint64_t t0 = timer_begin();
    fn(n, i, A, B, C);
    uint64_t t1 = timer_end();
    uint64_t out = elapsed_ns(t0, t1, nspv);
    reset_if_mutating(fn, n, i, C);
    return out;
}

static uint64_t measure_tick_after_cgt_prelude(kernel_fn fn, uint64_t n, uint64_t i, double *A, double *B, double *C, double nspv) {
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    volatile uint64_t pre_ns = ns_from_ts(&tv);
    uint64_t t0, t1;
    tick_start(&t0);
    fn(n, i, A, B, C);
    tick_stop(&t1);
    checksum_sink += (double)(pre_ns & 1u);
    uint64_t out = (uint64_t)((double)(t1 - t0) * nspv);
    reset_if_mutating(fn, n, i, C);
    return out;
}

static uint64_t measure_tick_after_cgt_call_only(kernel_fn fn, uint64_t n, uint64_t i, double *A, double *B, double *C, double nspv) {
    struct timespec tv;
    volatile uint64_t dummy = cgt_call_only(&tv);
    uint64_t t0, t1;
    tick_start(&t0);
    fn(n, i, A, B, C);
    tick_stop(&t1);
    volatile uint64_t delayed = ns_from_ts(&tv);
    checksum_sink += (double)((dummy + delayed) & 1u);
    uint64_t out = (uint64_t)((double)(t1 - t0) * nspv);
    reset_if_mutating(fn, n, i, C);
    return out;
}

static uint64_t measure_variant(kernel_fn fn, uint64_t n, uint64_t i, double *A, double *B, double *C, double nspv) {
#if defined(VAR_TICK_AFTER_CGT_PRELUDE_GEMM) || defined(VAR_TICK_AFTER_CGT_PRELUDE_DSUB) || defined(VAR_CNTVCTO_AFTER_CGT_PRELUDE_GEMM)
    return measure_tick_after_cgt_prelude(fn, n, i, A, B, C, nspv);
#elif defined(VAR_TICK_AFTER_CGT_CALL_ONLY_GEMM)
    return measure_tick_after_cgt_call_only(fn, n, i, A, B, C, nspv);
#else
    return measure_plain(fn, n, i, A, B, C, nspv);
#endif
}

static void vdso_probe(FILE *f) {
#ifdef __linux__
    FILE *maps = fopen("/proc/self/maps", "r");
    char line[512];
    int found = 0;
    while (maps && fgets(line, sizeof(line), maps)) if (strstr(line, "[vdso]")) { found = 1; fprintf(f, "vdso_map=%s", line); break; }
    if (maps) fclose(maps);
    if (!found) fprintf(f, "vdso_map=not_found\n");
#else
    fprintf(f, "vdso_map=not_linux\n");
#endif
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, nrank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nrank);
#ifdef USE_PAPI
    if (PAPI_library_init(PAPI_VER_CURRENT) != PAPI_VER_CURRENT) MPI_Abort(MPI_COMM_WORLD, 3);
#endif
    uint64_t n = argc > 1 ? strtoull(argv[1], NULL, 10) : 32;
    uint64_t nsamp = argc > 2 ? strtoull(argv[2], NULL, 10) : NSAMP_DEFAULT;
    const char *outdir = argc > 3 ? argv[3] : ".";
    double nspv = ns_per_tick();
    double *A = (double*)xmalloc(sizeof(double) * n * n);
    double *B = (double*)xmalloc(sizeof(double) * n * n);
    double *C = (double*)xmalloc(sizeof(double) * n * n);
    init_arrays(n, A, B, C);
    for (uint64_t w = 0; w < n; w++) gemm_row(n, w % n, A, B, C);
    init_arrays(n, A, B, C);
    MPI_Barrier(MPI_COMM_WORLD);
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s_rank%05d.csv", outdir, STR(TIMER_NAME), rank);
    FILE *fp = fopen(path, "w");
    if (!fp) MPI_Abort(MPI_COMM_WORLD, 4);
    fprintf(fp, "sample,rank,size,variant,tm_gemm_ns,te_dsub_ns,te_gemm_like_ns,te_gemm_row_ns,nspv,checksum\n");
    for (uint64_t s = 0; s < nsamp; s++) {
        uint64_t i = (s + (uint64_t)rank) % n;
        uint64_t tm = measure_variant(run_gemm_row, n, i, A, B, C, nspv);
        uint64_t te_d = measure_variant(run_dsub, n, i, A, B, C, nspv);
        uint64_t te_l = measure_variant(run_gemm_like, n, i, A, B, C, nspv);
        uint64_t te_r = measure_variant(run_gemm_row_bounded, n, i, A, B, C, nspv);
        double chk = C[i*n + (i % n)] + checksum_sink;
        fprintf(fp, "%llu,%d,%llu,%s,%llu,%llu,%llu,%llu,%.12g,%.17g\n",
                (unsigned long long)s, rank, (unsigned long long)n, STR(TIMER_NAME),
                (unsigned long long)tm, (unsigned long long)te_d, (unsigned long long)te_l,
                (unsigned long long)te_r, nspv, chk);
    }
    fclose(fp);
    if (rank == 0) {
        snprintf(path, sizeof(path), "%s/%s_meta.txt", outdir, STR(TIMER_NAME));
        FILE *mf = fopen(path, "w");
        if (mf) {
            fprintf(mf, "variant=%s\nsize=%llu\nnsamp=%llu\nnrank=%d\nnspv=%.12g\n", STR(TIMER_NAME), (unsigned long long)n, (unsigned long long)nsamp, nrank, nspv);
            vdso_probe(mf);
            fclose(mf);
        }
    }
    free(A); free(B); free(C);
#ifdef USE_PAPI
    PAPI_shutdown();
#endif
    MPI_Finalize();
    return 0;
}
