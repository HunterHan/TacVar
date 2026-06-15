#!/bin/bash -x
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

initialize(){
    local cpu_freq=$1
    if [ -e /sys/devices/system/cpu/cpufreq/boost ]; then
        echo 0 | sudo tee /sys/devices/system/cpu/cpufreq/boost >/dev/null 2>/dev/null || true
    fi
    if [ -e /sys/devices/system/cpu/intel_pstate/no_turbo ]; then
        echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo >/dev/null 2>/dev/null || true
    fi
    sudo cpupower frequency-set -u "${cpu_freq}GHz" -d "${cpu_freq}GHz" -g performance
    sudo cpupower frequency-info
    [ -r /sys/devices/system/cpu/intel_pstate/no_turbo ] && echo "intel_no_turbo=$(cat /sys/devices/system/cpu/intel_pstate/no_turbo)"
    [ -r /sys/devices/system/cpu/cpufreq/boost ] && echo "cpufreq_boost=$(cat /sys/devices/system/cpu/cpufreq/boost)"
}

cleanup(){
    sudo cpupower frequency-set -g schedutil || true
    if [ -e /sys/devices/system/cpu/intel_pstate/no_turbo ]; then
        echo 0 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo >/dev/null 2>/dev/null || true
    fi
    if [ -e /sys/devices/system/cpu/cpufreq/boost ]; then
        echo 1 | sudo tee /sys/devices/system/cpu/cpufreq/boost >/dev/null 2>/dev/null || true
    fi
}

cleanup_residual_processes(){
    local pattern='jacobi2d5p_.*\.x|tl_f90_cg_calc_w_.*\.x|gs2d5p_.*\.x|filt\.x|mpirun|prterun|orted|prted|run_entry_filt'
    local skip_pids=" $$ "
    local pid=$$
    local ppid
    while :; do
        ppid=$(ps -o ppid= -p "$pid" 2>/dev/null | awk '{print $1}') || true
        [ -z "${ppid:-}" ] && break
        skip_pids="${skip_pids} ${ppid} "
        [ "$ppid" = "1" ] && break
        pid="$ppid"
    done
    echo "[harness] residual processes before cleanup:"
    pgrep -af "${pattern}" | awk -v skip="$skip_pids" 'index(skip, " " $1 " ") == 0 {print}' || true
    pgrep -af "${pattern}" | awk -v skip="$skip_pids" 'index(skip, " " $1 " ") == 0 {print $1}' | xargs -r kill -9 || true
    sleep 1
    echo "[harness] residual processes after cleanup:"
    pgrep -af "${pattern}" | awk -v skip="$skip_pids" 'index(skip, " " $1 " ") == 0 {print}' || true
}

shuffle_timer_list(){
    local shuffle_idx=$1
    shift
    "${PYTHON}" - "${SHUFFLE_SEED}" "${shuffle_idx}" "$@" <<'PY_SHUFFLE'
import random
import sys
seed = sys.argv[1]
shuffle_idx = int(sys.argv[2])
items = sys.argv[3:]
rng = random.Random(f"{seed}:{shuffle_idx}:{' '.join(items)}")
rng.shuffle(items)
print(" ".join(items))
PY_SHUFFLE
}

write_root_meta(){
    local data_folder=$1
    mkdir -p "${data_folder}"
    {
        echo "script=$(realpath "$0")"
        echo "run_start_time=${SCRIPT_START_ISO}"
        echo "run_command=${SCRIPT_CMD}"
        echo "run_log=${data_folder}/run.log"
        echo "script_sha256=$(sha256sum "$0" 2>/dev/null | awk '{print $1}')"
        echo "binary_commit=${COMMIT_HASH}"
        echo "git_status_begin"
        git -C "${PROJ_ROOT}" status --short || true
        echo "git_status_end"
        echo "host=${HOSTNAME}"
        echo "arch=${ARCH}"
        echo "date_base=${DATE_BASE}"
        echo "date_stamp=${DATE_STAMP}"
        echo "data_folder=${data_folder}"
        echo "shuffle_seed=${SHUFFLE_SEED}"
        echo "shuffle_count=${SHUFFLE_COUNT}"
        echo "kernel_list=${KERNEL_LIST}"
        echo "timer_list=${TIMER_LIST}"
        echo "size_list=${SIZE_LIST}"
        echo "np_list=${NP_LIST}"
        echo "nsamp=${NSAMP}"
        echo "nsamp_ratio_list=${NSAMP_RATIO_LIST}"
        echo "insitu=${INSITU}"
        echo "insitu_tag=${INSITU_TAG}"
        echo "nspv=${NSPV:-}"
        echo "theoretical_nspv_factor=${THEORETICAL_NSPV_FACTOR:-}"
        echo "effective_nspv=${EFFECTIVE_NSPV:-}"
    } > "${data_folder}/meta.txt"
    cp -f "$0" "${data_folder}/$(basename "$0")"
}

