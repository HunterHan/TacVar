/**
 * @file none.c
 * @brief: NONE kernel - does nothing
 */
#include <stdio.h>
#include <stdint.h>

void init_kern_none(size_t flush_kib) {
    // No initialization needed
    (void)flush_kib; // Suppress unused parameter warning
}

void run_kern_none(void) {
    // No operation
}

void cleanup_kern_none(void) {
    // No cleanup needed
}
