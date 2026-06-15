#!/bin/bash
set -u

SCRIPT_START_EPOCH="$(date +%s)"
SCRIPT_START_ISO="$(date -Is)"
SCRIPT_CMD="$0${*:+ $*}"
RUN_LOG_STARTED=0
RUN_LOG_FILE=""

initialize(){
    return 0
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
    return 0
    sudo cpupower frequency-set -g schedutil
    if [ -e /sys/devices/system/cpu/intel_pstate/no_turbo ]; then
        echo 0 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo >/dev/null 2>/dev/null || true
    fi
    if [ -e /sys/devices/system/cpu/cpufreq/boost ]; then
        echo 1 | sudo tee /sys/devices/system/cpu/cpufreq/boost >/dev/null 2>/dev/null || true
    fi
}

choose_python(){
    if [ -x "${HOME}/miniconda3/bin/python" ]; then
        echo "${HOME}/miniconda3/bin/python"
    elif [ -x "${HOME}/miniconda/bin/python" ]; then
        echo "${HOME}/miniconda/bin/python"
    else
        echo python3
    fi
}

cpu_freq_for_host(){
    case "$1" in
        camd9554n1) echo 3.1 ;;
        cgnr6760pn2) echo 2.2 ;;
        c920bn3) echo 2.9 ;;
        *) echo "ERROR: unknown hostname: $1" >&2; return 1 ;;
    esac
}

timer_list_for_arch(){
    case "$1" in
        x86_64) echo "${TIMER_LIST:-clock_gettime mpi_wtime tsc tsc_asym papi papix6 likwid}" ;;
        aarch64) echo "${TIMER_LIST:-clock_gettime mpi_wtime cntvct cntvct_fence cntvcto papi papix6}" ;;
        *) echo "${TIMER_LIST:-clock_gettime mpi_wtime papi papix6}" ;;
    esac
}

filter_timers(){
    local out=""
    for timer in "$@"; do
        case "${timer}" in
            papi|papix6) [ "${USE_PAPI:-0}" = "1" ] && out="${out} ${timer}" ;;
            likwid) [ "${USE_LIKWID:-0}" = "1" ] && out="${out} ${timer}" ;;
            *) out="${out} ${timer}" ;;
        esac
    done
    echo "${out}"
}

cleanup_residual_processes(){
    local pattern='partes-mpi.x|detecing-mpi.0614.x|mpirun|prterun|orted|prted|run_entry_detecting|run_entry_filt'
    echo "[harness] residual processes before cleanup:"
    pgrep -af "${pattern}" | awk -v self="$$" -v parent="$PPID" '$1 != self && $1 != parent {print}' || true
    pgrep -af "${pattern}" | awk -v self="$$" -v parent="$PPID" '$1 != self && $1 != parent {print $1}' | xargs -r kill -9 || true
    sleep 1
    echo "[harness] residual processes after cleanup:"
    pgrep -af "${pattern}" | awk -v self="$$" -v parent="$PPID" '$1 != self && $1 != parent {print}' || true
}

start_run_log(){
    local data_folder=$1
    RUN_LOG_FILE="${data_folder}/run.log"
    exec > >(tee -a "${RUN_LOG_FILE}") 2>&1
    RUN_LOG_STARTED=1
    echo "run_start_time=${SCRIPT_START_ISO}"
    echo "run_command=${SCRIPT_CMD}"
    echo "run_cwd=$(pwd)"
    echo "run_pid=$$"
    echo "run_ppid=$PPID"
    echo "run_log=${RUN_LOG_FILE}"
}

finish_run(){
    local status=$?
    local end_epoch
    end_epoch="$(date +%s)"
    if [ "${RUN_LOG_STARTED:-0}" = "1" ]; then
        echo "run_end_time=$(date -Is)"
        echo "exit_status=${status}"
        echo "elapsed_sec=$((end_epoch - SCRIPT_START_EPOCH))"
    fi
    cleanup
    exit "${status}"
}

