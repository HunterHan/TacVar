#!/bin/bash

if [ "$#" -lt 1 ];then
    echo "Usage: $0 [MODE]"
    echo "MODE: 0 for walking list generateing, 1 for timing fluctuation detecting"
    echo "Example: $0 0 \"1000 10000 100000\" 0 100"
    echo "Example: $0 1 fsize ./walklists"
    exit 1

fi

MODE="$1"

echo "$MODE"

case $MODE in
    0)
        echo "Step 1: Generating Walking List"
        if [ "$#" -ne 4 ];then
            echo "Usage: $0 0 [TBASE_LIST] [Dist] [#Wals]"
            echo "TBASE_LIST: quoted interval list, e.g. \"1000 10000 100000\""
            echo "Dist: 0 for Normal, 1 for Uniform, 2 for Pareto"
            echo "#Wals: Number of walking lists to generate"
            echo "Example: $0 0 \"1000 10000 100000\" 0 100"
            exit 1
        fi
        SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
        TBASE_LIST="${2//,/ }"
        TBASE_LIST="${TBASE_LIST//，/ }"

        DIST="$3"

        case $DIST in
            0)
                DIST_NAME="Normal"
                ;;
            1)
                DIST_NAME="Uniform"
                ;;
            2)
                DIST_NAME="Pareto"
                ;;
            *)
                exit 1
                ;;
        esac

        NUM_WALK="$4"

        WALK_ROOT="${WALK_ROOT:-${SCRIPT_DIR}/walklists}"
        mkdir -p "${WALK_ROOT}"
        for TBASE in ${TBASE_LIST}; do
            OUT_CSV="${WALK_ROOT}/detecting_walking_list_${DIST_NAME}_n${NUM_WALK}_tbase${TBASE}.csv"
            echo "Generating ${OUT_CSV}"
            python "${SCRIPT_DIR}/generate_walking_list.py" \
                --tbase "${TBASE}" \
                --dist "${DIST}" \
                --num_walk "${NUM_WALK}" \
                --output_file "${OUT_CSV}"
        done
        
        ;;
    1)
        echo "Step 2: Detecting Timing Fluctuation"
        set -x
        # set -euo pipefail

        initialize(){
            local cpu_freq=$1
            sudo cpupower frequency-set -u "${cpu_freq}GHz" -d "${cpu_freq}GHz" -g performance
            sudo cpupower frequency-info
        }

        cleanup(){
            sudo cpupower frequency-set -g schedutil
        }

        # trap 'echo "ERR"' ERR
        # trap cleanup EXIT 


        SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
        PROJ_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

        set +u
        cd "${PROJ_ROOT}"
        source "${PROJ_ROOT}/env.bash"
        cd -
        set -u

        HOSTNAME="$(hostname -s 2>/dev/null || hostname)"
        ARCH="$(uname -m)"
        DATA_ROOT="${DATA_ROOT:-${HOME}/code/data}"
        DATE_BASE="${DATE_BASE:-$(date +%Y%m%d)}"
        DATE_STAMP="${DATE_STAMP:-$(date +%Y%m%d-%H%M%S)}"
        DATA_FOLDER="${DATA_ROOT}/${DATE_BASE}/${HOSTNAME}/outputDetecting/${DATE_STAMP}"

        EXPR="${2:-fsize}"
        WALK_ARG="${3:-}"
        case "${EXPR}" in
            timer|expr1.timer)
                EXPR="timer"
                EXPR_NAME="${EXPR_NAME:-detecting.expr1.timer}"
                MU_LIST="${MU_LIST:-10000}"
                FKERN_LIST="${FKERN_LIST:-copy}"
                RKERN_LIST="${RKERN_LIST:-none}"
                FSIZE_LIST="${FSIZE_LIST:-2048}"
                RSIZE_LIST="${RSIZE_LIST:-0}"
                ;;
            fsize|expr1.fsize)
                EXPR="fsize"
                EXPR_NAME="${EXPR_NAME:-detecting.expr1.fsize}"

                FSIZE_LIST="${FSIZE_LIST:-16 32 64 128 256 512 1024 2048 4096 8192}"

                MU_LIST="${MU_LIST:-10000}"
                FKERN_LIST="${FKERN_LIST:-copy}"
                RKERN_LIST="${RKERN_LIST:-none}"
                RSIZE_LIST="${RSIZE_LIST:-0}"
                ;;
            interval|expr2.interval)
                EXPR="interval"
                EXPR_NAME="${EXPR_NAME:-detecting.expr2.interval}"
                
                MU_LIST="${MU_LIST:-1000 10000 100000 1000000 10000000}"

                FKERN_LIST="${FKERN_LIST:-copy}"
                RKERN_LIST="${RKERN_LIST:-none}"
                FSIZE_LIST="${FSIZE_LIST:-4096}"
                RSIZE_LIST="${RSIZE_LIST:-0}"
                ;;
            frkern|expr3.frkern)
                EXPR="frkern"
                EXPR_NAME="${EXPR_NAME:-detecting.expr3.frkern}"

                FKERN_LIST="${FKERN_LIST:-copy add scale triad pow dgemm}"
                RKERN_LIST="${RKERN_LIST:-copy add scale triad pow dgemm}"

                MU_LIST="${MU_LIST:-10000}"
                FSIZE_LIST="${FSIZE_LIST:-4096}"
                RSIZE_LIST="${RSIZE_LIST:-0}"
                ;;
            *)
                if [ -e "${EXPR}" ]; then
                    WALK_ARG="${EXPR}"
                    EXPR="fsize"
                    EXPR_NAME="${EXPR_NAME:-detecting.expr1.fsize}"
                    MU_LIST="${MU_LIST:-10000}"
                    FKERN_LIST="${FKERN_LIST:-copy}"
                    RKERN_LIST="${RKERN_LIST:-none}"
                    FSIZE_LIST="${FSIZE_LIST:-16 32 64 128 256 512 1024 2048 4096 8192}"
                    RSIZE_LIST="${RSIZE_LIST:-0}"
                else
                    echo "ERROR: unknown detecting expr: ${EXPR}"
                    echo "Supported: timer, fsize, interval, frkern"
                    exit 1
                fi
                ;;
        esac

        CUT_P="${CUT_P:-0.995}"
        GAUGE="${GAUGE:-sub_scalar}"
        NTESTS="${NTESTS:-100}"
        NTILES="${NTILES:-100}"
        NP_LIST="${NP_LIST:-64}"
        MU_LIST="${MU_LIST//,/ }"
        MU_LIST="${MU_LIST//，/ }"

        case "${ARCH}" in
            x86_64)
                TIMER_LIST="${TIMER_LIST:-clock_gettime mpi_wtime tsc tsc_asym papi papix6 likwid}"
                ;;
            aarch64)
                TIMER_LIST="${TIMER_LIST:-clock_gettime mpi_wtime cntvct cntvct_fence cntvcto papi papix6}"
                ;;
            *)
                TIMER_LIST="${TIMER_LIST:-clock_gettime mpi_wtime papi papix6}"
                ;;
        esac
        FILTERED_TIMER_LIST=""
        for timer in ${TIMER_LIST}; do
            case "${timer}" in
                papi|papix6)
                    [ "${USE_PAPI:-0}" = "1" ] && FILTERED_TIMER_LIST="${FILTERED_TIMER_LIST} ${timer}"
                    ;;
                likwid)
                    [ "${USE_LIKWID:-0}" = "1" ] && FILTERED_TIMER_LIST="${FILTERED_TIMER_LIST} ${timer}"
                    ;;
                *)
                    FILTERED_TIMER_LIST="${FILTERED_TIMER_LIST} ${timer}"
                    ;;
            esac
        done
        TIMER_LIST="${FILTERED_TIMER_LIST}"

        if [ -n "${WALK_ARG}" ] && [ -f "${WALK_ARG}" ]; then
            WALK_ROOT="$(cd "$(dirname "${WALK_ARG}")" && pwd)"
        elif [ -n "${WALK_ARG}" ]; then
            WALK_ROOT="${WALK_ARG}"
        else
            WALK_ROOT="${WALK_ROOT:-${SCRIPT_DIR}/walklists}"
        fi

        resolve_walk_list(){
            local mu_ns="$1"
            local found
            found="$(find "${WALK_ROOT}" -maxdepth 1 -type f \
                -name "detecting_walking_list_*_tbase${mu_ns}.csv" \
                | sort | head -n 1)"
            if [ -z "${found}" ] || [ ! -s "${found}" ]; then
                echo "ERROR: walking list for interval/tbase ${mu_ns} ns not found in ${WALK_ROOT}" >&2
                echo "Expected filename like: detecting_walking_list_Normal_n100_tbase${mu_ns}.csv" >&2
                echo "Generate first, e.g.:" >&2
                echo "  $0 0 \"${MU_LIST}\" 0 100" >&2
                exit 1
            fi
            printf '%s\n' "${found}"
        }

        SRC_DIR="${PROJ_ROOT}/src/partes"
        BINARY="${BINARY:-${SRC_DIR}/partes-mpi.x}"

        if [ "${BUILD:-1}" = "1" ]; then
            pushd "${SRC_DIR}" >/dev/null
            make clean
            make -j USE_PAPI="${USE_PAPI:-0}" USE_LIKWID="${USE_LIKWID:-0}"
            popd >/dev/null
        fi

        if [ ! -x "${BINARY}" ]; then
            echo "ERROR: binary not executable: ${BINARY}"
            exit 1
        fi

        mkdir -p "${DATA_FOLDER}"
        cp -f "$0" "${DATA_FOLDER}/$(basename "$0")"

        echo "HOSTNAME: ${HOSTNAME}"
        echo "ARCH: ${ARCH}"
        echo "EXPR: ${EXPR}"
        echo "EXPR_NAME: ${EXPR_NAME}"
        echo "DATA_FOLDER: ${DATA_FOLDER}"
        echo "WALK_ROOT: ${WALK_ROOT}"
        echo "BINARY: ${BINARY}"
        echo "NP_LIST: ${NP_LIST}"
        echo "MU_LIST: ${MU_LIST}"
        echo "TIMER_LIST: ${TIMER_LIST}"
        echo "FKERN_LIST: ${FKERN_LIST}"
        echo "RKERN_LIST: ${RKERN_LIST}"
        echo "FSIZE_LIST: ${FSIZE_LIST}"
        echo "RSIZE_LIST: ${RSIZE_LIST}"
        echo "NTESTS: ${NTESTS}"
        echo "NTILES: ${NTILES}"
        echo "CUT_P: ${CUT_P}"
        echo "GAUGE: ${GAUGE}"
        mkdir -p "${DATA_FOLDER}/walklists"

        for NP in ${NP_LIST}; do
            for timer in ${TIMER_LIST}; do
                for mu_ns in ${MU_LIST}; do
                    WALK_LIST="$(resolve_walk_list "${mu_ns}")"
                    cp -f "${WALK_LIST}" "${DATA_FOLDER}/walklists/$(basename "${WALK_LIST}")"
                    for fkern in ${FKERN_LIST}; do
                        for rkern in ${RKERN_LIST}; do
                            for fsize_kib in ${FSIZE_LIST}; do
                                for rsize_kib in ${RSIZE_LIST}; do
                                combo_dir="${DATA_FOLDER}/${EXPR_NAME}/np${NP}/${timer}/interval${mu_ns}_fkern${fkern}_rkern${rkern}_fsize${fsize_kib}_rsize${rsize_kib}"
                                mkdir -p "${combo_dir}"

                                {
                                    echo "expr=${EXPR}"
                                    echo "expr_name=${EXPR_NAME}"
                                    echo "host=${HOSTNAME}"
                                    echo "arch=${ARCH}"
                                    echo "np=${NP}"
                                    echo "interval_ns=${mu_ns}"
                                    echo "timer=${timer}"
                                    echo "fkern=${fkern}"
                                    echo "rkern=${rkern}"
                                    echo "fsize_kib=${fsize_kib}"
                                    echo "rsize_kib=${rsize_kib}"
                                    echo "ntests=${NTESTS}"
                                    echo "ntiles=${NTILES}"
                                    echo "cut_p=${CUT_P}"
                                    echo "gauge=${GAUGE}"
                                    echo "binary=${BINARY}"
                                    echo "walk_list=${WALK_LIST}"
                                    sha256sum "${WALK_LIST}" 2>/dev/null || true
                                } > "${combo_dir}/meta.txt"

                                walk_id=0
                                while IFS=, read -r c1 c2 c3 extra; do
                                    c1="${c1%$'\r'}"
                                    c2="${c2%$'\r'}"
                                    c3="${c3%$'\r'}"

                                    [ -z "${c1}" ] && continue
                                    case "${c1}" in
                                        ns|walk_idx|ta|\#*) continue ;;
                                    esac

                                    if [ -n "${c3}" ]; then
                                        walk_id="${c1}"
                                        ta="${c2}"
                                        tb="${c3}"
                                    else
                                        walk_id=$((walk_id + 1))
                                        ta="${c1}"
                                        tb="${c1}"
                                    fi

                                    run_dir="$(printf "%s/w%04d_ta%s" "${combo_dir}" "${walk_id}" "${ta}")"
                                    mkdir -p "${run_dir}"

                                    echo ">>> expr=${EXPR} np=${NP} timer=${timer} interval=${mu_ns} fkern=${fkern} rkern=${rkern} fsize=${fsize_kib} rsize=${rsize_kib} walk=${walk_id} ta=${ta} tb=${tb}"
                                    (
                                        cd "${run_dir}"
                                        mpirun --map-by core --bind-to core -np "${NP}" "${BINARY}" \
                                            --ta "${ta}" \
                                            --tb "${tb}" \
                                            --ntests "${NTESTS}" \
                                            --ntiles "${NTILES}" \
                                            --cut-p "${CUT_P}" \
                                            --gauge "${GAUGE}" \
                                            --timer "${timer}" \
                                            --fkern-a "${fkern}" --fsize-a "${fsize_kib}" \
                                            --rkern-a "${rkern}" --rsize-a "${rsize_kib}" \
                                            > run.log 2>&1
                                    )
                                done < "${WALK_LIST}"
                                done
                            done
                        done
                    done
                done
            done
        done

        echo "Done. Results under: ${DATA_FOLDER}"
        ;;
    *)
        exit 1
        ;;
esac
