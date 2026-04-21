#!/bin/bash
set -euo pipefail

# Run ParTES with per-walk (ta==tb) driven by a Normal distribution.
# Outputs are organized as:
#   scripts/output/<timestamp>/
#     walk_list_normal.csv
#     meta.txt
#     walks/w0000_ta<ns>/
#       partes_*.csv
#       run.log

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${SCRIPT_DIR}/../src/partes"
BINARY="${SRC_DIR}/partes-mpi.x"

# ====== User-configurable parameters ======
EXPR_NAME="${EXPR_NAME:-partes_expr1_fsize}"
EXPR_ID="${EXPR_ID:-}"
NP="${NP:-8}"
GAUGE="${GAUGE:-sub_scalar}"

# TIMER_LIST: space-separated list of timers, e.g. \"clock_gettime mpi_wtime\".
# If not set, fall back to single TIMER or default clock_gettime.
TIMER="${TIMER:-clock_gettime mpi_wtime tsc_asym}"
TIMER_LIST="${TIMER_LIST:-${TIMER}}"

FKERN="copy"
FSIZE="${FSIZE:-0}"      # Single value is not used directly; fsize sweep is defined separately.
RKERN="${RKERN:-none}"
RSIZE="${RSIZE:-0}"      # KiB
NTESTS="${NTESTS:-1000}"
NTILES="${NTILES:-100}"
CUT_P="${CUT_P:-1.0}"

# Front-kernel fsize sweep list (KiB)
# FKERN_LIST=(copy add scale triad pow dgemm)
# RKERN_LIST=(copy add scale triad pow dgemm)

FKERN_LIST=(copy add scale triad pow dgemm)
RKERN_LIST=(copy add scale triad pow dgemm)

# This expr will use a fixed single base time (1000ns) in walk_list; BASE_NS / SIGMA / NWALKS / SEED are not used.

# ====== Output folder with timestamp ======
TS="$(date +%Y%m%d_%H%M%S)"
OUT_ROOT="${SCRIPT_DIR}/output/${TS}"
mkdir -p "${OUT_ROOT}"

WALK_LIST="${OUT_ROOT}/walk_list_normal.csv"
META_TXT="${OUT_ROOT}/meta.txt"
META_MD="${OUT_ROOT}/meta.md"

echo "Output root: ${OUT_ROOT}"

# ====== Build ======
pushd "${SRC_DIR}" >/dev/null
make -j
popd >/dev/null

# ====== Generate walk list (single base time: 1000ns) ======
python3 - <<'PY' "${WALK_LIST}" "${META_TXT}"
import csv
import os
import sys

walk_list, meta_file = sys.argv[1:]
vals = [1000]  # single base time in ns

