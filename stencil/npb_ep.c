#define _GNU_SOURCE
#define _ISOC11_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <math.h>
#include "mpi.h"

/*
 * Target kernel adapted from NASA NPB 3.4.4 EP random pair generation and Gaussian statistics core.
 *
 * TacVar adds MPI/timer/filter plumbing and times one source-level target
 * unit per sample. This is not a complete upstream benchmark and does not
 * call BLAS, FFTW, MKL, or vendor sparse libraries.
 */

#if defined(__x86_64__)
#include <x86intrin.h>
#endif

#ifdef TIMING
#if defined(USE_PAPI) || defined(USE_PAPIX6)
#include "papi.h"
#endif
#endif

#ifndef NWARM
#define NWARM 1000
#endif
#ifndef NTEST
#define NTEST 10
#endif
#ifndef NARR
#define NARR 100
#endif
#ifndef NPASS
#define NPASS 1
#endif

#if defined(__x86_64__)
static inline void cpuid_serialize(void) {
    unsigned int eax = 0, ebx, ecx = 0, edx;
    __asm__ __volatile__("cpuid" : "+a"(eax), "=b"(ebx), "+c"(ecx), "=d"(edx) :: "memory");
}
#else
static inline void cpuid_serialize(void) { __asm__ __volatile__("" ::: "memory"); }
#endif

#if defined(__x86_64__)
static inline void tsc_start(uint64_t *cycle){
#if defined(USE_TSC)
    unsigned ch, cl;
    __asm__ volatile ("CPUID\n\tRDTSC\n\tmov %%edx, %0\n\tmov %%eax, %1\n\t" : "=r" (ch), "=r" (cl) :: "%rax", "%rbx", "%rcx", "%rdx", "memory");
    *cycle = (((uint64_t)ch << 32) | cl);
#else
    *cycle = __rdtsc();
#endif
}
static inline void tsc_stop(uint64_t *cycle){
#if defined(USE_TSC)
    unsigned ch, cl;
    __asm__ volatile ("RDTSCP\n\tmov %%edx, %0\n\tmov %%eax, %1\n\tCPUID\n\t" : "=r" (ch), "=r" (cl) :: "%rax", "%rbx", "%rcx", "%rdx", "memory");
    *cycle = (((uint64_t)ch << 32) | cl);
#else
    unsigned aux; *cycle = __rdtscp(&aux);
#endif
}
static inline uint64_t nsec_now_raw(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC_RAW, &ts); return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec; }
static double calibrate_ns_per_tsc(void){ struct timespec req = {.tv_sec=0,.tv_nsec=200000000}; uint64_t c0,c1,n0=nsec_now_raw(); tsc_start(&c0); nanosleep(&req,NULL); tsc_stop(&c1); uint64_t n1=nsec_now_raw(); return (double)(n1-n0)/(double)(c1-c0); }
#endif

#if defined(USE_CNTVCT) || defined(USE_CNTVCTO)
static uint64_t g_cntfrq = 0;
static inline uint64_t cntvct_to_ns(uint64_t ticks){ return (uint64_t)(((__uint128_t)ticks * 1000000000ULL) / g_cntfrq); }
static inline uint64_t read_cntvct(void){ uint64_t ticks; __asm__ volatile("isb; mrs %0, cntvct_el0" : "=r"(ticks) :: "memory"); return ticks; }
static inline uint64_t read_cntvcto_start(void){ uint64_t ticks; __asm__ volatile("dsb sy; isb; mrs %0, cntvct_el0" : "=r"(ticks) :: "memory"); return ticks; }
static inline uint64_t read_cntvcto_stop(void){ uint64_t ticks; __asm__ volatile("isb; mrs %0, cntvct_el0; dsb sy; isb" : "=r"(ticks) :: "memory"); return ticks; }
static void init_cntvct_freq(void){ __asm__ __volatile__("mrs %0, cntfrq_el0" : "=r"(g_cntfrq)); }
#endif

static inline uint64_t timespec_to_ns_u64(const struct timespec *ts) { return (uint64_t)ts->tv_sec * 1000000000ULL + (uint64_t)ts->tv_nsec; }

