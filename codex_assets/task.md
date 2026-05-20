# TacVar Assessing Task

## Coarse Execution Steps

1. Create/switch TacVar branch on `af309`.
2. Update manuscript-side `memory.md` and create `task.md`.
3. Map tex figures/tables to old-paper sources and TacVar code/scripts.
4. Implement `assess-mpi.c` and build target.
5. Add architecture-specific timers.
6. Add/update assess run scripts.
7. Smoke-test assess build/scripts.
8. Upload/run experiments on compute nodes.
9. Download data to `af309`.
10. Run/adapt plotting notebooks.
11. Validate plots against tex expectations.
12. Debug gpns/abort only if data collection fails.

Each large stage should print `[Step X/12] <stage name>`. Do not print per-combination progress such as `[3/120]` unless explicitly requested.

## Current Priorities

1. Work only on `af309:~/code/TacVar`; branch: `codex/vibe0513`.
2. Preserve `partes-mpi.x` and old `run_detecting_*.sh` during migration.
3. Add ta-only assessing executable `src/partes/assess-mpi.c` and target `assess-mpi.x`.
4. Add assess scripts:
   - `scripts/run_assess_expr1_timer.sh`
   - `scripts/run_assess_expr1_fsize.sh`
   - `scripts/run_assess_expr2_interval.sh`
   - `scripts/run_assess_expr3_frkern.sh`
5. Add/use architecture-specific timers:
   - x86: `tsc_asym`
   - ARM: `cntvct`, `cntvct_fence`, `cntvcto`
6. Run smoke tests on compute nodes before full data collection.
7. Run current-node experiments on `c920bn3`, `camd9554n2`, `cgnr6760pn2`.
8. Download raw data with VS Code task `My Download`.
9. Use `dev/0130/hzy-workbranch/scripts_plot` notebooks as plotting references.

## Deliverables

- Updated manuscript memory/task notes.
- `af309:~/code/TacVar` branch `codex/vibe0513`.
- Buildable `assess-mpi.x`.
- Assess run scripts with coarse progress reporting.
- Raw assessing data, logs, and metadata from all current nodes.
- Adapted plotting notebook/script.
- Generated assessing figures matching tex axes/metrics and using current-node data only.
- Plot convention: do not overlay multiple processors/hosts in one axes; use host subplots or separate host figures, timer as legend, experiment variable on x-axis, and Wasserstein/assess metric on y-axis.
- Assessing output path: `~/code/data/<YYYYMMDD>/<host>/outputAssessing/<expr>/<timestamp>/`.

## Deferred Rule

Do not proactively patch gpns/abort behavior. If collection fails, inspect logs, reproduce the smallest failing case, fix the minimal root cause, compare against original code, then resume.

## Machine Rule

`af309` is only for editing, Git, upload/download, and plotting coordination. Do not build or run TacVar experiments on `af309`. Upload to `c920bn3`, `camd9554n2`, and `cgnr6760pn2`, then build/run on those compute nodes after sourcing `~/code/TacVar/env.bash`.

## Walk List Rule

Assessing walk lists should sample relative noise from `N(0, sigma=0.015)` and add `tbase`/interval to produce each `ta`.
