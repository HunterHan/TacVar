# Codex Memory

## Local Workspaces

- Manuscript workspace: `/Users/hunter/manuscript/TacVarX-TPDS-0427`
- Manuscript branch checked on 2026-05-13: `codex/vibe0513`
- TacVar code repository: `/Users/hunter/code/TacVar`
- TacVar branch checked on 2026-05-13: `dev/filt_branch_0421`
- TacVar remote: `git@github.com:HunterHan/TacVar.git`

## SSH And Proxy

- Before SSH from the Mac, source the local proxy script:
  ```bash
  source ~/proxy.bash
  ```
- Jump host:
  ```bash
  ssh af309
  ```
- After logging into `af309`, source the remote proxy script:
  ```bash
  source ~/proxy.bash
  ```
- `ssh af309` may print:
  ```text
  Warning: remote port forwarding failed for listen port 10800
  ```
  This warning did not prevent GitHub access or SSH to the tested compute nodes.

## af309 Network Check

- GitHub connectivity from `af309` was confirmed on 2026-05-13 after sourcing `~/proxy.bash`.
- Test used:
  ```bash
  curl -I -L --connect-timeout 10 https://github.com
  ```
- Observed result included:
  ```text
  HTTP/2 200
  ```

## Compute Nodes From af309

The following nodes were reachable from `af309` on 2026-05-13:

- `c920bn3`
  - `ls` output included: `code`, `data`, `local`, `miniconda3`, `opt`, `software`
- `camd9554n2`
  - `ls` output included: `code`, `cpupower.bak`, `data`, `local`, `miniconda3`, `nsight-systems-2026.1.1`, `opt`, `output`, `project`, `software`, `test`, `uprof`
- `cgnr6760pn2`
  - `ls` output included: `code`, `miniconda3`, `opt`, `software`

Note: the correct third hostname is `cgnr6760pn2`, not `cngr6760pn2`.

## TacVar Remote Workflow

- There is a twin TacVar checkout on `af309`: `~/code/TacVar`.
- Treat `af309:~/code/TacVar` as authoritative; local `/Users/hunter/code/TacVar` may be stale.
- TacVar implementation branch for this thread: `codex/vibe0513`, created from `dev/filt_branch_0421` on `af309`.
- Do not build or run experiments on `af309`; it is only for editing, Git, upload/download, and plotting coordination.
- Current workflow: edit `af309:~/code/TacVar`, then upload it to compute nodes using the VS Code workspace task `My Upload` from:
  ```text
  af309:~/code/vscode_workspace_config/proj-tacvarx.code-workspace
  ```
- Upload targets: `c920bn3`, `camd9554n2`, `cgnr6760pn2`.
- Build and run only on compute nodes: `c920bn3`, `camd9554n2`, `cgnr6760pn2`.
- Before building or running TacVar on compute nodes, source:
  ```bash
  source ~/code/TacVar/env.bash
  ```
- Assessing outputs should follow existing data layout:
  ```text
  ~/code/data/<YYYYMMDD>/<host>/outputAssessing/<expr>/<timestamp>/
  ```

## Paper Method To TacVar Code

- Paper method split: `sampling + filtering + assessing/detecting`.
- `sampling + filtering` maps to `af309:~/code/TacVar/stencil/run_filttest.sh`.
  - It builds stencil timing binaries, samples measured runtime and timing fluctuation via `mpirun`, then runs `src/filter/get_quantile.py`, `get_met.py`, `get_tf.py`, `get_binw.py`, and `src/filter/filt.x`.
  - `DO_SAMPLING=0` can skip resampling and rerun filtering on existing data.
- `af309:~/code/TacVar/opt_difference_workload/` is deprecated. Do not delete it.
  - Do not execute C code under this directory.
  - Its `run_filt.sh` is more regular and may be used only as a style/structure reference when improving `stencil/run_filttest.sh`.
