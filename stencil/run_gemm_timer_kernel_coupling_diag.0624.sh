#!/bin/bash -x
set -euo pipefail

SCRIPT_START_ISO=$(date -Is)
SCRIPT_START_EPOCH=$(date +%s)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJ_ROOT="${PROJ_ROOT:-$(cd "${SCRIPT_DIR}/.." && pwd)}"
if [ -f "${PROJ_ROOT}/env.bash" ]; then
    set +u
    source "${PROJ_ROOT}/env.bash"
    set -u
fi

STENCIL_ROOT="${PROJ_ROOT}/stencil"
DATA_ROOT="${DATA_ROOT:-${HOME}/code/data}"
HOSTNAME="$(hostname -s 2>/dev/null || hostname)"
ARCH="$(uname -m)"
DATE_BASE="${DATE_BASE:-20260624_timer_kernel_coupling}"
DATE_STAMP="${DATE_STAMP:-20260624-gemm-timer-kernel-coupling}"
OUT_ROOT="${DATA_ROOT}/${DATE_BASE}/${HOSTNAME}/output_gemm_timer_kernel_coupling_diag/${DATE_STAMP}"
COMMIT_HASH="${COMMIT_HASH:-$(git -C "${PROJ_ROOT}" rev-parse HEAD 2>/dev/null || echo unknown)}"
CFLAGS="${DIAG_CFLAGS:--O2 -Wall -g -std=c11}"
NP_LIST="${NP_LIST:-64}"
SIZE_LIST="${SIZE_LIST:-64}"
NSAMP="${NSAMP:-500}"

if [ "${SMOKE:-0}" = "1" ]; then
    NP_LIST="${SMOKE_NP_LIST:-1}"
    SIZE_LIST="${SMOKE_SIZE_LIST:-32}"
    NSAMP="${SMOKE_NSAMP:-20}"
fi

case "$HOSTNAME" in
    camd9554n1|camd9554n2) CPU_FREQ=3.1 ;;
    cgnr6760pn2) CPU_FREQ=2.2 ;;
    *) CPU_FREQ="" ;;
esac

if [ "$ARCH" != "x86_64" ]; then
    echo "This diagnostic is x86-only; got arch=$ARCH"
    exit 2
fi

TIMER_LIST="${TIMER_LIST:-tsc cgt papi wtime}"
KERNEL_LIST="${KERNEL_LIST:-gemm dsub}"

timer_macro() {
    case "$1" in
        tsc) echo USE_TSC ;;
        cgt) echo USE_CGT ;;
        papi) echo USE_PAPI ;;
        wtime) echo USE_WTIME ;;
        *) echo "unknown timer $1" >&2; exit 2 ;;
    esac
}

kernel_macro() {
    case "$1" in
        gemm) echo KERNEL_GEMM ;;
        dsub) echo KERNEL_DSUB ;;
        *) echo "unknown kernel $1" >&2; exit 2 ;;
    esac
}

needs_papi() {
    [ "$1" = "papi" ]
}

cleanup() {
    if [ -n "${CPU_FREQ}" ]; then sudo cpupower frequency-set -g schedutil || true; fi
}

finish() {
    st=$?
    echo "[harness] exit_status=${st}"
    echo "[harness] elapsed_sec=$(($(date +%s) - SCRIPT_START_EPOCH))"
    cleanup
    exit "$st"
}
trap finish EXIT

mkdir -p "$OUT_ROOT"
exec > >(tee -a "${OUT_ROOT}/run.log") 2>&1
echo "[harness] start_time=${SCRIPT_START_ISO} host=${HOSTNAME} arch=${ARCH}"
if [ -n "${CPU_FREQ}" ]; then
    sudo cpupower frequency-set -u "${CPU_FREQ}GHz" -d "${CPU_FREQ}GHz" -g performance || true
fi

{
    echo "binary_commit=${COMMIT_HASH}"
    echo "host=${HOSTNAME}"
    echo "arch=${ARCH}"
    echo "date_base=${DATE_BASE}"
    echo "date_stamp=${DATE_STAMP}"
    echo "timer_list=${TIMER_LIST}"
    echo "kernel_list=${KERNEL_LIST}"
    echo "size_list=${SIZE_LIST}"
    echo "np_list=${NP_LIST}"
    echo "nsamp=${NSAMP}"
    echo "source_sha256=$(sha256sum "${STENCIL_ROOT}/gemm_timer_kernel_coupling_diag.c" | cut -d ' ' -f1)"
    echo "runner_sha256=$(sha256sum "$0" | cut -d ' ' -f1)"
    git -C "$PROJ_ROOT" status --short || true
} > "${OUT_ROOT}/meta.txt"
lscpu > "${OUT_ROOT}/lscpu.txt" 2>&1 || true
sudo cpupower frequency-info > "${OUT_ROOT}/cpupower_frequency_info.txt" 2>&1 || true
sha256sum "${STENCIL_ROOT}/gemm_timer_kernel_coupling_diag.c" "$0" > "${OUT_ROOT}/source_manifest.txt"

cd "$STENCIL_ROOT"
rm -f gemm_timer_kernel_coupling_*.x
: > "${OUT_ROOT}/compile_commands.txt"

for timer in ${TIMER_LIST}; do
    for kernel in ${KERNEL_LIST}; do
        tm=$(timer_macro "$timer")
        km=$(kernel_macro "$kernel")
        flags="${CFLAGS} -D${tm} -D${km} -DTIMER_NAME=${timer} -DKERNEL_NAME=${kernel}"
        libs=""
        if needs_papi "$timer"; then
            flags="${flags} -I${PAPI_HOME}/include"
            libs="-L${PAPI_HOME}/lib -lpapi"
        fi
        bin="gemm_timer_kernel_coupling_${timer}_${kernel}.x"
        echo "mpicc ${flags} gemm_timer_kernel_coupling_diag.c -o ${bin} ${libs}" >> "${OUT_ROOT}/compile_commands.txt"
        mpicc ${flags} gemm_timer_kernel_coupling_diag.c -o "${bin}" ${libs}
    done
done

for np in ${NP_LIST}; do
    for size in ${SIZE_LIST}; do
        for timer in ${TIMER_LIST}; do
            for kernel in ${KERNEL_LIST}; do
                rdir="${OUT_ROOT}/${timer}_${kernel}_np${np}_size${size}"
                rm -rf "$rdir"
                mkdir -p "$rdir"
                mpirun -np "$np" --map-by core --bind-to core "./gemm_timer_kernel_coupling_${timer}_${kernel}.x" "$size" "$NSAMP" "$rdir"
                {
                    echo "timer=${timer}"
                    echo "kernel=${kernel}"
                    echo "np=${np}"
                    echo "size=${size}"
                    echo "nsamp=${NSAMP}"
                } > "${rdir}/meta.txt"
            done
        done
    done
done
