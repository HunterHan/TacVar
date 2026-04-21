#!/bin/bash -e
pkill -u $(whoami) -9 "mpirun" || true
pkill -u $(whoami) -9 "python3" || true

################################################################################
# !!! CPU frequency lock (cpupower) !!!
#
# Purpose:
# - Lock CPU scaling governor/frequency via `cpupower` for stable timing.
# - On exit (success/failure/CTRL-C), always switch back to `schedutil`.
#
# Notes:
# - Requires `cpupower` and usually `sudo` privileges.
# - Best-effort: failures WARN but will NOT abort the experiment.
# - Optional env overrides:
#   - CPU_LOCK (default: 1; set 0 to disable locking)
#   - CPU_GOV_LOCK (default: performance)
#   - CPU_GOV_UNLOCK (default: schedutil)
#   - CPU_FREQ_MIN (e.g. 2.0GHz)
#   - CPU_FREQ_MAX (e.g. 3.5GHz)
################################################################################

CPU_LOCK="${CPU_LOCK:-1}"
CPU_GOV_LOCK="${CPU_GOV_LOCK:-performance}"
CPU_GOV_UNLOCK="${CPU_GOV_UNLOCK:-schedutil}"
CPU_FREQ_MIN="${CPU_FREQ_MIN:-}"
CPU_FREQ_MAX="${CPU_FREQ_MAX:-}"

_pt_cpu_unlock_schedutil() {
  command -v cpupower >/dev/null 2>&1 || return 0
  sudo -n true >/dev/null 2>&1 || true
  sudo cpupower frequency-set -g "${CPU_GOV_UNLOCK}" >/dev/null 2>&1 || true
}

_pt_cpu_lock_performance() {
  if ! command -v cpupower >/dev/null 2>&1; then
    echo "[WARN] cpupower not found; skip CPU governor lock."
    return 0
  fi

  # Try non-interactive sudo first to avoid hanging on a password prompt.
  if ! sudo -n true >/dev/null 2>&1; then
    echo "[WARN] sudo (non-interactive) not available; skip cpupower lock."
    return 0
  fi

  local args=()
  [[ -n "${CPU_GOV_LOCK}" ]] && args+=( -g "${CPU_GOV_LOCK}" )
  [[ -n "${CPU_FREQ_MIN}" ]] && args+=( -d "${CPU_FREQ_MIN}" )
  [[ -n "${CPU_FREQ_MAX}" ]] && args+=( -u "${CPU_FREQ_MAX}" )

  if sudo cpupower frequency-set "${args[@]}" >/dev/null 2>&1; then
    echo "[INFO] cpupower locked: gov=${CPU_GOV_LOCK} min=${CPU_FREQ_MIN:-<unchanged>} max=${CPU_FREQ_MAX:-<unchanged>}"
  else
    echo "[WARN] cpupower frequency-set failed; running without CPU lock."
  fi
  return 0
}

if [[ "${CPU_LOCK}" != "0" ]]; then
  trap _pt_cpu_unlock_schedutil EXIT INT TERM
  _pt_cpu_lock_performance
else
  echo "[INFO] CPU_LOCK=0: skip cpupower lock."
fi

date


# check last user
# { last -n 20; last | grep still; }

# Run ParTES with per-walk (ta==tb) driven by a Normal distribution.
# One shared walk list per run:
#   scripts/output/<host>/<expr>/<timestamp>/walk_list_normal.csv
# Per combo: <timestamp>/<timer>/<combo>/meta.md and <timer>_walks/...
EXPR_NAME="detecting.expr1.timer.fixed"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${SCRIPT_DIR}/../src/partes"
BINARY="${SRC_DIR}/partes-mpi.x"

# Toolchains / deps (PAPI/LIKWID/MPI/OpenBLAS, etc.)
set +u
source "${SCRIPT_DIR}/../env.bash"
set -u

# ====== Timer list filtering based on env switches ======
# env.bash exports USE_PAPI / USE_LIKWID (0/1). If a dep is missing, skip its timers here.
_pt_filter_timers() {
  local in_list="$1"
  local out=()
  local t
  for t in ${in_list}; do
    case "${t}" in
      papi|papix6)
        [[ "${USE_PAPI:-0}" == "1" ]] && out+=("${t}") || true
        ;;
      likwid)
        [[ "${USE_LIKWID:-0}" == "1" ]] && out+=("${t}") || true
        ;;
      *)
        out+=("${t}")
        ;;
    esac
  done
  printf '%s' "${out[*]}"
}

