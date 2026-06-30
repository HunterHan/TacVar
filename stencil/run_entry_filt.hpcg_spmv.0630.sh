#!/bin/bash
# 0630 per-kernel filtering runner for hpcg_spmv.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export KERNEL_NAME="hpcg_spmv"
export KERNEL_EXT="cpp"
export OUTPUT_ROOT="output_hpcg_spmv_filt_dsub"
export DATE_STAMP_DEFAULT="20260630-hpcg-spmv-dsub"
export SIZE_LIST_DEFAULT="8 12 16 24 32 48 64"
export SMOKE_SIZE_DEFAULT="16"
export EXTRA_LIBS=""
exec "${SCRIPT_DIR}/run_entry_filt.kernel_common.0630.sh" "$@"
