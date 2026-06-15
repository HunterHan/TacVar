#define _GNU_SOURCE
#define _ISOC11_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_rng.h>
#include "mpi.h"

#if defined(USE_PAPI) || defined(USE_PAPIX6)
#include "papi.h"

#elif defined(USE_LIKWID)
#include "likwid-marker.h"
#define NEV 20

#endif

// Warmup for 1000ms.
#ifndef NWARM
#define NWARM 1000
#endif

// Ignoring some tests at the beginning
#ifndef NPASS
#define NPASS 2
#endif

// Number of tests for each interval
#ifndef NTEST
#define NTEST 1000
#endif

#ifndef TBASE
#define TBASE 1000
#endif

// Uniform: V1: Number of DSub between two bins; V2: Number of bins.
// Normal: V1: sigma, standard error
// Pareto: V1: alpha, Pareto exponent 
#ifndef V1
#define V1 100
#endif

// Number of intervals (>=1)
#ifndef V2
#define V2 20
#endif

#ifndef FSIZE
#define FSIZE 0
#endif

#ifndef LCUT
#define LCUT 1-0x7fffffff
#endif

#ifndef HCUT
#define HCUT 0x7fffffff-1
#endif

// Prefetching the benchmarking instructions.
#ifndef NPRECALC
#define NPRECALC 1
#endif

// Timing helpers, kept in sync with stencil/jacobi2d5p.c.
#define _read_ns(_ns) \
    do {                                                \
        register uint64_t ns;                           \
        asm volatile(                                   \
            "\n\tRDTSCP"                                \
            "\n\tshl $32, %%rdx"                        \
            "\n\tor  %%rax, %%rdx"                      \
            "\n\tmov %%rdx, %0"                         \
            "\n\t"                                      \
            :"=r" (ns)                                  \
            :                                           \
            : "memory", "%rax", "%rdx");                \
        _ns = ns;                                       \
    } while(0)

#define _mfence asm volatile("lfence" "\n\t":::)

#if defined(__x86_64__)
#include <x86intrin.h>

static inline void tsc_start(uint64_t *cycle)
{
#if defined(USE_TSC)
    unsigned ch, cl;
    asm volatile ("CPUID\n\t"
                  "RDTSC\n\t"
                  "mov %%edx, %0\n\t"
                  "mov %%eax, %1\n\t"
                  : "=r" (ch), "=r" (cl)
                  :
                  : "%rax", "%rbx", "%rcx", "%rdx");
    *cycle = (((uint64_t)ch << 32) | cl);
#elif defined(USE_TSC_FENCE)
    _mm_lfence();
    *cycle = __rdtsc();
#else
    *cycle = __rdtsc();
#endif
}

static inline void tsc_stop(uint64_t *cycle)
{
#if defined(USE_TSC)
    unsigned ch, cl;
    asm volatile ("RDTSCP\n\t"
                  "mov %%edx, %0\n\t"
                  "mov %%eax, %1\n\t"
                  "CPUID\n\t"
                  : "=r" (ch), "=r" (cl)
                  :
                  : "%rax", "%rbx", "%rcx", "%rdx");
    *cycle = (((uint64_t)ch << 32) | cl);
#else
    unsigned aux;
    *cycle = __rdtscp(&aux);
#if defined(USE_TSC_FENCE)
    _mm_lfence();
#endif
#endif
}

static inline uint64_t nsec_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

static double calibrate_ns_per_tsc(void)
{
    struct timespec req = {.tv_sec = 0, .tv_nsec = 200000000};
    uint64_t c0, c1;
    uint64_t n0 = nsec_now();
    tsc_start(&c0);
    nanosleep(&req, NULL);
    tsc_stop(&c1);
    uint64_t n1 = nsec_now();
    return (double)(n1 - n0) / (double)(c1 - c0);
}
#endif

#if defined(USE_CNTVCT) || defined(USE_CNTVCTO)
static uint64_t g_cntfrq = 0;

static inline uint64_t read_cntfrq(void)
{
    uint64_t v;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(v));
    return v;
}

static inline uint64_t read_cntvct(void)
{
    uint64_t v;
    asm volatile("isb; mrs %0, cntvct_el0" : "=r"(v) :: "memory");
    return v;
}

