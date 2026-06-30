#!/bin/bash
# 0630 mid-grain full-run orchestrator. Run on af309.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJ_ROOT="${PROJ_ROOT:-$(cd "${SCRIPT_DIR}/.." && pwd)}"
DATE_BASE="${DATE_BASE:-20260630}"
LOCAL_DATA_ROOT="${LOCAL_DATA_ROOT:-/astrum/home/hpchzy/code/data}"
REMOTE_DATA_ROOT="${REMOTE_DATA_ROOT:-/home/hpchzy/code/data}"
REMOTE_PROJ="${REMOTE_PROJ:-/home/hpchzy/code/TacVar}"
HOSTS="${HOSTS:-cgnr6760pn2 camd9554n1 c920bn3}"
KERNELS="${KERNELS:-jacobi2d5p gs2d5p tl_f90_cg_calc_w stream_triad openblas_gemm openblas_gemv openblas_dot openblas_axpy hpcg_spmv npb_ft_fft npb_ep}"
SHUFFLE_SELECT="${SHUFFLE_SELECT:-shuffle2}"
INSITU="${INSITU:-INSITU_DSUB_ASM}"
NSAMP="${NSAMP:-1000}"
NSAMP_RATIO_LIST="${NSAMP_RATIO_LIST:-0.5}"
NP_LIST="${NP_LIST:-64}"
SHUFFLE_COUNT="${SHUFFLE_COUNT:-3}"
PATH_CONFIG="${PATH_CONFIG:-${SCRIPT_DIR}/kernel_full_paths.0630.json}"
PATH_LOCK="${PATH_CONFIG}.lock"
LOG_ROOT="${LOG_ROOT:-${SCRIPT_DIR}/logs_midgrain_0630/$(date +%Y%m%d_%H%M%S)}"
COMMIT_HASH="${COMMIT_HASH:-$(git -C "${PROJ_ROOT}" rev-parse HEAD)}"
UPDATE_PATH_CONFIG="${UPDATE_PATH_CONFIG:-1}"

mkdir -p "${LOG_ROOT}"

declare -A OUTPUT_ROOTS=(
    [jacobi2d5p]=output_jacobi2d5p_filt_dsub
    [gs2d5p]=output_gs2d5p_filt_dsub
    [tl_f90_cg_calc_w]=output_tl_f90_cg_calc_w_filt_dsub
    [stream_triad]=output_stream_triad_filt_dsub
    [openblas_gemm]=output_openblas_gemm_filt_dsub
    [openblas_gemv]=output_openblas_gemv_filt_dsub
    [openblas_dot]=output_openblas_dot_filt_dsub
    [openblas_axpy]=output_openblas_axpy_filt_dsub
    [hpcg_spmv]=output_hpcg_spmv_filt_dsub
    [npb_ft_fft]=output_npb_ft_fft_filt_dsub
    [npb_ep]=output_npb_ep_filt_dsub
)

declare -A EXT=(
    [hpcg_spmv]=cpp
)

kernel_dash(){ echo "$1" | tr '_' '-'; }
kernel_ext(){ local k=$1; echo "${EXT[$k]:-c}"; }
run_id_for(){
    local k=$1
    if [ "${SMOKE:-0}" = "1" ]; then
        echo "${DATE_BASE}-smoke-midgrain-$(kernel_dash "$k")-dsub"
    else
        echo "${DATE_BASE}-full-$(kernel_dash "$k")-dsub"
    fi
}

