/**
 * @file mpi_bcast.c
 * @brief: MPI_BCAST kernel - MPI broadcast operation
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mpi.h"
#include "../pterr.h"

typedef struct {
    volatile double *a;
    uint64_t npf;
    double key;
} data_mpi_bcast_t;

static data_mpi_bcast_t *p_kdata_head[4] = {NULL, NULL, NULL, NULL};
static int myrank = 0;

int init_kern_mpi_bcast(size_t flush_kib, int id, size_t *flush_kib_real) {
    int err = PTERR_SUCCESS;
    if (flush_kib == 0) {
        return err;
    }
    
    p_kdata_head[id] = (data_mpi_bcast_t*)malloc(sizeof(data_mpi_bcast_t));
    if (!p_kdata_head[id]) { 
        printf("[mpi_bcast] malloc failed id=%d\n", id);
        err = PTERR_MALLOC_FAILED;
        return err; 
    }
    p_kdata_head[id]->key = 0.0;

    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    p_kdata_head[id]->npf = (size_t)((double)flush_kib * 1024 / sizeof(double));
    *flush_kib_real = p_kdata_head[id]->npf * sizeof(double) / 1024;
    p_kdata_head[id]->a = (double *)malloc(p_kdata_head[id]->npf * sizeof(double));
    if (p_kdata_head[id]->a == NULL) {
        printf("[mpi_bcast] malloc failed id=%d\n", id);
        err = PTERR_MALLOC_FAILED;
        free(p_kdata_head[id]);
        p_kdata_head[id] = NULL;
        return err; 
    }
    for (uint64_t i = 0; i < p_kdata_head[id]->npf; i++) { 
        p_kdata_head[id]->a[i] = 1.01 + i + myrank;
    }

    return err;
}

void run_kern_mpi_bcast(int id) {
    data_mpi_bcast_t *d = p_kdata_head[id];
    if (d->npf) {
        MPI_Bcast((void *)d->a, d->npf, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    }
}

void update_key_mpi_bcast(int id) {
    if (p_kdata_head[id] == NULL) return;
    if (p_kdata_head[id]->npf) {
        for (uint64_t i = 0; i < p_kdata_head[id]->npf; i++) {
            p_kdata_head[id]->key += p_kdata_head[id]->a[i];
        }
    }
}

int check_key_mpi_bcast(int id, int ntests, double *perc_gap) {
    int err = PTERR_SUCCESS;
    
    double key_target = 0;
    for (uint64_t i = 0; i < p_kdata_head[id]->npf; i++) {
        key_target += 1.01 + i;
    }
    key_target *= ntests;
    
    // Calculate absolute percentage deviation
    if (fabs(key_target) > 1e-12) {
        *perc_gap = fabs(p_kdata_head[id]->key - key_target) / fabs(key_target) * 100.0;
    } else {
        *perc_gap = 0.0;
    }
    
    if (fabs(p_kdata_head[id]->key - key_target) > 1e-6) {
        err = PTERR_KEY_CHECK_FAILED;
    }
    return err;
}

void cleanup_kern_mpi_bcast(int id) {
    data_mpi_bcast_t *d = p_kdata_head[id];
    if (!d) return;
    free((void *)d->a);
    free(d);
    p_kdata_head[id] = NULL;
}
