#!/bin/bash -x
set -euo pipefail
SCRIPT_START_ISO=$(date -Is)
SCRIPT_START_EPOCH=$(date +%s)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJ_ROOT="${PROJ_ROOT:-$(cd "${SCRIPT_DIR}/.." && pwd)}"
if [ -f "${PROJ_ROOT}/env.bash" ]; then set +u; source "${PROJ_ROOT}/env.bash"; set -u; fi
STENCIL_ROOT="${PROJ_ROOT}/stencil"
DATA_ROOT="${DATA_ROOT:-${HOME}/code/data}"
PYTHON="${PYTHON:-python3}"
HOSTNAME="$(hostname -s 2>/dev/null || hostname)"
ARCH="$(uname -m)"
DATE_BASE="${DATE_BASE:-20260624_timer_diag}"
DATE_STAMP="${DATE_STAMP:-20260624-gemm-cgt-kernel-depend}"
OUT_ROOT="${DATA_ROOT}/${DATE_BASE}/${HOSTNAME}/output_gemm_cgt_kernel_depend_diag/${DATE_STAMP}"
COMMIT_HASH="${COMMIT_HASH:-$(git -C "${PROJ_ROOT}" rev-parse HEAD 2>/dev/null || echo unknown)}"
CFLAGS="${DIAG_CFLAGS:--O2 -Wall -g -std=c11}"
NP_LIST="${NP_LIST:-64}"
SIZE_LIST="${SIZE_LIST:-32 64}"
NSAMP="${NSAMP:-200}"
SHUFFLE_COUNT="${SHUFFLE_COUNT:-3}"
SHUFFLE_SEED="${SHUFFLE_SEED:-0624}"
if [ "${SMOKE:-0}" = "1" ]; then SIZE_LIST="${SMOKE_SIZE_LIST:-32}"; NSAMP="${SMOKE_NSAMP:-100}"; SHUFFLE_COUNT=1; fi
case "$HOSTNAME" in camd9554n1|camd9554n2) CPU_FREQ=3.1 ;; cgnr6760pn2) CPU_FREQ=2.2 ;; c920bn3) CPU_FREQ=2.9 ;; *) CPU_FREQ="" ;; esac
case "$ARCH" in
  x86_64) VARIANT_LIST="${VARIANT_LIST:-tsc_current_gemm tsc_after_cgt_prelude_gemm tsc_after_cgt_call_only_gemm tsc_after_cgt_prelude_dsub papi_time_only cgt_current wtime_current}" ;;
  aarch64) VARIANT_LIST="${VARIANT_LIST:-cntvcto_current_gemm cntvcto_after_cgt_prelude_gemm papi_time_only cgt_current wtime_current}" ;;
  *) echo "unsupported arch $ARCH"; exit 2 ;;