- `assessing/detecting` maps to `af309:~/code/TacVar/scripts/run_detecting_*.sh`.
  - Entry wrapper: `scripts/run_detecting_pipeline_entry.sh`.
  - Experiment scripts: `run_detecting_expr1_timer.sh`, `run_detecting_expr1_fsize.sh`, `run_detecting_expr2_interval.sh`, `run_detecting_expr3_frkern.sh`.
  - These scripts source `env.bash`, build `src/partes/partes-mpi.x`, read af309-prepared walking lists, and run `mpirun ... partes-mpi.x --ta ... --tb ...`.
  - Core source path: `af309:~/code/TacVar/src/partes/partes-mpi.c`.

## Paper Reference Mapping

- Reference PDFs are under `assets/00..03`.
- Current tex should align motivation, method, and experiments with prior work, but use new measurements and machines.
- Motivation mainly follows `assets/02-...CCGRID'24.pdf`.
- Model and sampling mainly follow CCGRID `tvcond` and `tvfilt` in-situ sampling.
  - `tvfilt` deconvolution is deprecated here, or used only as a comparison baseline.
- Filtering follows `assets/03-向颖谦毕业论文_5_14.pdf` sampling-simulation filtering.
  - Do not use differential sampling.
- Assessing/detecting follows a revised version of the ParEST/Computer Science paper method, `assets/00-...计算机科学.pdf`.
  - In the current paper call this `assess`; old `detecting` names may still remain in tex/code and are being renamed gradually.
- Many tex experiment plots borrow design ideas from these papers, except GoodNotes hand-drawn figures.
- Machines in current tex/experiments must be the current nodes: `c920bn3`, `camd9554n2`, `cgnr6760pn2`.
  - Do not reuse old-paper machine names or old experimental results.
  - Old figures may guide experiment design, axes, and metrics, but data curves must come from new runs.
- Experiment consistency rule: tex figures/data that correspond to old-paper experiments should map to scripts/code under `af309:~/code/TacVar`, especially `stencil/run_filttest.sh` for sampling/filtering and `scripts/run_detecting_*.sh` plus `src/partes/partes-mpi.c` for assess/detecting.

## Tex/Method Reference Table

| Current tex part | Source/reference | Notes |
|---|---|---|
| Motivation | `assets/02-2024-liao-TacVar - Tackling Variability in Short-Interval Timing Measurements on X86-CCGRID'24.pdf` | Reuse timing inconsistency motivation; current machines/data must be regenerated. |
| Model | CCGRID `tvcond` / `tvfilt` | Use measured/runtime/fluctuation model. |
| Sampling | CCGRID `tvcond` / `tvfilt` in-situ sampling | `tvfilt` deconvolution is deprecated or baseline only. |
| Filtering | `assets/03-向颖谦毕业论文_5_14.pdf` | Use sampling-simulation filtering; do not use differential sampling. |
| Assessing | `assets/00-2025-廖-并行计时偏差评测指标及工具-计算机科学.pdf` | Revised ParEST/Computer Science method; call it `assess` in current tex. |
| Experiment design | Prior papers `00..03` | Axes/metrics/layout may be referenced; old machines and old data must not be reused. |

## Tex Figure/Table To TacVar Code

