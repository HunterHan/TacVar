#!/bin/bash -x
set -euo pipefail

SCRIPT_START_EPOCH=$(date +%s)
SCRIPT_START_ISO=$(date -Is)
SCRIPT_CMD="$0${*:+ $*}"
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
    PYTHON=${PYTHON:-"$HOME/miniconda3/bin/python"}
elif [ -x "$HOME/miniconda/bin/python" ]; then
    PYTHON=${PYTHON:-"$HOME/miniconda/bin/python"}
else
    PYTHON=${PYTHON:-"python3"}
fi

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
    local pattern='gemm_timer_diag_.*\.x|filt\.x|mpirun|prterun|orted|prted|run_gemm_timer_diag'
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
    pgrep -af "${pattern}" | awk -v skip="$skip_pids" 'index(skip, " " $1 " ") == 0 {print}' || true
    pgrep -af "${pattern}" | awk -v skip="$skip_pids" 'index(skip, " " $1 " ") == 0 {print $1}' | xargs -r kill -9 || true
}

shuffle_list(){
    local shuffle_idx=$1
    shift
    "${PYTHON}" - "${SHUFFLE_SEED}" "${shuffle_idx}" "$@" <<'PY'
import random, sys
rng = random.Random(f"{sys.argv[1]}:{sys.argv[2]}:{' '.join(sys.argv[3:])}")
items = sys.argv[3:]
rng.shuffle(items)
print(" ".join(items))
PY
}

macro_for_variant(){
    case "$1" in
        tsc_current) echo USE_TSC ;;
        tsc_native_current) echo USE_TSC_NATIVE ;;
        tsc_cpuid_mem) echo USE_TSC ;;
        tsc_lfence_mem) echo USE_TSC_FENCE ;;
        tsc_pre_cgt) echo USE_TSC ;;
        papi_time_only) echo USE_PAPI ;;
        papix6_current) echo USE_PAPIX6 ;;
        papix6_read_before_ns0) echo USE_PAPIX6 ;;
        papix6_no_read) echo USE_PAPIX6 ;;
        cgt_current) echo USE_CGT ;;
        wtime_current) echo USE_WTIME ;;
        cntvct_current) echo USE_CNTVCT ;;
        cntvcto_current) echo USE_CNTVCTO ;;
        *) echo "unknown variant $1" >&2; exit 2 ;;
    esac
}

patch_variant_source(){
    local variant=$1
    local src=$2
    case "$variant" in
        tsc_cpuid_mem)
            "${PYTHON}" - "$src" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1])
s = p.read_text()
old = ': "%rax", "%rbx", "%rcx", "%rdx");'
n = s.count(old)
if n < 2:
    raise SystemExit(f"expected at least 2 TSC clobber sites, found {n}")
s = s.replace(old, ': "memory", "%rax", "%rbx", "%rcx", "%rdx");')
p.write_text(s)
PY
            ;;
        tsc_pre_cgt)
            "${PYTHON}" - "$src" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1])
s = p.read_text()
old = '            tsc_start(&ns0);'
new = '            struct timespec gemm_diag_dummy_ts;\n            clock_gettime(CLOCK_MONOTONIC_RAW, &gemm_diag_dummy_ts);\n            tsc_start(&ns0);'
n = s.count(old)
if n < 2:
    raise SystemExit(f"expected at least 2 tsc_start sites, found {n}")
s = s.replace(old, new)
p.write_text(s)
PY
            ;;
        papix6_read_before_ns0)
            "${PYTHON}" - "$src" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1])
s = p.read_text()
old = '''#elif USE_PAPIX6
            ns0 = PAPI_get_real_nsec();
            PAPI_read(eventset, ev_vals_0);
'''
new = '''#elif USE_PAPIX6
            PAPI_read(eventset, ev_vals_0);
            ns0 = PAPI_get_real_nsec();
'''
n = s.count(old)
if n < 2:
    raise SystemExit(f"expected at least 2 PAPIX6 start sites, found {n}")
s = s.replace(old, new)
p.write_text(s)
PY
            ;;
        papix6_no_read)
            "${PYTHON}" - "$src" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1])
