#!/bin/bash
# 0630 per-kernel filtering runner for PolyBench/C GEMM.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export KERNEL_NAME="polybench_gemm"
export KERNEL_EXT="c"
export OUTPUT_ROOT="output_polybench_gemm_full_0630_dsub"
export DATE_STAMP_DEFAULT="20260630-full-polybench-gemm-dsub"
export SIZE_LIST_DEFAULT="32 64 128 256"
export SMOKE_SIZE_DEFAULT="32"
export EXTRA_LIBS=""
exec "${SCRIPT_DIR}/run_entry_filt.kernel_common.0630.sh" "$@"
