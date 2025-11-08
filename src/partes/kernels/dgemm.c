/**
 * @file dgemm.c
 * @brief: DGEMM kernel - matrix multiplication
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../pterr.h"

typedef struct {
    volatile double *a, *b, *c;
    uint64_t npf;
    uint64_t sq_npf;
    double key;
} data_dgemm_t;

static data_dgemm_t *p_kdata_head[4] = {NULL, NULL, NULL, NULL};

int init_kern_dgemm(size_t flush_kib, int id, size_t *flush_kib_real) {
    int err = PTERR_SUCCESS;
    if (flush_kib == 0) {
        *flush_kib_real = 0;
        return err;
    }
    
    p_kdata_head[id] = (data_dgemm_t*)malloc(sizeof(data_dgemm_t));
    if (!p_kdata_head[id]) { 
        printf("[dgemm] malloc failed id=%d\n", id);
        err = PTERR_MALLOC_FAILED;
        return err; 
    }
    
    p_kdata_head[id]->key = 0.0;

    p_kdata_head[id]->sq_npf = (size_t)sqrt((double)flush_kib * 1024 / 3 / sizeof(double));
    p_kdata_head[id]->npf = p_kdata_head[id]->sq_npf * p_kdata_head[id]->sq_npf;
    *flush_kib_real = p_kdata_head[id]->npf * sizeof(double) * 3 / 1024;
    p_kdata_head[id]->a = (double *)malloc(p_kdata_head[id]->npf * sizeof(double));
    if (p_kdata_head[id]->a == NULL) {
        printf("[dgemm] malloc failed id=%d\n", id);
        err = PTERR_MALLOC_FAILED;
        return err; 
    }
    p_kdata_head[id]->b = (double *)malloc(p_kdata_head[id]->npf * sizeof(double));
    if (p_kdata_head[id]->b == NULL) {
        printf("[dgemm] malloc failed id=%d\n", id);
        err = PTERR_MALLOC_FAILED;
        free((void *)p_kdata_head[id]->a);
        free(p_kdata_head[id]);
        p_kdata_head[id] = NULL;
        return err; 
    }
    p_kdata_head[id]->c = (double *)malloc(p_kdata_head[id]->npf * sizeof(double));
    if (p_kdata_head[id]->c == NULL) {
        printf("[dgemm] malloc failed id=%d\n", id);
        err = PTERR_MALLOC_FAILED;
        free((void *)p_kdata_head[id]->a);
        free((void *)p_kdata_head[id]->b);
        free(p_kdata_head[id]);
        p_kdata_head[id] = NULL;
        return err; 
    }
    
    for (uint64_t i = 0; i < p_kdata_head[id]->npf; i++) {
        // Initialize matrices
        // a: zero matrix (will be computed result)
        p_kdata_head[id]->a[i] = 0.0;
        
        // b: unit matrix (identity matrix)
        uint64_t row = i / p_kdata_head[id]->sq_npf;
        uint64_t col = i % p_kdata_head[id]->sq_npf;
        if (row == col) {
            p_kdata_head[id]->b[i] = 1.0;
        } else {
            p_kdata_head[id]->b[i] = 0.0;
        }
        
        // c: initialized to 1.01 + i
        p_kdata_head[id]->c[i] = 1.01 + i;
    }

    return err;
}

void run_kern_dgemm(int id) {
    if (p_kdata_head[id] == NULL) return;
    data_dgemm_t *d = p_kdata_head[id];
    uint64_t n = d->sq_npf;
    for (uint64_t i = 0; i < n; i++) {
        for (uint64_t j = 0; j < n; j++) {
            double sum = 0.0;
            for (uint64_t k = 0; k < n; k++) {
                sum += d->a[i * n + k] * d->b[k * n + j];
            }
            d->c[i * n + j] = sum;
        }
    }
}

void update_key_dgemm(int id) {
    if (p_kdata_head[id] == NULL) return;
    if (p_kdata_head[id]->npf) {
        for (uint64_t i = 0; i < p_kdata_head[id]->npf; i++) {
            p_kdata_head[id]->key += p_kdata_head[id]->c[i];
            p_kdata_head[id]->c[i] = 0.0;
        }
    }
}

int check_key_dgemm(int id, int ntests, double *perc_gap) {
    if (p_kdata_head[id] == NULL) return PTERR_SUCCESS;
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

void cleanup_kern_dgemm(int id) {
    data_dgemm_t *d = p_kdata_head[id];
    if (!d) return;
    free((void *)d->a);
    free((void *)d->b);
    free((void *)d->c);
    free(d);
    p_kdata_head[id] = NULL;
}
