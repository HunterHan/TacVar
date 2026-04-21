#!/bin/bash -x
pkill -u $(whoami) -9 "mpirun" 2>/dev/null
pkill -u $(whoami) -9 "python3" 2>/dev/null

PROJ_ROOT="$(pwd)/../"
FILT_ROOT="${PROJ_ROOT}/src/filter"
PYTHON3="/home/hpchzy/miniconda3/bin/python3"

source "${PROJ_ROOT}/env.bash"

which mpicc
which python3

${PYTHON3} -c "import numpy, pandas"

# ----------------------------
# User-tunable knobs (defaults)
# ----------------------------
narr=2048
nt=100
np=64
host=$(hostname)
kernel=jacobi2d5p

# tsc (a.k.a. TSC_NS): ticks per ns (~ CPU GHz). Used only by USE_TSC timer to convert cycles -> ns.
tsc_default=2.494102

# nspv: ns_per_samp (a.k.a. NSAMP_PER_VKERN). Used to (1) convert target ns -> nsamp and (2) TF correction.
nspv_default=0.4166667

# Switches:
#   TSC_MODE:   manual | ghz
#   NSPV_MODE:  manual | estimate
# Optional:
#   LOCK_GOVERNOR=1        lock governor=performance (best-effort; no password prompt)
#   RESTORE_GOVERNOR=1     restore governor on exit (best-effort)
TSC_MODE="${TSC_MODE:-ghz}"
NSPV_MODE="${NSPV_MODE:-estimate}"
LOCK_GOVERNOR="${LOCK_GOVERNOR:-1}"
RESTORE_GOVERNOR="${RESTORE_GOVERNOR:-1}"

tsc="${tsc_default}"
nspv="${nspv_default}"

_GOV_FILE="/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
_SAVED_GOV=""

read_governor() {
    [[ -r "${_GOV_FILE}" ]] && cat "${_GOV_FILE}" 2>/dev/null || true
}

set_governor() {
    local gov="${1:?governor is required}"
    if [[ -w "${_GOV_FILE}" ]]; then
        echo "${gov}" >"${_GOV_FILE}" 2>/dev/null && return 0
    fi
    if command -v cpupower >/dev/null 2>&1; then
        sudo -n cpupower frequency-set -g "${gov}" >/dev/null 2>&1 && return 0
    fi
    return 1
}

restore_governor_on_exit() {
    [[ "${RESTORE_GOVERNOR}" != "1" ]] && return 0
    local gov="${_SAVED_GOV:-}"
    if [[ -z "${gov}" ]]; then
        gov="schedutil"
    fi
    if set_governor "${gov}"; then
        echo "Restored CPU governor=${gov}"
    else
        echo "[WARN] Failed to restore CPU governor=${gov} (insufficient permissions?)"
    fi
}

# Always try to lock frequency (manual/ghz both benefit). Restore on exit.
if [[ "${LOCK_GOVERNOR}" == "1" ]]; then
    _SAVED_GOV="$(read_governor)"
    trap restore_governor_on_exit EXIT INT TERM
    if set_governor "performance"; then
        echo "Locked CPU governor=performance (saved=${_SAVED_GOV:-unknown})"
        if command -v cpupower >/dev/null 2>&1; then
            sudo -n cpupower frequency-info || true
        fi
    else
        echo "[WARN] Failed to lock CPU governor=performance (need permission). Continuing."
    fi
fi

read_cpu_mhz_for_tsc() {
    local k m
    if [[ -r /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq ]]; then
        k="$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq 2>/dev/null || true)"
        if [[ -n "${k}" && "${k}" =~ ^[0-9]+$ && "${k}" -gt 0 ]]; then
            echo $((k / 1000))
            return 0
        fi
    fi
    m="$(lscpu 2>/dev/null | awk -F: '/CPU max MHz/ {gsub(/^ +/,"",$2); gsub(/,/,".",$2); printf "%.0f", $2; exit}')"
    if [[ -n "${m}" && "${m}" != "0" ]]; then
        echo "${m}"
        return 0
    fi
    m="$(grep -m1 '^cpu MHz' /proc/cpuinfo 2>/dev/null | awk '{printf "%.0f", $4}')"
    [[ -n "${m}" ]] && echo "${m}"
}

