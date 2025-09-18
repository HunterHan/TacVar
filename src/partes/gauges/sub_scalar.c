/**
 * @file sub_scalar.c
 * @brief: Scalar subtraction gauge kernel using function calls instead of macros
 */
#include <stdint.h>
#include "../pterr.h"

int init_gauge_sub_scalar(void) {
    return PTERR_SUCCESS;
}

void run_gauge_sub_scalar(int64_t n) {
#if defined(__x86_64__)
    uint64_t ra = (uint64_t)n;
    __asm__ __volatile__(
        "1:\n\t"
        "sub $1, %0\n\t"
        "jnz 1b\n\t"
        "2:\n\t"
        : "+r"(ra)
        :
        : "cc");

#elif defined(__aarch64__)
    uint64_t ra = (uint64_t)n;
    __asm__ __volatile__(
        "1:\n\t"
        "subs %0, %0, #1\n\t"
        "bne 1b\n\t"
        : "+r"(ra)
        :
        : "cc");

#elif defined(__riscv) && (__riscv_xlen == 64)
    uint64_t ra = (uint64_t)n;
    __asm__ __volatile__(
        "1:\n\t"
        "addi %0, %0, -1\n\t"
        "bnez %0, 1b\n\t"
        : "+r"(ra)
        :
        : );

#elif defined(__loongarch64)
    uint64_t ra = (uint64_t)n;
    __asm__ __volatile__(
        "1:\n\t"
        "addi.d %0, %0, -1\n\t"
        "bnez %0, 1b\n\t"
        : "+r"(ra)
        :
        : );

#else
    volatile uint64_t ra = (uint64_t)n;
    while (ra) { 
        ra -= 1; 
    }
#endif
}

void cleanup_gauge_sub_scalar(void) {
    return;
}