| Tex item | Meaning | Axes / metrics | TacVar source | Data status |
|---|---|---|---|---|
| `fig:expr-sampling-overview` | Sampling overview for `t_m`, `t_e`, `t_m^{2p}` | timing distributions/ICDF-style data | `stencil/run_filttest.sh`, `src/filter/*` | regenerate if used |
| `fig:expr-sampling-baseline` | Baseline DSub in-situ sampling filtering consistency | filtered runtime distributions across timers | `stencil/run_filttest.sh` | regenerate/current data needed |
| `fig:expr-sampling-opt` | Improved sampling filtering consistency | filtered runtime distributions across timers | `stencil/run_filttest.sh` | regenerate/current data needed |
| `fig:expr-sampling-scaling` | Sampling quality under grid-size scaling | x: Jacobi size; y: Wasserstein across timers | `stencil/run_filttest.sh` | regenerate/current data needed |
| `expr:expr_filt_erep_wd` | Filtering quality / W-metric comparison | x: data/workload size; y: W-metric/error indicators | `stencil/run_filttest.sh`, `src/filter/filt.x` | regenerate/current data needed |
| `expr:expr_filt_jacobi_draft` | Jacobi filtered vs measured distributions | ICDF/distribution by timer | `stencil/run_filttest.sh` | regenerate/current data needed |
| `expr:expr_filt_tl_draft` | TeaLeaf filtered vs measured distributions | ICDF/distribution by timer | TeaLeaf/stencil outputs if enabled | verify source before use |
| `fig:motivation-observation-1` | Assessing timer distributions under fixed flush size | histogram + theoretical/measured quantile per timer | `scripts/run_detecting_expr1_timer.sh`; target `run_assess_expr1_timer.sh` | must regenerate |
| `fig:motivation-observation-1-fig3` | fsize impact on timer deviation | x: flush size KiB; y: `R_L`, `R_H` | `scripts/run_detecting_expr1_fsize.sh`; target `run_assess_expr1_fsize.sh` | must regenerate |
| `fig:motivation-observation-2` | interval impact on deviation | x: interval/`ta` ns; y: `R_L`, `R_H` | `scripts/run_detecting_expr2_interval.sh`; target `run_assess_expr2_interval.sh` | must regenerate |
| `tab:motivation-observation-3` | front/rear flush kernel impact | table metrics: `R_L`, `R_H` per kernel pair | `scripts/run_detecting_expr3_frkern.sh`; target `run_assess_expr3_frkern.sh` | must regenerate |
| `fig:motivation-observation-4` | cross-node assessing comparison | x: interval/timer setting; y: `R_L`, `R_H` | assess scripts on `c920bn3`, `camd9554n2`, `cgnr6760pn2` | must regenerate |
| `fig:motivation-observation-4-fig6` | second cross-node assessing comparison | x: interval/timer setting; y: `R_L`, `R_H` | assess scripts on current nodes | must regenerate |

## Assessing Plotting Rules

- Do not draw multiple processors/hosts in the same axes.
- Use one subplot per processor/host, or separate figures if subplots are too crowded.
- Within each subplot/figure, legend should normally be the timer.
- X-axis should be the experiment variable, e.g. `fsize`, `interval`/`ta`, or kernel-pair category.
- Y-axis should normally be Wasserstein/assess metric, e.g. `R_L`, `R_H`, or another explicitly documented Wasserstein-derived metric.
- Assessing Wasserstein means the distance between the actual measured distribution and the theoretical walking-list distribution, not raw elapsed time and not only mean absolute distance to one scalar `ta`.
- `expr1.fsize`: script values `16 32 ... 8192` already mean KiB; plot them directly as KiB.
- `expr1.timer`: do not use timer as an x-axis line plot. For each timer, plot two panels: left histogram, right walking-list distribution vs measured distribution, following the Computer Science paper Fig. 2 intent.
- `expr2.interval`: plot interval/`ta` on the x-axis in microseconds.
- `expr3.frkern`: do not draw a chart; output tables with rows=`fkern`, columns=`rkern`, and cells=Wasserstein.
- For cross-node comparison figures, compare current nodes `c920bn3`, `camd9554n2`, `cgnr6760pn2` by subplots/panels, not by overlaying host curves in one axes.
- Quick-look quantile deltas are acceptable for smoke checks only; final tex-facing assessing plots should use the intended Wasserstein/assessment metrics.

## Assessing Implementation Notes

