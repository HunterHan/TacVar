#include <stdint.h>
#include <inttypes.h>
#include <float.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <mpi.h>
#include "stat.h"
#include "pterr.h"
#include "partes_types.h"

static inline int64_t _run_sub(uint64_t nsub);

/**
 * @brief: run ra=nsub, ra-=1 until ra==0.
 */
static inline int64_t 
_run_sub(uint64_t nsub)
{
    register uint64_t ra = nsub;
    register uint64_t rb = 1;
    struct timespec tv;
    int64_t ns0, ns1;

    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Barrier(MPI_COMM_WORLD);

    clock_gettime(CLOCK_MONOTONIC, &tv);
    ns0 = (int64_t)tv.tv_sec * 1000000000ULL + (int64_t)tv.tv_nsec;

#if defined(__x86_64__)
    __asm__ __volatile__(
        "1:\n\t"
        "cmp $0, %0\n\t"
        "je 2f\n\t"
        "sub %1, %0\n\t"
        "jmp 1b\n\t"
        "2:\n\t"
        : "+r"(ra)
        : "r"(rb)
        : "cc");
#elif defined(__aarch64__)
    __asm__ __volatile__(
        "1:\n\t"
        "cmp %0, #0\n\t"
        "beq 2f\n\t"
        "sub %0, %0, %1\n\t"
        "b 1b\n\t"
        "2:\n\t"
        : "+r"(ra)
        : "r"(rb)
        : "cc");
#else
    while (ra) { ra -= rb; }
#endif

    clock_gettime(CLOCK_MONOTONIC, &tv);
    ns1 = (int64_t)tv.tv_sec * 1000000000ULL + (int64_t)tv.tv_nsec;

    return (int64_t)(ns1 - ns0);
}

/*
 * Detect theoretical time per sub-op and its short-term stability.
 * On rank 0 prints results; returns 0 on success.
 */
int
fit_sub_time(int myrank, int nrank, pt_timer_info_t *timer_info, pt_gauge_info_t *gauge_info)
{
    uint64_t upper_hz = UPPER_HZ;
    uint64_t lower_hz = LOWER_HZ;
    uint64_t dt = timer_info->tick; // dx=10ticks
    uint64_t *pmet = NULL;
    uint64_t xlen = NUM_IGNORE_TIMING + 1000;
    int64_t delta, delta2, delta2_old = INT64_MAX;  // Gap between measured and actual time gap of dx.
    uint64_t f, dx, nsub_min;
    uint64_t conv_me = 0, conv_other = 0, conv_target = 0, conv_now = 0;

    pmet = (uint64_t *)malloc(xlen * sizeof(uint64_t));
    if (pmet == NULL) {
        return PTERR_MALLOC_FAILED;
    }

    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

    gauge_info->wtime_per_op = 0;
    gauge_info->nop_per_tick = 0;
    gauge_info->core_freq = 0;

    // Try f to minimize:
    // delta = sigma_(i=NUM_IGNORE_TIMING+1)^(xlen-1) (pmet[i] - pmet[i-1] - dt)
    // delta2 = sigma_(i=NUM_IGNORE_TIMING+1)^(xlen-1) delta^2
    // delta<0: increase f; delta>0: decrease f
    // exit when delta2 >= delta2_old
    for (int i = 0; i < nrank; i ++) {
        conv_target |= (1ULL << i);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    while (conv_now != conv_target) {
        if (conv_me == 0) {
            f = 0.5 * (upper_hz + lower_hz);
            double nop_per_ns = f * gauge_info->cy_per_op / 1e9;
            dx = (uint64_t)(nop_per_ns * dt);
            nsub_min = (timer_info->ovh + dt * NUM_IGNORE_TIMING) * (f / 1e9) * gauge_info->cy_per_op;

            delta = 0;
            delta2 = 0;
            printf("Rank %d: Trying frequency %" PRIu64 " Hz, dx=%" PRIu64 " ticks, dt=%" PRIu64 " ns, nsub_min=%" PRIu64 "\n", myrank, f, dx, dt, nsub_min);
        }
        for (uint64_t i = 0; i < xlen; i++) {
            pmet[i] = _run_sub(nsub_min + i * dx);
        }
        for (uint64_t i = NUM_IGNORE_TIMING; i < xlen; i++) {
            delta += pmet[i] - pmet[i-1] - dt;
            delta2 += (pmet[i] - pmet[i-1] - dt) * (pmet[i] - pmet[i-1] - dt);
        }
        printf("Rank %d: delta=%" PRId64 ", delta2=%" PRId64 "\n", myrank, delta, delta2);
        if (delta < 0) {
            lower_hz = f;
        } else if (delta > 0) {
            upper_hz = f;
        } else if (delta == 0 || upper_hz <= lower_hz) {
            gauge_info->core_freq = f;
            gauge_info->wtime_per_op = 1.0e9 / f;
            gauge_info->nop_per_tick = (uint64_t)(f * timer_info->tick / 1e9 / gauge_info->cy_per_op);
            conv_me = 1;
        }
        delta2_old = delta2;
        if (myrank == 0) {
            conv_now |= conv_me;
            for (int r = 1; r < nrank; r ++) {
                MPI_Recv(&conv_other, 1, MPI_UINT64_T, r, r, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                conv_now |= conv_other << r;
            }
            MPI_Bcast(&conv_now, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);
            printf("Rank %d: conv_now=", myrank);
            for (int bit = 63; bit >= 0; bit--) {
                printf("%d", (conv_now >> bit) & 1);
            }
            printf(", conv_target=");
            for (int bit = 63; bit >= 0; bit--) {
                printf("%d", (conv_target >> bit) & 1);
            }
            printf("\n");
        }
        else {
            MPI_Send(&conv_me, 1, MPI_UINT64_T, 0, myrank, MPI_COMM_WORLD);
            MPI_Bcast(&conv_now, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);
        }
    }

    free(pmet);
    pmet = NULL;

    return PTERR_SUCCESS;
}