s = p.read_text()
old_start = '''#elif USE_PAPIX6
            ns0 = PAPI_get_real_nsec();
            PAPI_read(eventset, ev_vals_0);
'''
new_start = '''#elif USE_PAPIX6
            ns0 = PAPI_get_real_nsec();
'''
old_stop = '''#elif USE_PAPIX6
            ns1 = PAPI_get_real_nsec();
            PAPI_read(eventset, ev_vals_1);
            for (int iev = 0; iev < nev; iev ++) {
                p_ev[it * narr * nev + i * nev + iev] = (int64_t)(ev_vals_1[iev] - ev_vals_0[iev]);
            }
            p_ns[it*narr+i] = (uint64_t)(ns1 - ns0);
'''
new_stop = '''#elif USE_PAPIX6
            ns1 = PAPI_get_real_nsec();
            for (int iev = 0; iev < nev; iev ++) {
                p_ev[it * narr * nev + i * nev + iev] = 0;
            }
            p_ns[it*narr+i] = (uint64_t)(ns1 - ns0);
'''
ns = s.count(old_start)
ne = s.count(old_stop)
if ns < 2 or ne < 1:
    raise SystemExit(f"expected PAPIX6 start/stop sites, found start={ns} stop={ne}")
s = s.replace(old_start, new_start).replace(old_stop, new_stop)
p.write_text(s)
PY
            ;;
    esac
}

needs_papi(){
    case "$1" in
        papi_time_only|papix6_current|papix6_read_before_ns0|papix6_no_read) return 0 ;;
        *) return 1 ;;
    esac
}

finish_run(){
    local status=$?
    local end_epoch
    end_epoch=$(date +%s)
    echo "[harness] end_time=$(date -Is)"
    echo "[harness] exit_status=${status}"
    echo "[harness] elapsed_sec=$((end_epoch - SCRIPT_START_EPOCH))"
    cleanup
    exit "${status}"
}

trap 'status=$?; echo "[harness] ERR status=${status} line=${LINENO} command=${BASH_COMMAND}"' ERR
trap finish_run EXIT

CFLAGS="${DIAG_CFLAGS:--O2 -Wall -g}"
BINW_MIN=${BINW_MIN:-10}
P_LOW=${P_LOW:-0.01}
NSAMP=${NSAMP:-1000}
NSAMP_RATIO_LIST=${NSAMP_RATIO_LIST:-"0.5"}
SHUFFLE_COUNT=${SHUFFLE_COUNT:-3}
SHUFFLE_SEED=${SHUFFLE_SEED:-0623}
NP_LIST=${NP_LIST:-"64"}
SIZE_LIST=${SIZE_LIST:-"32 64 128 256"}
INSITU=${INSITU:-INSITU_DSUB_ASM}
THEORETICAL_NSPV_FACTOR=${THEORETICAL_NSPV_FACTOR:-2}

ARCH=$(uname -m)
HOSTNAME=$(hostname -s 2>/dev/null || hostname)
DATE_BASE=${DATE_BASE:-20260623_timer_diag}
DATE_STAMP=${DATE_STAMP:-20260623-gemm-timer-diag-v2}
DATA_FOLDER="${DATA_ROOT}/${DATE_BASE}/${HOSTNAME}/output_gemm_timer_diag/${DATE_STAMP}"
COMMIT_HASH=${COMMIT_HASH:-$(git -C "${PROJ_ROOT}" rev-parse HEAD 2>/dev/null || echo unknown)}

case "$HOSTNAME" in
    camd9554n1|camd9554n2) CPU_FREQ=3.1 ;;
    cgnr6760pn2) CPU_FREQ=2.2 ;;
    c920bn3) CPU_FREQ=2.9 ;;
    *) echo "Unknown hostname: $HOSTNAME"; exit 1 ;;
esac

: "${PAPI_HOME:?PAPI_HOME is not set}"
: "${OPENBLAS_HOME:?OPENBLAS_HOME is not set}"

case "$ARCH" in
    x86_64)
        VARIANT_LIST=${VARIANT_LIST:-"tsc_current tsc_native_current tsc_cpuid_mem tsc_lfence_mem tsc_pre_cgt papi_time_only papix6_current papix6_read_before_ns0 papix6_no_read cgt_current wtime_current"}
        ;;
    aarch64)
        VARIANT_LIST=${VARIANT_LIST:-"cntvcto_current cntvct_current papi_time_only papix6_current papix6_read_before_ns0 cgt_current wtime_current"}
        ;;
    *) echo "Unsupported arch: $ARCH"; exit 1 ;;