shuffle_list(){
    local shuffle_id=$1
    shift
    python3 - "${SHUFFLE_SEED}" "${shuffle_id}" "$@" <<'PY_SHUFFLE'
import random
import sys
seed = sys.argv[1]
shuffle_id = int(sys.argv[2])
items = sys.argv[3:]
rng = random.Random(f"{seed}:{shuffle_id}:{' '.join(items)}")
rng.shuffle(items)
print(" ".join(items))
PY_SHUFFLE
}

gen_walklist(){
    local out=$1
    local tbase=$2
    local num_walk=$3
    local py
    py="$(choose_python)"
    mkdir -p "$(dirname "${out}")"
    echo "Generating ${out}"
    "${py}" "${SCRIPT_DIR}/generate_walking_list.py" \
        --tbase "${tbase}" \
        --dist 0 \
        --num_walk "${num_walk}" \
        --output_file "${out}"
}

read_walks(){
    local walk_list=$1
    awk 'NF && $1 !~ /^(ns|walk_idx|ta|#)/ {n++} END {print n+0}' "${walk_list}"
}

run_one(){
    local combo_dir=$1
    local walk_list=$2
    local np=$3
    local timer=$4
    local fkern=$5
    local fsize_kib=$6
    local rkern=$7
    local rsize_kib=$8
    local interval_ns=$9
    local shuffle_id=${10}
    local shuffle_fkern_list=${11}

    mkdir -p "${combo_dir}"
    {
        echo "expr_name=${EXPR_NAME}"
        echo "host=${HOSTNAME}"
        echo "arch=${ARCH}"
        echo "np=${np}"
        echo "timer=${timer}"
        echo "fkern=${fkern}"
        echo "rkern=${rkern}"
        echo "fsize_kib=${fsize_kib}"
        echo "rsize_kib=${rsize_kib}"
        echo "interval_ns=${interval_ns}"
        echo "ntests=${NTESTS}"
        echo "ntiles=${NTILES}"
        echo "cut_p=${CUT_P}"
        echo "gauge=${GAUGE}"
        echo "binary=${BINARY}"
        echo "binary_commit=${COMMIT_HASH}"
        echo "shuffle_id=${shuffle_id}"
        echo "shuffle_seed=${SHUFFLE_SEED}"
        echo "shuffle_fkern_list=${shuffle_fkern_list}"
        echo "walk_list=${walk_list}"
        sha256sum "${walk_list}" 2>/dev/null || true
    } > "${combo_dir}/meta.txt"

    local walk_id=0 c1 c2 c3 extra ta tb run_dir
    while IFS=, read -r c1 c2 c3 extra; do
        c1="${c1%$'\r'}"; c2="${c2%$'\r'}"; c3="${c3%$'\r'}"
        [ -z "${c1}" ] && continue
        case "${c1}" in ns|walk_idx|ta|\#*) continue ;; esac
        if [ -n "${c3}" ]; then
            walk_id="${c1}"; ta="${c2}"; tb="${c3}"
        else
            walk_id=$((walk_id + 1)); ta="${c1}"; tb="${c1}"
        fi

        run_dir="$(printf "%s/w%04d_ta%s" "${combo_dir}" "${walk_id}" "${ta}")"
        mkdir -p "${run_dir}"
        {
            cat "${combo_dir}/meta.txt"
            echo "walk_id=${walk_id}"
            echo "mu_ns=${ta}"
            echo "ta=${ta}"
        } > "${run_dir}/meta.txt"

        echo ">>> expr=${EXPR_NAME} np=${np} timer=${timer} interval=${interval_ns}ns fkern=${fkern} fsize=${fsize_kib}KiB rkern=${rkern} walk=${walk_id}"
        (
            cd "${run_dir}" || exit 1
            { echo "binary_commit=${COMMIT_HASH}"; \
              echo "shuffle_id=${shuffle_id}"; \
              echo "shuffle_seed=${SHUFFLE_SEED}"; \
              echo "shuffle_fkern_list=${shuffle_fkern_list}"; \
            mpirun --map-by core --bind-to core -np "${np}" "${BINARY}" \
                --ta "${ta}" --tb "${ta}" \
                --ntests "${NTESTS}" --ntiles "${NTILES}" --cut-p "${CUT_P}" \
                --gauge "${GAUGE}" --timer "${timer}" \
                --fkern-a "${fkern}" --fsize-a "${fsize_kib}" \
                --rkern-a "${rkern}" --rsize-a "${rsize_kib}"; } \
                > run.log 2>&1 < /dev/null
        )
    done < "${walk_list}"
}

