/**
 * @file partes_types.h
 * @brief: Types for partes.
 */
#ifndef PARTES_TYPES_H
#define PARTES_TYPES_H

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
    void (*init_fkern)(size_t flush_kib);
    void (*run_fkern)(void);
    void (*cleanup_fkern)(void);
    void (*init_rkern)(size_t flush_kib);
    void (*run_rkern)(void);
    void (*cleanup_rkern)(void);
} pt_kern_func_t;

typedef struct {
    void (*init_timer)(void);
    void (*gettime)(void);
    void (*gettime_s)(void);
    void (*gettime_e)(void);
    void (*close_timer)(void);
} pt_timer_func_t;

#endif