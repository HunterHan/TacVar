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
int init_kern_none(size_t flush_kib, int id, size_t *flush_kib_real);
void run_kern_none(int id);
void update_key_none(int id);
int check_key_none(int id, int ntests, double *perc_gap);
void cleanup_kern_none(int id);

// TRIAD kernel
int init_kern_triad(size_t flush_kib, int id, size_t *flush_kib_real);
void run_kern_triad(int id);
void update_key_triad(int id);
int check_key_triad(int id, int ntests, double *perc_gap);
void cleanup_kern_triad(int id);

// SCALE kernel
int init_kern_scale(size_t flush_kib, int id, size_t *flush_kib_real);
void update_key_scale(int id);
int check_key_scale(int id, int ntests, double *perc_gap);
void run_kern_scale(int id);
void cleanup_kern_scale(int id);

// COPY kernel
int init_kern_copy(size_t flush_kib, int id, size_t *flush_kib_real);
void update_key_copy(int id);
int check_key_copy(int id, int ntests, double *perc_gap);
void run_kern_copy(int id);
void cleanup_kern_copy(int id);

// ADD kernel
int init_kern_add(size_t flush_kib, int id, size_t *flush_kib_real);
void update_key_add(int id);
int check_key_add(int id, int ntests, double *perc_gap);
void run_kern_add(int id);
void cleanup_kern_add(int id);

// POW kernel
int init_kern_pow(size_t flush_kib, int id, size_t *flush_kib_real);
void update_key_pow(int id);
int check_key_pow(int id, int ntests, double *perc_gap);
void run_kern_pow(int id);
void cleanup_kern_pow(int id);

// DGEMM kernel
int init_kern_dgemm(size_t flush_kib, int id, size_t *flush_kib_real);
void update_key_dgemm(int id);
int check_key_dgemm(int id, int ntests, double *perc_gap);
void run_kern_dgemm(int id);
void cleanup_kern_dgemm(int id);

// MPI_BCAST kernel
int init_kern_mpi_bcast(size_t flush_kib, int id, size_t *flush_kib_real);
void update_key_mpi_bcast(int id);
int check_key_mpi_bcast(int id, int ntests, double *perc_gap);
void run_kern_mpi_bcast(int id);
void cleanup_kern_mpi_bcast(int id);

#endif
