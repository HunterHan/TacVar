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
} add_data_t;

static add_data_t *kern_data = NULL;

void init_kern_add(size_t flush_kib) {
    if (kern_data == NULL) {
        kern_data = malloc(sizeof(add_data_t));
        if (kern_data == NULL) {
            fprintf(stderr, "Failed to allocate memory for add kernel\n");
            return;
        }
        
        // Calculate array size from flush_kib
        // Each element needs 3 doubles (a, b, c)
        size_t total_bytes = flush_kib * 1024;
        size_t doubles_per_element = 3;
        size_t bytes_per_element = doubles_per_element * sizeof(double);
        
        // Round down to ensure we don't exceed flush_kib
        kern_data->npf = total_bytes / bytes_per_element;
        
        if (kern_data->npf > 0) {
            kern_data->a = malloc(kern_data->npf * sizeof(double));
            kern_data->b = malloc(kern_data->npf * sizeof(double));
            kern_data->c = malloc(kern_data->npf * sizeof(double));
            
            if (kern_data->a && kern_data->b && kern_data->c) {
                for (uint64_t i = 0; i < kern_data->npf; i++) {
                    kern_data->a[i] = 1.01;
                    kern_data->b[i] = 1.01;
                    kern_data->c[i] = 1.01;
                }
            }
        }
    }
}

void run_kern_add(void) {
    if (kern_data && kern_data->a && kern_data->b && kern_data->c) {
        for (uint64_t i = 0; i < kern_data->npf; i++) {
            kern_data->a[i] = kern_data->b[i] + kern_data->c[i];
        }
    }
}

void cleanup_kern_add(void) {
    if (kern_data) {
        if (kern_data->a) free(kern_data->a);
        if (kern_data->b) free(kern_data->b);
        if (kern_data->c) free(kern_data->c);
        free(kern_data);
        kern_data = NULL;
    }
}
