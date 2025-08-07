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
} dgemm_data_t;

static dgemm_data_t *kern_data = NULL;

void init_kern_dgemm(size_t flush_kib) {
    if (kern_data == NULL) {
        kern_data = malloc(sizeof(dgemm_data_t));
        if (kern_data == NULL) {
            fprintf(stderr, "Failed to allocate memory for dgemm kernel\n");
            return;
        }
        
        // Calculate array size from flush_kib
        // Each element needs 3 doubles (a, b, c) for square matrices
        size_t total_bytes = flush_kib * 1024;
        size_t doubles_per_element = 3;
        size_t bytes_per_element = doubles_per_element * sizeof(double);
        
        // Round down to ensure we don't exceed flush_kib
        kern_data->npf = total_bytes / bytes_per_element;
        kern_data->sq_npf = (uint64_t)sqrt(kern_data->npf);
        
        if (kern_data->sq_npf > 0) {
            size_t matrix_size = kern_data->sq_npf * kern_data->sq_npf;
            kern_data->a = malloc(matrix_size * sizeof(double));
            kern_data->b = malloc(matrix_size * sizeof(double));
            kern_data->c = malloc(matrix_size * sizeof(double));
            
            if (kern_data->a && kern_data->b && kern_data->c) {
                for (uint64_t i = 0; i < matrix_size; i++) {
                    kern_data->a[i] = 1.01;
                    kern_data->b[i] = 1.01;
                    kern_data->c[i] = 1.01;
                }
            }
        }
    }
}

void run_kern_dgemm(void) {
    if (kern_data && kern_data->a && kern_data->b && kern_data->c) {
        // Simple matrix multiplication (not optimized BLAS)
        uint64_t n = kern_data->sq_npf;
        for (uint64_t i = 0; i < n; i++) {
            for (uint64_t j = 0; j < n; j++) {
                double sum = 0.0;
                for (uint64_t k = 0; k < n; k++) {
                    sum += kern_data->a[i * n + k] * kern_data->b[k * n + j];
                }
                kern_data->c[i * n + j] = sum;
            }
        }
    }
}

void cleanup_kern_dgemm(void) {
    if (kern_data) {
        if (kern_data->a) free(kern_data->a);
        if (kern_data->b) free(kern_data->b);
        if (kern_data->c) free(kern_data->c);
        free(kern_data);
        kern_data = NULL;
    }
}