remote_cleanup(){
    local host=$1 kernel=$2
    ssh "$host" python3 - "${REMOTE_PROJ}" "$kernel" <<'REMOTE_CLEAN_PY'
import os
import re
import signal
import subprocess
import sys
import time
from pathlib import Path

remote_proj, kernel = sys.argv[1:3]
stencil = Path(remote_proj) / 'stencil'
stencil.mkdir(parents=True, exist_ok=True)
os.chdir(stencil)
for path in stencil.glob(f'{kernel}_*.x'):
    try:
        path.unlink()
    except FileNotFoundError:
        pass
for name in ('filt.x',):
    try:
        (stencil / name).unlink()
    except FileNotFoundError:
        pass
for path in stencil.glob('*.csv'):
    try:
        path.unlink()
    except FileNotFoundError:
        pass

suite_re = re.compile(r'jacobi2d5p_.*\.x|gs2d5p_.*\.x|tl_f90_cg_calc_w_.*\.x|stream_triad_.*\.x|openblas_gemm_.*\.x|openblas_gemv_.*\.x|openblas_dot_.*\.x|openblas_axpy_.*\.x|hpcg_spmv_.*\.x|npb_ft_fft_.*\.x|npb_ep_.*\.x|filt\.x|mpirun|prterun|orted|prted|run_entry_filt\..*0630')
user = os.environ.get('USER') or os.environ.get('LOGNAME')
cmd = ['ps', '-u', user, '-o', 'pid=,ppid=,args='] if user else ['ps', '-axo', 'pid=,ppid=,args=']
proc = subprocess.run(cmd, text=True, capture_output=True, check=False)
skip = {os.getpid(), os.getppid()}
killed = []
for line in proc.stdout.splitlines():
    parts = line.strip().split(None, 2)
    if len(parts) < 3:
        continue
    try:
        pid = int(parts[0]); ppid = int(parts[1])
    except ValueError:
        continue
    args = parts[2]
    if pid in skip or ppid in skip:
        continue
    if suite_re.search(args):
        try:
            os.kill(pid, signal.SIGKILL)
            killed.append((pid, args[:160]))
        except ProcessLookupError:
            pass
print('[midgrain-cleanup] killed=%d' % len(killed))
for pid, args in killed:
    print('[midgrain-cleanup] killed pid=%s args=%s' % (pid, args))
time.sleep(1)
REMOTE_CLEAN_PY
}

upload_kernel_files(){
    local host=$1 kernel=$2 ext
    ext=$(kernel_ext "$kernel")
    ssh "$host" "mkdir -p '${REMOTE_PROJ}/stencil' '${REMOTE_PROJ}/src/filter'"
    scp "${PROJ_ROOT}/stencil/${kernel}.${ext}" "${PROJ_ROOT}/stencil/run_entry_filt.${kernel}.0630.sh" "${PROJ_ROOT}/stencil/run_entry_filt.kernel_common.0630.sh" "${host}:${REMOTE_PROJ}/stencil/"
    scp "${PROJ_ROOT}/src/filter/filt_v2.0608.c" "${PROJ_ROOT}/src/filter/get_quantile.py" "${PROJ_ROOT}/src/filter/get_met.py" "${PROJ_ROOT}/src/filter/get_tf.py" "${PROJ_ROOT}/src/filter/get_binw.py" "${host}:${REMOTE_PROJ}/src/filter/"
}

make_manifest_remote(){
    local host=$1 run_dir=$2
    ssh "$host" "set -e; cd '${run_dir}'; find . -type f ! -name MANIFEST.sha256 ! -name MANIFEST.af309.sha256 -print0 | sort -z | xargs -0 sha256sum > MANIFEST.sha256"
}

pull_and_verify(){
    local host=$1 remote_run=$2 local_run=$3
    mkdir -p "$(dirname "$local_run")"
    rm -rf "$local_run"
    scp -r "${host}:${remote_run}" "$(dirname "$local_run")/"
    (cd "$local_run" && find . -type f ! -name MANIFEST.sha256 ! -name MANIFEST.af309.sha256 -print0 | sort -z | xargs -0 sha256sum > MANIFEST.af309.sha256)
    diff -u "${local_run}/MANIFEST.sha256" "${local_run}/MANIFEST.af309.sha256"
}

