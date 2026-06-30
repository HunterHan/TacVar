#!/bin/bash -x
# 0630 multi-kernel smoke filtering runner. Fixed to cgt-only smoke; no full sweep.
set -euo pipefail

SCRIPT_START_EPOCH=$(date +%s)
SCRIPT_START_ISO=$(date -Is)
SCRIPT_CMD="$0${*:+ $*}"
LOG_CAPTURED=0
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJ_ROOT="${PROJ_ROOT:-$(cd "${SCRIPT_DIR}/.." && pwd)}"
if [ -f "${PROJ_ROOT}/env.bash" ]; then
    set +u
    source "${PROJ_ROOT}/env.bash"
    set -u
fi
PROJ_ROOT="${PROJ_ROOT:-$(cd "${SCRIPT_DIR}/.." && pwd)}"
DATA_ROOT="${DATA_ROOT:-${HOME}/code/data}"
FILTER_ROOT="${PROJ_ROOT}/src/filter"
STENCIL_ROOT="${PROJ_ROOT}/stencil"
if [ -x "$HOME/miniconda3/bin/python" ]; then
    PYTHON="${PYTHON:-$HOME/miniconda3/bin/python}"
elif [ -x "$HOME/miniconda/bin/python" ]; then
    PYTHON="${PYTHON:-$HOME/miniconda/bin/python}"
else
    PYTHON="${PYTHON:-python3}"
fi

initialize(){
    local cpu_freq=$1
    if [ -e /sys/devices/system/cpu/cpufreq/boost ]; then echo 0 | sudo tee /sys/devices/system/cpu/cpufreq/boost >/dev/null 2>/dev/null || true; fi
    if [ -e /sys/devices/system/cpu/intel_pstate/no_turbo ]; then echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo >/dev/null 2>/dev/null || true; fi
    sudo cpupower frequency-set -u "${cpu_freq}GHz" -d "${cpu_freq}GHz" -g performance || true
    sudo cpupower frequency-info || true
}
cleanup(){
    sudo cpupower frequency-set -g schedutil || true
    if [ -e /sys/devices/system/cpu/intel_pstate/no_turbo ]; then echo 0 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo >/dev/null 2>/dev/null || true; fi
    if [ -e /sys/devices/system/cpu/cpufreq/boost ]; then echo 1 | sudo tee /sys/devices/system/cpu/cpufreq/boost >/dev/null 2>/dev/null || true; fi
}
cleanup_residual_processes(){
    local pattern='openblas_.*\.x|npb_ep_.*\.x|stream_triad_.*\.x|hpcg_spmv_.*\.x|npb_ft_fft_.*\.x|filt\.x|mpirun|prterun|orted|prted|run_entry_filt\.kernels_smoke\.0630'
    pgrep -af "${pattern}" || true
    pgrep -af "${pattern}" | awk -v self="$$" '$1 != self {print $1}' | xargs -r kill -9 || true
}
finish_run(){
    local status=$?
    local end_epoch=$(date +%s)
    echo "[harness] end_time=$(date -Is)"
    echo "[harness] exit_status=${status}"
    echo "[harness] elapsed_sec=$((end_epoch - SCRIPT_START_EPOCH))"
    cleanup
    exit "${status}"
}
trap 'status=$?; echo "[harness] ERR status=${status} line=${LINENO} command=${BASH_COMMAND}"' ERR
trap finish_run EXIT

INSITU=${INSITU:-INSITU_DSUB_ASM}
case "${INSITU}" in
    INSITU_SUB_ASM) INSITU_TAG=${INSITU_TAG:-sub}; THEORETICAL_NSPV_FACTOR=${THEORETICAL_NSPV_FACTOR:-1} ;;
    INSITU_DSUB_ASM) INSITU_TAG=${INSITU_TAG:-dsub}; THEORETICAL_NSPV_FACTOR=${THEORETICAL_NSPV_FACTOR:-2} ;;
    *) INSITU_TAG=${INSITU_TAG:-custom}; THEORETICAL_NSPV_FACTOR=${THEORETICAL_NSPV_FACTOR:-1} ;;
esac