__attribute__((noinline)) uint64_t sub_loop(uint64_t ra, uint64_t rb, uint64_t lower) {
#if defined(__x86_64__)
    __asm__ __volatile__("1:\n\tsubq %[rb], %[ra]\n\tcmpq %[lower], %[ra]\n\tja 1b\n\t" : [ra] "+&r"(ra) : [rb] "r"(rb), [lower] "r"(lower) : "cc"); return ra;
#elif defined(__aarch64__)
    __asm__ __volatile__("1:\n\tsub %[ra], %[ra], %[rb]\n\tcmp %[ra], %[lower]\n\tb.hi 1b\n\t" : [ra] "+&r"(ra) : [rb] "r"(rb), [lower] "r"(lower) : "cc"); return ra;
#else
    while (ra > lower) ra -= rb; return ra;
#endif
}
__attribute__((noinline)) uint64_t dsub_loop(uint64_t ra, uint64_t rb, uint64_t lower) {
#if defined(__x86_64__)
    __asm__ __volatile__("1:\n\tsubq %[rb], %[ra]\n\tsubq %[rb], %[ra]\n\tcmpq %[lower], %[ra]\n\tja 1b\n\t" : [ra] "+&r"(ra) : [rb] "r"(rb), [lower] "r"(lower) : "cc"); return ra;
#elif defined(__aarch64__)
    __asm__ __volatile__("1:\n\tsub %[ra], %[ra], %[rb]\n\tsub %[ra], %[ra], %[rb]\n\tcmp %[ra], %[lower]\n\tb.hi 1b\n\t" : [ra] "+&r"(ra) : [rb] "r"(rb), [lower] "r"(lower) : "cc"); return ra;
#else
    while (ra > lower) { ra -= rb; ra -= rb; } return ra;
#endif
}

static double *xcalloc(size_t n) { double *p=(double*)calloc(n,sizeof(double)); if(!p){perror("calloc"); MPI_Abort(MPI_COMM_WORLD,2);} return p; }
static void init_data(double *a, double *b, double *c, double *d, uint64_t n) {
    uint64_t nn = n*n;
    for (uint64_t i=0;i<nn;i++) { a[i]=(double)((i*17+3)%1009)/1009.0; b[i]=(double)((i*13+5)%997)/997.0; c[i]=(double)((i*11+7)%991)/991.0; d[i]=(double)((i*7+11)%983)/983.0; }
}

static double randlc_step(uint64_t *state) {
    const uint64_t a = 1220703125ULL;
    const uint64_t m = (1ULL << 46);
    *state = (a * (*state)) & (m - 1ULL);
    return (double)(*state) / (double)m;
}
__attribute__((noinline)) static double target_unit(double *A, double *B, double *C, double *D, uint64_t n, uint64_t row) {
    (void)A; (void)B; (void)C; (void)D;
    uint64_t seed = 271828183ULL + 1315423911ULL * (row + 1ULL);
    double sx = 0.0, sy = 0.0;
    uint64_t m = n < 16 ? 16 : n;
    for (uint64_t i = 0; i < m; i++) {
        double x1 = 2.0 * randlc_step(&seed) - 1.0;
        double x2 = 2.0 * randlc_step(&seed) - 1.0;
        double t1 = x1 * x1 + x2 * x2;
        if (t1 <= 1.0 && t1 > 0.0) {
            double t2 = sqrt(-2.0 * log(t1) / t1);
            sx += x1 * t2;
            sy += x2 * t2;
        }
    }
    return sx + sy;
}


int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int myrank=0, nrank=1; MPI_Comm_rank(MPI_COMM_WORLD,&myrank); MPI_Comm_size(MPI_COMM_WORLD,&nrank);
    char myhost[256]; gethostname(myhost, sizeof(myhost));
    uint64_t narr = (argc > 1) ? strtoull(argv[1], NULL, 10) : NARR;
    uint64_t ra_lower_boundary = (argc > 2) ? strtoull(argv[2], NULL, 10) : 0;
    uint64_t rb_step = (argc > 3) ? strtoull(argv[3], NULL, 10) : 1;
