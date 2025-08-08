/**
 * @file dgemm.c
 * @brief: DGEMM kernel - matrix multiplication
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
    double *a, *b, *c;
    uint64_t npf;
    uint64_t sq_npf;
} data_dgemm_t;

static data_dgemm_t **p_kdata_head = NULL;
static int kdata_len = 0;

void init_kern_dgemm(size_t flush_kib, int id, size_t *flush_kib_real) {
    if (id < 0) return;
    if (id >= kdata_len) {
        int new_n = id + 1;
        data_dgemm_t **p_tmp = (data_dgemm_t**)realloc(p_kdata_head, new_n * sizeof(*p_tmp));
        if (!p_tmp) { fprintf(stderr, "[dgemm] realloc failed id=%d\n", id); return; }
        for (int i = kdata_len; i < new_n; i++) p_tmp[i] = NULL;
        p_kdata_head = p_tmp; kdata_len = new_n;
    }
    if (p_kdata_head[id] != NULL) return;

    data_dgemm_t *st = (data_dgemm_t*)malloc(sizeof(data_dgemm_t));
    if (!st) { fprintf(stderr, "[dgemm] state alloc failed id=%d\n", id); return; }

    size_t total_bytes = flush_kib * 1024;
    size_t bytes_per_element = 3 * sizeof(double);
    st->npf = total_bytes / bytes_per_element;
    st->sq_npf = (uint64_t)sqrt(st->npf);
    if (st->sq_npf > 0) {
        size_t matrix_size = st->sq_npf * st->sq_npf;
        st->a = (double*)malloc(matrix_size * sizeof(double));
        st->b = (double*)malloc(matrix_size * sizeof(double));
        st->c = (double*)malloc(matrix_size * sizeof(double));
        for (uint64_t i = 0; i < matrix_size; i++) { st->a[i] = 1.01; st->b[i] = 1.01; st->c[i] = 1.01; }
        if (flush_kib_real) *flush_kib_real = (matrix_size * 3 * sizeof(double)) / 1024;
    } else {
        st->a = st->b = st->c = NULL; if (flush_kib_real) *flush_kib_real = 0;
    }
    p_kdata_head[id] = st;
}

void run_kern_dgemm(int id) {
    if (id < 0 || id >= kdata_len) return;
    data_dgemm_t *d = p_kdata_head[id];
    if (!d || !d->a || !d->b || !d->c) return;
    uint64_t n = d->sq_npf;
    for (uint64_t i = 0; i < n; i++) {
        for (uint64_t j = 0; j < n; j++) {
            double sum = 0.0;
            for (uint64_t k = 0; k < n; k++) sum += d->a[i * n + k] * d->b[k * n + j];
            d->c[i * n + j] = sum;
        }
    }
}

void cleanup_kern_dgemm(int id) {
    if (id < 0 || id >= kdata_len) return;
    data_dgemm_t *d = p_kdata_head[id];
    if (!d) return;
    free(d->a); free(d->b); free(d->c); free(d);
    p_kdata_head[id] = NULL;
}
