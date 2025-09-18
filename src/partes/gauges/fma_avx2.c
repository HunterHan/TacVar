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
        "vxorpd %%ymm2, %%ymm2, %%ymm2\n\t"       // Zero ymm2
        "vxorpd %%ymm3, %%ymm3, %%ymm3\n\t"       // Zero ymm3
        "vxorpd %%ymm4, %%ymm4, %%ymm4\n\t"       // Zero ymm4
        "vxorpd %%ymm5, %%ymm5, %%ymm5\n\t"       // Zero ymm5
        "vxorpd %%ymm6, %%ymm6, %%ymm6\n\t"       // Zero ymm6
        "vxorpd %%ymm7, %%ymm7, %%ymm7\n\t"       // Zero ymm7
        "vxorpd %%ymm8, %%ymm8, %%ymm8\n\t"       // Zero ymm8
        "vxorpd %%ymm9, %%ymm9, %%ymm9\n\t"       // Zero ymm9
        "vxorpd %%ymm10, %%ymm10, %%ymm10\n\t"    // Zero ymm10
        "vxorpd %%ymm11, %%ymm11, %%ymm11\n\t"    // Zero ymm11
        "vxorpd %%ymm12, %%ymm12, %%ymm12\n\t"    // Zero ymm12
        "vxorpd %%ymm13, %%ymm13, %%ymm13\n\t"    // Zero ymm13
        "vxorpd %%ymm14, %%ymm14, %%ymm14\n\t"    // Zero ymm14
        "vxorpd %%ymm15, %%ymm15, %%ymm15\n\t"    // Zero ymm15
        "vbroadcastsd %1, %%ymm15\n\t"            // Broadcast 1.0 to all lanes of ymm15
        "1:\n\t"
        "vfmadd231pd %%ymm15, %%ymm15, %%ymm0\n\t"  // ymm0 = ymm0 + ymm15 * ymm15 (4x packed double FMA)
        "vfmadd231pd %%ymm15, %%ymm15, %%ymm1\n\t"  // ymm1 = ymm1 + ymm15 * ymm15 (4x packed double FMA)
        "vfmadd231pd %%ymm15, %%ymm15, %%ymm2\n\t"  // ymm2 = ymm2 + ymm15 * ymm15 (4x packed double FMA)
        "vfmadd231pd %%ymm15, %%ymm15, %%ymm3\n\t"  // ymm3 = ymm3 + ymm15 * ymm15 (4x packed double FMA)
        "vfmadd231pd %%ymm15, %%ymm15, %%ymm4\n\t"  // ymm4 = ymm4 + ymm15 * ymm15 (4x packed double FMA)
        "vfmadd231pd %%ymm15, %%ymm15, %%ymm5\n\t"  // ymm5 = ymm5 + ymm15 * ymm15 (4x packed double FMA)
        "vfmadd231pd %%ymm15, %%ymm15, %%ymm6\n\t"  // ymm6 = ymm6 + ymm15 * ymm15 (4x packed double FMA)
        "vfmadd231pd %%ymm15, %%ymm15, %%ymm7\n\t"  // ymm7 = ymm7 + ymm15 * ymm15 (4x packed double FMA)
        "vfmadd231pd %%ymm15, %%ymm15, %%ymm8\n\t"  // ymm8 = ymm8 + ymm15 * ymm15 (4x packed double FMA)
        "vfmadd231pd %%ymm15, %%ymm15, %%ymm9\n\t"  // ymm9 = ymm9 + ymm15 * ymm15 (4x packed double FMA)
        "vfmadd231pd %%ymm15, %%ymm15, %%ymm10\n\t" // ymm10 = ymm10 + ymm15 * ymm15 (4x packed double FMA)
        "vfmadd231pd %%ymm15, %%ymm15, %%ymm11\n\t" // ymm11 = ymm11 + ymm15 * ymm15 (4x packed double FMA)
        "vfmadd231pd %%ymm15, %%ymm15, %%ymm12\n\t" // ymm12 = ymm12 + ymm15 * ymm15 (4x packed double FMA)
        "vfmadd231pd %%ymm15, %%ymm15, %%ymm13\n\t" // ymm13 = ymm13 + ymm15 * ymm15 (4x packed double FMA)
        "vfmadd231pd %%ymm15, %%ymm15, %%ymm14\n\t" // ymm14 = ymm14 + ymm15 * ymm15 (4x packed double FMA)
        "sub $1, %0\n\t"
        "jnz 1b\n\t"
        "vzeroupper\n\t"                          // Clear upper 128 bits of all YMM registers
        : "+r"(ra)
        : "m"((double){1.0})
        : "cc", "ymm0", "ymm1", "ymm2", "ymm3", "ymm4", "ymm5", "ymm6", "ymm7",
          "ymm8", "ymm9", "ymm10", "ymm11", "ymm12", "ymm13", "ymm14", "ymm15");
#else
    (void)n;
#endif
}

void cleanup_gauge_fma_avx2(void) {
    return;
}
