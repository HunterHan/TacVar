/**
 * @file none.c
 * @brief: NONE kernel - does nothing
 */
#include <stdio.h>
#include <stdint.h>
#include "../pterr.h"

int init_kern_none(size_t flush_kib, int id, size_t *flush_kib_real) {
    (void)flush_kib;
    (void)id;
    *flush_kib_real = 0;
    __asm__ __volatile__ ("nop");

    return PTERR_SUCCESS;
}

void run_kern_none(int id) {
    (void)id;
    __asm__ __volatile__ ("nop");

    return;
}

void update_key_none(int id) {
    (void)id;
    __asm__ __volatile__ ("nop");

    return;
}

int check_key_none(int id, int ntests, double *perc_gap) {
    (void)id;
    (void)ntests;
    *perc_gap = 0.0;
    __asm__ __volatile__ ("nop");

    return PTERR_SUCCESS;
}

void cleanup_kern_none(int id) {
    (void)id;
    __asm__ __volatile__ ("nop");

    return;
}