static inline uint64_t read_cntvcto_start(void)
{
    uint64_t v;
    asm volatile("dsb sy; isb; mrs %0, cntvct_el0" : "=r"(v) :: "memory");
    return v;
}

static inline uint64_t read_cntvcto_stop(void)
{
    uint64_t v;
    asm volatile("isb; mrs %0, cntvct_el0; dsb sy; isb" : "=r"(v) :: "memory");
    return v;
}

static inline uint64_t cntvct_to_ns(uint64_t ticks)
{
    return (uint64_t)(((__uint128_t)ticks * 1000000000ULL) / g_cntfrq);
}
#endif

/**
 * @brief A desginated one-line computing kernel.
 */
void
flush_cache(double *pf_a, double *pf_b, double *pf_c, uint64_t npf) {
#ifdef INIT
    for (uint64_t i = 0; i < npf; i ++) {
        pf_a[i] = i;
    }

#elif TRIAD
    for (uint64_t i = 0; i < npf; i ++) {
        pf_a[i] = 0.42 * pf_b[i] + pf_c[i];
    }

#elif SCALE
    for (uint64_t i = 0; i < npf; i ++) {
        pf_a[i] = 1.0001 * pf_b[i];
        pf_b[i] = 1.0001 * pf_a[i];
    }

#endif
    return;
}

/**
 * @brief Reading urandom file for random seeds.
 */
int 
get_urandom(uint64_t *x) {
    FILE *fp = fopen("/dev/urandom", "r");
    if (fp == NULL) {
        printf("Failed to open random file.\n");
        return 1;
    } 

    fread(x, sizeof(uint64_t), 1, fp); 

    fclose(fp);

    return 0;
}

/**
 * @brief Generate a shuffled list to loop through all ADD array's lengths.
 */
void
gen_walklist(uint64_t *len_list) {
#ifndef WALK_FILE
#ifdef UNIFORM
    for (int it = 0; it < NTEST; it ++) {
        for (int i = 0; i < V2; i ++) {
            len_list[NPASS+it*V2+i] = TBASE + i * V1;
        }
    }
    // Shuffle
    // Random seeds using nanosec timestamp
    struct timespec tv;
    uint64_t sec, nsec;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    sec = tv.tv_sec;
    nsec = tv.tv_nsec;
    nsec = sec * 1e9 + nsec + NWARM * 1e6;
    srand(nsec); 
    for (int i = NPASS; i < NPASS + NTEST * V2; i ++){
        int r = (int) (((float)rand() / (float)RAND_MAX) * (float)(NTEST * V2));
        uint64_t temp = len_list[i];
        // Randomly swapping two vkern loop length
        len_list[i] = len_list[NPASS+r];
        len_list[NPASS+r] = temp;
    }
    for (int i = 0; i < NPASS; i ++) {
        len_list[i] = TBASE + V1 * V2;
    }
#else
    double v1 = V1;
    uint64_t seed;
    const gsl_rng_type * T;
    gsl_rng * r;
    gsl_rng_env_setup();
    T = gsl_rng_ranlux389;
    r = gsl_rng_alloc(T);
    get_urandom(&seed);
    gsl_rng_set(r, seed);
    for (int i = 0; i < NPASS+NTEST; i++) {
        double x = 0;
#ifdef NORMAL
        do {
            x = 1.0 + gsl_ran_gaussian(r, v1);
        } while (x < LCUT || x > HCUT);
#elif PARETO
        do {
            x = gsl_ran_pareto(r, v1, 1.0);
        } while (x > HCUT);
#endif
        len_list[i] = (int)(x*TBASE);
    }
    gsl_rng_free(r);
#endif
#else
    // Reading walk list from file
    FILE *fp = fopen("walk.csv", "r");
    for (int it = 0; it < NTEST + NPASS; it ++) {
        fscanf(fp, "%llu\n", len_list+it);
    }
    fclose(fp);
#endif
}


static inline uint64_t sub_loop(uint64_t ra, uint64_t rb, uint64_t lower) {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "1:\n\t"
        "subq %[rb], %[ra]\n\t"
        "cmpq %[lower], %[ra]\n\t"
        "ja 1b\n\t"
        : [ra] "+&r"(ra)
        : [rb] "r"(rb),
        [lower] "r"(lower)
        : "cc"
    );
    return ra;

