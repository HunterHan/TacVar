#!/bin/bash -x

: "${PROJ_ROOT:?PROJ_ROOT is not set! Please source env.bash !}"

initialize(){
    local cpu_freq=$1
    sudo cpupower frequency-set -u "${cpu_freq}GHz" -d "${cpu_freq}GHz" -g performance
    sudo cpupower frequency-info
}

cleanup(){
    sudo cpupower frequency-set -g schedutil
}

trap 'echo "ERR"' ERR
trap cleanup EXIT 


CFLAGS="-O2 -Wall -g"

# Meta
FILTER_ROOT="${PROJ_ROOT}/src/filter/"
PYTHON="python3"
BINW_MIN=10
P_LOW=0.01
# NSAMP=1000
NSAMP=100000

ARCH=$(uname -m)
HOSTNAME=$(hostname)
DATE_BASE=$(date +%Y%m%d)
DATE_STAMP=$(date +%Y%m%d-%H%M%S)
DATA_FOLDER="${DATA_ROOT}/${DATE_BASE}/${HOSTNAME}/output_filt/${DATE_STAMP}/"

mkdir -p "$DATA_FOLDER"

KERNEL_LIST=${KERNEL_LIST:-"jacobi2d5p "}
NP_LIST=${NP_LIST:-"64"}
# TIMER_LIST=${TIMER_LIST:-"cgt papi papix6 wtime"}
TIMER_LIST=${TIMER_LIST:-"cgt papi"}
SIZE_LIST=${SIZE_LIST:-"512"}

case $HOSTNAME in
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
        echo "Unkown hostname: $hostname"
        exit 1
        ;;
esac

echo "ARCH: $ARCH"
echo "HOSTNAME: $HOSTNAME"
echo "DATE_BASE: $DATE_BASE"
echo "KERNEL_LIST: $KERNEL_LIST"
echo "NP_LIST: $NP_LIST"
echo "TIMER_LIST: $TIMER_LIST"
echo "SIZE_LIST: $SIZE_LIST"

# Check ENVs
: "${PAPI_HOME:?PAPI_HOME is not set}"
: "${OPENBLAS_HOME:?OPENBLAS_HOME is not set}"

if [ "$ARCH" = "x86_64" ]; then
    : "${LIKWID_HOME:?LIKWID_HOME is not set}"
fi

# # Compile
# $CC $CFLAGS -o "${kernel}_tsc.x" "${kernel}.c" -DTIMING -DUSE_TSC -I"${OPENBLAS_HOME}/include" -L"${OPENBLAS_HOME}/lib" -lgsl -lopenblas

initialize "$CPU_FREQ"
CPU_FREQ_KHZ_REAL=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq)
NSPV=$(echo "scale=12; 1000000 / $CPU_FREQ_KHZ_REAL * 2" |bc)
# case "${NSPV}" in
#     .*) NSPV="0${NSPV}" ;;
# esac
echo "CPU_FREQ: ${CPU_FREQ}GHz"
echo "CPU_FREQ_KHZ_REAL: ${CPU_FREQ_KHZ_REAL} KHz"
echo "NSPV: ${NSPV} ns per cycle"


# Adapte
case $ARCH in 
    "x86_64")
        TIMER_LIST="$TIMER_LIST tsc likwid"
        ;;
    "aarch64")
        TIMER_LIST="$TIMER_LIST cntvct cntvcto"
        ;;
    *)
        echo "Unsupported architecture: $ARCH"
        exit 1
        ;;
esac

# Execution
echo $TIMER_LIST

rm -f *.x

for kernel in $KERNEL_LIST; do
    for timer in $TIMER_LIST; do
        case $timer in 
            "papi" | "papix6")
                CFLAGS="$CFLAGS -I${PAPI_HOME}/include -L${PAPI_HOME}/lib -lpapi"
                ;;
            "likwid")
                CFLAGS="$CFLAGS -I${LIKWID_HOME}/include -L${LIKWID_HOME}/lib -llikwid"
                ;;
            *)
                ;;
        esac
        mpicc -o "${kernel}_${timer}.x" "${kernel}.c"  $CFLAGS -DTIMING "-DUSE_${timer^^}" -I"${OPENBLAS_HOME}/include" -L"${OPENBLAS_HOME}/lib" -lgsl -lopenblas
        mpicc -o "${kernel}_${timer}_tf.x" "${kernel}.c"  $CFLAGS -DSTAGE_TF -DTIMING "-DUSE_${timer^^}" -I"${OPENBLAS_HOME}/include" -L"${OPENBLAS_HOME}/lib" -lgsl -lopenblas

        for np in $NP_LIST; do
            for size in $SIZE_LIST; do
                tm_dir="${DATA_FOLDER}/${kernel}_${timer}_np${np}_size${size}"
                te_dir="${tm_dir}_tf"
                res_dir="${tm_dir}_filt"

                echo "tm_dir: $tm_dir"
                echo "te_dir: $te_dir"
                echo "res_dir: $res_dir"

                rm -rf "$tm_dir" "$te_dir" "$res_dir"
                rm -rf ./*.csv
                mkdir -p "$tm_dir" "$te_dir" "$res_dir"
                mpirun -np $np --map-by core --bind-to core "./${kernel}_${timer}.x" "${size}" "${NSAMP}"
                mv ./*.csv "$tm_dir/"
                mpirun -np $np --map-by core --bind-to core "./${kernel}_${timer}_tf.x" "${size}" "${NSAMP}"
                mv ./*.csv "$te_dir/"

                "${PYTHON}" "${FILTER_ROOT}/get_met.py" "${tm_dir}" 1
                "${PYTHON}" "${FILTER_ROOT}/get_tf.py" "${te_dir}" 1 2 "${NSPV}"
                binw=$("${PYTHON}" "${FILTER_ROOT}/get_binw.py" "${tm_dir}" 1 "${BINW_MIN}")
                "${FILTER_ROOT}/filt.x" -w "${binw}" -n 100000 -l "${P_LOW}" -x 0.005 -y 0.005 -z 0.005
                mv met.csv tf.csv tr_hist.csv tm_hist.csv sim_cdf.csv er.out ep.out wd.out "${res_dir}/"

            done
        done
    done
done

cleanup


# Plot
