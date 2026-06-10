#!/bin/bash -x
# Test run for gs2d5p and tl_f90_cg_calc_w on camd9554n2
# Based on run_entry_filt.0526.sh

INSITU="INSITU_SUB_ASM"

: "${PROJ_ROOT:?PROJ_ROOT is not set! Please source env.bash !}"

initialize(){
    local cpu_freq=$1
    if [ -e /sys/devices/system/cpu/cpufreq/boost ]; then
        echo 0 | sudo tee /sys/devices/system/cpu/cpufreq/boost
    fi
    if [ -e /sys/devices/system/cpu/intel_pstate/no_turbo ]; then
        echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo
    fi
    sudo cpupower frequency-set -u "${cpu_freq}GHz" -d "${cpu_freq}GHz" -g performance
    sudo cpupower frequency-info
    [ -r /sys/devices/system/cpu/intel_pstate/no_turbo ] && echo "intel_no_turbo=$(cat /sys/devices/system/cpu/intel_pstate/no_turbo)"
    [ -r /sys/devices/system/cpu/cpufreq/boost ] && echo "cpufreq_boost=$(cat /sys/devices/system/cpu/cpufreq/boost)"
}

cleanup(){
    sudo cpupower frequency-set -g schedutil
    if [ -e /sys/devices/system/cpu/intel_pstate/no_turbo ]; then
        echo 0 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo
    fi
    if [ -e /sys/devices/system/cpu/cpufreq/boost ]; then
        echo 1 | sudo tee /sys/devices/system/cpu/cpufreq/boost
    fi
}

trap 'echo "ERR"' ERR
trap cleanup EXIT 

CFLAGS="-O2 -Wall -g"
BASE_CFLAGS="$CFLAGS"

FILTER_ROOT="${PROJ_ROOT}/src/filter/"
if [ -z "${PYTHON:-}" ]; then
    if [ -x "$HOME/miniconda3/bin/python" ]; then
        PYTHON="$HOME/miniconda3/bin/python"
    elif [ -x "$HOME/miniconda/bin/python" ]; then
        PYTHON="$HOME/miniconda/bin/python"
    else
        PYTHON=python3
    fi
fi
BINW_MIN=10
P_LOW=0.01
NSAMP=${NSAMP:-1000}
NSAMP_TF_MIN=${NSAMP_TF_MIN:-1}
NSAMP_RATIO_LIST=${NSAMP_RATIO_LIST:-"0.5 0.8"}

ARCH=$(uname -m)
HOSTNAME=$(hostname)
DATE_BASE=$(date +%Y%m%d)
DATE_STAMP=$(date +%Y%m%d-%H%M%S)
DATA_FOLDER="${DATA_ROOT}/${DATE_BASE}/${HOSTNAME}/output_filt/${DATE_STAMP}/"
mkdir -p "$DATA_FOLDER"

# === TEST CONFIG: only gs2d5p and tl_f90_cg_calc_w ===
KERNEL_LIST=${KERNEL_LIST:-"gs2d5p tl_f90_cg_calc_w"}
NP_LIST=${NP_LIST:-"4"}
TIMER_LIST=${TIMER_LIST:-"cgt"}
SIZE_LIST=${SIZE_LIST:-"128 256"}

case $HOSTNAME in
    "camd9554n1")
        CPU_FREQ=3.1
        ;;
    "camd9554n2")
        CPU_FREQ=3.1
        ;;
    "cgnr6760pn2")
        CPU_FREQ=2.2
        ;;
    "c920bn3")
        CPU_FREQ=2.9
        ;;
    *)
        echo "Unknown hostname: $HOSTNAME"
        exit 1
        ;;
esac

echo "ARCH: $ARCH"
echo "HOSTNAME: $HOSTNAME"
echo "KERNEL_LIST: $KERNEL_LIST"
echo "NP_LIST: $NP_LIST"
echo "TIMER_LIST: $TIMER_LIST"
echo "SIZE_LIST: $SIZE_LIST"

: "${PAPI_HOME:?PAPI_HOME is not set}"
: "${OPENBLAS_HOME:?OPENBLAS_HOME is not set}"

initialize "$CPU_FREQ"
CPU_FREQ_KHZ_REAL=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq)

case ${INSITU} in
    "INSITU_SUB_ASM")
        NSPV=$(echo "scale=12; 1000000 / $CPU_FREQ_KHZ_REAL" |bc)
        ;;
    *)
        exit 1
        ;;
esac

echo "CPU_FREQ: ${CPU_FREQ}GHz"
echo "CPU_FREQ_KHZ_REAL: ${CPU_FREQ_KHZ_REAL} KHz"
echo "NSPV: ${NSPV} ns per cycle"

