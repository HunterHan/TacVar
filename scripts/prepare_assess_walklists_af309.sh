#!/bin/bash
set -euo pipefail

# Generate shared assess/detecting walking lists on af309 before uploading TacVar
# to compute nodes. The run scripts on compute nodes look for these files under
# codex_assets/walklists and fail if they are missing, unless
# ASSESS_ALLOW_LOCAL_WALKLIST=1 is set explicitly for debugging.
#
# Typical manual Fig6/fsize use:
#   cd ~/code/TacVar
#   EXPR_LIST="assess.expr1.fsize.np64 assess.expr1.fsize.np128" \
#   MU_LIST="10000" NWALKS=3 SIGMA_REL=0.015 \
#     scripts/prepare_assess_walklists_af309.sh
#
# If EXPR_LIST is omitted, ASSESS_EXPR1_FSIZE_NP_LIST can append convenient
# np-specific fsize names to the default list, e.g.:
#   ASSESS_EXPR1_FSIZE_NP_LIST="64 128" NWALKS=3 scripts/prepare_assess_walklists_af309.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

HOST="$(hostname)"
if [[ "${HOST}" != "af309" ]]; then
  echo "[WARN] This script is intended to be run on af309; current host=${HOST}" >&2
fi

# EXPR_LIST="${EXPR_LIST:-assess.expr1.timer assess.expr1.fsize assess.expr2.interval assess.expr3.frkern partes_expr1_fsize}"
EXPR_LIST="${EXPR_LIST:-assess.expr1.fsize}"
if [[ -n "${ASSESS_EXPR1_FSIZE_NP_LIST:-}" ]]; then
  for np in ${ASSESS_EXPR1_FSIZE_NP_LIST}; do
    EXPR_LIST="${EXPR_LIST} assess.expr1.fsize.np${np}"
  done
fi
MU_LIST="${MU_LIST:-10000}"
NWALKS="${NWALKS:-20}"
SIGMA_REL="${SIGMA_REL:-0.015}"
ASSESS_WALKLIST_ROOT="${ASSESS_WALKLIST_ROOT:-${REPO_DIR}/codex_assets/walklists}"

source "${SCRIPT_DIR}/assess_walklist_common.sh"

mkdir -p "${ASSESS_WALKLIST_ROOT}"
echo "[INFO] repo=${REPO_DIR}"
echo "[INFO] walklist_root=${ASSESS_WALKLIST_ROOT}"
echo "[INFO] expr_list=${EXPR_LIST}"
echo "[INFO] mu_list=${MU_LIST}"
echo "[INFO] nwalks=${NWALKS} sigma_rel=${SIGMA_REL}"

for expr in ${EXPR_LIST}; do
  EXPR_NAME="${expr}"
  for mu_ns in ${MU_LIST}; do
    out_dir="$(assess_resolve_pregen_walklist "${EXPR_NAME}" "${mu_ns}" "${NWALKS}" "${SIGMA_REL}")"
    out_csv="${out_dir}/walk_list_normal.csv"
    out_meta="${out_dir}/walk_list_meta.txt"
    mkdir -p "${out_dir}"
    "$(assess_python)" "${REPO_DIR}/utils/gen_walklist.py" \
      --out "${out_csv}" \
      --meta-out "${out_meta}" \
      --dist normal \
      --mu-ns "${mu_ns}" \
      --sigma-rel "${SIGMA_REL}" \
      --nwalks "${NWALKS}"
    {
      echo "generated_on_host=${HOST}"
      echo "generated_at=$(date +%Y-%m-%d_%H:%M:%S)"
      echo "repo=${REPO_DIR}"
      echo "expr_name=${EXPR_NAME}"
      echo "mu_ns=${mu_ns}"
      echo "nwalks=${NWALKS}"
      echo "sigma_rel=${SIGMA_REL}"
    } >> "${out_meta}"
    count="$(assess_count_walks "${out_csv}")"
    sha="$(sha256sum "${out_csv}" | awk '{print $1}')"
    echo "sha256=${sha}" >> "${out_meta}"
    echo "[OK] ${EXPR_NAME} mu=${mu_ns}: ${out_csv} (${count} walks, sha256=${sha})"
  done
done
