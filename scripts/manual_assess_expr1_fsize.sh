#!/bin/bash
set -euo pipefail

################################################################################
# Manual Fig6 / assess.expr1.fsize runner for compute nodes.
#
# Run this only after:
#   1. af309 generated codex_assets/walklists with
#        scripts/prepare_assess_walklists_af309.sh
#   2. the whole TacVar checkout was uploaded to this compute node.
#
# This wrapper only sets a convenient manual environment and then calls
# scripts/run_assess_expr1_fsize.sh.  The called script must read pre-generated
# walk lists from codex_assets/walklists; missing lists are fatal.
################################################################################

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  cat <<'EOF'
Usage:
  cd ~/code/TacVar
  source env.bash
  NP_LIST="64 128" DATE_BASE=20260521 NWALKS=3 NTESTS=100 NTILES=100 \
    scripts/manual_assess_expr1_fsize.sh

Common overrides:
  NP_LIST="64 128"
  TIMER_LIST="tsc clock_gettime mpi_wtime"
  TIMER_LIST="cntvct cntvcto clock_gettime mpi_wtime"
  DATE_BASE=20260521
  OUT_BASE=~/code/data
  OUTPUT_DIR=outputAssessing
  FSIZE_LIST_OVERRIDE="16 32 64 128 256 512 1024 2048 4096 8192"
  MU_LIST=10000
  WALK_MU_NS=10000
  NWALKS=3
  NTESTS=100
  NTILES=100
EOF
  exit 0
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

HOST_SHORT="$(hostname -s 2>/dev/null || hostname)"
ASSESS_ALLOWED_HOSTS="${ASSESS_ALLOWED_HOSTS:-c920bn3 camd9554n2 cgnr6760pn2}"
if [[ "${ASSESS_ALLOW_OTHER_HOST:-0}" != "1" && " ${ASSESS_ALLOWED_HOSTS} " != *" ${HOST_SHORT} "* ]]; then
  echo "ERROR: this wrapper is allowed only on compute nodes by default." >&2
  echo "  current host: ${HOST_SHORT}" >&2
  echo "  allowed hosts: ${ASSESS_ALLOWED_HOSTS}" >&2
  echo "Set ASSESS_ALLOW_OTHER_HOST=1 only for deliberate debugging." >&2
  exit 3
fi

cd "${REPO_DIR}"
set +u
source "${REPO_DIR}/env.bash"
set -u

export PYTHON="${PYTHON:-${HOME}/miniconda3/bin/python}"
export DATE_BASE="${DATE_BASE:-$(date +%Y%m%d)}"
export OUT_BASE="${OUT_BASE:-${HOME}/code/data}"
export OUTPUT_DIR="${OUTPUT_DIR:-outputAssessing}"
export MU_LIST="${MU_LIST:-10000}"
export WALK_MU_NS="${WALK_MU_NS:-10000}"
export SIGMA_REL="${SIGMA_REL:-0.015}"
export NWALKS="${NWALKS:-20}"
export NTESTS="${NTESTS:-20}"
export NTILES="${NTILES:-20}"
export FSIZE_LIST_OVERRIDE="${FSIZE_LIST_OVERRIDE:-16 32 64 128 256 512 1024 2048 4096 8192}"
export FKERN_LIST="${FKERN_LIST:-copy}"
export RKERN_LIST="${RKERN_LIST:-none}"
export CPU_LOCK="${CPU_LOCK:-1}"
export ASSESS_KILL_STALE="${ASSESS_KILL_STALE:-0}"

if [[ -z "${TIMER_LIST:-}" ]]; then
  case "$(uname -m)" in
    x86_64) export TIMER_LIST="tsc clock_gettime mpi_wtime" ;;
    aarch64) export TIMER_LIST="cntvct cntvcto clock_gettime mpi_wtime" ;;
    *) export TIMER_LIST="clock_gettime mpi_wtime" ;;
  esac
fi

NP_LIST="${NP_LIST:-64}"

echo "[manual assess expr1 fsize]"
echo "  host=$(hostname)"
echo "  repo=${REPO_DIR}"
echo "  date_base=${DATE_BASE}"
echo "  out_base=${OUT_BASE}"
echo "  np_list=${NP_LIST}"
echo "  timers=${TIMER_LIST}"
echo "  nwalks=${NWALKS} ntests=${NTESTS} ntiles=${NTILES}"
echo "  fsize(KiB)=${FSIZE_LIST_OVERRIDE}"

for np_now in ${NP_LIST}; do
  export NP="${np_now}"
  export EXPR_NAME="${EXPR_NAME_PREFIX:-assess.expr1.fsize}.np${np_now}"
  echo ""
  echo "===== manual run ${EXPR_NAME} NP=${NP} $(date) ====="
  "${SCRIPT_DIR}/run_assess_expr1_fsize.sh"
done
