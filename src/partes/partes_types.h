/**
 * @file partes_types.h
 * @brief: Types for partes.
 */
#ifndef PARTES_TYPES_H
#define PARTES_TYPES_H

#include <stdint.h>
#include <stddef.h>

#define PT_CALL_ID_TA_FRONT 0
#define PT_CALL_ID_TA_REAR  1
#define PT_CALL_ID_TB_FRONT 2
#define PT_CALL_ID_TB_REAR  3
#define MAX_TRY_HZ 10000000000ULL // 10GHz
#define MIN_TRY_HZ 100000000ULL   // 100MHz
#define PT_THRS_GUESS_VAR 0.01 // 1% variance threshold for exponential guessing
#define PT_THRES_GUESS_NSUB_TIME 1000000000ULL // Time threshold for exponential guessing
#define PT_VAR_START_NSTEP 5 // Start calculating variance after 5 steps
#define PT_VAR_MAX_NSTEP 25 // Maximum number of steps to calculate variance

typedef struct {
    int64_t ta, tb, ntests;
    size_t fsize_a, rsize_a, fsize_b, rsize_b;
    size_t fsize_real_a, rsize_real_a, fsize_real_b, rsize_real_b;
    double cut_p;
    int fkern, rkern, timer, gauge, ntiles;
    char fkern_name[128], rkern_name[128], timer_name[128], gauge_name[128];
} pt_opts_t;

typedef struct {
    /* Front kernel function set */
    int (*init_fkern)(size_t flush_kib, int id, size_t *flush_kib_real);
    void (*run_fkern)(int id);
    void (*update_fkern_key)(int id);
    int (*check_fkern_key)(int id, int ntests, double *perc_gap);
    void (*cleanup_fkern)(int id);
    /* Rear kernel function set */
    int (*init_rkern)(size_t flush_kib, int id, size_t *flush_kib_real);
    void (*run_rkern)(int id);
    void (*update_rkern_key)(int id);
    int (*check_rkern_key)(int id, int ntests, double *perc_gap);
    void (*cleanup_rkern)(int id);
} pt_kern_func_t;

typedef struct {
    int (*init_timer)(void);
    int64_t (*tick)(void);
    int64_t (*tock)(void);
    int64_t (*get_stamp)(void);
} pt_timer_func_t;

typedef struct {
    int (*init_gauge)(void);
    void (*run_gauge)(int64_t n);
    void (*cleanup_gauge)(void);
} pt_gauge_func_t;

typedef struct {
    int64_t tick; // Nanoseconds per tick
    int64_t ovh; // Overhead in ticks
} pt_timer_spec_t;

typedef struct {
    uint64_t cy_per_op; // Cycles per operation
    double wtime_per_op; // Wall time per operation
    double gpns; // Gauges per nanosecond
    double gpt; // Gauges per tick
} pt_gauge_info_t;


#endif