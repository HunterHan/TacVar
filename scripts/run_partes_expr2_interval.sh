#!/bin/bash
set -euo pipefail

# Run ParTES expr2 (interval): sweep timers × interval (normal mean via --mu-ns).
#
# Outputs are organized as:
#   scripts/output_expr1_fsize/<timestamp>/
#     <timer>/               # one subdirectory per timer
#       mu<mu_ns>/           # interval sweep dimension (normal mean)
#         walk_list_normal.csv
#         meta.txt
#         meta.md
#         <timer>_walks/
#           w0000_ta<ns>/
#             partes_*.csv
#             run.log

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${SCRIPT_DIR}/../src/partes"
BINARY="${SRC_DIR}/partes-mpi.x"

# ====== User-configurable parameters ======
EXPR_NAME="${EXPR_NAME:-partes_expr1_timer_fsize}"
EXPR_ID="${EXPR_ID:-}"
NP="${NP:-8}"
GAUGE="${GAUGE:-sub_scalar}"

# TIMER_LIST: space-separated list of timers, e.g. "clock_gettime mpi_wtime tsc_asym".
# If not set, fall back to single TIMER or default clock_gettime.
TIMER="${TIMER:-clock_gettime mpi_wtime tsc_asym}"
TIMER_LIST="${TIMER_LIST:-${TIMER}}"

# Interval sweep (normal mean): space-separated list in ns. Default is single 10000.
MU_LIST="${MU_LIST:-10000}"

FKERN="${FKERN:-copy}"
FSIZE="${FSIZE:-0}"      # KiB
RKERN="${RKERN:-none}"
RSIZE="${RSIZE:-0}"      # KiB
NTESTS="${NTESTS:-1000}"
NTILES="${NTILES:-100}"
CUT_P="${CUT_P:-1.0}"

# ====== Output folder with timestamp (separate from expr1_timer) ======
TS="$(date +%Y%m%d_%H%M%S)"
OUT_ROOT="${SCRIPT_DIR}/output_expr1_timer_interval/${TS}"
mkdir -p "${OUT_ROOT}"

echo "Output root: ${OUT_ROOT}"

# ====== Build ======
pushd "${SRC_DIR}" >/dev/null
make -j
popd >/dev/null

# ====== Capture CPU info from lscpu (architecture + model name, locale-agnostic) ======
LSCPU_OUT="$(lscpu 2>/dev/null || true)"
LSCPU_ARCH_LINE="$(printf '%s\n' "${LSCPU_OUT}" | grep -m1 -E '^(Architecture:|架构)' || true)"
LSCPU_MODEL_LINE="$(printf '%s\n' "${LSCPU_OUT}" | grep -m1 -E '^(Model name:|型号)' || true)"

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
  local mu_ns="$5"

  # Per-timer, per-mu walks directory.
  local timer_root="${OUT_ROOT}/${timer}/mu${mu_ns}"
  local walks_dir="${timer_root}/${timer}_walks"
  mkdir -p "${walks_dir}"

  local run_dir
  run_dir="$(printf "%s/w%04d_ta%d" "${walks_dir}" "${walk_idx}" "${ta}")"
  mkdir -p "${run_dir}"

  local core_list
  core_list="$(seq -s, 0 $((NP - 1)))"

  echo ""
  echo ">>> timer=${timer} walk ${walk_idx}: ta=${ta} tb=${tb} -> ${run_dir}"

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
        --fkern-a "${FKERN}" --fsize-a "${FSIZE}" \
        --fkern-b "${FKERN}" --fsize-b "${FSIZE}" \
        --rkern-a "${RKERN}" --rsize-a "${RSIZE}" \
        --rkern-b "${RKERN}" --rsize-b "${RSIZE}" \
      >run.log 2>&1 < /dev/null
  )

  # No sleep: mpirun returned; outputs should be present in run_dir.
  if ! check_csvs "${run_dir}" "${NP}"; then
    echo "Run log (tail):"
    tail -n 80 "${run_dir}/run.log" || true
    die "Missing expected CSV outputs in ${run_dir}"
  fi
}

# ====== Iterate walks (per timer, per mu) ======
for timer in ${TIMER_LIST}; do
  for mu_ns in ${MU_LIST}; do
    timer_mu_root="${OUT_ROOT}/${timer}/mu${mu_ns}"
    mkdir -p "${timer_mu_root}"
    WALK_LIST="${timer_mu_root}/walk_list_normal.csv"
    META_TXT="${timer_mu_root}/meta.txt"
    META_MD="${timer_mu_root}/meta.md"

    python3 "${SCRIPT_DIR}/../utils/gen_walklist.py" \
      --out "${WALK_LIST}" \
      --meta-out "${META_TXT}" \
      --mu-ns "${mu_ns}"

    walk_count="$(python3 - <<'PY' "${WALK_LIST}"
import csv, sys
with open(sys.argv[1], newline="") as f:
    r = csv.DictReader(f)
    print(sum(1 for _ in r))
PY
)"

    {
      echo "## TacVar ParTES experiment meta (expr2 interval)"
      echo
      echo "| Key | Value |"
      echo "|---|---|"
      echo "| expr_name | \`${EXPR_NAME}\` |"
      echo "| expr_id | \`${EXPR_ID:-}\` |"
      echo "| timestamp | \`${TS}\` |"
      echo "| output_root | \`${OUT_ROOT}\` |"
      echo "| timer | \`${timer}\` |"
      echo "| mu_ns | \`${mu_ns}\` |"
      echo "| mu_list | \`${MU_LIST}\` |"
      echo "| walk_list | \`${WALK_LIST}\` |"
      echo "| meta_txt | \`${META_TXT}\` |"
      echo "| binary | \`${BINARY}\` |"
      echo "| np | \`${NP}\` |"
      echo "| gauge | \`${GAUGE}\` |"
      echo "| fkern | \`${FKERN}\` |"
      echo "| fsize (KiB) | \`${FSIZE}\` |"
      echo "| rkern | \`${RKERN}\` |"
      echo "| rsize (KiB) | \`${RSIZE}\` |"
      echo "| ntests | \`${NTESTS}\` |"
      echo "| ntiles | \`${NTILES}\` |"
      echo "| cut-p | \`${CUT_P}\` |"
      echo "| lscpu_arch_line | \`${LSCPU_ARCH_LINE}\` |"
      echo "| lscpu_model_line | \`${LSCPU_MODEL_LINE}\` |"
    } > "${META_MD}"

    echo ""
    echo "=== Running all walks for timer=${timer}, mu=${mu_ns} ns ==="
    idx=0
    while IFS=, read -r walk_idx ta_ns tb_ns; do
      # Skip header
      if [[ "${walk_idx}" == "walk_idx" ]]; then
        continue
      fi
      # Defensive: strip Windows CR if present
      walk_idx="${walk_idx%$'\r'}"
      ta_ns="${ta_ns%$'\r'}"
      tb_ns="${tb_ns%$'\r'}"
      idx=$((idx+1))
      run_one_walk "${walk_idx}" "${ta_ns}" "${tb_ns}" "${timer}" "${mu_ns}"
      echo "Completed ${idx}/${walk_count} for timer=${timer}, mu=${mu_ns} ns"
    done < "${WALK_LIST}"
  done
done

echo ""
echo "All done. Results under: ${OUT_ROOT}"

