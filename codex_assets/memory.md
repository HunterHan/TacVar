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
- Defer gpns/abort stabilization until a data run actually fails.
