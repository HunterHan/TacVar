/**
 * @file partes_types.h
 * @brief: Types for partes.
 */
#ifndef PARTES_TYPES_H
#define PARTES_TYPES_H

#include <stddef.h>

#define PT_CALL_ID_FRONT 0
#define PT_CALL_ID_REAR  1
#define MAX_TRY_HZ 10000000000ULL // 10GHz
#define MIN_TRY_HZ 100000000ULL   // 100MHz
#define NUM_IGNORE_TIMING 2 // Ignore the first 2 results by default
#define PT_THRS_GUESS_VAR 0.01 // 1% variance threshold for exponential guessing
#define PT_THRES_GUESS_NSUB_TIME 1000000000ULL // Time threshold for exponential guessing
#define PT_VAR_START_NSTEP 5 // Start calculating variance after 5 steps
#define PT_VAR_MAX_NSTEP 25 // Maximum number of steps to calculate variance

typedef struct {
    size_t fsize, rsize;
    int fkern, rkern, timer;
    int ntests, nsub;
} pt_test_options_t;


enum timer_name {
    TIMER_NONE = 0,
    TIMER_CLOCK_GETTIME,
    TIMER_MPI_WTIME
};

enum kern_name {
    KERN_NONE = 0,
    KERN_TRIAD,
    KERN_SCALE,
    KERN_COPY,
    KERN_ADD,
    KERN_POW,
    KERN_DGEMM,
    KERN_MPI_BCAST
};

typedef struct {
    /* Front kernel function set */
    void (*init_fkern)(size_t flush_kib, int id, size_t *flush_kib_real);
    void (*run_fkern)(int id);
    void (*cleanup_fkern)(int id);
    /* Rear kernel function set */
    void (*init_rkern)(size_t flush_kib, int id, size_t *flush_kib_real);
    void (*run_rkern)(int id);
    void (*cleanup_rkern)(int id);
} pt_kern_func_t;

typedef struct {
    void (*init_timer)(void);
    void (*gettime)(void);
    void (*gettime_s)(void);
    void (*gettime_e)(void);
    void (*close_timer)(void);
} pt_timer_func_t;

typedef struct {
    uint64_t tick; // Nanoseconds per tick
    int64_t ovh; // Overhead per tick
} pt_timer_info_t;

typedef struct {
    uint64_t cy_per_op; // Cycles per operation
    double wtime_per_op; // Wall time per operation
    double gpt; // Gauges per tick
} pt_gauge_info_t;


#endif