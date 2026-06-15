#define _GNU_SOURCE
#define _ISOC11_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#if defined(__x86_64__)
#include <x86intrin.h>
#endif
#include "mpi.h"

#ifdef TIMING
#if defined(USE_PAPI) || defined(USE_PAPIX6)
#include "papi.h"

#elif defined(USE_LIKWID)
#include "likwid-marker.h"
#define NEV 20
#endif

#endif

// Warmup for 1000ms.
#ifndef NWARM
#define NWARM 1000
#endif

// Number of tests for each interval
#ifndef NTEST
#define NTEST 10
#endif

#ifndef NARR
#define NARR 100
#endif

#ifndef NPASS
#define NPASS 1 
#endif


// Timing macros
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
    } while(0);

#define _read_cy(_cy) \
    do {                                                \
        register uint64_t cy;                           \
        asm volatile(                                   \
            _pfc_read(0x40000001)                       \
            : "=r"(cy)                                  \
            :                                           \
            : "memory", "rax", "rcx", "rdx"             \
        );                                              \
        _cy = cy;                                       \
    } while(0)

#define _mfence asm volatile("lfence"   "\n\t":::);


#define NS_PER_TICK  1

#if defined(__x86_64__) && (defined(USE_TSC) || defined(USE_TSC_FENCE) || defined(USE_TSC_NATIVE))
static inline void tsc_start(uint64_t *cycle){
#if defined(USE_TSC)
    unsigned ch, cl;

    asm volatile (  "CPUID" "\n\t"
                    "RDTSC" "\n\t"
                    "mov %%edx, %0" "\n\t"
                    "mov %%eax, %1" "\n\t"
                    : "=r" (ch), "=r" (cl)
                    :
                    : "%rax", "%rbx", "%rcx", "%rdx");
    *cycle = ( ((uint64_t)ch << 32) | cl );
#elif defined(USE_TSC_FENCE)
    _mm_lfence();
    *cycle = __rdtsc();
#else
    *cycle = __rdtsc();
#endif
}

static inline void tsc_stop(uint64_t *cycle){
#if defined(USE_TSC)
    unsigned ch, cl;

    asm volatile (  "RDTSCP" "\n\t"
                    "mov %%edx, %0" "\n\t"
                    "mov %%eax, %1" "\n\t"
                    "CPUID" "\n\t"
                    : "=r" (ch), "=r" (cl)
                    :
                    : "%rax", "%rbx", "%rcx", "%rdx");

    *cycle = ( ((uint64_t)ch << 32) | cl );
#else
    unsigned aux;
    *cycle = __rdtscp(&aux);
#if defined(USE_TSC_FENCE)
    _mm_lfence();
#endif
#endif
}

static inline uint64_t nsec_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

static double calibrate_ns_per_tsc(void){
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

    return (double)(n1 - n0) / (double)(c1 - c0);
}
#endif

#if defined(USE_CNTVCT) || defined(USE_CNTVCTO)
static uint64_t g_cntfrq = 0;

static inline uint64_t
cntvct_to_ns(uint64_t ticks)
{
    return (uint64_t)(((__uint128_t)ticks * 1000000000ULL) / g_cntfrq);
}

static inline uint64_t
read_cntvct(void)
{
    uint64_t ticks;
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(ticks));
    return ticks;
}

static inline uint64_t
read_cntvcto_start(void)
{
    uint64_t ticks;
    __asm__ __volatile__("isb\n\t"
                         "mrs %0, cntvct_el0\n\t"
                         "isb"
                         : "=r"(ticks)
                         :
                         : "memory");
    return ticks;
}

static inline uint64_t
read_cntvcto_stop(void)
{
    uint64_t ticks;
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(ticks) :: "memory");
    return ticks;
}

static void
init_cntvct_freq(void)
{
    __asm__ __volatile__("mrs %0, cntfrq_el0" : "=r"(g_cntfrq));
}
#endif

/**
 * @brief Fill arr[size] with random number.
 */
void
fill_random(double *arr, size_t size) {
    struct timespec tv;
    uint64_t sec, nsec;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    sec = tv.tv_sec;
    nsec = tv.tv_nsec;
    nsec = sec * 1e9 + nsec + NWARM * 1e6;
    srand(nsec);
    for (size_t i = 0; i < size; i ++){
        arr[i] = (float)rand() / (float)RAND_MAX;
    }

    return;
}