# For x86, add tsc timers
case $ARCH in 
    "x86_64")
        TIMER_LIST="$TIMER_LIST tsc tsc_native"
        ;;
    "aarch64")
        TIMER_LIST="$TIMER_LIST cntvct cntvcto"
        ;;
esac

echo "TIMER_LIST final: $TIMER_LIST"

rm -f *.x

mpicc -O2 -Wall -o "${FILTER_ROOT}/filt.x" "${FILTER_ROOT}/filt_v2.0608.c"
for kernel in $KERNEL_LIST; do
    for timer in $TIMER_LIST; do
        timer_cflags="$BASE_CFLAGS"
        case $timer in 
            "papi" | "papix6")
                timer_cflags="$timer_cflags -I${PAPI_HOME}/include -L${PAPI_HOME}/lib -lpapi"
                ;;
            "likwid")
                timer_cflags="$timer_cflags -DLIKWID_PERFMON -I${LIKWID_HOME}/include -L${LIKWID_HOME}/lib -llikwid"
                ;;
            *)
                ;;
        esac
        echo "=== Compiling ${kernel}_${timer}.x ==="
        mpicc -o "${kernel}_${timer}.x" "${kernel}.c" $timer_cflags -D"$INSITU" -DTIMING "-DUSE_${timer^^}" -I"${OPENBLAS_HOME}/include" -L"${OPENBLAS_HOME}/lib" || { echo "FAIL: compile ${kernel}_${timer}"; exit 1; }
        echo "=== Compiling ${kernel}_${timer}_tf.x ==="
        mpicc -o "${kernel}_${timer}_tf.x" "${kernel}.c" $timer_cflags -D"$INSITU" -DSTAGE_TF -DTIMING "-DUSE_${timer^^}" -I"${OPENBLAS_HOME}/include" -L"${OPENBLAS_HOME}/lib" || { echo "FAIL: compile ${kernel}_${timer}_tf"; exit 1; }

        for np in $NP_LIST; do
            for size in $SIZE_LIST; do
                tm_dir="${DATA_FOLDER}/${kernel}_${timer}_np${np}_size${size}"
                rm -rf "$tm_dir"
                rm -rf ./*.csv
                mkdir -p "$tm_dir"
                
                run_cmd=(mpirun -np "$np" --map-by core --bind-to core)
                echo "=== Running ${kernel}_${timer} np=${np} size=${size} ==="
                "${run_cmd[@]}" "./${kernel}_${timer}.x" "${size}" 0 1 || { echo "FAIL: run ${kernel}_${timer}"; exit 1; }
                sleep 1
                mv ./*.csv "$tm_dir/"

                for nsamp_ratio in ${NSAMP_RATIO_LIST}; do
                    nsamp_tf=$("${PYTHON}" "${FILTER_ROOT}/get_quantile.py" "${tm_dir}" 1 "${nsamp_ratio}" "${NSPV}") || exit 1
                    if ! [[ "$nsamp_tf" =~ ^[0-9]+$ ]]; then
                        echo "Invalid nsamp_tf: ${nsamp_tf}"
                        exit 1
                    fi
                    if [ "$nsamp_tf" -lt "$NSAMP_TF_MIN" ]; then
                        nsamp_tf="$NSAMP_TF_MIN"
                    fi

                    te_dir="${tm_dir}_nsampRatio${nsamp_ratio}_nsamp${nsamp_tf}_tf"
                    res_dir="${tm_dir}_nsampRatio${nsamp_ratio}_nsamp${nsamp_tf}_filt"
                    rm -rf "$te_dir" "$res_dir"
                    mkdir -p "$te_dir" "$res_dir"

                    echo "=== Running ${kernel}_${timer}_tf np=${np} size=${size} nsamp=${nsamp_tf} ==="
                    "${run_cmd[@]}" "./${kernel}_${timer}_tf.x" "${size}" 0 1 "${nsamp_tf}" || { echo "FAIL: run ${kernel}_${timer}_tf"; exit 1; }
                    sleep 1
                    mv ./*.csv "$te_dir/"

                    "${PYTHON}" "${FILTER_ROOT}/get_met.py" "${tm_dir}" 1
                    "${PYTHON}" "${FILTER_ROOT}/get_tf.py" "${te_dir}" 1 2 "${NSPV}"
                    binw=$("${PYTHON}" "${FILTER_ROOT}/get_binw.py" "${tm_dir}" 1 "${BINW_MIN}")
                    "${FILTER_ROOT}/filt.x" -w "${binw}" -n 100000 -l "${P_LOW}" -x 0.005 -y 0.005 -z 0.005
                    mv met.csv tf.csv tr_hist.csv tm_hist.csv sim_cdf.csv er.out ep.out wd.out calc_tr_residual.0608.csv "${res_dir}/"
                done
            done
        done
    done
done

cleanup
echo "=== ALL DONE ==="