- Coarse execution steps should be reported as `[Step X/12] ...`; do not print per-combination progress like `[3/120]` unless requested.
- Add `src/partes/assess-mpi.c` as the ta-only assess executable; keep `partes-mpi.x` and old `run_detecting_*.sh` usable.
- Add assess scripts mirroring current detecting scripts: `run_assess_expr1_timer.sh`, `run_assess_expr1_fsize.sh`, `run_assess_expr2_interval.sh`, `run_assess_expr3_frkern.sh`.
- Timer defaults: x86 nodes use `tsc_asym`; ARM `c920bn3` should use `cntvct`, `cntvct_fence`, `cntvcto`; common timers remain `clock_gettime`, `mpi_wtime`, `papi`, `papix6`, `likwid` when available.
- Assessing walk-list generation should use relative noise `N(0, sigma=0.015)` and then add `tbase`/interval to obtain each `ta`.
- Shared walking-list rule: generate detecting/assess walk lists once on `af309` with `scripts/prepare_assess_walklists_af309.sh`, then upload `codex_assets/walklists/` with TacVar to `c920bn3`, `camd9554n2`, and `cgnr6760pn2`.
  - Compute nodes must read the pre-generated CSVs via `scripts/assess_walklist_common.sh`; they should not generate their own random walking lists during normal runs.
  - Missing pre-generated CSVs are fatal unless `ASSESS_ALLOW_LOCAL_WALKLIST=1` is explicitly set for debugging.
  - Current shared-list path pattern: `codex_assets/walklists/<expr_name>/mu<mu_ns>_n<NWALKS>_sigma0p015/walk_list_normal.csv`.
  - For one batch, all three compute nodes must have identical checksums for the same walk-list path before comparing `R_L`, `R_H`, or Wasserstein results.