#elif defined(__aarch64__)
    __asm__ __volatile__(
        "1:\n\t"
        "sub %[ra], %[ra], %[rb]\n\t"
        "cmp %[ra], %[lower]\n\t"
        "b.hi 1b\n\t"
        : [ra] "+&r"(ra)
        : [rb] "r"(rb),
        [lower] "r"(lower)
        : "cc"
    );
    return ra;

#else
    do {
        ra -= rb;
    } while (ra > lower);
    return ra;
#endif
}


static inline void dsub_loop(uint64_t ra, uint64_t rb, uint64_t lower) {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "1:\n\t"
        "subq %[rb], %[ra]\n\t"
        "subq %[rb], %[ra]\n\t"
        "cmpq %[lower], %[ra]\n\t"
        "ja 1b\n\t"
        : [ra] "+&r"(ra)
        : [rb] "r"(rb),
        [lower] "r"(lower)
        : "cc"
    );

#elif defined(__aarch64__)
    __asm__ __volatile__(
        "1:\n\t"
        "sub %[ra], %[ra], %[rb]\n\t"
        "sub %[ra], %[ra], %[rb]\n\t"
        "cmp %[ra], %[lower]\n\t"
        "b.hi 1b\n\t"
        : [ra] "+&r"(ra)
        : [rb] "r"(rb),
        [lower] "r"(lower)
        : "cc"
    );

#else
    do {
        ra -= rb;
        ra -= rb;
    } while (ra > lower);
#endif
}


static __attribute__((noinline)) uint64_t dsub_loop_c(uint64_t ra, uint64_t rb, uint64_t lower) {
    volatile uint64_t vra = ra;
    const uint64_t vrb = rb;
    const uint64_t vlower = lower;
    do {
        vra -= vrb;
        vra -= vrb;
    } while (vra > vlower);
    return vra;
}


static inline uint64_t dsub_split_loop(uint64_t ra, uint64_t rb, uint64_t lower) {
    ra = sub_loop(ra, rb, lower + rb);
    if (ra > lower) {
        ra = sub_loop(ra, rb, lower);
    }
    return ra;
}