os.makedirs(os.path.dirname(walk_list), exist_ok=True)
with open(walk_list, "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["walk_idx", "ta_ns", "tb_ns"])
    for i, ns in enumerate(vals):
        w.writerow([i, ns, ns])

with open(meta_file, "w", encoding="utf-8") as f:
    f.write("walk_list_type=fixed\n")
    f.write("values=1000\n")
print(f"Wrote {len(vals)} walks to {walk_list}")
PY

echo "Walk list: ${WALK_LIST}"

# ====== Capture CPU info from lscpu (architecture + model name, locale-agnostic) ======
LSCPU_OUT="$(lscpu 2>/dev/null || true)"
LSCPU_ARCH_LINE="$(printf '%s\n' \"${LSCPU_OUT}\" | grep -m1 -E '^(Architecture:|架构)' || true)"
LSCPU_MODEL_LINE="$(printf '%s\n' \"${LSCPU_OUT}\" | grep -m1 -E '^(Model name:|型号)' || true)"

# ====== Write meta.md (human-readable config) ======
{
  echo "## TacVar ParTES experiment meta"
  echo
  echo "| Key | Value |"
  echo "|---|---|"
  echo "| expr_name | \`${EXPR_NAME}\` |"
  echo "| expr_id | \`${EXPR_ID:-}\` |"
  echo "| timestamp | \`${TS}\` |"
  echo "| output_root | \`${OUT_ROOT}\` |"
  echo "| walk_list | \`${WALK_LIST}\` |"
  echo "| meta_txt | \`${META_TXT}\` |"
  echo "| timer_list | \`${TIMER_LIST}\` |"
  echo "| binary | \`${BINARY}\` |"
  echo
  echo "### Walk-list (fixed base time)"
  echo
  echo "| Parameter | Value |"
  echo "|---|---|"
  echo "| type | \`fixed\` |"
  echo "| values (ns) | \`1000\` |"
  echo
  echo "### Fixed ParTES / MPI config"
  echo
  echo "| Parameter | Value |"
  echo "|---|---|"
  echo "| np | \`${NP}\` |"
  echo "| timers | \`${TIMER_LIST}\` |"
  echo "| gauge | \`${GAUGE}\` |"
  echo "| fkern | \`${FKERN}\` |"
  echo "| fkern_list | \`${FKERN_LIST[*]}\` |"
  echo "| rkern_list | \`${RKERN_LIST[*]}\` |"
  echo "| rkern | \`${RKERN}\` |"
  echo "| rsize (KiB) | \`${RSIZE}\` |"
  echo "| ntests | \`${NTESTS}\` |"
  echo "| ntiles | \`${NTILES}\` |"
  echo "| cut-p | \`${CUT_P}\` |"
  echo
  echo "### Runtime environment"
  echo
  echo "| Item | Value |"
  echo "|---|---|"
  echo "| hostname | \`$(hostname 2>/dev/null || echo unknown)\` |"
  echo "| user | \`$(whoami 2>/dev/null || echo unknown)\` |"
  echo "| pwd | \`$(pwd)\` |"
  echo "| kernel | \`$(uname -sr 2>/dev/null || echo unknown)\` |"
  echo "| lscpu_arch_line | \`${LSCPU_ARCH_LINE}\` |"
  echo "| lscpu_model_line | \`${LSCPU_MODEL_LINE}\` |"
  echo
  echo "### Command template (per walk)"
  echo
  echo '```'
  echo "mpirun --map-by core --bind-to core -np ${NP} taskset -c 0..$((NP-1)) \\"
  echo "  ${BINARY} --ta <walk_ta> --tb <walk_tb> --timer <timer> \\"
  echo "  --ntests ${NTESTS} --ntiles ${NTILES} --cut-p ${CUT_P} \\"
  echo "  --gauge ${GAUGE} \\"
  echo "  --fkern-a ${FKERN} --fsize-a ${FSIZE} --fkern-b ${FKERN} --fsize-b ${FSIZE} \\"
  echo "  --rkern-a ${RKERN} --rsize-a ${RSIZE} --rkern-b ${RKERN} --rsize-b ${RSIZE}"
  echo '```'
} > "${META_MD}"

echo "Meta (md): ${META_MD}"

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
  local fsize_kib="$5"

  # Per-timer, per-fsize walks directory, e.g. clock_gettime_fsize32_walks/
  local timer_root="${OUT_ROOT}/${timer}"
  local walks_dir="${timer_root}/${timer}_fsize${fsize_kib}_walks"
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
        --fkern-a "${FKERN}" --fsize-a "${fsize_kib}" \
        --fkern-b "${FKERN}" --fsize-b "${fsize_kib}" \
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

# ====== Iterate walks ======
python3 - <<'PY' "${WALK_LIST}"
import csv, sys
path = sys.argv[1]
with open(path, newline="") as f:
    r = csv.DictReader(f)
    rows = list(r)
print(len(rows))
PY

walk_count="$(python3 - <<'PY' "${WALK_LIST}"
import csv, sys
with open(sys.argv[1], newline="") as f:
    r = csv.DictReader(f)
    print(sum(1 for _ in r))
PY
)"

for timer in ${TIMER_LIST}; do
  for fkern in "${FKERN_LIST[@]}"; do
    for rkern in "${RKERN_LIST[@]}"; do
      echo ""
      echo "=== Running all walks for timer=${timer}, fkern=${fkern}, rkern=${rkern} ==="
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
        run_one_walk "${walk_idx}" "${ta_ns}" "${tb_ns}" "${timer}" "${fkern}" "${rkern}" "${fsize_kib}"
        echo "Completed ${idx}/${walk_count} for timer=${timer}, fkern=${fkern}, rkern=${rkern}, fsize_kib=${fsize_kib}"
      done < "${WALK_LIST}"
    done
  done
done

echo ""
echo "All done. Results under: ${OUT_ROOT}"