write_shuffle_meta(){
    local shuffle_dir=$1
    local shuffle_id=$2
    local shuffled_timers=$3
    mkdir -p "${shuffle_dir}"
    {
        cat "${DATA_FOLDER}/meta.txt"
        echo "shuffle_id=${shuffle_id}"
        echo "shuffle_timer_list=${shuffled_timers}"
    } > "${shuffle_dir}/meta.txt"
}

start_run_log(){
    mkdir -p "${DATA_FOLDER}"
    if [ "${LOG_CAPTURED}" = "0" ]; then
        LOG_CAPTURED=1
        exec > >(tee -a "${DATA_FOLDER}/run.log") 2>&1
    fi
    echo "[harness] start_time=${SCRIPT_START_ISO}"
    echo "[harness] command=${SCRIPT_CMD}"
    echo "[harness] cwd=$(pwd)"
    echo "[harness] pid=$$ ppid=$PPID"
}

finish_run(){
    local status=$?
    local end_epoch end_iso
    end_epoch=$(date +%s)
    end_iso=$(date -Is)
    echo "[harness] end_time=${end_iso}"
    echo "[harness] exit_status=${status}"
    echo "[harness] elapsed_sec=$((end_epoch - SCRIPT_START_EPOCH))"
    cleanup
    exit "${status}"
}

trap 'status=$?; echo "[harness] ERR status=${status} line=${LINENO} command=${BASH_COMMAND}"' ERR
trap finish_run EXIT

CFLAGS="-O2 -Wall -g"
BASE_CFLAGS="$CFLAGS"
PREWARM=${PREWARM:-0}
if [ "$PREWARM" = "1" ]; then
    BASE_CFLAGS="$BASE_CFLAGS -DUSE_PREWARM"
fi

FILTER_ROOT="${PROJ_ROOT}/src/filter"
if [ -x "$HOME/miniconda3/bin/python" ]; then
    PYTHON=${PYTHON:-"$HOME/miniconda3/bin/python"}
elif [ -x "$HOME/miniconda/bin/python" ]; then
    PYTHON=${PYTHON:-"$HOME/miniconda/bin/python"}
else
    PYTHON=${PYTHON:-"python3"}
fi

INSITU=${INSITU:-INSITU_DSUB_ASM}
case "${INSITU}" in
    INSITU_SUB_ASM) INSITU_TAG=${INSITU_TAG:-sub}; THEORETICAL_NSPV_FACTOR=${THEORETICAL_NSPV_FACTOR:-1} ;;
    INSITU_DSUB_ASM) INSITU_TAG=${INSITU_TAG:-dsub}; THEORETICAL_NSPV_FACTOR=${THEORETICAL_NSPV_FACTOR:-2} ;;
    *) INSITU_TAG=${INSITU_TAG:-custom}; THEORETICAL_NSPV_FACTOR=${THEORETICAL_NSPV_FACTOR:-1} ;;
esac

BINW_MIN=${BINW_MIN:-10}
P_LOW=${P_LOW:-0.01}
NSAMP=${NSAMP:-1000}
NSAMP_RATIO_LIST=${NSAMP_RATIO_LIST:-"0.5"}
SHUFFLE_COUNT=${SHUFFLE_COUNT:-3}
SHUFFLE_SEED=${SHUFFLE_SEED:-0614}
KERNEL_LIST=${KERNEL_LIST:-"jacobi2d5p tl_f90_cg_calc_w gs2d5p"}
NP_LIST=${NP_LIST:-"64"}
TIMER_LIST=${TIMER_LIST:-"cgt papi papix6 wtime"}
SIZE_LIST=${SIZE_LIST:-"64 128 256 512 1024 2048"}

