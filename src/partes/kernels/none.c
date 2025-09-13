/**
 * @file none.c
 * @brief: NONE kernel - does nothing
 */
#include <stdio.h>
#include <stdint.h>

void init_kern_none(size_t flush_kib, int id, size_t *flush_kib_real) {
    *flush_kib_real = 0;
}

void run_kern_none(int id) {
    return;
}

void cleanup_kern_none(int id) {
    return;
}
