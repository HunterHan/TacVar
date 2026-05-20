#!/bin/bash

# Shared walking-list helpers for assess/detecting runners.
# Intended workflow:
#   1. Generate walk lists once on af309 with prepare_assess_walklists_af309.sh.
#   2. Upload the TacVar checkout to compute nodes.
#   3. Compute nodes read these pre-generated CSV files; they do not generate
#      per-node random walk lists unless ASSESS_ALLOW_LOCAL_WALKLIST=1 is set.

assess_walklist_key() {
  local mu_ns="$1"
  local nwalks="$2"
  local sigma_rel="$3"
  printf 'mu%s_n%s_sigma%s' "${mu_ns}" "${nwalks}" "${sigma_rel}" | tr '. ' 'p_'
}

assess_walklist_root() {
  local root="${ASSESS_WALKLIST_ROOT:-${SCRIPT_DIR}/../codex_assets/walklists}"
  printf '%s' "${root}"
}

assess_python() {
  if [[ -n "${PYTHON:-}" ]]; then
    printf '%s' "${PYTHON}"
  elif [[ -x "${HOME}/miniconda3/bin/python" ]]; then
    printf '%s' "${HOME}/miniconda3/bin/python"
  else
    command -v python3
  fi
}

assess_count_walks() {
  local csv_path="$1"
  "$(assess_python)" - "${csv_path}" <<'PY'
import csv, sys
with open(sys.argv[1], newline="") as f:
    r = csv.DictReader(f)
    print(sum(1 for _ in r))
PY
}

assess_resolve_pregen_walklist() {
  local expr_name="$1"
  local mu_ns="$2"
  local nwalks="$3"
  local sigma_rel="$4"
  local key
  key="$(assess_walklist_key "${mu_ns}" "${nwalks}" "${sigma_rel}")"
  local root
  root="$(assess_walklist_root)"
  printf '%s/%s/%s' "${root}" "${expr_name}" "${key}"
}

assess_use_shared_walklist() {
  local mu_ns="$1"
  local out_root="$2"
  local out_csv="${out_root}/walk_list_normal.csv"
  local out_meta="${out_root}/walk_list_meta.txt"
  local pregen_dir
  pregen_dir="$(assess_resolve_pregen_walklist "${EXPR_NAME}" "${mu_ns}" "${NWALKS:-100}" "${SIGMA_REL:-0.015}")"
  local pregen_csv="${pregen_dir}/walk_list_normal.csv"
  local pregen_meta="${pregen_dir}/walk_list_meta.txt"

  if [[ -s "${pregen_csv}" ]]; then
    cp -f "${pregen_csv}" "${out_csv}"
    [[ -s "${pregen_meta}" ]] && cp -f "${pregen_meta}" "${out_meta}" || true
    ASSESS_WALKLIST_SOURCE="${pregen_csv}"
    echo "[INFO] Using pre-generated af309 walk list: ${pregen_csv}"
  elif [[ "${ASSESS_ALLOW_LOCAL_WALKLIST:-0}" == "1" ]]; then
    echo "[WARN] Missing pre-generated walk list: ${pregen_csv}"
    echo "[WARN] ASSESS_ALLOW_LOCAL_WALKLIST=1, generating locally on $(hostname)."
    "$(assess_python)" "${SCRIPT_DIR}/../utils/gen_walklist.py" \
      --out "${out_csv}" \
      --meta-out "${out_meta}" \
      --dist normal \
      --mu-ns "${mu_ns}" \
      --sigma-rel "${SIGMA_REL:-0.015}" \
      --nwalks "${NWALKS:-100}"
    ASSESS_WALKLIST_SOURCE="LOCAL_GENERATED:${out_csv}"
  else
    echo "ERROR: missing pre-generated walk list: ${pregen_csv}" >&2
    echo "Run on af309 before upload:" >&2
    echo "  cd ~/code/TacVar && EXPR_LIST='${EXPR_NAME}' MU_LIST='${MU_LIST}' NWALKS='${NWALKS:-100}' SIGMA_REL='${SIGMA_REL:-0.015}' scripts/prepare_assess_walklists_af309.sh" >&2
    exit 2
  fi

  SHARED_WALK_LIST="${out_csv}"
  SHARED_WALK_META="${out_meta}"
  WALK_LIST="${out_csv}"
  META_TXT="${out_meta}"
  walk_count="$(assess_count_walks "${SHARED_WALK_LIST}")"
}

assess_use_interval_walklist() {
  local interval_ns="$1"
  local out_dir="$2"
  local walk_mu_ns="${WALK_MU_NS:-${interval_ns}}"
  mkdir -p "${out_dir}"
  assess_use_shared_walklist "${walk_mu_ns}" "${out_dir}"
  WALK_LIST="${SHARED_WALK_LIST}"
  META_TXT="${SHARED_WALK_META}"
}