__attribute__((noinline)) uint64_t sub_loop(uint64_t ra, uint64_t rb, uint64_t lower) {
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

static inline uint64_t dsub_loop(uint64_t ra, uint64_t rb, uint64_t lower) {
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
    return ra;
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
    return ra;
#else
    do {
        ra -= rb;
        ra -= rb;
    } while (ra > lower);
    return ra;
#endif
}


int
main(int argc, char **argv) {
    uint64_t ntest;
    /* Vars for Stencil */
    double **w, **Di, **p, **Kx, **Ky;
    double rx, ry, pw;
    uint64_t narr;
    struct timespec tv;
    uint64_t volatile nsec_st, nsec_en; // For warmup
    int myrank, nrank, errid;
    double tsc_ns = 1.0;
    uint64_t nsamp;
    uint64_t ra_lower_boundary, rb_step;

    if (argc >= 2) {
        narr = (uint64_t)atoll(argv[1]);
    } else {
        narr = NARR;
    }

    if (argc >= 4) {
        ra_lower_boundary = (uint64_t)atoll(argv[2]);
        rb_step = (uint64_t)atoll(argv[3]);
        printf("ra_lower_boundary = %lu, rb_step = %lu\n", ra_lower_boundary, rb_step);
    } else {
        printf("ra boundary missing!\n");
        return -1;
    }

#ifdef STAGE_TF
    if (argc >= 5) {
        nsamp = (uint64_t)atoll(argv[4]);
        printf("NSAMP = %lu\n", nsamp);
    } else {
        printf("NSAMP IS MISSING\n");
        return -1;
    }
#endif

    w = (double **)malloc(narr * sizeof(double*));
    for (size_t i = 0; i < narr; i ++) {
        w[i] = (double *)malloc(narr * sizeof(double));
    }
    Di = (double **)malloc(narr * sizeof(double*));
    for (size_t i = 0; i < narr; i ++) {
        Di[i] = (double *)malloc(narr * sizeof(double));
    }
    p = (double **)malloc(narr * sizeof(double*));
    for (size_t i = 0; i < narr; i ++) {
        p[i] = (double *)malloc(narr * sizeof(double));
    }
    Kx = (double **)malloc(narr * sizeof(double*));
    for (size_t i = 0; i < narr; i ++) {
        Kx[i] = (double *)malloc(narr * sizeof(double));
    }
    Ky = (double **)malloc(narr * sizeof(double*));
    for (size_t i = 0; i < narr; i ++) {
        Ky[i] = (double *)malloc(narr * sizeof(double));
    }

    ntest = NPASS + NTEST;

    errid = MPI_Init(NULL, NULL);
    if (errid != MPI_SUCCESS) {
        printf("Faild to init MPI.\n");
        exit(1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &nrank);
#if defined(USE_CNTVCT) || defined(USE_CNTVCTO)
    init_cntvct_freq();
#endif
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

#if defined(__x86_64__) && (defined(USE_TSC) || defined(USE_TSC_FENCE) || defined(USE_TSC_NATIVE))
    tsc_ns = calibrate_ns_per_tsc();
    if (myrank == 0) {
        printf("Calibrated TSC frequency: %f GHz\n", 1e0 / tsc_ns);
    }
#endif

    if (myrank == 0) {
        printf("TeaLeaf cg_calc_w kernel.\nNTEST=%lu, NPASS=%u, NARR=%lu \n", 
                ntest, NPASS, narr);
    }

    // Warm up
    pw = 0;
    do {
        for (uint64_t i = 0; i < narr; i ++) {
            fill_random(w[i], narr);
            fill_random(Di[i], narr);
            fill_random(p[i], narr);
            fill_random(Kx[i], narr);
            fill_random(Ky[i], narr);
        }
        fill_random(&rx, 1);
        fill_random(&ry, 1);

        if (myrank == 0) {
            printf("Warming up for %d ms.\n", NWARM);
        }
        struct timespec tv;
        uint64_t volatile sec, nsec; // For warmup
        clock_gettime(CLOCK_MONOTONIC, &tv);
        sec = tv.tv_sec;
        nsec = tv.tv_nsec;
        nsec = sec * 1e9 + nsec + NWARM * 1e6;

        while (tv.tv_sec * 1e9 + tv.tv_nsec < nsec) {
            clock_gettime(CLOCK_MONOTONIC, &tv);
            for (uint64_t i = 1; i < narr-1; i ++) {
                for (uint64_t j = 1; j < narr-1; j ++) {
                    w[i][j] = Di[i][j] * p[i][j]  \
                              - ry * (Ky[i+1][j] * p[i+1][j] + Ky[i][j] * p[i-1][j]) \
                              - rx * (Kx[i][j+1] * p[i][j+1] + Kx[i][j] * p[i][j-1]);
                }
                for (uint64_t j = 0; j < narr; j ++) {
                    pw = pw + w[i][j] * p[i][j];
                }
            }
        }
    } while (0);



#ifdef TIMING
    uint64_t *p_ns, ns0 = 0, ns1 = 0;

#ifdef USE_PAPI
    // Init PAPI
    int eventset = PAPI_NULL;
    PAPI_library_init(PAPI_VER_CURRENT);
    PAPI_create_eventset(&eventset);
    PAPI_start(eventset);
    
#elif USE_PAPIX6
    // Init PAPI
    int eventset = PAPI_NULL;
    int nev = 6;
    long long int ev_vals_0[6]={0}, ev_vals_1[6]={0};
    int64_t *p_ev;
    p_ev = (int64_t *)malloc(ntest * narr * nev * sizeof(int64_t));
    for (int iev = 0; iev < nev; iev ++) {
        ev_vals_0[iev] = 0;
        ev_vals_1[iev] = 0;
    }
    PAPI_library_init(PAPI_VER_CURRENT);
    PAPI_create_eventset(&eventset);
    PAPI_add_named_event(eventset, "cpu-cycles");
    PAPI_add_named_event(eventset, "instructions");
    PAPI_add_named_event(eventset, "cache-references");
    PAPI_add_named_event(eventset, "cache-misses");
    PAPI_add_named_event(eventset, "branches");
    PAPI_add_named_event(eventset, "branch-misses");
    PAPI_start(eventset);

#elif USE_LIKWID
    // Init LIKWID
    double ev_vals_0[NEV]={0}, ev_vals_1[NEV]={0}, time=0;
    int nev = NEV, count;
    int64_t *p_ev;
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
    p_ev = (int64_t *)malloc(ntest * narr * nev * sizeof(int64_t));
    for (int iev = 0; iev < nev; iev ++) {
        ev_vals_0[iev] = 0;
        ev_vals_1[iev] = 0;
    }

#endif

    p_ns = (uint64_t *)malloc(ntest * narr * sizeof(uint64_t));
#endif

    for (uint64_t i = 0; i < narr; i ++) {
        fill_random(w[i], narr);
        fill_random(Di[i], narr);
        fill_random(p[i], narr);
        fill_random(Kx[i], narr);
        fill_random(Ky[i], narr);
    }
    if (argc > 1) {
        rx = 0.001;
        ry = 0.001;
    }
    pw = 0;

    if (myrank == 0) {
        printf("Start running.\n");
        fflush(stdout);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    clock_gettime(CLOCK_MONOTONIC, &tv);
    nsec_st = tv.tv_sec * 1e9 + tv.tv_nsec;

    uint64_t ra_res = 0;
    for (int it = 0; it < ntest; it ++) {
        for (uint64_t j = 1; j < narr-1; j ++) {

#if defined(USE_PREWARM) && !defined(STAGE_TF)
            for (uint64_t k = 1; k < narr-1; k ++) {
                w[j][k] = Di[j][k] * p[j][k]  \
                          - ry * (Ky[j+1][k] * p[j+1][k] + Ky[j][k] * p[j-1][k]) \
                          - rx * (Kx[j][k+1] * p[j][k+1] + Kx[j][k] * p[j][k-1]);
            }


#endif

#ifndef STAGE_TF
#ifdef TIMING

// Timing.
#ifdef USE_PAPI
            ns0 = PAPI_get_real_nsec();

#elif USE_PAPIX6
            PAPI_read(eventset, ev_vals_0);
            ns0 = PAPI_get_real_nsec();

#elif USE_CGT
            // asm volatile("cpuid" ::: "rax", "rbx", "rcx", "rdx", "memory");
            clock_gettime(CLOCK_MONOTONIC, &tv);
            ns0 = tv.tv_sec * 1e9 + tv.tv_nsec;

#elif USE_WTIME
            // asm volatile("cpuid" ::: "rax", "rbx", "rcx", "rdx", "memory");
            ns0 = (uint64_t)(MPI_Wtime() * 1e9);

#elif USE_CNTVCT
            ns0 = cntvct_to_ns(read_cntvct());

#elif USE_CNTVCTO
            ns0 = cntvct_to_ns(read_cntvcto_start());

#elif USE_LIKWID
            //ns0 = 0;
            LIKWID_MARKER_START("vkern"); 

#elif defined(USE_TSC) || defined(USE_TSC_FENCE) || defined(USE_TSC_NATIVE)
            tsc_start(&ns0);

#else
            _read_ns (ns0);
            _mfence;

#endif

#endif

#endif
            for (uint64_t k = 1; k < narr-1; k ++) {
                w[j][k] = Di[j][k] * p[j][k]  \
                          - ry * (Ky[j+1][k] * p[j+1][k] + Ky[j][k] * p[j-1][k]) \
                          - rx * (Kx[j][k+1] * p[j][k+1] + Kx[j][k] * p[j][k-1]);
            }

#ifdef STAGE_TF
#ifdef TIMING

// Timing.
#ifdef USE_PAPI
            ns0 = PAPI_get_real_nsec();

#elif USE_PAPIX6
            PAPI_read(eventset, ev_vals_0);
            ns0 = PAPI_get_real_nsec();


#elif USE_CGT
            // asm volatile("cpuid" ::: "rax", "rbx", "rcx", "rdx", "memory");
            clock_gettime(CLOCK_MONOTONIC, &tv);
            ns0 = tv.tv_sec * 1e9 + tv.tv_nsec;

#elif USE_WTIME
            // asm volatile("cpuid" ::: "rax", "rbx", "rcx", "rdx", "memory");
            ns0 = (uint64_t)(MPI_Wtime() * 1e9);

#elif USE_CNTVCT
            ns0 = cntvct_to_ns(read_cntvct());

#elif USE_CNTVCTO
            ns0 = cntvct_to_ns(read_cntvcto_start());

#elif USE_LIKWID
            LIKWID_MARKER_START("vkern"); 

#elif defined(USE_TSC) || defined(USE_TSC_FENCE) || defined(USE_TSC_NATIVE)
            tsc_start(&ns0);

#else
            _read_ns (ns0);
            _mfence;

#endif

#endif
            register uint64_t rb = rb_step;
            register uint64_t ra;
            register uint64_t lower = ra_lower_boundary;
#ifdef INSITU_SUB_ASM
            ra = nsamp * rb;
            sub_loop(ra, rb, lower);
#endif
#ifdef INSITU_DSUB_ASM
            ra = nsamp * rb * 2;
            dsub_loop(ra, rb, lower);
#endif
#endif

#ifdef TIMING

#ifdef USE_PAPI
            ns1 = PAPI_get_real_nsec();
            p_ns[it*narr+j] = (uint64_t)(ns1 - ns0);

#elif USE_PAPIX6
            ns1 = PAPI_get_real_nsec();
            PAPI_read(eventset, ev_vals_1);
            for (int iev = 0; iev < nev; iev ++) {
                p_ev[it * narr * nev + j * nev + iev] = (int64_t)(ev_vals_1[iev] - ev_vals_0[iev]);
            }
            p_ns[it*narr+j] = (uint64_t)(ns1 - ns0);

#elif USE_CGT
            clock_gettime(CLOCK_MONOTONIC, &tv);
            // asm volatile("cpuid" ::: "rax", "rbx", "rcx", "rdx", "memory");
            ns1 = tv.tv_sec * 1e9 + tv.tv_nsec;
            p_ns[it*narr+j] = ns1 - ns0;

#elif USE_WTIME
            ns1 = (uint64_t)(MPI_Wtime() * 1e9);
            // asm volatile("cpuid" ::: "rax", "rbx", "rcx", "rdx", "memory");
            p_ns[it*narr+j] = ns1 - ns0;

#elif USE_CNTVCT
            ns1 = cntvct_to_ns(read_cntvct());
            p_ns[it*narr+j] = ns1 - ns0;

#elif USE_CNTVCTO
            ns1 = cntvct_to_ns(read_cntvcto_stop());
            p_ns[it*narr+j] = ns1 - ns0;

#elif USE_LIKWID
            LIKWID_MARKER_STOP("vkern"); 
            LIKWID_MARKER_GET("vkern", &nev, (double*)ev_vals_1, &time, &count);
            for (int iev = 0; iev < nev; iev ++) {
                p_ev[it * narr * nev + j * nev + iev] = (int64_t)ev_vals_1[iev] - (int64_t)ev_vals_0[iev];
                ev_vals_0[iev] = ev_vals_1[iev];
            }
            // We do not use "time" argument as the timestamp because the perfmon swith the timer
            // unexpectedly. It is good to use FIXC2: CPU_CLK_UNHALTED_REF and convert with tsc_ns
            // ns1 = (uint64_t)((double)p_ev[it * narr * nev + j * nev + 2] / tsc_ns);
            ns1 = (uint64_t) (time * 1e9);
            p_ns[it*narr+j] = ns1 - ns0;
            ns0 = ns1;

#elif defined(USE_TSC) || defined(USE_TSC_FENCE) || defined(USE_TSC_NATIVE)
            tsc_stop(&ns1);
            p_ns[it*narr+j] = (uint64_t)((double)(ns1 - ns0) * tsc_ns);

#else
            _read_ns (ns1);
            _mfence;
            p_ns[it*narr+j] = (uint64_t)((double)(ns1 - ns0) * tsc_ns);
#endif

#endif
            for (uint64_t k = 0; k < narr; k ++) {
                pw = pw + w[j][k] * p[j][k];
            }
            if (nrank > 1) {
                MPI_Allreduce(&pw, &pw, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &tv);
    nsec_en = tv.tv_sec * 1e9 + tv.tv_nsec;
    printf("Rank %d run time: %lu ns\n", myrank, nsec_en - nsec_st);

    MPI_Barrier(MPI_COMM_WORLD);

#if defined(USE_PAPI) || defined(USE_PAPIX6)
// #if defined(USE_PAPIX6)
    PAPI_shutdown();

#elif USE_LIKWID
    // Finalize LIKWID
    LIKWID_MARKER_CLOSE;    
#endif

    // Each rank writes its own file.
#ifdef TIMING
    char fname[4096], myhost[1024];
    gethostname(myhost, 1024);
#ifdef USE_PAPI
    sprintf(fname, "tl_f90_cg_calc_w_papi_time_%d_%s.csv", myrank, myhost);
    FILE *fp = fopen(fname, "w");
#elif USE_CGT
    sprintf(fname, "tl_f90_cg_calc_w_cgt_time_%d_%s.csv", myrank, myhost);
    FILE *fp = fopen(fname, "w");
#elif USE_WTIME
    sprintf(fname, "tl_f90_cg_calc_w_wtime_time_%d_%s.csv", myrank, myhost);
    FILE *fp = fopen(fname, "w");
#elif USE_CNTVCT
    sprintf(fname, "tl_f90_cg_calc_w_cntvct_time_%d_%s.csv", myrank, myhost);
    FILE *fp = fopen(fname, "w");
#elif USE_CNTVCTO
    sprintf(fname, "tl_f90_cg_calc_w_cntvcto_time_%d_%s.csv", myrank, myhost);
    FILE *fp = fopen(fname, "w");
#elif USE_PAPIX6
    sprintf(fname, "tl_f90_cg_calc_w_papix6_time_%d_%s.csv", myrank, myhost);
    FILE *fp = fopen(fname, "w");
#elif USE_LIKWID
    sprintf(fname, "tl_f90_cg_calc_w_likwid_time_%d_%s.csv", myrank, myhost);
    FILE *fp = fopen(fname, "w");
#elif defined(USE_TSC_FENCE)
    sprintf(fname, "tl_f90_cg_calc_w_tsc_fence_time_%d_%s.csv", myrank, myhost);
    FILE *fp = fopen(fname, "w");
#elif defined(USE_TSC_NATIVE)
    sprintf(fname, "tl_f90_cg_calc_w_tsc_native_time_%d_%s.csv", myrank, myhost);
    FILE *fp = fopen(fname, "w");
#elif defined(USE_TSC)
    sprintf(fname, "tl_f90_cg_calc_w_tsc_time_%d_%s.csv", myrank, myhost);
    FILE *fp = fopen(fname, "w");
#else
    sprintf(fname, "tl_f90_cg_calc_w_stiming_time_%d_%s.csv", myrank, myhost);
    FILE *fp = fopen(fname, "w");
#endif

    for (int it = NPASS; it < ntest; it ++) {
        for (size_t j = 1; j < narr-1; j ++) {
#ifndef STAGE_TF
            fprintf(fp, "%d,%lu", myrank, p_ns[it*narr+j]);
#else
            fprintf(fp, "%d,%lu,%lu", myrank, nsamp, p_ns[it*narr+j]);
#endif

#if defined(USE_LIKWID) || defined(USE_PAPIX6)
            for (int iev = 0; iev < nev; iev ++) {
                fprintf(fp, ",%ld", p_ev[it*narr*nev+j*nev+iev]);
            }
#endif
            fprintf(fp, "\n");
        }
    }

    fclose(fp);
    free(p_ns);


#if defined(USE_LIKWID) || defined(USE_PAPIX6)
    free(p_ev);
#endif

#endif

    if (myrank == 0) {
        printf("Done. %f %lu\n", pw, ra_res);
    }

    for (size_t i = 0; i < narr; i ++) {
        free(w[i]);
        free(Di[i]);
        free(p[i]);
        free(Kx[i]);
        free(Ky[i]);
    }

    free(w);
    free(Di);
    free(p);
    free(Kx);
    free(Ky);


    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Finalize();

    return 0;
}
