/**
 * @file pow.c
 * @brief: POW kernel - a[i] = pow(b[i], 1.0001)
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
    double *a, *b;
    uint64_t npf;
} data_pow_t;

static data_pow_t **p_kdata_head = NULL;
static int kdata_len = 0;

void init_kern_pow(size_t flush_kib, int id, size_t *flush_kib_real) {
    if (id < 0) return;
    if (id >= kdata_len) {
        int new_n = id + 1;
        data_pow_t **p_tmp = (data_pow_t**)realloc(p_kdata_head, new_n * sizeof(*p_tmp));
        if (!p_tmp) { fprintf(stderr, "[pow] realloc failed id=%d\n", id); return; }
        for (int i = kdata_len; i < new_n; i++) p_tmp[i] = NULL;
        p_kdata_head = p_tmp; kdata_len = new_n;
    }
    if (p_kdata_head[id] != NULL) return;

    data_pow_t *st = (data_pow_t*)malloc(sizeof(data_pow_t));
    if (!st) { fprintf(stderr, "[pow] state alloc failed id=%d\n", id); return; }

    size_t total_bytes = flush_kib * 1024;
    size_t bytes_per_element = 2 * sizeof(double);
    st->npf = total_bytes / bytes_per_element;
    if (st->npf > 0) {
        st->a = (double*)malloc(st->npf * sizeof(double));
        st->b = (double*)malloc(st->npf * sizeof(double));
        for (uint64_t i = 0; i < st->npf; i++) { st->a[i] = 1.01; st->b[i] = 1.01; }
        if (flush_kib_real) *flush_kib_real = (st->npf * bytes_per_element) / 1024;
    } else { st->a = st->b = NULL; if (flush_kib_real) *flush_kib_real = 0; }
    p_kdata_head[id] = st;
}

void run_kern_pow(int id) {
    if (id < 0 || id >= kdata_len) return;
    data_pow_t *d = p_kdata_head[id];
    if (!d || !d->a || !d->b) return;
    for (uint64_t i = 0; i < d->npf; i++) d->a[i] = pow(d->b[i], 1.0001);
}

void cleanup_kern_pow(int id) {
    if (id < 0 || id >= kdata_len) return;
    data_pow_t *d = p_kdata_head[id];
    if (!d) return;
    free(d->a);
    free(d->b);
    free(d);
    p_kdata_head[id] = NULL;
}
