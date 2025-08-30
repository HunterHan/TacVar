/**
 * @file sub.h
 * @brief: Subtraction gauge kernel.
 */
#ifndef _SUB_H
#define _SUB_H
#include <stdint.h>

#if defined(__x86_64__)
#define __gauge_sub_intrinsic(n)       \
    uint64_t ra = (n);        \
    __asm__ __volatile__(               \
        "1:\n\t"                        \
        "sub $1, %0\n\t"                \
        "jnz 1b\n\t"                    \
        "2:\n\t"                        \
        : "+r"(ra)                      \
        :                               \
        : "cc")

#elif defined(__aarch64__)
#define __gauge_sub_intrinsic(n)       \
    uint64_t ra = (n);        \
    __asm__ __volatile__(               \
        "1:\n\t"                        \
        "subs %0, %0, #1\n\t"           \
        "bne 1b\n\t"                    \
        : "+r"(ra)                      \
        :                               \
        : "cc")

#elif defined(__riscv) && (__riscv_xlen == 64)
#define __gauge_sub_intrinsic(n)       \
    uint64_t ra = (n);        \
    __asm__ __volatile__(               \
        "1:\n\t"                        \
        "addi %0, %0, -1\n\t"           \
        "bnez %0, 1b\n\t"               \
        : "+r"(ra)                      \
        :                               \
        : )

#elif defined(__loongarch64)
#define __gauge_sub_intrinsic(n)       \
    uint64_t ra = (n);        \
    __asm__ __volatile__(               \
        "1:\n\t"                        \
        "addi.d %0, %0, -1\n\t"         \
        "bnez %0, 1b\n\t"               \
        : "+r"(ra)                      \
        :                               \
        : )

#else
    #define __gauge_sub_intrinsic(n)   \
    volatile uint64_t ra = (n); \
    while (ra) { ra -= 1; }
#endif // __x86_64__ || __aarch64__

#endif // _SUB_H