if [[ "${TSC_MODE}" == "ghz" ]]; then
    _mhz="$(read_cpu_mhz_for_tsc || true)"
    if [[ -n "${_mhz}" && "${_mhz}" != "0" ]]; then
        tsc="$(awk -v m="${_mhz}" 'BEGIN{printf "%.6f", m/1000.0}')"
        echo "TSC_MODE=ghz => tsc=${tsc} (from ~${_mhz} MHz / 1000)"
    else
        echo "[WARN] TSC_MODE=ghz but cannot read CPU MHz; fallback tsc=${tsc}"
    fi
else
    echo "TSC_MODE=manual => tsc=${tsc}"
fi

rm *\.x

mpicc -O2 -Wall $CFLAGS $LDFLAGS -o ${kernel}_tsc_tf.x ./${kernel}_tf.c -DUSE_TF -DTIMING -DUSE_TSC -DNTEST=$nt -DNPASS=1 -lgsl -lopenblas
mpicc -O2 -Wall $CFLAGS $LDFLAGS -o ${kernel}_cgt_tf.x ./${kernel}_tf.c -DUSE_TF -DTIMING -DUSE_CGT -DNTEST=$nt -DNPASS=1 -lgsl -lopenblas
mpicc -O2 -Wall $CFLAGS $LDFLAGS -o ${kernel}_papi_tf.x ./${kernel}_tf.c -DUSE_TF -DTIMING -DUSE_PAPI -DNTEST=$nt -DNPASS=1 -lgsl -lopenblas -lpapi                               
mpicc -O2 -Wall $CFLAGS $LDFLAGS -o ${kernel}_papix6_tf.x ./${kernel}_tf.c -DUSE_TF -DTIMING -DUSE_PAPIX6 -DNTEST=$nt -DNPASS=1 -lgsl -lopenblas -lpapi                           
#mpicc -O2 -Wall $CFLAGS $LDFLAGS -o ${kernel}_likwid_tf.x ./${kernel}_tf.c -DUSE_TF -DTIMING -DUSE_LIKWID -DLIKWID_PERFMON -DNTEST=$nt -DNPASS=1 -lgsl -lopenblas -llikwid

mpicc -O2 -Wall $CFLAGS $LDFLAGS -o ${kernel}_tsc.x ./${kernel}.c  -DTIMING -DUSE_TSC -DNTEST=$nt -DNPASS=1 -lgsl -lopenblas
mpicc -O2 -Wall $CFLAGS $LDFLAGS -o ${kernel}_cgt.x ./${kernel}.c  -DTIMING -DUSE_CGT -DNTEST=$nt -DNPASS=1 -lgsl -lopenblas
mpicc -O2 -Wall $CFLAGS $LDFLAGS -o ${kernel}_papi.x ./${kernel}.c  -DTIMING -DUSE_PAPI -DNTEST=$nt -DNPASS=1 -lgsl -lopenblas -lpapi
mpicc -O2 -Wall $CFLAGS $LDFLAGS -o ${kernel}_papix6.x ./${kernel}.c  -DTIMING -DUSE_PAPIX6 -DNTEST=$nt -DNPASS=1 -lgsl -lopenblas -lpapi                        
#mpicc -O2 -Wall $CFLAGS $LDFLAGS -o ${kernel}_likwid.x ./${kernel}.c  -DTIMING -DUSE_LIKWID -DLIKWID_PERFMON -DNTEST=$nt -DNPASS=1 -lgsl -lopenblas -llikwid  

output_dir="output/${host}/$(date +'%Y%m%d_%H%M%S')"
mkdir -p $output_dir

# Generate a walk list for follow-up tests (ta==tb).
walk_list="${output_dir}/walk_list.csv"
meta_txt="${output_dir}/walk_meta.txt"
${PYTHON3} "${PROJ_ROOT}/utils/gen_walklist.py" --out "${walk_list}" --meta-out "${meta_txt}"

