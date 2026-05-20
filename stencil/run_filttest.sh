#!/usr/bin/env bash
set -euo pipefail
set -x

date +%Y-%m-%d_%H:%M:%S

host=$(hostname)
PROJ_ROOT=$(realpath "$(pwd)/..")
UTILS_ROOT=$(realpath "${PROJ_ROOT}/utils")
FILTER_ROOT=$(realpath "${PROJ_ROOT}/src/filter")

RUN_DATE=${RUN_DATE:-20260520}
OUTPUT_NAME=${OUTPUT_NAME:-outputStencil}
DATA_ROOT=${DATA_ROOT:-${HOME}/code/data/${RUN_DATE}/${host}/${OUTPUT_NAME}}
mkdir -p "${DATA_ROOT}"
DATA_ROOT=$(realpath "${DATA_ROOT}")
NP_LIST=${NP_LIST:-"64 128"}
KERNEL_LIST=${KERNEL_LIST:-"jacobi2d5p tl_f90_cg_calc_w"}
SIZE_LIST=${SIZE_LIST:-"64 128 256 512 1024"}
NT=${NT:-100}
BINW_MIN=${BINW_MIN:-10}
P_LOW=${P_LOW:-0.01}
NSPV_RATIO=${NSPV_RATIO:-1.0}
DO_SAMPLING=${DO_SAMPLING:-1}
RERUN_INVALID_LIKWID=${RERUN_INVALID_LIKWID:-1}

echo "HOST: ${host}"
echo "DATA_ROOT: ${DATA_ROOT}"
echo "NP_LIST: ${NP_LIST}"
echo "KERNEL_LIST: ${KERNEL_LIST}"
echo "SIZE_LIST: ${SIZE_LIST}"
echo "NT: ${NT}"
echo "DO_SAMPLING: ${DO_SAMPLING}"

if [ "${host}" = "c920bn3" ]; then
    tsc=2.900000
else
    TSC_METHOD=${TSC_METHOD:-calibrate}
    tsc=$("${UTILS_ROOT}/print_tsc_freq.sh" -m "${TSC_METHOD}" -q)
fi
nspv=$(echo "scale=9; 1.0 / ${tsc} * ${NSPV_RATIO}" | bc)
git_commit=$(git -C "${PROJ_ROOT}" rev-parse --short HEAD 2>/dev/null || echo unknown)

echo "TSC FREQUENCY: ${tsc}"
echo "NSPV: ${nspv}"
echo "GIT_COMMIT: ${git_commit}"

if [ -z "${PYTHON:-}" ]; then
    if [ -x "${HOME}/miniconda3/bin/python" ]; then
        PYTHON="${HOME}/miniconda3/bin/python"
    elif [ -x "${HOME}/anaconda3/bin/python" ]; then
        PYTHON="${HOME}/anaconda3/bin/python"
    else
        PYTHON=$(command -v python3)
    fi
fi
"${PYTHON}" - <<'PY'
import numpy
print("PYTHON_NUMPY", numpy.__version__)
PY


timer_list_for_host() {
    if [ "${host}" = "c920bn3" ]; then
        echo "${TIMER_LIST:-cgt wtime papi papix6 cntvct cntvcto}"
    else
        echo "${TIMER_LIST:-cgt wtime papi papix6 tsc likwid}"
    fi
}

timer_macro() {
    case "$1" in
        cgt) echo USE_CGT ;;
        wtime) echo USE_WTIME ;;
        papi) echo USE_PAPI ;;
        papix6) echo USE_PAPIX6 ;;
        tsc) echo USE_TSC ;;
        likwid) echo USE_LIKWID ;;
        cntvct) echo USE_CNTVCT ;;
        cntvcto) echo USE_CNTVCTO ;;
        *) echo "Unknown timer: $1" >&2; return 1 ;;
    esac
}

timer_ldflags() {
    case "$1" in
        papi|papix6) echo "-lpapi" ;;
        likwid) echo "-llikwid" ;;
        *) echo "" ;;
    esac
}

