/**
 * @file kernels.h
 * @brief: Header file for all kernel functions
 */
#ifndef KERNELS_H
#define KERNELS_H

#include <stdint.h>
#include <stddef.h>

// NONE kernel
void init_kern_none(size_t flush_kib);
void run_kern_none(void);
void cleanup_kern_none(void);

// TRIAD kernel
void init_kern_triad(size_t flush_kib);
void run_kern_triad(void);
void cleanup_kern_triad(void);

// SCALE kernel
void init_kern_scale(size_t flush_kib);
void run_kern_scale(void);
void cleanup_kern_scale(void);

// COPY kernel
void init_kern_copy(size_t flush_kib);
void run_kern_copy(void);
void cleanup_kern_copy(void);

// ADD kernel
void init_kern_add(size_t flush_kib);
void run_kern_add(void);
void cleanup_kern_add(void);

// POW kernel
void init_kern_pow(size_t flush_kib);
void run_kern_pow(void);
void cleanup_kern_pow(void);

// DGEMM kernel
void init_kern_dgemm(size_t flush_kib);
void run_kern_dgemm(void);
void cleanup_kern_dgemm(void);

// BCAST kernel
void init_kern_bcast(size_t flush_kib);
void run_kern_bcast(void);
void cleanup_kern_bcast(void);

#endif