KERNEL_LIST="openblas_gemm openblas_gemv openblas_dot openblas_axpy stream_triad hpcg_spmv npb_ft_fft npb_ep"
TIMER_LIST="cgt"
NP_LIST="64"
NSAMP=20
NSAMP_RATIO_LIST="0.5"
SHUFFLE_COUNT=1
SHUFFLE_SEED=0630
BINW_MIN=${BINW_MIN:-10}
P_LOW=${P_LOW:-0.01}
SMOKE=1

ARCH=$(uname -m)
HOSTNAME=$(hostname -s 2>/dev/null || hostname)
DATE_BASE=${DATE_BASE:-20260630}
DATE_STAMP=${DATE_STAMP:-20260630-kernels-smoke-${INSITU_TAG}}
DATA_FOLDER="${DATA_ROOT}/${DATE_BASE}/${HOSTNAME}/output_kernels_smoke_0630_${INSITU_TAG}/${DATE_STAMP}"
COMMIT_HASH=${COMMIT_HASH:-$(git -C "${PROJ_ROOT}" rev-parse HEAD 2>/dev/null || echo unknown)}
case $HOSTNAME in
    camd9554n1|camd9554n2) CPU_FREQ=3.1 ;;
    cgnr6760pn2) CPU_FREQ=2.2 ;;
    c920bn3) CPU_FREQ=2.9 ;;
    *) echo "Unknown hostname: $HOSTNAME"; exit 1 ;;
esac

mkdir -p "${DATA_FOLDER}"
if [ "${LOG_CAPTURED}" = "0" ]; then LOG_CAPTURED=1; exec > >(tee -a "${DATA_FOLDER}/run.log") 2>&1; fi
cleanup_residual_processes
initialize "$CPU_FREQ"
CPU_FREQ_KHZ_REAL=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq 2>/dev/null || echo 1000000)
NSPV=$(echo "scale=12; 1000000 / $CPU_FREQ_KHZ_REAL" | bc)
EFFECTIVE_NSPV=$(echo "scale=12; ${NSPV} * ${THEORETICAL_NSPV_FACTOR}" | bc)

{
    echo "script=$(realpath "$0")"
    echo "run_start_time=${SCRIPT_START_ISO}"
    echo "run_command=${SCRIPT_CMD}"
    echo "binary_commit=${COMMIT_HASH}"
    echo "host=${HOSTNAME}"
    echo "arch=${ARCH}"
    echo "date_base=${DATE_BASE}"
    echo "date_stamp=${DATE_STAMP}"
    echo "data_folder=${DATA_FOLDER}"
    echo "kernel_list=${KERNEL_LIST}"
    echo "timer_list=${TIMER_LIST}"
    echo "np_list=${NP_LIST}"
    echo "nsamp=${NSAMP}"
    echo "nsamp_ratio_list=${NSAMP_RATIO_LIST}"
    echo "insitu=${INSITU}"
    echo "effective_nspv=${EFFECTIVE_NSPV}"
    for k in ${KERNEL_LIST}; do src="${STENCIL_ROOT}/${k}.c"; [ "$k" = "hpcg_spmv" ] && src="${STENCIL_ROOT}/${k}.cpp"; echo "${k}_source_sha256=$(sha256sum "$src" | awk '{print $1}')"; done
    echo "script_sha256=$(sha256sum "$0" | awk '{print $1}')"
} > "${DATA_FOLDER}/meta.txt"
cp -f "$0" "${DATA_FOLDER}/$(basename "$0")"

