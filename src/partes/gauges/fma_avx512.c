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
        "vxorpd %%zmm2, %%zmm2, %%zmm2\n\t"       // Zero zmm2
        "vxorpd %%zmm3, %%zmm3, %%zmm3\n\t"       // Zero zmm3
        "vxorpd %%zmm4, %%zmm4, %%zmm4\n\t"       // Zero zmm4
        "vxorpd %%zmm5, %%zmm5, %%zmm5\n\t"       // Zero zmm5
        "vxorpd %%zmm6, %%zmm6, %%zmm6\n\t"       // Zero zmm6
        "vxorpd %%zmm7, %%zmm7, %%zmm7\n\t"       // Zero zmm7
        "vxorpd %%zmm8, %%zmm8, %%zmm8\n\t"       // Zero zmm8
        "vxorpd %%zmm9, %%zmm9, %%zmm9\n\t"       // Zero zmm9
        "vxorpd %%zmm10, %%zmm10, %%zmm10\n\t"    // Zero zmm10
        "vxorpd %%zmm11, %%zmm11, %%zmm11\n\t"    // Zero zmm11
        "vxorpd %%zmm12, %%zmm12, %%zmm12\n\t"    // Zero zmm12
        "vxorpd %%zmm13, %%zmm13, %%zmm13\n\t"    // Zero zmm13
        "vxorpd %%zmm14, %%zmm14, %%zmm14\n\t"    // Zero zmm14
        "vxorpd %%zmm15, %%zmm15, %%zmm15\n\t"    // Zero zmm15
        "vbroadcastsd %1, %%zmm15\n\t"            // Broadcast 1.0 to all lanes of zmm15
        "1:\n\t"
        "vfmadd231pd %%zmm15, %%zmm15, %%zmm0\n\t"  // zmm0 = zmm0 + zmm15 * zmm15 (8x packed double FMA)
        "vfmadd231pd %%zmm15, %%zmm15, %%zmm1\n\t"  // zmm1 = zmm1 + zmm15 * zmm15 (8x packed double FMA)
        "vfmadd231pd %%zmm15, %%zmm15, %%zmm2\n\t"  // zmm2 = zmm2 + zmm15 * zmm15 (8x packed double FMA)
        "vfmadd231pd %%zmm15, %%zmm15, %%zmm3\n\t"  // zmm3 = zmm3 + zmm15 * zmm15 (8x packed double FMA)
        "vfmadd231pd %%zmm15, %%zmm15, %%zmm4\n\t"  // zmm4 = zmm4 + zmm15 * zmm15 (8x packed double FMA)
        "vfmadd231pd %%zmm15, %%zmm15, %%zmm5\n\t"  // zmm5 = zmm5 + zmm15 * zmm15 (8x packed double FMA)
        "vfmadd231pd %%zmm15, %%zmm15, %%zmm6\n\t"  // zmm6 = zmm6 + zmm15 * zmm15 (8x packed double FMA)
        "vfmadd231pd %%zmm15, %%zmm15, %%zmm7\n\t"  // zmm7 = zmm7 + zmm15 * zmm15 (8x packed double FMA)
        "vfmadd231pd %%zmm15, %%zmm15, %%zmm8\n\t"  // zmm8 = zmm8 + zmm15 * zmm15 (8x packed double FMA)
        "vfmadd231pd %%zmm15, %%zmm15, %%zmm9\n\t"  // zmm9 = zmm9 + zmm15 * zmm15 (8x packed double FMA)
        "vfmadd231pd %%zmm15, %%zmm15, %%zmm10\n\t" // zmm10 = zmm10 + zmm15 * zmm15 (8x packed double FMA)
        "vfmadd231pd %%zmm15, %%zmm15, %%zmm11\n\t" // zmm11 = zmm11 + zmm15 * zmm15 (8x packed double FMA)
        "vfmadd231pd %%zmm15, %%zmm15, %%zmm12\n\t" // zmm12 = zmm12 + zmm15 * zmm15 (8x packed double FMA)
        "vfmadd231pd %%zmm15, %%zmm15, %%zmm13\n\t" // zmm13 = zmm13 + zmm15 * zmm15 (8x packed double FMA)
        "vfmadd231pd %%zmm15, %%zmm15, %%zmm14\n\t" // zmm14 = zmm14 + zmm15 * zmm15 (8x packed double FMA)
        "sub $1, %0\n\t"
        "jnz 1b\n\t"
        "vzeroupper\n\t"                          // Clear upper bits of all vector registers
        : "+r"(ra)
        : "m"((double){1.0})
        : "cc", "zmm0", "zmm1", "zmm2", "zmm3", "zmm4", "zmm5", "zmm6", "zmm7",
          "zmm8", "zmm9", "zmm10", "zmm11", "zmm12", "zmm13", "zmm14", "zmm15");
#else
    (void)n;
#endif
}

void cleanup_gauge_fma_avx512(void) {
    return;
}