- Fig6 / `assess.expr1.fsize` rerun condition check on 2026-05-20:
  - Prior-paper constraints to preserve: fixed/performance CPU frequency when possible, one MPI rank per core, avoid SMT interference, current-node fsize/cache interpretation, consistent walk list, and `R_L/R_H` quantile metric.
  - Current nodes report `Thread(s) per core: 1`, so SMT/hyperthreading is already disabled at platform level for `c920bn3`, `camd9554n2`, and `cgnr6760pn2`.
  - `scripts/run_assess_expr1_fsize.sh` uses best-effort `cpupower frequency-set -g performance`, `mpirun --map-by core --bind-to core`, and a generated physical-core `taskset` list instead of assuming logical CPU ids `0..NP-1`.
  - Fig6 rerun must cover both `NP=64` and `NP=128`; keep the two datasets separate by `EXPR_NAME` or metadata, and plot them as separate fig6 variants unless the tex explicitly asks to combine them.
  - Use compute-node `~/miniconda3/bin/python` for helper Python, and do not build/run on `af309`.
  - Final rerun data path: `af309:~/code/data/20260520/<host>/outputAssessing/assess.expr1.fsize.np64|np128/`.
  - Shared walk-list checksums used for this batch:
    - `np64`: `e449d78611ca1b7de5bfb476948e6093c660f8e48e05638eb37fd6272d0e52f2`.
    - `np128`: `02d16b3bf9c4a6a1ea41037195034d1342c847026c9c0ddb5eeed0b9fcae4df0`.
  - Final plotting script/artifacts:
    - `scripts_plot/plot_fig6_np64_np128_final.py`.
    - `scripts_plot/outputAssessing/fig6_np64_np128_final/assess.expr1.fsize.np64_rl_rh_20260520.png`.
    - `scripts_plot/outputAssessing/fig6_np64_np128_final/assess.expr1.fsize.np128_rl_rh_20260520.png`.
    - `scripts_plot/outputAssessing/fig6_np64_np128_final/fig6_final_summary_20260520.csv`.
    - `scripts_plot/outputAssessing/fig6_np64_np128_final/fig6_final_status_20260520.md`.
  - Final selection rule: choose the newest complete row per `(host, expr, timer, fsize)` with `n_measured=n_theory=300`, then plot a timer line only if all 10 fsize values are present.
  - Final plotted timers:
    - `c920bn3`: `clock_gettime`, `mpi_wtime`, `cntvct`, `cntvcto` for both `np64` and `np128`.
    - `camd9554n2`: `tsc`, `tsc_asym`, `clock_gettime`, `mpi_wtime` for `np64`; `tsc`, `clock_gettime`, `mpi_wtime` for `np128`.
    - `cgnr6760pn2`: `tsc`, `tsc_asym`, `clock_gettime`, `mpi_wtime` for both `np64` and `np128`.
  - `camd9554n2 np128 tsc_asym` hung at the larger fsize part of the original batch and was excluded from the final plot because it lacked complete points for `1024 2048 4096 8192` KiB. The run was not broadly patched; stable timers were resumed in a separate batch.
  - Fig6 `R_H` diagnosis: c920b looks smoother because its high-tail samples stay near the 10us target, while x86 runs contain sparse long-tail outliers. Example: `cgnr6760pn2 np128 clock_gettime fsize16` has CDF tail up to `929931 ns`, with rank-local spikes on rank 0/127 and many ~30us spikes; comparable c920b case max is only ~`10380 ns`.
  - This is not a walking-list mismatch: the three nodes use the same shared list, so `R_H` denominator is the same per `np` (`~279 ns` for `np64`, `~177 ns` for `np128`). Sparse x86 outliers divided by this small denominator inflate `R_H` to hundreds/thousands and can make small-fsize points look unordered.
  - Likely contributors: no `isolcpus`, `np128` occupies all 128 cores, x86 nodes have inactive `irqbalance`, and OS/daemon/interrupt noise can land on experiment ranks. Treat current x86 `R_H` as high-tail noise sensitive; for tex-quality reruns prefer cleaner cores/housekeeping isolation or repeated batches with median-of-runs.
  - Plot-only top-1% truncation (`99%`) did not stabilize x86 enough: `cgnr6760pn2` small-fsize `R_H` still reached about `999.8%` for `np64` and `1387.4%` for `np128`; `camd9554n2 np128` still reached about `398%`. Do not treat 99% truncation alone as a valid fix.
  - Guarded repeat2 on 2026-05-21:
    - `camd9554n2` stopped before running as intended because precheck found a high-CPU process (`hpchzy systemd`, about `24%` CPU). Do not relaunch camd repeat2 until the node is clean or the user explicitly asks.
    - `cgnr6760pn2` passed `last`/`ps` precheck and completed `assess.expr1.fsize.np64.repeat2` and `np128.repeat2`.
    - Repeat2 comparison artifacts: `scripts_plot/outputAssessing/fig6_repeat2_guarded_compare/cgnr_repeat2_compare.md`, `cgnr_repeat2_vs_base_repeat1.csv`, `cgnr_repeat2_stats.csv`, and `cgnr_repeat2_worst_rh.csv`.
    - Repeat2 did not prove the x86 `R_H` issue is solved by a clean precheck: small-fsize max `R_H` for `cgnr6760pn2` was `1481.4%` (`np64`) and `4056.3%` (`np128`, `mpi_wtime`). This suggests tail instability remains metric/data-sensitive, not only an obvious concurrent high-CPU-process artifact.
  - Camd no-guard repeat2 on 2026-05-21:
    - User requested running even with the `hpchzy systemd --user` process present. The no-guard script still printed `last` and `ps -eo user,pid,psr,pcpu,pmem,comm --sort=-pcpu | head -n 30`, but did not stop on high CPU.
    - Completed `camd9554n2` `assess.expr1.fsize.np64.repeat2` batch `20260521_184802` and `np128.repeat2` batch `20260521_185512`; raw data was rsynced back to `af309:~/code/data/20260521/camd9554n2/outputAssessing/`.
    - Camd repeat2 quicklook artifacts: `scripts_plot/outputAssessing/fig6_repeat2_camd_noguard/all_batches/assess_summary_20260521.csv` and per-np RL/RH/Wasserstein PNGs.
    - Combined fig6 with `c920bn3` previous final (`20260520`), `camd9554n2` repeat2 no-guard (`20260521`), and `cgnr6760pn2` repeat2 guarded (`20260521`) is under `scripts_plot/outputAssessing/fig6_combined_c920_final_cgnr_camd_repeat2/`.
    - Combined fig6 files: `assess.expr1.fsize.np64_rl_rh_combined_c920final_cgnr-camd-repeat2_20260521.png`, `assess.expr1.fsize.np128_rl_rh_combined_c920final_cgnr-camd-repeat2_20260521.png`, `fig6_combined_summary_20260521.csv`, and `fig6_combined_status_20260521.md`.
  - Manual assess expr1 fsize workflow:
    - On `af309`, generate walking lists before upload. Example: `EXPR_LIST="assess.expr1.fsize.np64 assess.expr1.fsize.np128" MU_LIST=10000 NWALKS=3 SIGMA_REL=0.015 scripts/prepare_assess_walklists_af309.sh`.
    - Upload the whole TacVar checkout to compute nodes after the walk lists are present under `codex_assets/walklists/`.
    - On compute nodes, use `scripts/manual_assess_expr1_fsize.sh` or call `scripts/run_assess_expr1_fsize.sh` directly with `EXPR_NAME=assess.expr1.fsize.np64|np128`, `NP=64|128`, `DATE_BASE`, `OUT_BASE`, `OUTPUT_DIR`, `TIMER_LIST`, `NWALKS`, `NTESTS`, and `NTILES`.
    - `run_assess_expr1_fsize.sh` must read the pre-generated walk list and fail if it is missing; `ASSESS_ALLOW_LOCAL_WALKLIST=1` is debugging-only. It is allowed only on `c920bn3`, `camd9554n2`, and `cgnr6760pn2` by default; override with `ASSESS_ALLOW_OTHER_HOST=1` only for deliberate debugging.
    - Plot manually downloaded data on `af309` with `scripts_plot/assess_expr1_fsize_manual_plot.ipynb`; edit the parameter cell for `DATA_ROOT`, `DATE`, `HOSTS`, `EXPRS`, `OUTPUT_DIR`, expected samples, fsize list, and optional batch selection.