# ====== User-configurable parameters ======
EXPR_NAME="${EXPR_NAME:-partes_expr1_fsize}"
EXPR_ID="${EXPR_ID:-}"
NP="${NP:-64}"
GAUGE="${GAUGE:-sub_scalar}"

# Interval sweep (combo path labels): space-separated list in ns.
# Default is single 10000.
MU_LIST="${MU_LIST:-10000}"
# Single OUT_ROOT walk list: gen_walklist --dist fixed --fixed-ns WALK_MU_NS (default: first token of MU_LIST).
WALK_MU_NS="${WALK_MU_NS:-}"
# Number of walks in gen_walklist.py (--dist fixed); default 1.
NWALKS="${NWALKS:-1}"

# Kernel search spaces (defaults kept minimal; can override via environment).
FKERN_LIST="${FKERN_LIST:-copy}"
RKERN_LIST="${RKERN_LIST:-none}"

# TIMER_LIST: space-separated list of timers, e.g. "clock_gettime mpi_wtime".
# If not set, fall back to single TIMER or default clock_gettime.
TIMER="${TIMER:-tsc_asym clock_gettime mpi_wtime papi papix6 likwid}"
TIMER_LIST="${TIMER_LIST:-${TIMER}}"
TIMER_LIST="$(_pt_filter_timers "${TIMER_LIST}")"
echo "[INFO] USE_PAPI=${USE_PAPI:-0} USE_LIKWID=${USE_LIKWID:-0} => TIMER_LIST='${TIMER_LIST}'"
echo "[INFO] MU_LIST='${MU_LIST}'"
if [[ -z "${WALK_MU_NS}" ]]; then
  WALK_MU_NS="$(echo ${MU_LIST} | awk '{print $1}')"
fi
echo "[INFO] WALK_MU_NS='${WALK_MU_NS}' (single walk_list_normal.csv at OUT_ROOT, --dist fixed --fixed-ns)"
echo "[INFO] NWALKS='${NWALKS}'"
echo "[INFO] FKERN_LIST='${FKERN_LIST}' RKERN_LIST='${RKERN_LIST}'"

FSIZE="${FSIZE:-0}"      # Single value is not used directly; fsize sweep is defined separately.
RSIZE="${RSIZE:-0}"      # KiB
NTESTS="${NTESTS:-100}"
NTILES="${NTILES:-100}"
CUT_P="${CUT_P:-0.995}"

# Front-kernel fsize sweep list (KiB)
# FSIZE_LIST=(32 64 128 256 512 1024 2048 4096)
FSIZE_LIST=(2048)

# Walk list: --dist fixed at OUT_ROOT with --fixed-ns WALK_MU_NS and NWALKS (see gen_walklist.py).

# ====== Output folder with timestamp ======
echo "Set output folder"
TS="$(date +%Y%m%d_%H%M%S)"
HOSTNAME="$(hostname)"
OUT_ROOT="${SCRIPT_DIR}/output/${HOSTNAME}/${EXPR_NAME}/${TS}"
mkdir -p "${OUT_ROOT}"

# ====== Archive script + capture logs into OUT_ROOT ======
RUN_LOG="${OUT_ROOT}/run_detecting.log"
ARCHIVE_DIR="${OUT_ROOT}/_archive"
mkdir -p "${ARCHIVE_DIR}"

# Save an exact copy of the runner + key configs/scripts used by this run.
cp -f "${BASH_SOURCE[0]}" "${ARCHIVE_DIR}/run_detecting_expr1_timer_fixed10000_0416.sh"
cp -f "${SCRIPT_DIR}/../env.bash" "${ARCHIVE_DIR}/env.bash" || true
cp -f "${SCRIPT_DIR}/../utils/gen_walklist.py" "${ARCHIVE_DIR}/gen_walklist.py" || true

# From here on, mirror all output to a log file for this batch.
exec > >(tee -a "${RUN_LOG}") 2>&1
echo "Run log: ${RUN_LOG}"
echo "Command: $0 $*"
date
pwd
uname -a || true

echo "Output root: ${OUT_ROOT}"

# ====== Build ======
pushd "${SRC_DIR}" >/dev/null
export PAPI_PREFIX="${PAPI_PREFIX:-${PAPI_HOME:-}}"
export LIKWID_PREFIX="${LIKWID_PREFIX:-${LIKWID_HOME:-}}"
export USE_LIKWID="${USE_LIKWID:-1}"