esac

mkdir -p "${DATA_FOLDER}"
exec > >(tee -a "${DATA_FOLDER}/run.log") 2>&1
echo "[harness] start_time=${SCRIPT_START_ISO}"
echo "[harness] command=${SCRIPT_CMD}"
echo "[harness] hostname=${HOSTNAME} arch=${ARCH}"

cleanup_residual_processes
initialize "$CPU_FREQ"
CPU_FREQ_KHZ_REAL=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq)
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
    echo "variant_list=${VARIANT_LIST}"
    echo "size_list=${SIZE_LIST}"
    echo "np_list=${NP_LIST}"
    echo "shuffle_count=${SHUFFLE_COUNT}"
    echo "shuffle_seed=${SHUFFLE_SEED}"
    echo "nsamp=${NSAMP}"
    echo "nsamp_ratio_list=${NSAMP_RATIO_LIST}"
    echo "nspv=${NSPV}"
    echo "effective_nspv=${EFFECTIVE_NSPV}"
    echo "polybench_gemm_sha256=$(sha256sum "${STENCIL_ROOT}/polybench_gemm.c" | awk '{print $1}')"
    echo "diag_source_sha256=$(sha256sum "${STENCIL_ROOT}/gemm_timer_diag.c" | awk '{print $1}')"
    echo "diag_build_policy=variants_generated_from_polybench_gemm_c_with_minimal_timer_patch"
    echo "runner_sha256=$(sha256sum "$0" | awk '{print $1}')"
    echo "filter_sha256=$(sha256sum "${FILTER_ROOT}/filt_v2.0608.c" | awk '{print $1}')"
    echo "git_status_begin"
    git -C "${PROJ_ROOT}" status --short || true
    echo "git_status_end"
} > "${DATA_FOLDER}/meta.txt"
cp -f "$0" "${DATA_FOLDER}/$(basename "$0")"
lscpu > "${DATA_FOLDER}/lscpu.txt" 2>&1 || true
sudo cpupower frequency-info > "${DATA_FOLDER}/cpupower_frequency_info.txt" 2>&1 || true
sha256sum "${STENCIL_ROOT}/polybench_gemm.c" "${STENCIL_ROOT}/gemm_timer_diag.c" "$0" "${FILTER_ROOT}/filt_v2.0608.c" > "${DATA_FOLDER}/source_manifest.txt"

