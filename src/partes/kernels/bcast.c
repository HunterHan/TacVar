/**
 * @file bcast.c
 * @brief: MPI_BCAST kernel - MPI broadcast operation
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "mpi.h"

typedef struct {
    double *data;
    uint64_t npf;
} data_bcast_t;

static data_bcast_t **p_kdata_head = NULL;
static int kdata_len = 0;

void init_kern_bcast(size_t flush_kib, int id, size_t *flush_kib_real) {
    if (id < 0) return;
    if (id >= kdata_len) {
        int new_n = id + 1;
        data_bcast_t **p_tmp = (data_bcast_t**)realloc(p_kdata_head, new_n * sizeof(*p_tmp));
        if (!p_tmp) { fprintf(stderr, "[bcast] realloc failed id=%d\n", id); return; }
        for (int i = kdata_len; i < new_n; i++) p_tmp[i] = NULL;
        p_kdata_head = p_tmp; kdata_len = new_n;
    }
    if (p_kdata_head[id] != NULL) return;

    data_bcast_t *st = (data_bcast_t*)malloc(sizeof(data_bcast_t));
    if (!st) { fprintf(stderr, "[bcast] state alloc failed id=%d\n", id); return; }

    size_t total_bytes = flush_kib * 1024;
    size_t bytes_per_element = sizeof(double);
    st->npf = total_bytes / bytes_per_element;
    if (st->npf > 0) {
        st->data = (double*)malloc(st->npf * sizeof(double));
        for (uint64_t i = 0; i < st->npf; i++) st->data[i] = 1.01;
        if (flush_kib_real) *flush_kib_real = (st->npf * bytes_per_element) / 1024;
    } else { st->data = NULL; if (flush_kib_real) *flush_kib_real = 0; }
    p_kdata_head[id] = st;
}

void run_kern_bcast(int id) {
    if (id < 0 || id >= kdata_len) return;
    data_bcast_t *d = p_kdata_head[id];
    if (!d || !d->data) return;
    MPI_Bcast(d->data, d->npf, MPI_DOUBLE, 0, MPI_COMM_WORLD);
}

void cleanup_kern_bcast(int id) {
    if (id < 0 || id >= kdata_len) return;
    data_bcast_t *d = p_kdata_head[id];
    if (!d) return;
    free(d->data); free(d);
    p_kdata_head[id] = NULL;
}
