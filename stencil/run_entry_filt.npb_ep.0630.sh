#!/bin/bash
# 0630 per-kernel filtering runner for npb_ep.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export KERNEL_NAME="npb_ep"
export KERNEL_EXT="c"
export OUTPUT_ROOT="output_npb_ep_filt_dsub"
export DATE_STAMP_DEFAULT="20260630-npb-ep-dsub"
export SIZE_LIST_DEFAULT="64 128 256 512 1024 2048"
export SMOKE_SIZE_DEFAULT="64"
export EXTRA_LIBS="-lm"
exec "${SCRIPT_DIR}/run_entry_filt.kernel_common.0630.sh" "$@"