- Defer gpns/abort stabilization until a data run actually fails.

## Harness Engineering: Remote Command Smoothness

Recorded after the 2026-06-15 detecting expr3/expr4 0614 work. These rules prevent repeated Codex/remote-shell friction.

- Avoid deeply nested one-shot commands like ssh-af309 wrapping python/heredoc when the embedded script contains shell variables or awk fields such as dollar-one, PPID, dollar-dollar, pattern variables, backslashes, or TeX row terminators. The local shell may expand or mangle them before the remote shell sees them.
- For nontrivial remote edits, first open an interactive ssh af309 session, cd to ~/code/TacVar, then paste a remote-side single-quoted heredoc such as python3 heredoc with quoted PY marker. In that shape, shell variables and awk fields stay literal because the heredoc is interpreted only on af309.
- Keep remote patch scripts short and verify immediately: bash -n for shell scripts, python syntax checks for helper scripts, and JSON-load checks for notebooks.
- Do not hand-copy long base64 blobs into a command. In the 0614 expr3/expr4 work, a manual base64 paste was truncated and failed decoding. Prefer an interactive remote heredoc or a small remote temp script.
- Avoid Python triple-quote collisions in generated helper scripts. If a script must write a shell function containing quotes/backslashes, use a clearly different delimiter style or write the function as a list of lines joined with newline characters; run a syntax check before sending it remote.
- Avoid very long background-launch commands in an interactive terminal; line wrapping and control-channel ownership can make the prompt look stuck. Prefer a tiny remote launch script, or use ssh -f plus nohup with stdin redirected from /dev/null and then confirm with pgrep.
- When launching long experiments, record DATE_STAMP, host, expected counts, and launch log path immediately. Pull data per node only after pgrep confirms that node has fully stopped; never rsync a node while its experiment is still running.
- Disable pagers for final git checks in remote sessions: use git --no-pager log and git status --short. A pager in a limited terminal caused garbled follow-up input once.
- If a remote rsync seems to hang silently, check from a second shell whether an rsync process and target directory actually exist before waiting indefinitely. If no rsync process exists and no target directory appears, interrupt the stuck control command and rerun a short, standalone rsync.
