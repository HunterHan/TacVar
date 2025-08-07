/**
 * @file scale.c
 * @brief: SCALE kernel - a[i] = 1.0001 * b[i]; b[i] = 1.0001 * a[i]
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    double *a, *b;
    uint64_t npf;
} scale_data_t;

static scale_data_t *kern_data = NULL;

void init_kern_scale(size_t flush_kib) {
    if (kern_data == NULL) {
        kern_data = malloc(sizeof(scale_data_t));
        if (kern_data == NULL) {
            fprintf(stderr, "Failed to allocate memory for scale kernel\n");
            return;
        }
        
        // Calculate array size from flush_kib
        // Each element needs 2 doubles (a, b)
        size_t total_bytes = flush_kib * 1024;
        size_t doubles_per_element = 2;
        size_t bytes_per_element = doubles_per_element * sizeof(double);
        
        // Round down to ensure we don't exceed flush_kib
        kern_data->npf = total_bytes / bytes_per_element;
        
        if (kern_data->npf > 0) {
            kern_data->a = malloc(kern_data->npf * sizeof(double));
            kern_data->b = malloc(kern_data->npf * sizeof(double));
            
            if (kern_data->a && kern_data->b) {
                for (uint64_t i = 0; i < kern_data->npf; i++) {
                    kern_data->a[i] = 1.01;
                    kern_data->b[i] = 1.01;
                }
            }
        }
    }
}

void run_kern_scale(void) {
    if (kern_data && kern_data->a && kern_data->b) {
        for (uint64_t i = 0; i < kern_data->npf; i++) {
            kern_data->a[i] = 1.0001 * kern_data->b[i];
            kern_data->b[i] = 1.0001 * kern_data->a[i];
        }
    }
}

void cleanup_kern_scale(void) {
    if (kern_data) {
        if (kern_data->a) free(kern_data->a);
        if (kern_data->b) free(kern_data->b);
        free(kern_data);
        kern_data = NULL;
    }
}