timer_extra_defs() {
    case "$1" in
        likwid) echo "-DLIKWID_PERFMON" ;;
        *) echo "" ;;
    esac
}

check_csv_positive() {
    local dir=$1
    local col=$2
    local min_files=$3
    local nfiles
    nfiles=$(find "${dir}" -maxdepth 1 -type f -name '*.csv' | wc -l)
    [ "${nfiles}" -ge "${min_files}" ] || return 1
    awk -F, -v c="${col}" '
        NF >= c && $c ~ /^-?[0-9]+([.][0-9]+)?$/ && $c > 0 { ok=1 }
        END { exit ok ? 0 : 1 }
    ' "${dir}"/*.csv
}

check_filter_outputs() {
    local dir=$1
    for f in met.csv tf.csv tr_hist.csv tm_hist.csv sim_cdf.csv er.out ep.out wd.out; do
        [ -s "${dir}/${f}" ] || return 1
    done
    awk 'NF && $1 !~ /^[-+]?([0-9]*[.])?[0-9]+([eE][-+]?[0-9]+)?$/ { bad=1 } END { exit bad ? 1 : 0 }' \
        "${dir}/er.out" "${dir}/ep.out" "${dir}/wd.out"
}

write_metadata() {
    local path=$1 status=$2 kernel=$3 size=$4 timer=$5 np=$6 nsamp=$7 binw=$8
    {
        echo "status=${status}"
        echo "host=${host}"
        echo "np=${np}"
        echo "kernel=${kernel}"
        echo "size=${size}"
        echo "timer=${timer}"
        echo "nt=${NT}"
        echo "tsc=${tsc}"
        echo "nspv=${nspv}"
        echo "nsamp=${nsamp}"
        echo "binw=${binw}"
        echo "git_commit=${git_commit}"
        echo "data_root=${DATA_ROOT}"
        echo "timestamp=$(date +%Y-%m-%d_%H:%M:%S)"
    } > "${path}"
}

if [ "${DO_SAMPLING}" -eq 1 ]; then
    : "${PAPI_HOME:?PAPI_HOME is not set}"
    : "${LIKWID_HOME:?LIKWID_HOME is not set}"
    : "${OPENBLAS_HOME:?OPENBLAS_HOME is not set}"
fi

CFLAGS="-I${PAPI_HOME:-}/include/ -I${LIKWID_HOME:-}/include/ -I${OPENBLAS_HOME:-}/include"
LDFLAGS="-L${PAPI_HOME:-}/lib/ -L${LIKWID_HOME:-}/lib/ -L${OPENBLAS_HOME:-}/lib/"
if [ "${host}" != "c920bn3" ]; then
    LDFLAGS="${LDFLAGS} -lgsl -lopenblas"
fi

build_timer() {
    local kernel=$1 timer=$2 macro defs libs
    macro=$(timer_macro "${timer}")
    defs=$(timer_extra_defs "${timer}")
    libs=$(timer_ldflags "${timer}")
    mpicc -O2 -Wall -o "${kernel}_${timer}.x" "./${kernel}.c" \
        -DTIMING "-D${macro}" ${defs} -DNTEST="${NT}" -DNPASS=1 ${CFLAGS} ${LDFLAGS} ${libs}
    mpicc -O2 -Wall -o "${kernel}_${timer}_tf.x" "./${kernel}_tf.c" \
        -DTIMING "-D${macro}" ${defs} -DNTEST="${NT}" -DNPASS=1 ${CFLAGS} ${LDFLAGS} ${libs}
}

if [ "${DO_SAMPLING}" -eq 1 ]; then
    for kernel in ${KERNEL_LIST}; do
        for timer in $(timer_list_for_host); do
            build_timer "${kernel}" "${timer}"
        done
    done

    if [ "${host}" = "camd9554n2" ]; then
        sudo cpupower frequency-set -u 3.1GHz -d 3.1GHz
    elif [ "${host}" = "cgnr6760pn2" ]; then
        sudo cpupower frequency-set -u 2.2GHz -d 2.2GHz
    elif [ "${host}" = "c920bn3" ]; then
        sudo cpupower frequency-set -u 2.9GHz -d 2.9GHz
    fi
    sudo cpupower frequency-info
