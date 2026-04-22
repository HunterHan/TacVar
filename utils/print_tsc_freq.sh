#!/usr/bin/env bash
# TSC helpers for stencil tsc= (cycles per ns).
#
# Usage: print_tsc_freq.sh -m METHOD [-q]
#   calibrate  - tsc_calibrate.c fine mode (recommended)
#   coarse     - tsc_calibrate.c coarse mode (rough)
#   sysfs      - /sys/.../tsc_freq_khz when readable
#   dmesg      - sudo dmesg | grep -i 'tsc:'
#   cpuinfo    - constant_tsc / nonstop_tsc (no numeric rate)
#   -q         - calibrate|coarse|sysfs: stdout is only tsc_ns (one line)
#
# Example: tsc=$(print_tsc_freq.sh -m calibrate -q)
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
CC=${CC:-gcc}
TSC_SYSFS=/sys/devices/system/cpu/cpu0/tsc_freq_khz
TSC_SRC=tsc_calibrate.c

usage() {
  cat <<'EOF'
Usage: print_tsc_freq.sh -m METHOD [-q]

  METHOD
    calibrate   tsc_calibrate.c fine (recommended)
    coarse      tsc_calibrate.c coarse (rough)
    sysfs       /sys/.../cpu0/tsc_freq_khz
    dmesg       sudo dmesg | grep -i 'tsc:'
    cpuinfo     constant_tsc / nonstop_tsc lines

  -q   For calibrate|coarse|sysfs: stdout is only tsc_ns (one line).

Example:
  tsc=$(print_tsc_freq.sh -m calibrate -q)
EOF
}

die() {
  echo "print_tsc_freq.sh: $*" >&2
  exit 1
}

tmp_bin() {
  mktemp "${TMPDIR:-/tmp}/tsc_util.XXXXXX"
}

# Build tsc_calibrate.c, run as: tsc_calibrate MODE
build_run_tsc() {
  local mode=$1 bin
  bin=$(tmp_bin)
  trap 'rm -f "$bin"' RETURN
  "$CC" -O2 -Wall -Wextra -std=c11 -o "$bin" "$SCRIPT_DIR/$TSC_SRC" || die "compile failed: $TSC_SRC"
  if [[ -n "${QUIET:-}" ]]; then
    "$bin" "$mode" 2>/dev/null
  else
    "$bin" "$mode"
  fi
}

tsc_ns_from_sysfs() {
  [[ -r "$TSC_SYSFS" ]] || die "sysfs not readable: $TSC_SYSFS"
  local khz
  khz=$(tr -d ' \n' <"$TSC_SYSFS")
  [[ -n "$khz" && "$khz" != 0 ]] || die "sysfs empty or zero"
  awk -v k="$khz" 'BEGIN { printf "%.9f\n", k / 1000000.0 }'
}

emit_numeric() {
  local tag=$1 val=$2 hz
  if [[ -n "${QUIET:-}" ]]; then
    printf '%s\n' "$val"
    return
  fi
  hz=$(awk -v x="$val" 'BEGIN { printf "%.0f\n", x * 1e9 }')
  echo "${tag}_tsc_ns=${val}"
  echo "${tag}_tsc_freq_hz=${hz}"
  echo "tsc=${val}   # for run_filttest.sh"
}

run_calibrate() {
  emit_numeric calibrate "$(build_run_tsc calibrate)"
}

run_coarse() {
  emit_numeric coarse "$(build_run_tsc coarse)"
}

run_sysfs() {
  emit_numeric sysfs "$(tsc_ns_from_sysfs)"
}

run_dmesg() {
  [[ -n "${QUIET:-}" ]] && die "-q only with calibrate, coarse, or sysfs"
  echo "--- sudo dmesg | grep -i 'tsc:' ---"
  if command -v sudo >/dev/null 2>&1; then
    sudo dmesg 2>/dev/null | grep -i "tsc:" || echo "(no lines or sudo/dmesg failed)"
  else
    echo "(sudo not in PATH)"
  fi
}

run_cpuinfo() {
  [[ -n "${QUIET:-}" ]] && die "-q only with calibrate, coarse, or sysfs"
  echo "--- /proc/cpuinfo: constant_tsc | nonstop_tsc (uniq) ---"
  grep -E "constant_tsc|nonstop_tsc" /proc/cpuinfo | uniq || true
}

METHOD=""
QUIET=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    -m) METHOD=${2:?}; shift 2 ;;
    -q) QUIET=1; shift ;;
    -h | --help) usage; exit 0 ;;
    *) die "unknown arg: $1" ;;
  esac
done

[[ -n "$METHOD" ]] || {
  usage >&2
  die "missing -m METHOD"
}

case "$METHOD" in
  calibrate) run_calibrate ;;
  coarse) run_coarse ;;
  sysfs) run_sysfs ;;
  dmesg) run_dmesg ;;
  cpuinfo) run_cpuinfo ;;
  *) die "unknown METHOD: $METHOD" ;;
esac