cd "${STENCIL_ROOT}"
rm -f ./*.x ./*.csv
mpicc -O2 -Wall -o "${FILTER_ROOT}/filt.x" "${FILTER_ROOT}/filt_v2.0608.c"

compile_one(){
    local variant=$1
    local macro
    local flags
    local src
    macro=$(macro_for_variant "$variant")
    src="gemm_timer_diag_build_${variant}.c"
    cp -f polybench_gemm.c "$src"
    patch_variant_source "$variant" "$src"
    sha256sum "$src" > "${DATA_FOLDER}/source_${variant}.sha256"
    flags="${CFLAGS} -D${INSITU} -DTIMING -D${macro}"
    if needs_papi "$variant"; then
        flags="${flags} -I${PAPI_HOME}/include -L${PAPI_HOME}/lib -lpapi"
    fi
    echo "mpicc -std=c11 -o gemm_timer_diag_${variant}.x ${src} ${flags} -I${OPENBLAS_HOME}/include -L${OPENBLAS_HOME}/lib" >> "${DATA_FOLDER}/compile_commands.txt"
    mpicc -std=c11 -o "gemm_timer_diag_${variant}.x" "$src" ${flags} -I"${OPENBLAS_HOME}/include" -L"${OPENBLAS_HOME}/lib"
    echo "mpicc -std=c11 -o gemm_timer_diag_${variant}_tf.x ${src} ${flags} -DSTAGE_TF -I${OPENBLAS_HOME}/include -L${OPENBLAS_HOME}/lib" >> "${DATA_FOLDER}/compile_commands.txt"
    mpicc -std=c11 -o "gemm_timer_diag_${variant}_tf.x" "$src" ${flags} -DSTAGE_TF -I"${OPENBLAS_HOME}/include" -L"${OPENBLAS_HOME}/lib"
    mkdir -p "${DATA_FOLDER}/generated_sources"
    cp -f "$src" "${DATA_FOLDER}/generated_sources/${src}"
}

if [ "$ARCH" = "x86_64" ]; then
    mpicc -std=c11 -o gemm_timer_diag_sanity.x gemm_timer_diag.c ${CFLAGS} -DSANITY_ALL -DTIMING -DTIMER_NAME=sanity -I"${PAPI_HOME}/include" -L"${PAPI_HOME}/lib" -lpapi
    mpirun -np 1 --map-by core --bind-to core ./gemm_timer_diag_sanity.x --sanity
    mkdir -p "${DATA_FOLDER}/sanity"
    mv ./*sanity*.csv "${DATA_FOLDER}/sanity/"
fi

for variant in ${VARIANT_LIST}; do
    compile_one "$variant"
done

for variant in ${VARIANT_LIST}; do
    objdump -d "gemm_timer_diag_${variant}.x" > "${DATA_FOLDER}/objdump_${variant}.txt" || true
done

for shuffle_idx in $(seq 0 $((SHUFFLE_COUNT - 1))); do
    shuffle_id="shuffle${shuffle_idx}"
    shuffle_dir="${DATA_FOLDER}/${shuffle_id}"
    mkdir -p "$shuffle_dir"
    shuffled_variants="$(shuffle_list "${shuffle_idx}" ${VARIANT_LIST})"
    echo "${shuffle_id}_variant_list=${shuffled_variants}" | tee -a "${DATA_FOLDER}/shuffle.log"
    for variant in ${shuffled_variants}; do
        for np in ${NP_LIST}; do
            for size in ${SIZE_LIST}; do
                tm_dir="${shuffle_dir}/gemm_timer_diag_${variant}_np${np}_size${size}"
                rm -rf "$tm_dir" ./*.csv
                mkdir -p "$tm_dir"
                mpirun -np "$np" --map-by core --bind-to core "./gemm_timer_diag_${variant}.x" "$size" 0 1
                mv ./*.csv "$tm_dir/"
                for nsamp_ratio in ${NSAMP_RATIO_LIST}; do
                    nsamp_tf=$("${PYTHON}" "${FILTER_ROOT}/get_quantile.py" "$tm_dir" 1 "$nsamp_ratio" "${EFFECTIVE_NSPV}")
                    te_dir="${tm_dir}_nsampRatio${nsamp_ratio}_nsamp${nsamp_tf}_tf"
                    res_dir="${tm_dir}_nsampRatio${nsamp_ratio}_nsamp${nsamp_tf}_filt"
                    rm -rf "$te_dir" "$res_dir" ./*.csv
                    mkdir -p "$te_dir" "$res_dir"
                    mpirun -np "$np" --map-by core --bind-to core "./gemm_timer_diag_${variant}_tf.x" "$size" 0 1 "$nsamp_tf"
                    mv ./*.csv "$te_dir/"
                    "${PYTHON}" "${FILTER_ROOT}/get_met.py" "$tm_dir" 1
                    "${PYTHON}" "${FILTER_ROOT}/get_tf.py" "$te_dir" 1 2 "${EFFECTIVE_NSPV}"
                    binw=$("${PYTHON}" "${FILTER_ROOT}/get_binw.py" "$tm_dir" 1 "$BINW_MIN")
                    "${FILTER_ROOT}/filt.x" -w "$binw" -n 100000 -l "$P_LOW" -x 0.005 -y 0.005 -z 0.005
                    {
                        echo "variant=${variant}"
                        echo "binary_commit=${COMMIT_HASH}"
                        echo "shuffle_id=${shuffle_id}"
                        echo "size=${size}"
                        echo "np=${np}"
                        echo "nsamp_tf=${nsamp_tf}"
                        echo "effective_nspv=${EFFECTIVE_NSPV}"
                    } > "${res_dir}/meta.txt"
                    mv met.csv tf.csv tr_hist.csv tm_hist.csv sim_cdf.csv er.out ep.out wd.out calc_tr_residual.0608.csv "$res_dir/"
                done
            done
        done
    done
done
