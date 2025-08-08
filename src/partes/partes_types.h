/**
 * @file partes_types.h
 * @brief: Types for partes.
 */
#ifndef PARTES_TYPES_H
#define PARTES_TYPES_H

#include <stddef.h>

typedef struct {
    size_t fsize, rsize;
    int fkern, rkern, timer;
    int ntests, nsub;
} pt_test_options_t;

#define PT_CALL_ID_FRONT 0
#define PT_CALL_ID_REAR  1

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

#endif