cd "${STENCIL_ROOT}"
rm -f ./*.x ./*.csv
mpicc -O2 -Wall -o "${FILTER_ROOT}/filt.x" "${FILTER_ROOT}/filt_v2.0608.c"
BASE_CFLAGS="-O2 -Wall -g -DNWARM=20 -DNTEST=3"
for kernel in ${KERNEL_LIST}; do
    for timer in ${TIMER_LIST}; do
        if [ "$kernel" = "hpcg_spmv" ]; then
            mpicxx -std=c++11 -o "${kernel}_${timer}.x" "${kernel}.cpp" ${BASE_CFLAGS} -D"$INSITU" -DTIMING "-DUSE_${timer^^}"
            mpicxx -std=c++11 -o "${kernel}_${timer}_tf.x" "${kernel}.cpp" ${BASE_CFLAGS} -D"$INSITU" -DSTAGE_TF -DTIMING "-DUSE_${timer^^}"
        else
            extra=""
            case "$kernel" in npb_ft_fft|npb_ep) extra="-lm" ;; esac
            mpicc -std=c11 -o "${kernel}_${timer}.x" "${kernel}.c" ${BASE_CFLAGS} -D"$INSITU" -DTIMING "-DUSE_${timer^^}" ${extra}
            mpicc -std=c11 -o "${kernel}_${timer}_tf.x" "${kernel}.c" ${BASE_CFLAGS} -D"$INSITU" -DSTAGE_TF -DTIMING "-DUSE_${timer^^}" ${extra}
        fi
    done
done

kernel_size(){
    case "$1" in
        openblas_gemm) echo 32 ;;
        openblas_gemv|openblas_dot|openblas_axpy) echo 128 ;;
        stream_triad) echo 64 ;;
        hpcg_spmv) echo 16 ;;
        npb_ft_fft) echo 256 ;;
        npb_ep) echo 64 ;;
        *) echo 64 ;;
    esac
}

for shuffle_idx in 0; do
    shuffle_id="shuffle${shuffle_idx}"
    shuffle_dir="${DATA_FOLDER}/${shuffle_id}"
    mkdir -p "$shuffle_dir"
    cp "${DATA_FOLDER}/meta.txt" "${shuffle_dir}/meta.txt"
    for kernel in ${KERNEL_LIST}; do
        size=$(kernel_size "$kernel")
        for timer in ${TIMER_LIST}; do
            for np in ${NP_LIST}; do
                tm_dir="${shuffle_dir}/${kernel}_${timer}_np${np}_size${size}"
                rm -rf "$tm_dir" ./*.csv
                mkdir -p "$tm_dir"
                mpirun -np "$np" --map-by core --bind-to core "./${kernel}_${timer}.x" "$size" 0 1
                mv ./*.csv "$tm_dir/"
                for nsamp_ratio in ${NSAMP_RATIO_LIST}; do
                    nsamp_tf=$("${PYTHON}" "${FILTER_ROOT}/get_quantile.py" "$tm_dir" 1 "$nsamp_ratio" "${EFFECTIVE_NSPV}")
                    te_dir="${tm_dir}_nsampRatio${nsamp_ratio}_nsamp${nsamp_tf}_tf"
                    res_dir="${tm_dir}_nsampRatio${nsamp_ratio}_nsamp${nsamp_tf}_filt"
                    rm -rf "$te_dir" "$res_dir" ./*.csv
                    mkdir -p "$te_dir" "$res_dir"
                    mpirun -np "$np" --map-by core --bind-to core "./${kernel}_${timer}_tf.x" "$size" 0 1 "$nsamp_tf"
                    mv ./*.csv "$te_dir/"
                    "${PYTHON}" "${FILTER_ROOT}/get_met.py" "$tm_dir" 1
                    "${PYTHON}" "${FILTER_ROOT}/get_tf.py" "$te_dir" 1 2 "${EFFECTIVE_NSPV}"
                    binw=$("${PYTHON}" "${FILTER_ROOT}/get_binw.py" "$tm_dir" 1 "$BINW_MIN")
                    "${FILTER_ROOT}/filt.x" -w "$binw" -n 100000 -l "$P_LOW" -x 0.005 -y 0.005 -z 0.005
                    {
                        echo "binary_commit=${COMMIT_HASH}"
                        echo "shuffle_id=${shuffle_id}"
                        echo "kernel=${kernel}"
                        echo "timer=${timer}"
                        echo "np=${np}"
                        echo "size=${size}"
                        echo "nsamp_ratio=${nsamp_ratio}"
                        echo "nsamp=${nsamp_tf}"
                        echo "insitu=${INSITU}"
                        echo "effective_nspv=${EFFECTIVE_NSPV}"
                    } > "${res_dir}/meta.txt"
                    mv met.csv tf.csv tr_hist.csv tm_hist.csv sim_cdf.csv er.out ep.out wd.out calc_tr_residual.0608.csv "$res_dir/"
                done
            done
        done
    done
done