if [ "${SMOKE:-0}" = "1" ]; then
    SHUFFLE_COUNT=1
    KERNEL_LIST="jacobi2d5p tl_f90_cg_calc_w gs2d5p"
    TIMER_LIST="cgt"
    SIZE_LIST="64"
    NP_LIST="64"
    NSAMP_RATIO_LIST="0.5"
    NSAMP=20
fi

ARCH=$(uname -m)
HOSTNAME=$(hostname -s 2>/dev/null || hostname)
DATE_BASE=${DATE_BASE:-$(date +%Y%m%d)}
DATE_STAMP=${DATE_STAMP:-$(date +%Y%m%d-%H%M%S)-stencil-${INSITU_TAG}}
DATA_FOLDER="${DATA_ROOT}/${DATE_BASE}/${HOSTNAME}/output_stencil_filt_${INSITU_TAG}/${DATE_STAMP}"
COMMIT_HASH=${COMMIT_HASH:-$(git -C "${PROJ_ROOT}" rev-parse HEAD 2>/dev/null || echo unknown)}

case $HOSTNAME in
    camd9554n1|camd9554n2) CPU_FREQ=3.1 ;;
    cgnr6760pn2) CPU_FREQ=2.2 ;;
    c920bn3) CPU_FREQ=2.9 ;;
    *) echo "Unknown hostname: $HOSTNAME"; exit 1 ;;
esac

: "${PAPI_HOME:?PAPI_HOME is not set}"
: "${OPENBLAS_HOME:?OPENBLAS_HOME is not set}"
if [ "$ARCH" = "x86_64" ]; then
    : "${LIKWID_HOME:?LIKWID_HOME is not set}"
fi

start_run_log
cleanup_residual_processes
initialize "$CPU_FREQ"
CPU_FREQ_KHZ_REAL=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq)
NSPV=$(echo "scale=12; 1000000 / $CPU_FREQ_KHZ_REAL" | bc)
EFFECTIVE_NSPV=$(echo "scale=12; ${NSPV} * ${THEORETICAL_NSPV_FACTOR}" | bc)

case $ARCH in
    x86_64) TIMER_LIST="$TIMER_LIST tsc tsc_native" ;;
    aarch64) TIMER_LIST="$TIMER_LIST cntvct cntvcto" ;;
    *) echo "Unsupported architecture: $ARCH"; exit 1 ;;
esac
[ "${SMOKE:-0}" = "1" ] && TIMER_LIST="cgt"

printf 'ARCH: %s\nHOSTNAME: %s\nDATA_FOLDER: %s\nKERNEL_LIST: %s\nNP_LIST: %s\nTIMER_LIST: %s\nSIZE_LIST: %s\nINSITU: %s\nINSITU_TAG: %s\nSHUFFLE_COUNT: %s\nSHUFFLE_SEED: %s\nCOMMIT_HASH: %s\nNSPV: %s\nTHEORETICAL_NSPV_FACTOR: %s\nEFFECTIVE_NSPV: %s\n' "$ARCH" "$HOSTNAME" "$DATA_FOLDER" "$KERNEL_LIST" "$NP_LIST" "$TIMER_LIST" "$SIZE_LIST" "$INSITU" "$INSITU_TAG" "$SHUFFLE_COUNT" "$SHUFFLE_SEED" "$COMMIT_HASH" "$NSPV" "$THEORETICAL_NSPV_FACTOR" "$EFFECTIVE_NSPV"