int
main(int argc, char **argv) {
    int ntest;
    uint64_t *p_len, *p_ns;
    uint64_t ns0 = 0, ns1 = 0;
    uint64_t tbase = TBASE, fsize = 0, npf = 0;
    uint64_t nsamp = 0, lower = 0, rb_step = 1;
    uint64_t sink = 0;
    double *pf_a = NULL, *pf_b = NULL, *pf_c = NULL;
    int myrank = 0, nrank = 1, errid = 0;
    struct timespec tv;
    double tsc_ns = 1.0;

#ifdef UNIFORM
    uint64_t v1 = V1, v2 = V2;
#elif NORMAL
    double v1 = V1;
#elif PARETO
    double v1 = V1;
#endif

    errid = MPI_Init(NULL, NULL);
    if (errid != MPI_SUCCESS) {
        printf("Failed to init MPI.\n");
        exit(1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &nrank);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

    if (argc < 4) {
        if (myrank == 0) {
            printf("Usage: %s <fsize_kib> <lower> <rb_step> [nsamp]\n", argv[0]);
        }
        MPI_Finalize();
        return 1;
    }
    fsize = (uint64_t)atoll(argv[1]) * 1024ull;
    lower = (uint64_t)atoll(argv[2]);
    rb_step = (uint64_t)atoll(argv[3]);
#ifdef STAGE_TF
    if (argc < 5) {
        if (myrank == 0) {
            printf("NSAMP IS MISSING\n");
        }
        MPI_Finalize();
        return 1;
    }
    nsamp = (uint64_t)atoll(argv[4]);
    if (myrank == 0) {
        printf("NSAMP = %lu\n", nsamp);
    }
#endif
    if (rb_step == 0) {
        if (myrank == 0) {
            printf("rb_step must be nonzero.\n");
        }
        MPI_Finalize();
        return 1;
    }

#ifdef UNIFORM
    ntest = NPASS + NTEST * V2;
#else
    ntest = NTEST + NPASS;
#endif

#if defined(__x86_64__) && (defined(USE_TSC) || defined(USE_TSC_FENCE) || defined(USE_TSC_NATIVE))
    tsc_ns = calibrate_ns_per_tsc();
    if (myrank == 0) {
        printf("Calibrated TSC frequency: %f GHz\n", 1.0 / tsc_ns);
    }
#endif
#if defined(USE_CNTVCT) || defined(USE_CNTVCTO)
    g_cntfrq = read_cntfrq();
#endif

#ifdef USE_PAPI
    PAPI_library_init(PAPI_VER_CURRENT);
#elif defined(USE_PAPIX6)
    int eventset = PAPI_NULL;
    int nev = 6;
    long long int ev_vals_0[6] = {0}, ev_vals_1[6] = {0};
    int64_t *p_ev = (int64_t *)malloc(ntest * nev * sizeof(int64_t));
    PAPI_library_init(PAPI_VER_CURRENT);
    PAPI_create_eventset(&eventset);
    PAPI_add_named_event(eventset, "cpu-cycles");
    PAPI_add_named_event(eventset, "instructions");
    PAPI_add_named_event(eventset, "cache-references");
    PAPI_add_named_event(eventset, "cache-misses");
    PAPI_add_named_event(eventset, "branches");
    PAPI_add_named_event(eventset, "branch-misses");
    PAPI_start(eventset);
#elif defined(USE_LIKWID)
    double ev_vals_0[NEV] = {0}, ev_vals_1[NEV] = {0}, time = 0;
    int nev = NEV, count = 0;
    int64_t *p_ev = NULL;
    LIKWID_MARKER_INIT;
    LIKWID_MARKER_THREADINIT;
    LIKWID_MARKER_REGISTER("vkern");
    LIKWID_MARKER_REGISTER("nev_count");
    LIKWID_MARKER_START("nev_count");
    LIKWID_MARKER_STOP("nev_count");
    LIKWID_MARKER_GET("nev_count", &nev, (double *)ev_vals_1, &time, &count);
    if (myrank == 0) {
        printf("LIKWID event count = %d\n", nev);
    }
    p_ev = (int64_t *)malloc(ntest * nev * sizeof(int64_t));
#endif

    p_len = (uint64_t *)malloc(ntest * sizeof(uint64_t));
    p_ns = (uint64_t *)malloc(ntest * sizeof(uint64_t));
    if (fsize) {
        npf = fsize / sizeof(double);
        pf_a = (double *)malloc(npf * sizeof(double));
        pf_b = (double *)malloc(npf * sizeof(double));
        pf_c = (double *)malloc(npf * sizeof(double));
        for (uint64_t i = 0; i < npf; i++) {
            pf_a[i] = 1.1;
            pf_b[i] = 1.1;
            pf_c[i] = 1.1;
        }
    }

    if (myrank == 0) {
#ifdef UNIFORM
        printf("Generating uniform distribution. Tbase=%lu, interval=%lu, nint=%lu, Ntest=%u.\n",
               tbase, v1, v2, NTEST);
#elif NORMAL
        printf("Generating normal distribution. Tbase=%lu, sigma=%.4f, Ntest=%u.\n",
               tbase, v1, NTEST);
#elif PARETO
        printf("Generating Pareto distribution. Tbase=%lu, alpha=%.4f, Ntest=%u.\n",
               tbase, v1, NTEST);
#else
        printf("Reading vkern walk list from walk.csv.\n");
#endif
        gen_walklist(p_len);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Bcast(p_len, ntest, MPI_UINT64_T, 0, MPI_COMM_WORLD);

    if (myrank == 0) {
        printf("Warming up for %d ms.\n", NWARM);
    }
    clock_gettime(CLOCK_MONOTONIC, &tv);
    uint64_t warm_until = (uint64_t)tv.tv_sec * 1000000000ull + tv.tv_nsec + NWARM * 1000000ull;
    while ((uint64_t)tv.tv_sec * 1000000000ull + tv.tv_nsec < warm_until) {
#if defined(INSITU_DSUB_ASM)
        uint64_t ra = NPRECALC * rb_step * 2;
        dsub_loop(ra, rb_step, 0);
#elif defined(INSITU_DSUB_C_FALLBACK)
        uint64_t ra = NPRECALC * rb_step * 2;
        sink += dsub_loop_c(ra, rb_step, 0);
#elif defined(INSITU_DSUB_SPLIT_ASM)
        uint64_t ra = NPRECALC * rb_step * 2;
        sink += dsub_split_loop(ra, rb_step, 0);
#else
        uint64_t ra = NPRECALC * rb_step;
        sink += sub_loop(ra, rb_step, 0);
#endif
        clock_gettime(CLOCK_MONOTONIC, &tv);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    for (int iwalk = 0; iwalk < ntest; iwalk++) {
#ifndef STAGE_TF
        uint64_t work = p_len[iwalk];
#else
        uint64_t work = nsamp;
#endif
        if (npf) {
            flush_cache(pf_a, pf_b, pf_c, npf);
        }
#if defined(INSITU_DSUB_ASM)
        {
            uint64_t ra = NPRECALC * rb_step * 2;
            dsub_loop(ra, rb_step, 0);
        }
#elif defined(INSITU_DSUB_C_FALLBACK)
        {
            uint64_t ra = NPRECALC * rb_step * 2;
            sink += dsub_loop_c(ra, rb_step, 0);
        }
#elif defined(INSITU_DSUB_SPLIT_ASM)
        {
            uint64_t ra = NPRECALC * rb_step * 2;
            sink += dsub_split_loop(ra, rb_step, 0);
        }
#else
        {
            uint64_t ra = NPRECALC * rb_step;
            sink += sub_loop(ra, rb_step, 0);
        }
#endif

#ifdef USE_PAPI
        ns0 = PAPI_get_real_nsec();
#elif defined(USE_PAPIX6)
        ns0 = PAPI_get_real_nsec();
        PAPI_read(eventset, ev_vals_0);
#elif defined(USE_CGT)
        clock_gettime(CLOCK_MONOTONIC, &tv);
        ns0 = (uint64_t)tv.tv_sec * 1000000000ull + tv.tv_nsec;
#elif defined(USE_WTIME)
        ns0 = (uint64_t)(MPI_Wtime() * 1e9);
#elif defined(USE_CNTVCT)
        ns0 = cntvct_to_ns(read_cntvct());
#elif defined(USE_CNTVCTO)
        ns0 = cntvct_to_ns(read_cntvcto_start());
#elif defined(USE_LIKWID)
        ns0 = ns1;
        LIKWID_MARKER_START("vkern");
#elif defined(USE_TSC) || defined(USE_TSC_FENCE) || defined(USE_TSC_NATIVE)
        tsc_start(&ns0);
#else
        _read_ns(ns0);
        _mfence;
#endif

#if defined(INSITU_DSUB_ASM)
        {
            uint64_t ra = work * rb_step;
            if (ra > lower) {
                ra = work * rb_step * 2;
                dsub_loop(ra, rb_step, lower);
            }
        }
#elif defined(INSITU_DSUB_C_FALLBACK)
        {
            uint64_t ra = work * rb_step;
            if (ra > lower) {
                ra = work * rb_step * 2;
                sink += dsub_loop_c(ra, rb_step, lower);
            }
        }
#elif defined(INSITU_DSUB_SPLIT_ASM)
        {
            uint64_t ra = work * rb_step;
            if (ra > lower) {
                ra = work * rb_step * 2;
                sink += dsub_split_loop(ra, rb_step, lower);
            }
        }
#else
        {
            uint64_t ra = work * rb_step;
            if (ra > lower) {
                sink += sub_loop(ra, rb_step, lower);
            }
        }
#endif

#ifdef USE_PAPI
        ns1 = PAPI_get_real_nsec();
        p_ns[iwalk] = ns1 - ns0;
#elif defined(USE_PAPIX6)
        ns1 = PAPI_get_real_nsec();
        PAPI_read(eventset, ev_vals_1);
        for (int iev = 0; iev < nev; iev++) {
            p_ev[iwalk * nev + iev] = (int64_t)(ev_vals_1[iev] - ev_vals_0[iev]);
        }
        p_ns[iwalk] = ns1 - ns0;
#elif defined(USE_CGT)
        clock_gettime(CLOCK_MONOTONIC, &tv);
        ns1 = (uint64_t)tv.tv_sec * 1000000000ull + tv.tv_nsec;
        p_ns[iwalk] = ns1 - ns0;
#elif defined(USE_WTIME)
        ns1 = (uint64_t)(MPI_Wtime() * 1e9);
        p_ns[iwalk] = ns1 - ns0;
#elif defined(USE_CNTVCT)
        ns1 = cntvct_to_ns(read_cntvct());
        p_ns[iwalk] = ns1 - ns0;
#elif defined(USE_CNTVCTO)
        ns1 = cntvct_to_ns(read_cntvcto_stop());
        p_ns[iwalk] = ns1 - ns0;
#elif defined(USE_LIKWID)
        LIKWID_MARKER_STOP("vkern");
        LIKWID_MARKER_GET("vkern", &nev, (double *)ev_vals_1, &time, &count);
        for (int iev = 0; iev < nev; iev++) {
            p_ev[iwalk * nev + iev] = (int64_t)ev_vals_1[iev] - (int64_t)ev_vals_0[iev];
            ev_vals_0[iev] = ev_vals_1[iev];
        }
        ns1 = (uint64_t)(time * 1e9);
        p_ns[iwalk] = ns1 - ns0;
#elif defined(USE_TSC) || defined(USE_TSC_FENCE) || defined(USE_TSC_NATIVE)
        tsc_stop(&ns1);
        p_ns[iwalk] = (uint64_t)((double)(ns1 - ns0) * tsc_ns);
#else
        _read_ns(ns1);
        _mfence;
        p_ns[iwalk] = ns1 - ns0;
#endif
        MPI_Barrier(MPI_COMM_WORLD);
    }

#ifdef USE_PAPI
    PAPI_shutdown();
#elif defined(USE_PAPIX6)
    PAPI_shutdown();
#elif defined(USE_LIKWID)
    LIKWID_MARKER_CLOSE;
#endif
    MPI_Barrier(MPI_COMM_WORLD);

    char fname[4096], myhost[1024];
    gethostname(myhost, 1024);
#ifdef USE_PAPI
    sprintf(fname, "tvkern_papi_time_%d_%s.csv", myrank, myhost);
#elif defined(USE_CGT)
    sprintf(fname, "tvkern_cgt_time_%d_%s.csv", myrank, myhost);
#elif defined(USE_WTIME)
    sprintf(fname, "tvkern_wtime_time_%d_%s.csv", myrank, myhost);
#elif defined(USE_CNTVCT)
    sprintf(fname, "tvkern_cntvct_time_%d_%s.csv", myrank, myhost);
#elif defined(USE_CNTVCTO)
    sprintf(fname, "tvkern_cntvcto_time_%d_%s.csv", myrank, myhost);
#elif defined(USE_PAPIX6)
    sprintf(fname, "tvkern_papix6_time_%d_%s.csv", myrank, myhost);
#elif defined(USE_LIKWID)
    sprintf(fname, "tvkern_likwid_time_%d_%s.csv", myrank, myhost);
#elif defined(USE_TSC_FENCE)
    sprintf(fname, "tvkern_tsc_fence_time_%d_%s.csv", myrank, myhost);
#elif defined(USE_TSC_NATIVE)
    sprintf(fname, "tvkern_tsc_native_time_%d_%s.csv", myrank, myhost);
#elif defined(USE_TSC)
    sprintf(fname, "tvkern_tsc_time_%d_%s.csv", myrank, myhost);
#else
    sprintf(fname, "tvkern_stiming_time_%d_%s.csv", myrank, myhost);
#endif

    FILE *fp = fopen(fname, "w");
    for (int iwalk = NPASS; iwalk < ntest; iwalk++) {
#ifndef STAGE_TF
        fprintf(fp, "%d,%lu", myrank, p_ns[iwalk]);
#else
        fprintf(fp, "%d,%lu,%lu", myrank, nsamp, p_ns[iwalk]);
#endif
#if defined(USE_LIKWID) || defined(USE_PAPIX6)
        for (int iev = 0; iev < nev; iev++) {
            fprintf(fp, ",%ld", p_ev[iwalk * nev + iev]);
        }
#endif
        fprintf(fp, "\n");
    }
    fclose(fp);

#if defined(USE_LIKWID) || defined(USE_PAPIX6)
    free(p_ev);
#endif
    if (npf) {
        free(pf_a);
        free(pf_b);
        free(pf_c);
    }
    free(p_len);
    free(p_ns);

    if (myrank == 0) {
        printf("Done. %lu\n", sink);
    }
    MPI_Finalize();
    return 0;
}
