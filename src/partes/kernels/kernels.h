/**
 * @file kernels.h
 * @brief: Header file for all kernel functions (external interface)
 */
#ifndef KERNELS_H
#define KERNELS_H

#include <stddef.h>

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

// NONE kernel
void init_kern_none(size_t flush_kib, int id, size_t *flush_kib_real);
void run_kern_none(int id);
void cleanup_kern_none(int id);

// TRIAD kernel
void init_kern_triad(size_t flush_kib, int id, size_t *flush_kib_real);
void run_kern_triad(int id);
void cleanup_kern_triad(int id);

// SCALE kernel
void init_kern_scale(size_t flush_kib, int id, size_t *flush_kib_real);
void run_kern_scale(int id);
void cleanup_kern_scale(int id);

// COPY kernel
void init_kern_copy(size_t flush_kib, int id, size_t *flush_kib_real);
void run_kern_copy(int id);
void cleanup_kern_copy(int id);

// ADD kernel
void init_kern_add(size_t flush_kib, int id, size_t *flush_kib_real);
void run_kern_add(int id);
void cleanup_kern_add(int id);

// POW kernel
void init_kern_pow(size_t flush_kib, int id, size_t *flush_kib_real);
void run_kern_pow(int id);
void cleanup_kern_pow(int id);

// DGEMM kernel
void init_kern_dgemm(size_t flush_kib, int id, size_t *flush_kib_real);
void run_kern_dgemm(int id);
void cleanup_kern_dgemm(int id);

// BCAST kernel
void init_kern_bcast(size_t flush_kib, int id, size_t *flush_kib_real);
void run_kern_bcast(int id);
void cleanup_kern_bcast(int id);

#endif
