/**
 * @file copy.c
 * @brief: COPY kernel - a[i] = b[i]. Magic number key for verifying.
 *         key += sum(a) in each iter. 
 *         For each id, key == 1.01 * npf * ntests + sum(range(0:npf)) * ntests.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../pterr.h"

typedef struct {
    volatile double *a, *b;
    uint64_t npf;
    double key;
} data_copy_t;

static data_copy_t *p_kdata_head[4] = {NULL, NULL, NULL, NULL};

int init_kern_copy(size_t flush_kib, int id, size_t *flush_kib_real) {
    int err = PTERR_SUCCESS;
    if (flush_kib == 0) {
        return err;
    }
    
    p_kdata_head[id] = (data_copy_t*)malloc(sizeof(data_copy_t));
    if (!p_kdata_head[id]) { 
        printf("[copy] malloc failed id=%d\n", id);
        err = PTERR_MALLOC_FAILED;
        return err; 
    }
    p_kdata_head[id]->key = 0.0;

    p_kdata_head[id]->npf = (size_t)((double)flush_kib * 1024 / 2 / sizeof(double));
    *flush_kib_real = p_kdata_head[id]->npf * sizeof(double) * 2 / 1024;
    p_kdata_head[id]->a = (double *)malloc(p_kdata_head[id]->npf * sizeof(double));
    if (p_kdata_head[id]->a == NULL) {
        printf("[copy] malloc failed id=%d\n", id);
        err = PTERR_MALLOC_FAILED;
        return err; 
    }
    p_kdata_head[id]->b = (double *)malloc(p_kdata_head[id]->npf * sizeof(double));
    if (p_kdata_head[id]->b == NULL) {
        printf("[copy] malloc failed id=%d\n", id);
        err = PTERR_MALLOC_FAILED;
        free((void *)p_kdata_head[id]->a);
        free(p_kdata_head[id]);
        p_kdata_head[id] = NULL;
        return err; 
    }
    for (uint64_t i = 0; i < p_kdata_head[id]->npf; i++) { 
        p_kdata_head[id]->a[i] = 0.0;
        p_kdata_head[id]->b[i] = 1.01 + (double)i;
    }

    return err;
}

void run_kern_copy(int id) {
    if (p_kdata_head[id]->npf) {
        for (uint64_t i = 0; i < p_kdata_head[id]->npf; i++) {
            p_kdata_head[id]->a[i] = p_kdata_head[id]->b[i];
        }
    }
}

void update_key_copy(int id) {
    if (p_kdata_head[id] == NULL) return;
    if (p_kdata_head[id]->npf) {
        for (uint64_t i = 0; i < p_kdata_head[id]->npf; i++) {
            p_kdata_head[id]->key += p_kdata_head[id]->a[i];
            p_kdata_head[id]->a[i] = 0.0;
        }
    }
}

int check_key_copy(int id, int ntests, double *perc_gap) {
    int err = PTERR_SUCCESS;
    
    double key_target = 0;
    for (uint64_t i = 0; i < p_kdata_head[id]->npf; i++) {
        key_target += 1.01 + (double)i;
    }
    key_target *= (double)ntests;
    
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

void cleanup_kern_copy(int id) {
    data_copy_t *d = p_kdata_head[id];
    if (!d) return;
    free((void *)d->a);
    free((void *)d->b);
    free(d);
    p_kdata_head[id] = NULL;
}