esac
macro_for_variant(){
  case "$1" in
    tsc_current_gemm) echo VAR_TICK_CURRENT_GEMM ;;
    tsc_after_cgt_prelude_gemm) echo VAR_TICK_AFTER_CGT_PRELUDE_GEMM ;;
    tsc_after_cgt_call_only_gemm) echo VAR_TICK_AFTER_CGT_CALL_ONLY_GEMM ;;
    tsc_after_cgt_prelude_dsub) echo VAR_TICK_AFTER_CGT_PRELUDE_DSUB ;;
    cntvcto_current_gemm) echo VAR_TICK_CURRENT_GEMM ;;
    cntvcto_after_cgt_prelude_gemm) echo VAR_CNTVCTO_AFTER_CGT_PRELUDE_GEMM ;;
    papi_time_only) echo VAR_PAPI_TIME_ONLY ;;
    cgt_current) echo VAR_CGT_CURRENT ;;
    wtime_current) echo VAR_WTIME_CURRENT ;;
    *) echo "unknown variant $1" >&2; exit 2 ;;
  esac
}
needs_papi(){ [ "$1" = "papi_time_only" ]; }
shuffle_list(){ "$PYTHON" - "$SHUFFLE_SEED" "$1" ${VARIANT_LIST} <<'PY_SHUF'
import random,sys
rng=random.Random(f"{sys.argv[1]}:{sys.argv[2]}")
items=sys.argv[3:]
rng.shuffle(items)
print(" ".join(items))
PY_SHUF
}
cleanup(){ if [ -n "${CPU_FREQ}" ]; then sudo cpupower frequency-set -g schedutil || true; fi; }
finish(){ st=$?; echo "[harness] exit_status=${st}"; echo "[harness] elapsed_sec=$(($(date +%s)-SCRIPT_START_EPOCH))"; cleanup; exit "$st"; }
trap finish EXIT
mkdir -p "$OUT_ROOT"
exec > >(tee -a "${OUT_ROOT}/run.log") 2>&1
echo "[harness] start_time=${SCRIPT_START_ISO} host=${HOSTNAME} arch=${ARCH}"
if [ -n "${CPU_FREQ}" ]; then sudo cpupower frequency-set -u "${CPU_FREQ}GHz" -d "${CPU_FREQ}GHz" -g performance || true; fi
{
 echo "binary_commit=${COMMIT_HASH}"
 echo "host=${HOSTNAME}"
 echo "arch=${ARCH}"
 echo "date_base=${DATE_BASE}"
 echo "date_stamp=${DATE_STAMP}"
 echo "variant_list=${VARIANT_LIST}"
 echo "size_list=${SIZE_LIST}"
 echo "np_list=${NP_LIST}"
 echo "nsamp=${NSAMP}"
 echo "shuffle_count=${SHUFFLE_COUNT}"
 echo "diagnostic_default_note=proof-run defaults use SIZE_LIST=32 64 and NSAMP=200; larger sizes may be supplied explicitly but are intentionally not default because GEMM-row surrogate diagnostics are expensive"
 echo "runner_sha256=$(sha256sum "$0" | cut -d ' ' -f1)"
 echo "source_sha256=$(sha256sum "${STENCIL_ROOT}/gemm_cgt_kernel_depend_diag.c" | cut -d ' ' -f1)"
 echo "polybench_gemm_sha256=$(sha256sum "${STENCIL_ROOT}/polybench_gemm.c" | cut -d ' ' -f1)"
 git -C "$PROJ_ROOT" status --short || true
} > "${OUT_ROOT}/meta.txt"
lscpu > "${OUT_ROOT}/lscpu.txt" 2>&1 || true
sudo cpupower frequency-info > "${OUT_ROOT}/cpupower_frequency_info.txt" 2>&1 || true
sha256sum "${STENCIL_ROOT}/gemm_cgt_kernel_depend_diag.c" "${STENCIL_ROOT}/polybench_gemm.c" "$0" > "${OUT_ROOT}/source_manifest.txt"
cd "$STENCIL_ROOT"
rm -f gemm_cgt_kernel_depend_*.x
: > "${OUT_ROOT}/compile_commands.txt"
for v in ${VARIANT_LIST}; do
  m=$(macro_for_variant "$v")
  flags="${CFLAGS} -D${m} -DTIMER_NAME=${v}"
  libs=""
  if needs_papi "$v"; then flags="${flags} -DUSE_PAPI -I${PAPI_HOME}/include"; libs="-L${PAPI_HOME}/lib -lpapi"; fi
  echo "mpicc ${flags} gemm_cgt_kernel_depend_diag.c -o gemm_cgt_kernel_depend_${v}.x ${libs}" >> "${OUT_ROOT}/compile_commands.txt"
  mpicc ${flags} gemm_cgt_kernel_depend_diag.c -o "gemm_cgt_kernel_depend_${v}.x" ${libs}
  objdump -d "gemm_cgt_kernel_depend_${v}.x" > "${OUT_ROOT}/objdump_${v}.txt" || true
done
for shuf in $(seq 0 $((SHUFFLE_COUNT-1))); do
  sdir="${OUT_ROOT}/shuffle${shuf}"; mkdir -p "$sdir"
  order=$(shuffle_list "$shuf"); echo "shuffle${shuf}_variant_list=${order}" | tee -a "${OUT_ROOT}/shuffle.log"
  for v in ${order}; do
    for np in ${NP_LIST}; do
      for size in ${SIZE_LIST}; do
        rdir="${sdir}/${v}_np${np}_size${size}"; rm -rf "$rdir"; mkdir -p "$rdir"
        mpirun -np "$np" --map-by core --bind-to core "./gemm_cgt_kernel_depend_${v}.x" "$size" "$NSAMP" "$rdir"
        echo "variant=${v}" > "${rdir}/meta.txt"; echo "np=${np}" >> "${rdir}/meta.txt"; echo "size=${size}" >> "${rdir}/meta.txt"; echo "nsamp=${NSAMP}" >> "${rdir}/meta.txt"
      done
    done
  done
done