# gpns fitting slope mode:
#   0: original method
#   1: robust slope (skip non-monotonic steps)
ROBUST_GPNS_SLOPE="${ROBUST_GPNS_SLOPE:-0}"
# Force using the MPI wrapper compiler (avoid conda CC hijack).
make clean
if [[ "${USE_MPI:-0}" == "1" && -x "${MPI_HOME:-}/bin/mpicc" ]]; then
  make -j CC="${MPI_HOME}/bin/mpicc" USE_PAPI="${USE_PAPI:-0}" USE_LIKWID="${USE_LIKWID:-0}" ROBUST_GPNS_SLOPE="${ROBUST_GPNS_SLOPE}"
else
  make -j USE_PAPI="${USE_PAPI:-0}" USE_LIKWID="${USE_LIKWID:-0}" ROBUST_GPNS_SLOPE="${ROBUST_GPNS_SLOPE}"
fi
popd >/dev/null

# ====== Capture CPU info from lscpu (architecture + model name, locale-agnostic) ======
LSCPU_OUT="$(lscpu 2>/dev/null || true)"
LSCPU_ARCH_LINE="$(printf '%s\n' \"${LSCPU_OUT}\" | grep -m1 -E '^(Architecture:|架构)' || true)"
LSCPU_MODEL_LINE="$(printf '%s\n' \"${LSCPU_OUT}\" | grep -m1 -E '^(Model name:|型号)' || true)"

# ====== Single shared walk list (OUT_ROOT); all timers/combos read this file ======
SHARED_WALK_LIST="${OUT_ROOT}/walk_list_normal.csv"
SHARED_WALK_META="${OUT_ROOT}/walk_list_meta.txt"
if [[ -z "${WALK_MU_NS}" ]]; then
  WALK_MU_NS="$(echo ${MU_LIST} | awk '{print $1}')"
fi
_n_mu_words="$(echo ${MU_LIST} | wc -w | tr -d ' ')"
if [[ "${_n_mu_words}" -gt 1 ]]; then
  echo "[WARN] MU_LIST has multiple values; every combo still uses the same ${SHARED_WALK_LIST} generated with WALK_MU_NS=${WALK_MU_NS}. Override with env WALK_MU_NS=..."
fi
python3 "${SCRIPT_DIR}/../utils/gen_walklist.py" \
  --dist fixed \
  --nwalks "${NWALKS}" \
  --fixed-ns "${WALK_MU_NS}" \
  --out "${SHARED_WALK_LIST}" \
  --meta-out "${SHARED_WALK_META}"
walk_count="$(python3 - <<'PY' "${SHARED_WALK_LIST}"
import csv, sys
with open(sys.argv[1], newline="") as f:
    r = csv.DictReader(f)
    print(sum(1 for _ in r))
PY
)"
echo "[INFO] Shared walk list: ${SHARED_WALK_LIST} (${walk_count} walks)"

# ====== Helpers ======
die() { echo "ERROR: $*" >&2; exit 1; }

check_csvs() {
  local run_dir="$1"
  local np="$2"

  # Rank 0 should always produce these:
  [[ -s "${run_dir}/partes_ta_cdf.csv" ]] || return 1
  [[ -s "${run_dir}/partes_tb_cdf.csv" ]] || return 1

  # Each rank produces per-step files.
  for ((r=0; r<np; r++)); do
    [[ -s "${run_dir}/partes_ta_r${r}.csv" ]] || return 1
    [[ -s "${run_dir}/partes_tb_r${r}.csv" ]] || return 1
  done
  return 0
}

run_one_walk() {
  local walk_idx="$1"
  local ta="$2"
  local tb="$3"
  local timer="$4"
  local combo_root="$5"
  local fkern="$6"
  local rkern="$7"
  local fsize_kib="$8"
  local interval_ns="$9"

  local walks_dir="${combo_root}/${timer}_walks"
  mkdir -p "${walks_dir}"

  local run_dir
  run_dir="$(printf "%s/w%04d_ta%d" "${walks_dir}" "${walk_idx}" "${ta}")"
  mkdir -p "${run_dir}"

  local core_list
  core_list="$(seq -s, 0 $((NP - 1)))"

  echo ""
  echo ">>> timer=${timer} interval=${interval_ns} fkern=${fkern} rkern=${rkern} fsize=${fsize_kib} walk ${walk_idx}: ta=${ta} tb=${tb} -> ${run_dir}"

  (
    cd "${run_dir}"
    set -x
    mpirun --map-by core --bind-to core -np "${NP}" \
      taskset -c "${core_list}" \
      "${BINARY}" \
        --ta "${ta}" \
        --tb "${tb}" \
        --ntests "${NTESTS}" \
        --ntiles "${NTILES}" \
        --cut-p "${CUT_P}" \
        --gauge "${GAUGE}" \
        --timer "${timer}" \
        --fkern-a "${fkern}" --fsize-a "${fsize_kib}" \
        --fkern-b "${fkern}" --fsize-b "${fsize_kib}" \
        --rkern-a "${rkern}" --rsize-a "${RSIZE}" \
        --rkern-b "${rkern}" --rsize-b "${RSIZE}" \
      >run.log 2>&1 < /dev/null
  )

  # No sleep: mpirun returned; outputs should be present in run_dir.
  if ! check_csvs "${run_dir}" "${NP}"; then
    echo "Run log (tail):"
    tail -n 80 "${run_dir}/run.log" || true
    die "Missing expected CSV outputs in ${run_dir}"
  fi
}

