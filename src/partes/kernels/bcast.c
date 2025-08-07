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
} bcast_data_t;

static bcast_data_t *kern_data = NULL;

void init_kern_bcast(size_t flush_kib) {
    if (kern_data == NULL) {
        kern_data = malloc(sizeof(bcast_data_t));
        if (kern_data == NULL) {
            fprintf(stderr, "Failed to allocate memory for bcast kernel\n");
            return;
        }
        
        // Calculate array size from flush_kib
        // Each element needs 1 double
        size_t total_bytes = flush_kib * 1024;
        size_t doubles_per_element = 1;
        size_t bytes_per_element = doubles_per_element * sizeof(double);
        
        // Round down to ensure we don't exceed flush_kib
        kern_data->npf = total_bytes / bytes_per_element;
        
        if (kern_data->npf > 0) {
            kern_data->data = malloc(kern_data->npf * sizeof(double));
            
            if (kern_data->data) {
                for (uint64_t i = 0; i < kern_data->npf; i++) {
                    kern_data->data[i] = 1.01;
                }
            }
        }
    }
}

void run_kern_bcast(void) {
    if (kern_data && kern_data->data) {
        // Broadcast data from rank 0 to all ranks
        MPI_Bcast(kern_data->data, kern_data->npf, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    }
}

void cleanup_kern_bcast(void) {
    if (kern_data) {
        if (kern_data->data) free(kern_data->data);
        free(kern_data);
        kern_data = NULL;
    }
}
