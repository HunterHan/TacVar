#!/bin/bash
# 0630 per-kernel filtering runner for openblas_gemm.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export KERNEL_NAME="openblas_gemm"
export KERNEL_EXT="c"
export OUTPUT_ROOT="output_openblas_gemm_filt_dsub"
export DATE_STAMP_DEFAULT="20260630-openblas-gemm-dsub"
export SIZE_LIST_DEFAULT="32 64 128 256"
export SMOKE_SIZE_DEFAULT="32"
export EXTRA_LIBS=""
exec "${SCRIPT_DIR}/run_entry_filt.kernel_common.0630.sh" "$@"