fi

gcc -O2 -Wall -o "${FILTER_ROOT}/filt.x" "${FILTER_ROOT}/filt.c"
mkdir -p "${DATA_ROOT}/logs"

run_one() {
    local kernel=$1 size=$2 timer=$3 np=$4 attempt=${5:-1}
    local np_root met_dir tf_dir res_dir run_log nsamp binw status
    np_root="${DATA_ROOT}/np${np}"
    met_dir="${np_root}/${kernel}${size}n${NT}t_${timer}_${host}"
    tf_dir="${met_dir}_tf"
    res_dir="${met_dir}_filt"
    run_log="${DATA_ROOT}/logs/${kernel}_${size}_${timer}_np${np}_attempt${attempt}.log"
    mkdir -p "${np_root}" "${DATA_ROOT}/logs"

    if [ "${DO_SAMPLING}" -eq 1 ]; then
        rm -rf "${met_dir}" "${tf_dir}" "${res_dir}"
        mkdir -p "${met_dir}" "${tf_dir}" "${res_dir}"
        rm -f ./*.csv
        {
            echo "RUN_MET kernel=${kernel} size=${size} timer=${timer} np=${np}"
            mpirun --map-by core --bind-to core -np "${np}" "./${kernel}_${timer}.x" "${size}" "${tsc}"
            mv ./*.csv "${met_dir}/"
        } &> "${run_log}"
    else
        mkdir -p "${res_dir}"
    fi

    nsamp=$("${PYTHON}" "${FILTER_ROOT}/get_quantile.py" "${met_dir}" 1 0.5 "${nspv}")
    if [ "${DO_SAMPLING}" -eq 1 ]; then
        rm -f ./*.csv
        {
            echo "RUN_TF kernel=${kernel} size=${size} timer=${timer} np=${np} nsamp=${nsamp}"
            mpirun --map-by core --bind-to core -np "${np}" "./${kernel}_${timer}_tf.x" "${size}" "${nsamp}" "${tsc}"
            mv ./*.csv "${tf_dir}/"
        } >> "${run_log}" 2>&1
    fi

    "${PYTHON}" "${FILTER_ROOT}/get_met.py" "${met_dir}" 1
    "${PYTHON}" "${FILTER_ROOT}/get_tf.py" "${tf_dir}" 1 2 "${nspv}"
    binw=$("${PYTHON}" "${FILTER_ROOT}/get_binw.py" "${met_dir}" 1 "${BINW_MIN}")
    "${FILTER_ROOT}/filt.x" -w "${binw}" -n 100000 -l "${P_LOW}" -x 0.005 -y 0.005 -z 0.005
    mv met.csv tf.csv tr_hist.csv tm_hist.csv sim_cdf.csv er.out ep.out wd.out "${res_dir}/"

    status=ok
    check_csv_positive "${met_dir}" 2 "${np}" || status=invalid_met
    check_csv_positive "${tf_dir}" 3 "${np}" || status=invalid_tf
    check_filter_outputs "${res_dir}" || status=invalid_filter
    write_metadata "${res_dir}/metadata.env" "${status}" "${kernel}" "${size}" "${timer}" "${np}" "${nsamp}" "${binw}"
    [ "${status}" = "ok" ]
}

for np in ${NP_LIST}; do
    for kernel in ${KERNEL_LIST}; do
        for timer in $(timer_list_for_host); do
            for size in ${SIZE_LIST}; do
                if ! run_one "${kernel}" "${size}" "${timer}" "${np}" 1; then
                    if [ "${timer}" = "likwid" ] && [ "${RERUN_INVALID_LIKWID}" -eq 1 ]; then
                        run_one "${kernel}" "${size}" "${timer}" "${np}" 2 || true
                    fi
                fi
            done
        done
    done
done

date +%Y-%m-%d_%H:%M:%S
