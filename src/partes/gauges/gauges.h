/**
 * @file gauges.h
 * @brief: Header file for all gauge functions (external interface)
 */
#ifndef GAUGES_H
#define GAUGES_H

#include <stdint.h>

enum gauge_name {
    GAUGE_SUB_INTRINSIC = 0,
    GAUGE_SUB_SCALAR,
    GAUGE_NONE,
    GAUGE_SUB_SCALAR_2P,
    GAUGE_FMA_SCALAR,
    GAUGE_FMA_AVX2,
    GAUGE_FMA_AVX512
};

// SUB_SCALAR gauge
int init_gauge_sub_scalar(void);
void run_gauge_sub_scalar(int64_t n);
void cleanup_gauge_sub_scalar(void);

// NONE gauge
int init_gauge_none(void);
void run_gauge_none(int64_t n);
void cleanup_gauge_none(void);

// SUB_SCALAR_2P gauge
int init_gauge_sub_scalar_2p(void);
void run_gauge_sub_scalar_2p(int64_t n);
void cleanup_gauge_sub_scalar_2p(void);

// FMA_SCALAR gauge
int init_gauge_fma_scalar(void);
void run_gauge_fma_scalar(int64_t n);
void cleanup_gauge_fma_scalar(void);

// FMA_AVX2 gauge
int init_gauge_fma_avx2(void);
void run_gauge_fma_avx2(int64_t n);
void cleanup_gauge_fma_avx2(void);

// FMA_AVX512 gauge
int init_gauge_fma_avx512(void);
void run_gauge_fma_avx512(int64_t n);
void cleanup_gauge_fma_avx512(void);

#endif
