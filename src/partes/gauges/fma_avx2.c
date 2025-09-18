/**
 * @file fma_avx2.c
 * @brief: AVX2 FMA gauge kernel for x86-64 only
 */
#include <stdint.h>
#include "../pterr.h"

int init_gauge_fma_avx2(void) {
    return PTERR_SUCCESS;
}

void run_gauge_fma_avx2(int64_t n) {
#if defined(__x86_64__)
    uint64_t ra = (uint64_t)n;
    __asm__ __volatile__(
        "vxorpd %%ymm0, %%ymm0, %%ymm0\n\t"       // Zero ymm0
        "vxorpd %%ymm1, %%ymm1, %%ymm1\n\t"       // Zero ymm1
        "vbroadcastsd %1, %%ymm1\n\t"             // Broadcast 1.0 to all lanes of ymm1
        "1:\n\t"
        "vfmadd231pd %%ymm1, %%ymm1, %%ymm0\n\t"  // ymm0 = ymm0 + ymm1 * ymm1 (4x packed double FMA)
        "sub $1, %0\n\t"
        "jnz 1b\n\t"
        "vzeroupper\n\t"                          // Clear upper 128 bits of all YMM registers
        : "+r"(ra)
        : "m"((double){1.0})
        : "cc", "ymm0", "ymm1");
#else
    (void)n;
#endif
}

void cleanup_gauge_fma_avx2(void) {
    return;
}