main_preamble(){
    if [ "$#" -lt 1 ]; then
        echo "Usage:"
        echo "  $0 gen [NUM_WALK]"
        echo "  $0 detect [WALK_LIST_CSV|all]"
        exit 1
    fi

    MODE="$1"
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    PROJ_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
    WALK_ROOT="${WALK_ROOT:-${SCRIPT_DIR}/walklists}"
    if [ "$MODE" = "gen" ]; then NUM_WALK="${2:-${NUM_WALK:-20}}"; else NUM_WALK="${NUM_WALK:-20}"; fi
}

main_preamble "$@"

EXPR_NAME="detecting.expr4.frkern"
OUT_KIND="expr4frkern"
TBASE_NS="${TBASE_NS:-1000}"
FSIZE_KIB="${FSIZE_KIB:-0}"
FKERN_LIST="${FKERN_LIST:-copy add scale triad pow dgemm}"
RKERN_LIST="${RKERN_LIST:-copy add scale triad pow dgemm}"
RSIZE_KIB="${RSIZE_KIB:-0}"
SHUFFLE_COUNT="${SHUFFLE_COUNT:-3}"
SHUFFLE_SEED="${SHUFFLE_SEED:-0614}"

walk_default(){ echo "${WALK_ROOT}/detecting_expr4_frkern_Normal_n${NUM_WALK}_tbase${TBASE_NS}.csv"; }

do_gen(){
    gen_walklist "$(walk_default)" "${TBASE_NS}" "${NUM_WALK}"
}

do_detect(){
    local walk_list="${2:-$(walk_default)}"
    [ "$walk_list" = "all" ] && walk_list="$(walk_default)"
    [ -s "${walk_list}" ] || { echo "ERROR: missing walk list: ${walk_list}" >&2; exit 1; }
    local walk_count walk_tag data_folder shuffle_idx shuffle_id shuffled_fkerns
    walk_count="$(read_walks "${walk_list}")"
    walk_tag="${WALK_TAG:-walk${walk_count}}"
    data_folder="${DATA_ROOT}/${DATE_BASE}/${HOSTNAME}/outputDetecting/${OUT_KIND}/${walk_tag}/${DATE_STAMP}"
    mkdir -p "${data_folder}/walklists"
    cp -f "$0" "${data_folder}/$(basename "$0")"
    cp -f "${walk_list}" "${data_folder}/walklists/$(basename "${walk_list}")"
    echo "DATA_FOLDER: ${data_folder}"
    start_run_log "${data_folder}"

    {
        echo "expr_name=${EXPR_NAME}"
        echo "run_start_time=${SCRIPT_START_ISO}"
        echo "run_command=${SCRIPT_CMD}"
        echo "run_cwd=$(pwd)"
        echo "run_pid=$$"
        echo "run_ppid=$PPID"
        echo "host=${HOSTNAME}"
        echo "arch=${ARCH}"
        echo "date_base=${DATE_BASE}"
        echo "date_stamp=${DATE_STAMP}"
        echo "walk_tag=${walk_tag}"
        echo "walk_count=${walk_count}"
        echo "data_folder=${data_folder}"
        echo "binary=${BINARY}"
        echo "binary_commit=${COMMIT_HASH}"
        echo "gauge=${GAUGE}"
        echo "timer_list=${TIMER_LIST}"
        echo "np_list=${NP_LIST}"
        echo "ntests=${NTESTS}"
        echo "ntiles=${NTILES}"
        echo "cut_p=${CUT_P}"
        echo "shuffle_seed=${SHUFFLE_SEED}"
        echo "shuffle_count=${SHUFFLE_COUNT}"
        echo "original_fkern_list=${FKERN_LIST}"
        echo "rkern_list=${RKERN_LIST}"
    } > "${data_folder}/meta.txt"
    cp -f "${data_folder}/meta.txt" "${data_folder}/shuffle.log"

    for shuffle_idx in $(seq 0 $((SHUFFLE_COUNT - 1))); do
        shuffle_id="shuffle${shuffle_idx}"
        shuffled_fkerns="$(shuffle_list "${shuffle_idx}" ${FKERN_LIST})"
        echo "${shuffle_id}_fkern_list=${shuffled_fkerns}" | tee -a "${data_folder}/shuffle.log"
        
        for np in ${NP_LIST}; do
            for timer in ${TIMER_LIST}; do
                for fkern in ${shuffled_fkerns}; do
                    for rkern in ${RKERN_LIST}; do
                        combo="${data_folder}/${EXPR_NAME}.${shuffle_id}/np${np}/${timer}/interval${TBASE_NS}_fkern${fkern}_rkern${rkern}_fsize${FSIZE_KIB}_rsize${RSIZE_KIB}"
                        run_one "${combo}" "${walk_list}" "${np}" "${timer}" "${fkern}" "${FSIZE_KIB}" "${rkern}" "${RSIZE_KIB}" "${TBASE_NS}" "${shuffle_id}" "${shuffled_fkerns}"
                    done
                done
            done
        done
    done
}

