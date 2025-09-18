/**
 * @file fma_scalar.c
 * @brief: Scalar FMA gauge kernel for x86-64 only
 */
#include <stdint.h>
#include "../pterr.h"

int init_gauge_fma_scalar(void) {
    return PTERR_SUCCESS;
}

void run_gauge_fma_scalar(int64_t n) {
#if defined(__x86_64__)
    uint64_t ra = (uint64_t)n;
    __asm__ __volatile__(
        "vxorpd %%xmm0, %%xmm0, %%xmm0\n\t"   // Zero xmm0
        "vxorpd %%xmm1, %%xmm1, %%xmm1\n\t"   // Zero xmm1  
        "vmovsd %1, %%xmm1\n\t"                // Load 1.0 into xmm1
        "1:\n\t"
        "vfmadd231sd %%xmm1, %%xmm1, %%xmm0\n\t"  // xmm0 = xmm0 + xmm1 * xmm1 (scalar FMA)
        "sub $1, %0\n\t"
        "jnz 1b\n\t"
        : "+r"(ra)
        : "m"((double){1.0})
        : "cc", "xmm0", "xmm1");
#else
    (void)n;
#endif
}

void cleanup_gauge_fma_scalar(void) {
    return;
}
