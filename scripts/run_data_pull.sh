#!/usr/bin/env bash
# Run on the **compute node** after experiments.
# 1) Copy TacVar scripts/output → ~/code/data/YYYYMMDD/
# 2) Tell how to pull remote ~/code/data → login ~/code/data (same as VSCode task "My Download").
#    Optional: set LOGIN_NODE to push from this host to the login node via rsync over SSH.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="${SCRIPT_DIR}/output"
DAY="$(date +%Y%m%d)"
DEST="${HOME}/code/data/${DAY}"
COMPUTE_HOST="${COMPUTE_HOST:-$(hostname -f 2>/dev/null || hostname)}"

mkdir -p "${HOME}/code/data"
mkdir -p "${DEST}"

if [[ ! -d "${SRC}" ]]; then
  echo "ERROR: missing source dir: ${SRC}" >&2
  exit 1
fi

# Merge into day folder (same layout as `mv output/ DEST/` → DEST/output/).
# Plain `mv` fails if DEST/output/ already exists and is non-empty.
OUT_DEST="${DEST}/output"
mkdir -p "${OUT_DEST}"
rsync -a "${SRC}/" "${OUT_DEST}/"
echo "Synced: ${SRC}/  →  ${OUT_DEST}/"
rm -rf "${SRC}"
mkdir -p "${SRC}"
echo "Cleared local ${SRC}/ (empty dir recreated for next runs)"

# if [[ -n "${LOGIN_NODE:-}" ]]; then
#   echo "Pushing ${HOME}/code/data/ → ${LOGIN_NODE}:~/code/data/ ..."
#   rsync -av --checksum "${HOME}/code/data/" "${LOGIN_NODE}:~/code/data/"
#   echo "Done (push to login)."
# else
#   echo ""
#   echo "On **login node**, pull compute data into ~/code/data (same as VSCode task \"My Download\", folder=data):"
#   echo "  rsync -av --checksum '${COMPUTE_HOST}:~/code/data/' \"\${HOME}/code/data/\""
#   echo ""
#   echo "Or in Cursor/VSCode: Tasks → Run Task → \"My Download\" → server=${COMPUTE_HOST} → folder=data"
# fi