#ifdef STAGE_TF
    uint64_t nsamp = (argc > 4) ? strtoull(argv[4], NULL, 10) : 1000;
#endif
    int ntest = NTEST;
    double *A=xcalloc(narr*narr), *B=xcalloc(narr*narr), *C=xcalloc(narr*narr), *D=xcalloc(narr*narr);
    init_data(A,B,C,D,narr);
#if defined(USE_CNTVCT) || defined(USE_CNTVCTO)
    init_cntvct_freq();
#endif
#if defined(__x86_64__)
    double tsc_ns = calibrate_ns_per_tsc();
    if (myrank == 0) printf("Calibrated TSC frequency: %.6f GHz\n", 1.0/tsc_ns);
#endif
#if defined(USE_PAPI) || defined(USE_PAPIX6)
    PAPI_library_init(PAPI_VER_CURRENT);
#endif
    if (myrank == 0) printf("NASA NPB-EP target kernel. NTEST=%d, NPASS=%d, N=%lu\n", ntest, NPASS, narr);
    if (myrank == 0) printf("Warming up for %d ms.\n", NWARM);
    struct timespec tv; clock_gettime(CLOCK_MONOTONIC, &tv); uint64_t warm_end = timespec_to_ns_u64(&tv) + (uint64_t)NWARM * 1000000ULL;
    volatile double checksum = 0.0;
    do { for(uint64_t s=0;s<narr;s++) checksum += target_unit(A,B,C,D,narr,s % narr); clock_gettime(CLOCK_MONOTONIC, &tv); } while (timespec_to_ns_u64(&tv) < warm_end);
    init_data(A,B,C,D,narr);
    uint64_t nsamples = (uint64_t)ntest * narr;
    uint64_t *p_ns = (uint64_t*)malloc(nsamples * sizeof(uint64_t)); if(!p_ns){perror("malloc"); MPI_Abort(MPI_COMM_WORLD,3);} 
    MPI_Barrier(MPI_COMM_WORLD);
    if (myrank == 0) { printf("Start running.\n"); fflush(stdout); }
    clock_gettime(CLOCK_MONOTONIC, &tv); uint64_t nsec_st = timespec_to_ns_u64(&tv);
    uint64_t ns0=0, ns1=0; double wtime0=0.0, wtime1=0.0;
    for (int it=0; it<ntest; it++) {
        for (uint64_t s=0; s<narr; s++) {
#ifndef STAGE_TF
#ifdef TIMING
#if defined(USE_PAPI) || defined(USE_PAPIX6)
            cpuid_serialize(); ns0 = PAPI_get_real_nsec();
#elif defined(USE_CGT)
            cpuid_serialize(); clock_gettime(CLOCK_MONOTONIC, &tv); ns0 = timespec_to_ns_u64(&tv);
#elif defined(USE_WTIME)
            cpuid_serialize(); wtime0 = MPI_Wtime();
#elif defined(USE_CNTVCT)
            ns0 = cntvct_to_ns(read_cntvct());
#elif defined(USE_CNTVCTO)
            ns0 = cntvct_to_ns(read_cntvcto_start());
#elif defined(USE_TSC) || defined(USE_TSC_NATIVE)
            tsc_start(&ns0);
#else
            clock_gettime(CLOCK_MONOTONIC, &tv); ns0 = timespec_to_ns_u64(&tv);
#endif
#endif
#endif
            checksum += target_unit(A,B,C,D,narr,s);
#ifdef STAGE_TF
#ifdef INSITU_DSUB_ASM
            { uint64_t ra_pre = rb_step * 2; dsub_loop(ra_pre, rb_step, 0); }
#elif defined(INSITU_SUB_ASM)
            { uint64_t ra_pre = rb_step; sub_loop(ra_pre, rb_step, 0); }
#endif
#ifdef TIMING
#if defined(USE_PAPI) || defined(USE_PAPIX6)
            cpuid_serialize(); ns0 = PAPI_get_real_nsec();
#elif defined(USE_CGT)
            cpuid_serialize(); clock_gettime(CLOCK_MONOTONIC, &tv); ns0 = timespec_to_ns_u64(&tv);
#elif defined(USE_WTIME)
            cpuid_serialize(); wtime0 = MPI_Wtime();
#elif defined(USE_CNTVCT)
            ns0 = cntvct_to_ns(read_cntvct());
#elif defined(USE_CNTVCTO)
            ns0 = cntvct_to_ns(read_cntvcto_start());
#elif defined(USE_TSC) || defined(USE_TSC_NATIVE)
            tsc_start(&ns0);
#else
            clock_gettime(CLOCK_MONOTONIC, &tv); ns0 = timespec_to_ns_u64(&tv);
#endif
#endif
            uint64_t ra;
#ifdef INSITU_SUB_ASM
            ra = nsamp * rb_step; sub_loop(ra, rb_step, ra_lower_boundary);
#elif defined(INSITU_DSUB_ASM)
            ra = nsamp * rb_step; if (ra > ra_lower_boundary) { ra = nsamp * rb_step * 2; dsub_loop(ra, rb_step, ra_lower_boundary); }
#else
            ra = nsamp * rb_step; while (ra > ra_lower_boundary) ra -= rb_step;
#endif
#endif
#ifdef TIMING
#if defined(USE_PAPI) || defined(USE_PAPIX6)
            ns1 = PAPI_get_real_nsec(); cpuid_serialize(); p_ns[(uint64_t)it*narr+s] = ns1 - ns0;
#elif defined(USE_CGT)
            clock_gettime(CLOCK_MONOTONIC, &tv); cpuid_serialize(); ns1 = timespec_to_ns_u64(&tv); p_ns[(uint64_t)it*narr+s] = ns1 - ns0;
#elif defined(USE_WTIME)
            wtime1 = MPI_Wtime(); cpuid_serialize(); p_ns[(uint64_t)it*narr+s] = (uint64_t)((wtime1 - wtime0) * 1e9);
#elif defined(USE_CNTVCT)
            ns1 = cntvct_to_ns(read_cntvct()); p_ns[(uint64_t)it*narr+s] = ns1 - ns0;
#elif defined(USE_CNTVCTO)
            ns1 = cntvct_to_ns(read_cntvcto_stop()); p_ns[(uint64_t)it*narr+s] = ns1 - ns0;
#elif defined(USE_TSC) || defined(USE_TSC_NATIVE)
            tsc_stop(&ns1); p_ns[(uint64_t)it*narr+s] = (uint64_t)((double)(ns1 - ns0) * tsc_ns);
#else
            clock_gettime(CLOCK_MONOTONIC, &tv); ns1 = timespec_to_ns_u64(&tv); p_ns[(uint64_t)it*narr+s] = ns1 - ns0;
#endif
#endif
        }
    }
    MPI_Barrier(MPI_COMM_WORLD); clock_gettime(CLOCK_MONOTONIC, &tv); uint64_t nsec_en = timespec_to_ns_u64(&tv);
    printf("Rank %d run time: %lu ns\n", myrank, nsec_en - nsec_st);
    char fname[512]; sprintf(fname, "npb_ep_time_%d_%s.csv", myrank, myhost);
    FILE *fp = fopen(fname, "w"); if(!fp){perror("fopen"); MPI_Abort(MPI_COMM_WORLD,4);} 
    for (int it=0; it<ntest; it++) for (uint64_t s=0; s<narr; s++) {
#ifndef STAGE_TF
        fprintf(fp, "%d,%lu\n", myrank, p_ns[(uint64_t)it*narr+s]);
#else
        fprintf(fp, "%d,%lu,%lu\n", myrank, nsamp, p_ns[(uint64_t)it*narr+s]);
#endif
    }
    fclose(fp);
    checksum += A[(narr*narr)/2] + B[(narr*narr)/3] + C[(narr*narr)/4] + D[(narr*narr)/5];
    uint64_t ra_res = 0;
#ifdef STAGE_TF
    ra_res = nsamp;
#endif
    printf("Done. checksum=%.17g ra_res=%lu\n", (double)checksum, ra_res);
    free(p_ns); free(A); free(B); free(C); free(D); MPI_Finalize(); return 0;
}
