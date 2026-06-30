#!/bin/bash
# 0630 per-kernel filtering runner for jacobi2d5p.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export KERNEL_NAME="jacobi2d5p"
export KERNEL_EXT="c"
export OUTPUT_ROOT="output_jacobi2d5p_filt_dsub"
export DATE_STAMP_DEFAULT="20260630-jacobi2d5p-dsub"
export SIZE_LIST_DEFAULT="64 128 256 512 1024 2048"
export SMOKE_SIZE_DEFAULT="64"
export EXTRA_LIBS=""
exec "${SCRIPT_DIR}/run_entry_filt.kernel_common.0630.sh" "$@"
