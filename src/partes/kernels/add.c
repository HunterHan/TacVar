/**
 * @file add.c
 * @brief: ADD kernel - a[i] = b[i] + c[i]
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    double *a, *b, *c;
    uint64_t npf;
} data_add_t;

static data_add_t **p_kdata_head = NULL;
static int kdata_len = 0;

void init_kern_add(size_t flush_kib, int id, size_t *flush_kib_real) {
    if (id < 0) return;
    if (id >= kdata_len) {
        int new_n = id + 1;
        data_add_t **p_tmp = (data_add_t**)realloc(p_kdata_head, new_n * sizeof(*p_tmp));
        if (!p_tmp) { fprintf(stderr, "[add] realloc failed id=%d\n", id); return; }
        for (int i = kdata_len; i < new_n; i++) p_tmp[i] = NULL;
        p_kdata_head = p_tmp;
        kdata_len = new_n;
    }
    if (p_kdata_head[id] != NULL) return;

    data_add_t *st = (data_add_t*)malloc(sizeof(data_add_t));
    if (!st) { fprintf(stderr, "[add] state alloc failed id=%d\n", id); return; }

    size_t total_bytes = flush_kib * 1024;
    size_t bytes_per_element = 3 * sizeof(double);
    st->npf = total_bytes / bytes_per_element;
    if (st->npf > 0) {
        st->a = (double*)malloc(st->npf * sizeof(double));
        st->b = (double*)malloc(st->npf * sizeof(double));
        st->c = (double*)malloc(st->npf * sizeof(double));
        for (uint64_t i = 0; i < st->npf; i++) { st->a[i] = 1.01; st->b[i] = 1.01; st->c[i] = 1.01; }
        if (flush_kib_real) *flush_kib_real = (st->npf * bytes_per_element) / 1024;
    } else { st->a = st->b = st->c = NULL; if (flush_kib_real) *flush_kib_real = 0; }
    p_kdata_head[id] = st;
}

void run_kern_add(int id) {
    if (id < 0 || id >= kdata_len) return;
    data_add_t *d = p_kdata_head[id];
    if (!d || !d->a || !d->b || !d->c) return;
    for (uint64_t i = 0; i < d->npf; i++) d->a[i] = d->b[i] + d->c[i];
}

void cleanup_kern_add(int id) {
    if (id < 0 || id >= kdata_len) return;
    data_add_t *d = p_kdata_head[id];
    if (!d) return;
    free(d->a); free(d->b); free(d->c); free(d);
    p_kdata_head[id] = NULL;
}
