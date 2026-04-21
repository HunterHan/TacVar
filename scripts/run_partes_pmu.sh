#!/bin/bash
set -euo pipefail

echo "Configuration"
# ---------- configuration ----------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${SCRIPT_DIR}/../src/partes"
DATA_ROOT="${SCRIPT_DIR}/../data"
DATE_DIR="${DATA_ROOT}/$(date +%Y%m%d)"
mkdir -p "${DATE_DIR}"


# TA_LIST / TB_LIST: two modes
# Mode 1: geometric from 1000, multiply by 10 four times -> 1000 10000 100000 1000000
# Mode 2: manual specification -> 10000
# Uncomment the mode you want:

# --- Mode 1: geometric 10x from 1000, 4 steps ---
# TA_LIST=(1000 10000 100000 1000000)
TA_LIST=(10000)

# --- Mode 2: manual ---
# TA_LIST=(10000)

# NP_LIST=(1 2 4 8 16 32 64 128)
# GAUGE_LIST=(sub_scalar sub_scalar_2p sub_scalar_3p fma_avx2 fma_scalar fma_avx512 )
# FKERN_LIST=(none copy scale add triad pow dgemm)
# FSIZE_LIST=(0 16 32 512 1024 16384 32768)

# ---------- auto-detect CPU and configure ----------
detect_cpu_profile() {
  echo "in Detect"
  local cpu_model vendor_id
  cpu_model=Kunpeng
  # cpu_model=$(lscpu | grep -iE "Model name|型号名称" | sed 's/.*[：:][ \t]*//' | xargs)
  vendor_id=$(lscpu | grep -iE "Vendor ID|厂商 ID|厂商ID" | sed 's/.*[：:][ \t]*//' | xargs)

  echo "=== Detected CPU: ${cpu_model} ==="
  echo "=== Detected Vendor: ${vendor_id} ==="

  if echo "${cpu_model}" | grep -qiE "EPYC|AMD.*9[0-9]{3}" ; then
      # AMD EPYC / Zen (e.g. AMD 9554)
      echo "=== Using AMD EPYC profile ==="
      NP_LIST=(1 8 64)
      GAUGE_LIST=(sub_scalar sub_scalar_2p sub_scalar_3p fma_scalar)
      FKERN_LIST=(copy)
      FSIZE_LIST=(0)
      local fsize_val=32
      while [ ${fsize_val} -le 32768 ]; do
          FSIZE_LIST+=(${fsize_val})
          fsize_val=$((fsize_val * 2))
      done
      RKERN_LIST=(none)
      RSIZE_LIST=(0)
      TIMER_LIST=(clock_gettime mpi_wtime)
  else
      # Huawei Kunpeng 920 / HiSilicon ARMv8
      echo "=== Using Kunpeng 920 profile ==="
      NP_LIST=(1 8 32 64)
      GAUGE_LIST=(sub_scalar sub_scalar_2p sub_scalar_3p fma_scalar)
      FKERN_LIST=(copy)
      FSIZE_LIST=(0)
      local fsize_val=32
      while [ ${fsize_val} -le 57344 ]; do
          FSIZE_LIST+=(${fsize_val})
          fsize_val=$((fsize_val * 2))
      done
      RKERN_LIST=(none)
      RSIZE_LIST=(0)
      TIMER_LIST=(clock_gettime mpi_wtime)


  # elif echo "${cpu_model}" | grep -qi "Xeon\|Intel"; then
  #     # Intel Xeon
  #     echo "=== Using Intel Xeon profile ==="
  #     NP_LIST=(1 2 4 8 16 32 64 128)
  #     GAUGE_LIST=(sub_scalar sub_scalar_2p sub_scalar_3p fma_scalar fma_avx2 fma_avx512)
  #     FKERN_LIST=(none copy scale add triad)
  #     FSIZE_LIST=(0 32 512 1024 16384 32768)
  #     TIMER_LIST=(clock_gettime mpi_wtime tsc_asym)

  # else
  #     # Fallback: conservative defaults
  #     echo "=== Unknown CPU, using fallback profile ==="
  #     NP_LIST=(1 2 4 8 16 32 64)
  #     GAUGE_LIST=(sub_scalar sub_scalar_2p sub_scalar_3p fma_scalar)
  #     FKERN_LIST=(none copy scale add triad)
  #     FSIZE_LIST=(0 32 1024 32768)
  #     TIMER_LIST=(clock_gettime mpi_wtime)
  fi

  # Auto-cap NP_LIST to the number of online CPUs
  local ncpus
  ncpus=$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 128)
  local capped_np=()
  for np in "${NP_LIST[@]}"; do
      if [ "${np}" -le "${ncpus}" ]; then
          capped_np+=("${np}")
      fi
  done
  if [ ${#capped_np[@]} -gt 0 ]; then
      NP_LIST=("${capped_np[@]}")
  fi

  echo "=== Configuration ==="
  echo "    TA_LIST:    ${TA_LIST[*]}  (tb=ta)"
  echo "    NP_LIST:    ${NP_LIST[*]}"
  echo "    GAUGE_LIST: ${GAUGE_LIST[*]}"
  echo "    FKERN_LIST: ${FKERN_LIST[*]}"
  echo "    FSIZE_LIST: ${FSIZE_LIST[*]}"
  echo "    RKERN_LIST: ${RKERN_LIST[*]}"
  echo "    RSIZE_LIST: ${RSIZE_LIST[*]}"
  echo "    TIMER_LIST: ${TIMER_LIST[*]}"
}

echo "Detect"
detect_cpu_profile

echo "Build"
# ---------- build ----------
pushd "${SRC_DIR}" > /dev/null
make -j
popd > /dev/null

BINARY="${SRC_DIR}/partes-mpi.x"

# ---------- helper: kill leftover MPI / partes processes ----------
cleanup_procs() {
    # kill any lingering partes-mpi.x or mpirun owned by current user
    pkill -u "$(whoami)" -f "partes-mpi.x" 2>/dev/null || true
    pkill -u "$(whoami)" -f "mpirun" 2>/dev/null || true
    sleep 0.5
}

# ---------- main loop ----------
# ---------- main loop ----------
TOTAL=0
for np in "${NP_LIST[@]}"; do
  for gauge in "${GAUGE_LIST[@]}"; do
    for fkern in "${FKERN_LIST[@]}"; do
      for fsize in "${FSIZE_LIST[@]}"; do
        for rkern in "${RKERN_LIST[@]}"; do
          for rsize in "${RSIZE_LIST[@]}"; do
            for timer in "${TIMER_LIST[@]}"; do
              for ta in "${TA_LIST[@]}"; do
                TOTAL=$((TOTAL + 1))
              done
            done
          done
        done
      done
    done
  done
done

NP_TOTAL=${#NP_LIST[@]}
GAUGE_TOTAL=${#GAUGE_LIST[@]}
FKERN_TOTAL=${#FKERN_LIST[@]}
FSIZE_TOTAL=${#FSIZE_LIST[@]}
RKERN_TOTAL=${#RKERN_LIST[@]}
RSIZE_TOTAL=${#RSIZE_LIST[@]}
TIMER_TOTAL=${#TIMER_LIST[@]}
TA_TOTAL=${#TA_LIST[@]}

DONE=0
NP_IDX=0
for np in "${NP_LIST[@]}"; do
  NP_IDX=$((NP_IDX + 1))
  GAUGE_IDX=0
  for gauge in "${GAUGE_LIST[@]}"; do
    GAUGE_IDX=$((GAUGE_IDX + 1))
    FKERN_IDX=0
    for fkern in "${FKERN_LIST[@]}"; do
      FKERN_IDX=$((FKERN_IDX + 1))
      FSIZE_IDX=0
      for fsize in "${FSIZE_LIST[@]}"; do
        FSIZE_IDX=$((FSIZE_IDX + 1))
        RKERN_IDX=0
        for rkern in "${RKERN_LIST[@]}"; do
          RKERN_IDX=$((RKERN_IDX + 1))
          RSIZE_IDX=0
          for rsize in "${RSIZE_LIST[@]}"; do
            RSIZE_IDX=$((RSIZE_IDX + 1))
            TIMER_IDX=0
            for timer in "${TIMER_LIST[@]}"; do
              TIMER_IDX=$((TIMER_IDX + 1))
              TA_IDX=0
              for ta in "${TA_LIST[@]}"; do
                TA_IDX=$((TA_IDX + 1))
    tb="${ta}"
    DONE=$((DONE + 1))
    REMAINING=$((TOTAL - DONE))

    RUN_TAG="np${np}_ta${ta}_tb${tb}_gauge-${gauge}_fkern-${fkern}_fsize-${fsize}_rkern-${rkern}_rsize-${rsize}_timer-${timer}"
    RUN_DIR="${DATE_DIR}/${RUN_TAG}"
    mkdir -p "${RUN_DIR}"

    echo ""
    echo ">>> [${DONE}/${TOTAL}] ${RUN_TAG}  (remaining: ${REMAINING})"
    echo "    np: ${NP_IDX}/${NP_TOTAL} | gauge: ${GAUGE_IDX}/${GAUGE_TOTAL} | fkern: ${FKERN_IDX}/${FKERN_TOTAL} | fsize: ${FSIZE_IDX}/${FSIZE_TOTAL} | rkern: ${RKERN_IDX}/${RKERN_TOTAL} | rsize: ${RSIZE_IDX}/${RSIZE_TOTAL} | timer: ${TIMER_IDX}/${TIMER_TOTAL} | ta: ${TA_IDX}/${TA_TOTAL}"

    # build the core list "0,1,...,np-1"
    CORE_LIST=$(seq -s, 0 $((np - 1)))

    # print the full command before execution
    CMD=(mpirun --map-by core --bind-to core -np "${np}"
      taskset -c "${CORE_LIST}"
      "${BINARY}"
        --ta "${ta}"
        --tb "${tb}"
        --gauge "${gauge}"
        --timer "${timer}"
        --fkern-a "${fkern}"
        --fsize-a "${fsize}"
        --fkern-b "${fkern}"
        --fsize-b "${fsize}"
        --rkern-a "${rkern}"
        --rsize-a "${rsize}"
        --rkern-b "${rkern}"
        --rsize-b "${rsize}")
    echo "    CMD: ${CMD[*]}"

    if ! time "${CMD[@]}"; then
      echo "!!! FAILED: ${RUN_TAG}"
      sleep 1
      sync
      cleanup_procs
      # still try to salvage any partial csv output
      CSV_FILES=("${SRC_DIR}"/*.csv)
      if [ -e "${CSV_FILES[0]}" ]; then
        mv "${SRC_DIR}"/*.csv "${RUN_DIR}/"
        echo "    -> (partial) results saved to ${RUN_DIR}"
      fi
      continue
    fi

    # wait for filesystem buffers to flush before touching anything
    sync
    sleep 1
    sync

    # move generated csv files into the run-specific directory
    # csvs are expected to appear in src/partes/
    # retry up to 10 times (total ~10s) in case files appear with delay
    CSV_FOUND=0
    for _retry in $(seq 1 4); do
      CSV_FILES=("${SRC_DIR}"/*.csv)
      if [ -e "${CSV_FILES[0]}" ]; then
        CSV_FOUND=1
        break
      fi
      echo "    -> waiting for csv files (attempt ${_retry}/4)..."
      sleep 1
      sync
    done

    if [ "${CSV_FOUND}" -eq 1 ]; then
      mv "${SRC_DIR}"/*.csv "${RUN_DIR}/"
      echo "    -> results saved to ${RUN_DIR}"
    else
      # Check if the binary writes to a different directory (e.g. current working dir)
      CWD_CSV=(*.csv)
      if [ -e "${CWD_CSV[0]}" ]; then
        mv ./*.csv "${RUN_DIR}/"
        echo "    -> results saved to ${RUN_DIR} (found in cwd)"
      else
        echo "    -> WARNING: no csv files found for ${RUN_TAG}"
        echo "       Searched in: ${SRC_DIR} and $(pwd)"
        ls -la "${SRC_DIR}"/ | tail -20 || true
      fi
    fi

    # kill leftover processes only after csv files have been saved
    cleanup_procs

              done
            done
          done
        done
      done
    done
  done
done
