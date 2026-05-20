#!/bin/bash -e
# Assess ta-only variant generated from run_detecting_expr3_frkern.sh
set -euo pipefail
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
# One shared walk list for the whole run (all timers / combos read the same file):
#   scripts/output/<host>/<expr>/<timestamp>/walk_list_normal.csv
#   scripts/output/<host>/<expr>/<timestamp>/walk_list_meta.txt   (gen_walklist.py output)
# Per combo:
#   <timestamp>/<timer>/<combo>/meta.md
#   <timestamp>/<timer>/<combo>/<timer>_walks/w####_ta<ns>/...
EXPR_NAME="${EXPR_NAME:-assess.expr3.frkern}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${SCRIPT_DIR}/../src/partes"
BINARY="${SRC_DIR}/assess-mpi.x"

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
NP="${NP:-128}"
GAUGE="${GAUGE:-sub_scalar}"

# Interval sweep (normal mean for gen_walklist.py): space-separated list in ns.
# Default is single 10000.
MU_LIST="${MU_LIST:-10000}"
# Single walk list at OUT_ROOT uses --mu-ns WALK_MU_NS (default: first token of MU_LIST).
WALK_MU_NS="${WALK_MU_NS:-}"

# Kernel search spaces (defaults kept minimal; can override via environment).
FKERN_LIST="${FKERN_LIST:-copy add scale triad pow dgemm}"
RKERN_LIST="${RKERN_LIST:-copy add scale triad pow dgemm}"

# TIMER_LIST: space-separated list of timers, e.g. "clock_gettime mpi_wtime".
# If not set, fall back to single TIMER or default clock_gettime.
case "$(uname -m)" in
  x86_64) DEFAULT_TIMER_LIST="tsc tsc_asym clock_gettime mpi_wtime papi papix6 likwid" ;;
  aarch64) DEFAULT_TIMER_LIST="cntvct cntvct_fence cntvcto clock_gettime mpi_wtime papi papix6 likwid" ;;
  *) DEFAULT_TIMER_LIST="clock_gettime mpi_wtime papi papix6 likwid" ;;
esac
TIMER="${TIMER:-${DEFAULT_TIMER_LIST}}"
TIMER_LIST="${TIMER_LIST:-${TIMER}}"
TIMER_LIST="$(_pt_filter_timers "${TIMER_LIST}")"
echo "[INFO] USE_PAPI=${USE_PAPI:-0} USE_LIKWID=${USE_LIKWID:-0} => TIMER_LIST='${TIMER_LIST}'"
echo "[INFO] MU_LIST='${MU_LIST}'"
if [[ -z "${WALK_MU_NS}" ]]; then
  WALK_MU_NS="$(echo ${MU_LIST} | awk '{print $1}')"
fi
echo "[INFO] WALK_MU_NS='${WALK_MU_NS}' (single walk_list_normal.csv at OUT_ROOT)"
echo "[INFO] FKERN_LIST='${FKERN_LIST}' RKERN_LIST='${RKERN_LIST}'"

FSIZE="${FSIZE:-0}"      # Single value is not used directly; fsize sweep is defined separately.
RSIZE_LIST=(${RSIZE_LIST[@]:-0})
NTESTS="${NTESTS:-100}"
NTILES="${NTILES:-100}"
CUT_P="${CUT_P:-0.995}"
NWALKS="${NWALKS:-100}"
SIGMA_REL="${SIGMA_REL:-0.015}"

# Front-kernel fsize sweep list (KiB)
FSIZE_LIST=(4096)
# FSIZE_LIST=(2048)

if [[ -n "${FSIZE_LIST_OVERRIDE:-}" ]]; then
  read -r -a FSIZE_LIST <<< "${FSIZE_LIST_OVERRIDE}"
fi

# This expr will use a fixed single base time (1000ns) in walk_list; BASE_NS / SIGMA / NWALKS / SEED are not used.

# ====== Output folder with timestamp ======
echo "Set output folder"
TS="$(date +%Y%m%d_%H%M%S)"
DATE_BASE="${DATE_BASE:-$(date +%Y%m%d)}"
HOSTNAME="$(hostname)"
OUTPUT_DIR="${OUTPUT_DIR:-outputAssessing}"
OUT_BASE="${OUT_BASE:-${DATA_ROOT:-${SCRIPT_DIR}/output}}"
OUT_ROOT="${OUT_BASE}/${DATE_BASE}/${HOSTNAME}/${OUTPUT_DIR}/${EXPR_NAME}/${TS}"
mkdir -p "${OUT_ROOT}"

# ====== Archive script + capture logs into OUT_ROOT ======
RUN_LOG="${OUT_ROOT}/run_assess.log"
ARCHIVE_DIR="${OUT_ROOT}/_archive"
mkdir -p "${ARCHIVE_DIR}"

# Save an exact copy of the runner + key configs/scripts used by this run.
cp -f "${BASH_SOURCE[0]}" "${ARCHIVE_DIR}/$(basename "${BASH_SOURCE[0]}")"
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
  --out "${SHARED_WALK_LIST}" \
  --meta-out "${SHARED_WALK_META}" \
  --dist normal \
  --mu-ns "${WALK_MU_NS}" \
  --sigma-rel "${SIGMA_REL}" \
  --nwalks "${NWALKS}"
walk_count="$(python3 - <<'PY' "${SHARED_WALK_LIST}"
import csv, sys
with open(sys.argv[1], newline="") as f:
    r = csv.DictReader(f)
    print(sum(1 for _ in r))