if [ "${MODE}" = "gen" ]; then
    do_gen
    exit 0
fi

if [ "${MODE}" != "detect" ]; then
    echo "ERROR: unknown mode: ${MODE}" >&2
    exit 1
fi

set +u
cd "${PROJ_ROOT}" || exit 1
source "${PROJ_ROOT}/env.bash"
set -u

HOSTNAME="$(hostname -s 2>/dev/null || hostname)"
ARCH="$(uname -m)"
CPU_FREQ="$(cpu_freq_for_host "${HOSTNAME}")" || exit 1
DATA_ROOT="${DATA_ROOT:-${HOME}/code/data}"
DATE_BASE="${DATE_BASE:-$(date +%Y%m%d)}"
DATE_STAMP="${DATE_STAMP:-$(date +%Y%m%d-%H%M%S)}"
NP_LIST="${NP_LIST:-64}"
NTESTS="${NTESTS:-100}"
NTILES="${NTILES:-100}"
CUT_P="${CUT_P:-0.995}"
GAUGE="${GAUGE:-sub_scalar}"
BINARY="${BINARY:-${PROJ_ROOT}/src/partes/detecing-mpi.0614.x}"
COMMIT_HASH="${COMMIT_HASH:-$(git -C "${PROJ_ROOT}" rev-parse HEAD 2>/dev/null || echo unknown)}"
TIMER_LIST="$(filter_timers $(timer_list_for_arch "${ARCH}"))"

cleanup_residual_processes
initialize "${CPU_FREQ}"
trap 'echo "ERR"' ERR
trap finish_run EXIT

if [ "${BUILD:-1}" = "1" ]; then
    MPI_CC="${MPI_CC:-mpicc}"
    if [ -n "${MPI_HOME:-}" ] && [ -x "${MPI_HOME}/bin/mpicc" ]; then
        MPI_CC="${MPI_HOME}/bin/mpicc"
    fi
    make -C "${PROJ_ROOT}/src/partes" clean
    make -C "${PROJ_ROOT}/src/partes" -j detecing-mpi.0614.x CC="${MPI_CC}" USE_PAPI="${USE_PAPI:-0}" USE_LIKWID="${USE_LIKWID:-0}"
fi

if [ ! -x "${BINARY}" ]; then
    echo "ERROR: binary not executable: ${BINARY}" >&2
    exit 1
fi

do_detect "$@"
