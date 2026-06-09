#!/bin/bash -x

: "${PROJ_ROOT:?PROJ_ROOT is not set! Please source env.bash !}"
: "${DATA_ROOT:?DATA_ROOT is not set! Please source env.bash !}"

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
BASE_CFLAGS="$CFLAGS"

FILTER_ROOT="${PROJ_ROOT}/src/filter"
if [ -x "$HOME/miniconda3/bin/python" ]; then
    PYTHON=${PYTHON:-"$HOME/miniconda3/bin/python"}
else
    PYTHON=${PYTHON:-"python3"}
fi

KERNEL=tvkern
BINW_MIN=${BINW_MIN:-10}
P_LOW=${P_LOW:-0.01}
NP_LIST=${NP_LIST:-"64"}
TIMER_LIST=${TIMER_LIST:-"cgt papi"}
SIZE_LIST=${SIZE_LIST:-"0 16 32 64 128 256 512 1024 2048 4096 8192"}
NSAMP=${NSAMP:-1000}
RA_LOWER=${RA_LOWER:-0}
RB_STEP=${RB_STEP:-1}
NTEST=${NTEST:-1000}
TVKERN_DIST_CFLAGS=${TVKERN_DIST_CFLAGS:-"-DNORMAL -DTBASE=10000 -DV1=0.001"}

ARCH=$(uname -m)
HOSTNAME=$(hostname)
DATE_BASE=$(date +%Y%m%d)
DATE_STAMP=$(date +%Y%m%d-%H%M%S)
DATA_FOLDER="${DATA_ROOT}/${DATE_BASE}/${HOSTNAME}/output_tvkern_filt/${DATE_STAMP}"

case $HOSTNAME in
    "camd9554n2") CPU_FREQ=3.1 ;;
    "cgnr6760pn2") CPU_FREQ=2.2 ;;
    "c920bn3") CPU_FREQ=2.9 ;;
    *) echo "Unknown hostname: $HOSTNAME"; exit 1 ;;
esac

: "${PAPI_HOME:?PAPI_HOME is not set}"
: "${OPENBLAS_HOME:?OPENBLAS_HOME is not set}"
if [ "$ARCH" = "x86_64" ]; then
    : "${LIKWID_HOME:?LIKWID_HOME is not set}"
fi

initialize "$CPU_FREQ"
CPU_FREQ_KHZ_REAL=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq)
NSPV=$(echo "scale=12; 1000000 / $CPU_FREQ_KHZ_REAL * 2" | bc)

case $ARCH in
    "x86_64") TIMER_LIST="$TIMER_LIST tsc tsc_fence tsc_native likwid" ;;
    "aarch64") TIMER_LIST="$TIMER_LIST cntvct cntvcto" ;;
    *) echo "Unsupported architecture: $ARCH"; exit 1 ;;
esac

echo "ARCH: $ARCH"
echo "HOSTNAME: $HOSTNAME"
echo "DATA_FOLDER: $DATA_FOLDER"
echo "NP_LIST: $NP_LIST"
echo "TIMER_LIST: $TIMER_LIST"
echo "SIZE_LIST: $SIZE_LIST"
echo "NSAMP: $NSAMP"
echo "RA_LOWER: $RA_LOWER"
echo "RB_STEP: $RB_STEP"
echo "NTEST: $NTEST"
echo "TVKERN_DIST_CFLAGS: $TVKERN_DIST_CFLAGS"
echo "NSPV: $NSPV"

if command -v gsl-config >/dev/null 2>&1; then
    GSL_CFLAGS="$(gsl-config --cflags)"
    GSL_LIBS="$(gsl-config --libs)"
else
    : "${GSL_HOME:?GSL_HOME is not set and gsl-config is not found}"
    GSL_CFLAGS="-I${GSL_HOME}/include"
    GSL_LIBS="-L${GSL_HOME}/lib -lgsl -lgslcblas -lm"
fi

mkdir -p "$DATA_FOLDER"
rm -f ./*.x ./*.csv

mpicc -O2 -Wall -o "${FILTER_ROOT}/filt.x" "${FILTER_ROOT}/filt.c"
for timer in $TIMER_LIST; do
    timer_cflags="$BASE_CFLAGS"
    case $timer in
        "papi" | "papix6")
            timer_cflags="$timer_cflags -I${PAPI_HOME}/include -L${PAPI_HOME}/lib -lpapi"
            ;;
        "likwid")
            timer_cflags="$timer_cflags -DLIKWID_PERFMON -I${LIKWID_HOME}/include -L${LIKWID_HOME}/lib -llikwid"
            ;;
    esac

    mpicc -o "${KERNEL}_${timer}.x" "${KERNEL}.c" $timer_cflags $TVKERN_DIST_CFLAGS -DNTEST="$NTEST" -DTIMING "-DUSE_${timer^^}" \
        $GSL_CFLAGS $GSL_LIBS
    mpicc -o "${KERNEL}_${timer}_tf.x" "${KERNEL}.c" $timer_cflags $TVKERN_DIST_CFLAGS -DNTEST="$NTEST" -DSTAGE_TF -DTIMING "-DUSE_${timer^^}" \
        $GSL_CFLAGS $GSL_LIBS

    for np in $NP_LIST; do
        for size in $SIZE_LIST; do
            tm_dir="${DATA_FOLDER}/${KERNEL}_${timer}_np${np}_size${size}"
            te_dir="${tm_dir}_tf"
            res_dir="${tm_dir}_filt"

            rm -rf "$tm_dir" "$te_dir" "$res_dir" ./*.csv
            mkdir -p "$tm_dir" "$te_dir" "$res_dir"

            if [ "$timer" = "likwid" ]; then
                run_cmd=(likwid-mpirun -mpi openmpi -np "$np" -g "${LIKWID_GROUP:-L3}" -m)
            else
                run_cmd=(mpirun -np "$np" --map-by core --bind-to core)
            fi

            "${run_cmd[@]}" "./${KERNEL}_${timer}.x" "$size" "$NSAMP" "$RA_LOWER" "$RB_STEP"
            mv ./*.csv "$tm_dir/"
            "${run_cmd[@]}" "./${KERNEL}_${timer}_tf.x" "$size" "$NSAMP" "$RA_LOWER" "$RB_STEP"
            mv ./*.csv "$te_dir/"

            if [ "$timer" = "likwid" ]; then
                awk -F, '{if ($2 != 0) ok=1} END{exit ok ? 0 : 1}' "$tm_dir"/*.csv || { echo "Invalid LIKWID tm: all timing values are zero"; exit 1; }
                awk -F, '{if ($3 != 0) ok=1} END{exit ok ? 0 : 1}' "$te_dir"/*.csv || { echo "Invalid LIKWID tf: all timing values are zero"; exit 1; }
            fi

            "$PYTHON" "${FILTER_ROOT}/get_met.py" "$tm_dir" 1
            "$PYTHON" "${FILTER_ROOT}/get_tf.py" "$te_dir" 1 2 "$NSPV"
            binw=$("$PYTHON" "${FILTER_ROOT}/get_binw.py" "$tm_dir" 1 "$BINW_MIN")
            "${FILTER_ROOT}/filt.x" -w "$binw" -n 100000 -l "$P_LOW" -x 0.005 -y 0.005 -z 0.005
            mv met.csv tf.csv tr_hist.csv tm_hist.csv sim_cdf.csv er.out ep.out wd.out "$res_dir/"
        done
    done
done

cleanup