for timer in ${TIMER_LIST}; do
  for mu_ns in ${MU_LIST}; do
    for fkern in ${FKERN_LIST}; do
      for rkern in ${RKERN_LIST}; do
        for fsize_kib in "${FSIZE_LIST[@]}"; do
          combo="interval${mu_ns}_fkern${fkern}_rkern${rkern}_fsize${fsize_kib}"
          combo_root="${OUT_ROOT}/${timer}/${combo}"
          mkdir -p "${combo_root}"
          WALK_LIST="${SHARED_WALK_LIST}"
          META_TXT="${SHARED_WALK_META}"
          META_MD="${combo_root}/meta.md"

          {
            echo "## TacVar ParTES experiment meta"
            echo
            echo "| Key | Value |"
            echo "|---|---|"
            echo "| expr_name | \`${EXPR_NAME}\` |"
            echo "| expr_id | \`${EXPR_ID:-}\` |"
            echo "| timestamp | \`${TS}\` |"
            echo "| output_root | \`${OUT_ROOT}\` |"
            echo "| timer | \`${timer}\` |"
            echo "| interval_ns | \`${mu_ns}\` |"
            echo "| walk_fixed_ns | \`${WALK_MU_NS}\` |"
            echo "| nwalks | \`${NWALKS}\` |"
            echo "| mu_list | \`${MU_LIST}\` |"
            echo "| fkern | \`${fkern}\` |"
            echo "| fkern_list | \`${FKERN_LIST}\` |"
            echo "| rkern | \`${rkern}\` |"
            echo "| rkern_list | \`${RKERN_LIST}\` |"
            echo "| fsize_kib | \`${fsize_kib}\` |"
            echo "| walk_list | \`${SHARED_WALK_LIST}\` |"
            echo "| walk_list_meta | \`${SHARED_WALK_META}\` |"
            echo "| meta_txt | \`${META_TXT}\` |"
            echo "| binary | \`${BINARY}\` |"
            echo "| np | \`${NP}\` |"
            echo "| gauge | \`${GAUGE}\` |"
            echo "| ntests | \`${NTESTS}\` |"
            echo "| ntiles | \`${NTILES}\` |"
            echo "| cut-p | \`${CUT_P}\` |"
            echo "| lscpu_arch_line | \`${LSCPU_ARCH_LINE}\` |"
            echo "| lscpu_model_line | \`${LSCPU_MODEL_LINE}\` |"
          } > "${META_MD}"

          echo ""
          echo "=== Running all walks for timer=${timer}, interval=${mu_ns} ns, fkern=${fkern}, rkern=${rkern}, fsize=${fsize_kib} KiB ==="
          idx=0
          while IFS=, read -r walk_idx ta_ns tb_ns; do
            if [[ "${walk_idx}" == "walk_idx" ]]; then
              continue
            fi
            walk_idx="${walk_idx%$'\r'}"
            ta_ns="${ta_ns%$'\r'}"
            tb_ns="${tb_ns%$'\r'}"
            idx=$((idx+1))
            run_one_walk "${walk_idx}" "${ta_ns}" "${tb_ns}" "${timer}" "${combo_root}" "${fkern}" "${rkern}" "${fsize_kib}" "${mu_ns}"
            echo "Completed ${idx}/${walk_count} for timer=${timer}, interval=${mu_ns} ns, fkern=${fkern}, rkern=${rkern}, fsize=${fsize_kib} KiB"
          done < "${WALK_LIST}"
        done
      done
    done
  done
done

echo ""
echo "All done. Results under: ${OUT_ROOT}"

# check last user
{ last -n 20; last | grep still; }
