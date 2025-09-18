/**
 * @file fma_avx512.c
 * @brief: AVX512 FMA gauge kernel for x86-64 only
 */
#include <stdint.h>
#include "../pterr.h"

int init_gauge_fma_avx512(void) {
    return PTERR_SUCCESS;
}

void run_gauge_fma_avx512(int64_t n) {
#if defined(__x86_64__)
    uint64_t ra = (uint64_t)n;
    __asm__ __volatile__(
        "vxorpd %%zmm0, %%zmm0, %%zmm0\n\t"       // Zero zmm0
        "vxorpd %%zmm1, %%zmm1, %%zmm1\n\t"       // Zero zmm1
        "vbroadcastsd %1, %%zmm1\n\t"             // Broadcast 1.0 to all lanes of zmm1
        "1:\n\t"
        "vfmadd231pd %%zmm1, %%zmm1, %%zmm0\n\t"  // zmm0 = zmm0 + zmm1 * zmm1 (8x packed double FMA)
        "sub $1, %0\n\t"
        "jnz 1b\n\t"
        "vzeroupper\n\t"                          // Clear upper bits of all vector registers
        : "+r"(ra)
        : "m"((double){1.0})
        : "cc", "zmm0", "zmm1");
#else
    (void)n;
#endif
}

void cleanup_gauge_fma_avx512(void) {
    return;
}
