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

conda deactivate
conda deactivate

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${SCRIPT_DIR}/../src/partes"
BINARY="${SRC_DIR}/partes-mpi.x"

# ====== User-configurable parameters ======
EXPR_NAME="${EXPR_NAME:-partes_expr1_timers}"
EXPR_ID="${EXPR_ID:-}"
NP="${NP:-8}"
GAUGE="${GAUGE:-sub_scalar}"

# ====== MPI selection (use your own OpenMPI by default) ======
# Override via environment if needed, e.g.:
#   MPI_PREFIX=~/opt/openmpi-5.0.10 NP=16 ./run_partes_expr1_timer.sh
MPI_PREFIX="${MPI_PREFIX:-${HOME}/opt/openmpi-5.0.10}"
MPIRUN="${MPIRUN:-${MPI_PREFIX}/bin/mpirun}"
MPICC="${MPICC:-${MPI_PREFIX}/bin/mpicc}"

# Ensure our MPI is found first for build/runtime (also helps when MPI dlopens libs).
export PATH="${MPI_PREFIX}/bin:${PATH}"
export LD_LIBRARY_PATH="${MPI_PREFIX}/lib:${MPI_PREFIX}/lib64:${LD_LIBRARY_PATH:-}"
export OPAL_PREFIX="${MPI_PREFIX}"

# ====== PAPI selection ======
# Your PAPI is installed under ~/opt/papi (bin at ~/opt/papi/bin).
# We prepend its bin/lib paths so both build-time and runtime can find it.
PAPI_PREFIX="${PAPI_PREFIX:-${HOME}/opt/papi}"
export PATH="${PAPI_PREFIX}/bin:${PATH}"
export LD_LIBRARY_PATH="${PAPI_PREFIX}/lib:${PAPI_PREFIX}/lib64:${LD_LIBRARY_PATH:-}"

# TIMER_LIST: space-separated list of timers, e.g. \"clock_gettime mpi_wtime\".
# If not set, fall back to single TIMER or default clock_gettime.
TIMER="${TIMER:-clock_gettime mpi_wtime tsc_asym}"
TIMER_LIST="${TIMER_LIST:-${TIMER}}"

FKERN="${FKERN:-copy}"
FSIZE="${FSIZE:-0}"      # KiB
RKERN="${RKERN:-none}"
RSIZE="${RSIZE:-0}"      # KiB
NTESTS="${NTESTS:-1000}"
NTILES="${NTILES:-100}"
CUT_P="${CUT_P:-1.0}"

# Normal distribution for ta==tb:
#   ta = tb = round(BASE_NS * (1 + N(0, SIGMA)))
# SIGMA is relative stddev (e.g., 0.015 == 1.5%).
BASE_NS="${BASE_NS:-10000}"
SIGMA="${SIGMA:-0.015}"
NWALKS="${NWALKS:-50}"
SEED="${SEED:-}"

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
make -j CC="${MPICC}"
popd >/dev/null

# ====== Generate walk list (ta==tb per walk) ======
python3 - <<'PY' "${WALK_LIST}" "${META_TXT}" "${BASE_NS}" "${SIGMA}" "${NWALKS}" "${SEED}"
import csv
import math
import os
import random
import sys
import time

walk_list, meta_file, base_ns_s, sigma_s, nwalks_s, seed_s = sys.argv[1:]
base_ns = int(base_ns_s)
sigma = float(sigma_s)
nwalks = int(nwalks_s)

if seed_s.strip():
    seed = int(seed_s)
else:
    seed = int(time.time_ns() % (2**31 - 1))

rng = random.Random(seed)

def draw_ns():
    # Relative perturbation; clip to keep positive times.
    factor = 1.0 + rng.gauss(0.0, sigma)
    if factor <= 0.0:
        factor = 1e-6
    return max(1, int(round(base_ns * factor)))

vals = [draw_ns() for _ in range(nwalks)]

os.makedirs(os.path.dirname(walk_list), exist_ok=True)
with open(walk_list, "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["walk_idx", "ta_ns", "tb_ns"])
    for i, ns in enumerate(vals):
        w.writerow([i, ns, ns])

with open(meta_file, "w", encoding="utf-8") as f:
    f.write(f"seed={seed}\n")
    f.write(f"base_ns={base_ns}\n")
    f.write(f"sigma={sigma}\n")
    f.write(f"nwalks={nwalks}\n")
    f.write("definition: ta=tb=round(base_ns*(1+N(0,sigma))) with clipping to keep ta>0\n")
print(f"Wrote {nwalks} walks to {walk_list} (seed={seed})")
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
  echo "### Walk-list generation (Normal)"
  echo
  echo "| Parameter | Value |"
  echo "|---|---|"
  echo "| base_ns | \`${BASE_NS}\` |"
  echo "| sigma (relative) | \`${SIGMA}\` |"
  echo "| nwalks | \`${NWALKS}\` |"
  echo "| seed (optional) | \`${SEED:-auto}\` |"
  echo
  echo "### Fixed ParTES / MPI config"
  echo
  echo "| Parameter | Value |"
  echo "|---|---|"
  echo "| np | \`${NP}\` |"
  echo "| timers | \`${TIMER_LIST}\` |"
  echo "| gauge | \`${GAUGE}\` |"
  echo "| fkern | \`${FKERN}\` |"
  echo "| fsize (KiB) | \`${FSIZE}\` |"
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
  echo "| mpi_prefix | \`${MPI_PREFIX}\` |"
  echo "| mpicc | \`${MPICC}\` |"
  echo "| mpirun | \`${MPIRUN}\` |"
  echo "| mpirun_version | \`$(${MPIRUN} --version 2>/dev/null | head -n 1 || echo unknown)\` |"
  echo "| mpicc_version | \`$(${MPICC} --version 2>/dev/null | head -n 1 || echo unknown)\` |"
  echo "| ompi_info_version | \`$(${MPI_PREFIX}/bin/ompi_info --version 2>/dev/null | head -n 1 || echo n/a)\` |"
  echo "| libmpi_path | \`$(ldconfig -p 2>/dev/null | grep -m1 -E 'libmpi\\.so' | awk '{print $NF}' || echo unknown)\` |"
  echo
  echo "### Command template (per walk)"
  echo
  echo '```'
  echo "${MPIRUN} --map-by core --bind-to core -np ${NP} taskset -c 0..$((NP-1)) \\"
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

  # Per-timer walks directory, e.g. clock_gettime_walks/, mpi_wtime_walks/
  local timer_root="${OUT_ROOT}/${timer}"
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
    "${MPIRUN}" --map-by core --bind-to core -np "${NP}" \
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
  echo ""
  echo "=== Running all walks for timer: ${timer} ==="
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
    run_one_walk "${walk_idx}" "${ta_ns}" "${tb_ns}" "${timer}"
    echo "Completed ${idx}/${walk_count} for timer=${timer}"
  done < "${WALK_LIST}"
done

echo ""
echo "All done. Results under: ${OUT_ROOT}"