PY
)"
echo "[INFO] Shared walk list: ${SHARED_WALK_LIST} (${walk_count} walks)"
print_assess_overview() {
  echo ""
  echo "[Assess Overview]"
  echo "  expr_name: ${EXPR_NAME}"
  echo "  host: ${HOSTNAME}"
  echo "  output_root: ${OUT_ROOT}"
  echo "  binary: ${BINARY}"
  echo "  timers: ${TIMER_LIST}"
  echo "  mu_list(ns): ${MU_LIST}"
  echo "  fkern_list: ${FKERN_LIST}"
  echo "  rkern_list: ${RKERN_LIST}"
  echo "  fsize_list(KiB): ${FSIZE_LIST[*]}"
  echo "  np=${NP} ntests=${NTESTS} ntiles=${NTILES} cut_p=${CUT_P}"
  echo "  nwalks: ${NWALKS}"
  echo "  walk_distribution: N(0, sigma_rel=${SIGMA_REL}) + tbase"
  echo "  walk_count: ${walk_count}"
  echo ""
}
print_assess_overview


# ====== Helpers ======
die() { echo "ERROR: $*" >&2; exit 1; }

check_csvs() {
  local run_dir="$1"
  local np="$2"

  [[ -s "${run_dir}/assess_ta_cdf.csv" || -s "${run_dir}/partes_ta_cdf.csv" ]] || return 1

  for ((r=0; r<np; r++)); do
    [[ -s "${run_dir}/assess_ta_r${r}.csv" || -s "${run_dir}/partes_ta_r${r}.csv" ]] || return 1
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
  local rsize_kib="$9"
  local interval_ns="${10}"

  local walks_dir="${combo_root}/${timer}_walks"
  mkdir -p "${walks_dir}"

  local run_dir
  run_dir="$(printf "%s/w%04d_ta%d" "${walks_dir}" "${walk_idx}" "${ta}")"
  mkdir -p "${run_dir}"

  local core_list
  core_list="$(seq -s, 0 $((NP - 1)))"

  echo ""
  echo ">>> timer=${timer} interval=${interval_ns} fkern=${fkern} rkern=${rkern} fsize=${fsize_kib} rsize=${rsize_kib} walk ${walk_idx}: ta=${ta} tb=${tb} -> ${run_dir}"

  (
    cd "${run_dir}"
    set -x
    mpirun --map-by core --bind-to core -np "${NP}" \
      taskset -c "${core_list}" \
      "${BINARY}" \
        --ta "${ta}" \
        --tb "${ta}" \
        --ntests "${NTESTS}" \
        --ntiles "${NTILES}" \
        --cut-p "${CUT_P}" \
        --gauge "${GAUGE}" \
        --timer "${timer}" \
        --fkern-a "${fkern}" --fsize-a "${fsize_kib}" \
        --rkern-a "${rkern}" --rsize-a "${rsize_kib}" \
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
          for rsize_kib in "${RSIZE_LIST[@]}"; do
            combo="interval${mu_ns}_fkern${fkern}_rkern${rkern}_fsize${fsize_kib}_rsize${rsize_kib}"

            combo_root="${OUT_ROOT}/${timer}/${combo}"
            mkdir -p "${combo_root}"
            WALK_LIST="${SHARED_WALK_LIST}"
            META_TXT="${SHARED_WALK_META}"
            META_MD="${combo_root}/meta.md"

            {
            echo "## TacVar assess experiment meta"
            echo
            echo "| Key | Value |"
            echo "|---|---|"
            echo "| expr_name | \`${EXPR_NAME}\` |"
            echo "| expr_id | \`${EXPR_ID:-}\` |"
            echo "| timestamp | \`${TS}\` |"
            echo "| output_root | \`${OUT_ROOT}\` |"
            echo "| timer | \`${timer}\` |"
            echo "| interval_ns | \`${mu_ns}\` |"
            echo "| walk_mu_ns | \`${WALK_MU_NS}\` |"
            echo "| walk_dist | \`N(0, sigma_rel=${SIGMA_REL}) + tbase\` |"
            echo "| mu_list | \`${MU_LIST}\` |"
            echo "| fkern | \`${fkern}\` |"
            echo "| fkern_list | \`${FKERN_LIST}\` |"
            echo "| rkern | \`${rkern}\` |"
            echo "| rkern_list | \`${RKERN_LIST}\` |"
            echo "| fsize_kib | \`${fsize_kib}\` |"
            echo "| rsize_kib | \`${rsize_kib}\` |"
            echo "| rsize_list | \`${RSIZE_LIST[*]}\` |"
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
            echo "=== Running all walks for timer=${timer}, interval=${mu_ns} ns, fkern=${fkern}, rkern=${rkern}, fsize=${fsize_kib} KiB, rsize=${rsize_kib} KiB ==="
            idx=0
            while IFS=, read -r walk_idx ta_ns tb_ns; do
              if [[ "${walk_idx}" == "walk_idx" ]]; then
                continue
              fi
              walk_idx="${walk_idx%$'\r'}"
              ta_ns="${ta_ns%$'\r'}"
              tb_ns="${tb_ns%$'\r'}"
              idx=$((idx+1))
              run_one_walk "${walk_idx}" "${ta_ns}" "${tb_ns}" "${timer}" "${combo_root}" "${fkern}" "${rkern}" "${fsize_kib}" "${rsize_kib}" "${mu_ns}"
              [[ "${VERBOSE_WALKS:-0}" == "1" ]] && echo "Completed walk ${idx}/${walk_count} for timer=${timer}, interval=${mu_ns} ns, fkern=${fkern}, rkern=${rkern}, fsize=${fsize_kib} KiB, rsize=${rsize_kib} KiB"
            done < "${WALK_LIST}"
          done
        done
      done
    done
  done
done

echo ""
echo "All done. Results under: ${OUT_ROOT}"

# check last user
{ last -n 20; last | grep still; }
