#!/bin/bash
# 0630 per-kernel filtering runner for tl_f90_cg_calc_w.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export KERNEL_NAME="tl_f90_cg_calc_w"
export KERNEL_EXT="c"
export OUTPUT_ROOT="output_tl_f90_cg_calc_w_filt_dsub"
export DATE_STAMP_DEFAULT="20260630-tl-f90-cg-calc-w-dsub"
export SIZE_LIST_DEFAULT="64 128 256 512 1024 2048"
export SMOKE_SIZE_DEFAULT="64"
export EXTRA_LIBS=""
exec "${SCRIPT_DIR}/run_entry_filt.kernel_common.0630.sh" "$@"
