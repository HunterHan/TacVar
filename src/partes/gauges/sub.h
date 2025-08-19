/**
 * @file sub.h
 * @brief: Subtraction gauge kernel.
 */
#ifndef _SUB_H
#define _SUB_H
#include <stdint.h>

#if defined(__x86_64__)
#define __gauge_sub_intrinsic(n)       \
    register uint64_t ra = (n), rb=1;  \
    __asm__ __volatile__(               \
        "1:\n\t"                        \
        "cmp $0, %0\n\t"                \
        "je 2f\n\t"                     \
        "sub %1, %0\n\t"                \
        "jmp 1b\n\t"                    \
        "2:\n\t"                        \
        : "+r"(ra)                      \
        : "r"(rb)                       \
        : "cc")

#elif defined(__aarch64__)
#define __gauge_sub_intrinsic(n)       \
    register uint64_t ra = (n), rb=1;  \
    __asm__ __volatile__(               \
        "1:\n\t"                        \
        "cmp %0, #0\n\t"                \
        "beq 2f\n\t"                    \
        "sub %0, %0, %1\n\t"            \
        "b 1b\n\t"                      \
        "2:\n\t"                        \
        : "+r"(ra)                      \
        : "r"(rb)                       \
        : "cc")

#else
    #define __gauge_sub_intrinsic(n)   \
    register uint64_t ra = (n), rb=1;  \
    while (ra) { ra -= rb; }
#endif // __x86_64__ || __aarch64__

#endif // _SUB_H