if [[ "${NSPV_MODE}" == "estimate" ]]; then
    # Estimate ns_per_samp (nspv) from one TF calibration run:
    #   nspv ≈ median(time_ns) / median(nsamp)
    calib_timer="${NSPV_CALIB_TIMER:-cgt}"
    calib_narr="${NSPV_CALIB_NARR:-64}"
    calib_nsamp="${NSPV_CALIB_NSAMP:-1000000}"
    calib_np="${NSPV_CALIB_NP:-1}"
    calib_dir="${output_dir}/calib_nspv_${kernel}_${calib_timer}_${calib_narr}n_${calib_nsamp}nsamp_np${calib_np}"
    mkdir -p "${calib_dir}"
    rm -f ./*.csv 2>/dev/null || true
    echo "NSPV_MODE=estimate => calibrating with ${kernel}_${calib_timer}_tf.x narr=${calib_narr} nsamp=${calib_nsamp} np=${calib_np}"
    mpirun --map-by core --bind-to core -np "${calib_np}" "./${kernel}_${calib_timer}_tf.x" "${calib_narr}" "${calib_nsamp}" "${tsc}"
    mv ./*.csv "${calib_dir}/" 2>/dev/null || true
    nspv="$(
        TF_DIR="${calib_dir}" ${PYTHON3} - <<'PY'
import os
import numpy as np
import pandas as pd

tf_dir = os.environ["TF_DIR"]
nsamp_col = 1
time_col = 2

dfs = []
for fn in os.listdir(tf_dir):
    if not fn.endswith(".csv"):
        continue
    fp = os.path.join(tf_dir, fn)
    if os.path.isfile(fp):
        dfs.append(pd.read_csv(fp, header=None))
if not dfs:
    raise SystemExit(f"No CSV files found in {tf_dir}")

df_all = pd.concat(dfs, ignore_index=True)
nsamp_vals = df_all.iloc[:, nsamp_col].to_numpy(dtype=np.float64)
time_vals = df_all.iloc[:, time_col].to_numpy(dtype=np.float64)

nsamp_med = float(np.quantile(nsamp_vals, 0.5))
time_med = float(np.quantile(time_vals, 0.5))
if nsamp_med == 0.0:
    raise SystemExit("Median nsamp is 0; cannot estimate ns_per_samp")
print(time_med / nsamp_med)
PY
    )"
    echo "NSPV_MODE=estimate => nspv=${nspv} (from ${calib_dir})"
else
    echo "NSPV_MODE=manual => nspv=${nspv}"
fi

for timer in tsc cgt papi papix6
do
    for iarr in 64 128 256 512 1024 2048
    do
        case_dir=$output_dir/${kernel}${iarr}n${nt}t_${timer}_${host}
        tm_dir=${case_dir}/tm
        tf_dir=${case_dir}/tf
        res_dir=${case_dir}/filt
        rm -rf $case_dir
        rm -f ./*.csv 2>/dev/null || true
        mkdir -p $tm_dir
        mkdir -p $tf_dir
        mkdir -p $res_dir
        mpirun --map-by core --bind-to core -np ${np} ./${kernel}_${timer}.x $iarr $tsc
        mv ./*.csv $tm_dir
        nsamp=`${PYTHON3} ../src/filter/get_quantile.py ${tm_dir} 1 0.5 ${nspv}`
        mpirun --map-by core --bind-to core -np ${np} ./${kernel}_${timer}_tf.x $iarr $nsamp $tsc
        mv ./*.csv $tf_dir
        ${PYTHON3} ${FILT_ROOT}/get_met.py $tm_dir 1
        ${PYTHON3} ${FILT_ROOT}/get_tf.py $tf_dir 1 2 $nspv
        binw=`${PYTHON3} ${FILT_ROOT}/get_binw.py ${tm_dir} 1`
        pushd ${FILT_ROOT}
        rm -rf filt.x
        gcc -o filt.x filt.c -lm
        popd
        # cp -r ${FILT_ROOT}/filt.x ./
        ${FILT_ROOT}/filt.x -w $binw -n 100000 -l 0.01
        mv met.csv tf.csv tr_hist.csv sim_cdf.csv $res_dir
    done
done


