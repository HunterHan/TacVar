/**
 * @file none.c
 * @brief: NONE kernel - does nothing
 */
#include <stdio.h>
#include <stdint.h>
#include "../pterr.h"

int init_kern_none(size_t flush_kib, int id, size_t *flush_kib_real) {
    return PTERR_SUCCESS;
}

void run_kern_none(int id) {
    (void)id;
}

void update_key_none(int id) {
    (void)id;
}

int check_key_none(int id, int ntests, double *perc_gap) {
    *perc_gap = 0.0;
    return PTERR_SUCCESS;
}

void cleanup_kern_none(int id) {
    (void)id;
}