mkdir -p "$DATA_FOLDER"
write_root_meta "$DATA_FOLDER"
rm -f ./*.x ./*.csv
mpicc -O2 -Wall -o "${FILTER_ROOT}/filt.x" "${FILTER_ROOT}/filt_v2.0608.c"

for kernel in $KERNEL_LIST; do
    for timer in $TIMER_LIST; do
        timer_cflags="$BASE_CFLAGS"
        case $timer in
            papi|papix6) timer_cflags="$timer_cflags -I${PAPI_HOME}/include -L${PAPI_HOME}/lib -lpapi" ;;
            likwid) timer_cflags="$timer_cflags -DLIKWID_PERFMON -I${LIKWID_HOME}/include -L${LIKWID_HOME}/lib -llikwid" ;;
        esac
        mpicc -o "${kernel}_${timer}.x" "${kernel}.c" $timer_cflags -D"$INSITU" -DTIMING "-DUSE_${timer^^}" -I"${OPENBLAS_HOME}/include" -L"${OPENBLAS_HOME}/lib"
        mpicc -o "${kernel}_${timer}_tf.x" "${kernel}.c" $timer_cflags -D"$INSITU" -DSTAGE_TF -DTIMING "-DUSE_${timer^^}" -I"${OPENBLAS_HOME}/include" -L"${OPENBLAS_HOME}/lib"
    done
done

for shuffle_idx in $(seq 0 $((SHUFFLE_COUNT - 1))); do
    shuffle_id="shuffle${shuffle_idx}"
    shuffle_dir="${DATA_FOLDER}/${shuffle_id}"
    shuffled_timers="$(shuffle_timer_list "${shuffle_idx}" ${TIMER_LIST})"
    write_shuffle_meta "${shuffle_dir}" "${shuffle_id}" "${shuffled_timers}"
    echo "${shuffle_id}_timer_list=${shuffled_timers}" | tee -a "${DATA_FOLDER}/shuffle.log"
    for kernel in $KERNEL_LIST; do
        for timer in $shuffled_timers; do
            for np in $NP_LIST; do
                for size in $SIZE_LIST; do
                    tm_dir="${shuffle_dir}/${kernel}_${timer}_np${np}_size${size}"
                    rm -rf "$tm_dir" ./*.csv
                    mkdir -p "$tm_dir"
                    if [ "$timer" = "likwid" ]; then
                        run_cmd=(likwid-mpirun -mpi openmpi -np "$np" -g "${LIKWID_GROUP:-L3}" -m)
                    else
                        run_cmd=(mpirun -np "$np" --map-by core --bind-to core)
                    fi
                    "${run_cmd[@]}" "./${kernel}_${timer}.x" "${size}" 0 1
                    mv ./*.csv "$tm_dir/"
                    for nsamp_ratio in ${NSAMP_RATIO_LIST}; do
                        nsamp_tf=$("${PYTHON}" "${FILTER_ROOT}/get_quantile.py" "${tm_dir}" 1 "${nsamp_ratio}" "${EFFECTIVE_NSPV}")
                        te_dir="${tm_dir}_nsampRatio${nsamp_ratio}_nsamp${nsamp_tf}_tf"
                        res_dir="${tm_dir}_nsampRatio${nsamp_ratio}_nsamp${nsamp_tf}_filt"
                        rm -rf "$te_dir" "$res_dir" ./*.csv
                        mkdir -p "$te_dir" "$res_dir"
                        "${run_cmd[@]}" "./${kernel}_${timer}_tf.x" "${size}" 0 1 "${nsamp_tf}"
                        mv ./*.csv "$te_dir/"
                        if [ "$timer" = "likwid" ]; then
                            awk -F, '{if ($2 != 0) ok=1} END{exit ok ? 0 : 1}' "$tm_dir"/*.csv || { echo "Invalid LIKWID tm: all timing values are zero"; exit 1; }
                            awk -F, '{if ($3 != 0) ok=1} END{exit ok ? 0 : 1}' "$te_dir"/*.csv || { echo "Invalid LIKWID tf: all timing values are zero"; exit 1; }
                        fi
                        "${PYTHON}" "${FILTER_ROOT}/get_met.py" "$tm_dir" 1
                        "${PYTHON}" "${FILTER_ROOT}/get_tf.py" "$te_dir" 1 2 "${EFFECTIVE_NSPV}"
                        binw=$("${PYTHON}" "${FILTER_ROOT}/get_binw.py" "$tm_dir" 1 "$BINW_MIN")
                        "${FILTER_ROOT}/filt.x" -w "$binw" -n 100000 -l "$P_LOW" -x 0.005 -y 0.005 -z 0.005
                        {
                            echo "binary_commit=${COMMIT_HASH}"
                            echo "shuffle_id=${shuffle_id}"
                            echo "shuffle_seed=${SHUFFLE_SEED}"
                            echo "shuffle_timer_list=${shuffled_timers}"
                            echo "insitu=${INSITU}"
                            echo "insitu_tag=${INSITU_TAG}"
                            echo "nspv=${NSPV}"
                            echo "theoretical_nspv_factor=${THEORETICAL_NSPV_FACTOR}"
                            echo "effective_nspv=${EFFECTIVE_NSPV}"
                        } > "${res_dir}/meta.txt"
                        mv met.csv tf.csv tr_hist.csv tm_hist.csv sim_cdf.csv er.out ep.out wd.out calc_tr_residual.0608.csv "$res_dir/"
                    done
                done
            done
        done
    done
done