update_path_config(){
    local kernel=$1 host=$2 output_root=$3 run_id=$4
    [ "$UPDATE_PATH_CONFIG" = "1" ] || return 0
    mkdir -p "$(dirname "$PATH_CONFIG")"
    [ -f "$PATH_CONFIG" ] || echo '{}' > "$PATH_CONFIG"
    (
        flock 9
        python3 - "$PATH_CONFIG" "$kernel" "$host" "$DATE_BASE" "$output_root" "$run_id" "$SHUFFLE_SELECT" <<'PATH_UPDATE_PY'
import json
import sys
from pathlib import Path
path = Path(sys.argv[1])
kernel, host, date_base, output_root, run_id, shuffle = sys.argv[2:]
try:
    cfg = json.loads(path.read_text())
except FileNotFoundError:
    cfg = {}
cfg.setdefault(kernel, {})[host] = {
    "date_base": date_base,
    "output_root": output_root,
    "run_id": run_id,
    "shuffle": shuffle,
}
tmp = path.with_suffix(path.suffix + ".tmp")
tmp.write_text(json.dumps(cfg, indent=2, sort_keys=True) + "\n")
tmp.replace(path)
PATH_UPDATE_PY
    ) 9>"$PATH_LOCK"
}

run_one(){
    local host=$1 kernel=$2 output_root run_id remote_run local_run log
    output_root="${OUTPUT_ROOTS[$kernel]}"
    run_id=$(run_id_for "$kernel")
    remote_run="${REMOTE_DATA_ROOT}/${DATE_BASE}/${host}/${output_root}/${run_id}"
    local_run="${LOCAL_DATA_ROOT}/${DATE_BASE}/${host}/${output_root}/${run_id}"
    log="${LOG_ROOT}/${host}.${kernel}.log"
    {
        echo "[midgrain] host=${host} kernel=${kernel} run_id=${run_id} start=$(date -Is)"
        upload_kernel_files "$host" "$kernel"
        remote_cleanup "$host" "$kernel"
        ssh "$host" "cd '${REMOTE_PROJ}' && DATE_BASE='${DATE_BASE}' DATE_STAMP='${run_id}' COMMIT_HASH='${COMMIT_HASH}' INSITU='${INSITU}' SHUFFLE_COUNT='${SHUFFLE_COUNT}' NP_LIST='${NP_LIST}' NSAMP='${NSAMP}' NSAMP_RATIO_LIST='${NSAMP_RATIO_LIST}' SMOKE='${SMOKE:-0}' bash 'stencil/run_entry_filt.${kernel}.0630.sh'"
        ssh "$host" "grep -q '\[harness\] exit_status=0' '${remote_run}/run.log'"
        make_manifest_remote "$host" "$remote_run"
        pull_and_verify "$host" "$remote_run" "$local_run"
        update_path_config "$kernel" "$host" "$output_root" "$run_id"
        echo "[midgrain] host=${host} kernel=${kernel} done=$(date -Is) local_run=${local_run}"
    } 2>&1 | tee -a "$log"
}

worker_host(){ local host=$1; for kernel in $KERNELS; do run_one "$host" "$kernel"; done; }

printf '[midgrain] date_base=%s commit=%s hosts=%s kernels=%s log_root=%s path_config=%s\n' "$DATE_BASE" "$COMMIT_HASH" "$HOSTS" "$KERNELS" "$LOG_ROOT" "$PATH_CONFIG"

pids=()
for host in $HOSTS; do
    (worker_host "$host") >"${LOG_ROOT}/${host}.worker.log" 2>&1 &
    pids+=("$!")
done
status=0
for pid in "${pids[@]}"; do if ! wait "$pid"; then status=1; fi; done
if [ "$status" -ne 0 ]; then echo "[midgrain] one or more host workers failed; see ${LOG_ROOT}"; exit "$status"; fi
echo "[midgrain] all host workers completed; path_config=${PATH_CONFIG}"
