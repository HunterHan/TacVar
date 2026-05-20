# Current Result Checklist

## Fig6 / `assess.expr1.fsize`

- Date: `20260520`.
- Scope: `np=64` and `np=128` on `c920bn3`, `camd9554n2`, `cgnr6760pn2`.
- Script: `scripts/run_assess_expr1_fsize.sh`.
- Plotter: `scripts_plot/plot_fig6_np64_np128_final.py`.
- Raw data root on af309: `~/code/data/20260520/<host>/outputAssessing/`.
- Final artifacts:
  - `scripts_plot/outputAssessing/fig6_np64_np128_final/assess.expr1.fsize.np64_rl_rh_20260520.png`
  - `scripts_plot/outputAssessing/fig6_np64_np128_final/assess.expr1.fsize.np128_rl_rh_20260520.png`
  - `scripts_plot/outputAssessing/fig6_np64_np128_final/fig6_final_summary_20260520.csv`
  - `scripts_plot/outputAssessing/fig6_np64_np128_final/fig6_final_status_20260520.md`
- Conditions checked:
  - fsize values are KiB: `16 32 64 128 256 512 1024 2048 4096 8192`.
  - interval/`ta` is `10000 ns`.
  - walk list is shared across nodes and generated on `af309` using `N(0, sigma=0.015) + tbase`.
  - `NWALKS=3`, `NTESTS=100`, so a complete plotted point has `300` measured/theory samples.
  - MPI uses `--map-by core --bind-to core`.
  - script uses a physical-core `taskset` list.
  - nodes report `Thread(s) per core: 1`.
  - CPU frequency lock is attempted with performance governor where available.
- Plotted lines:
  - `c920bn3`: `clock_gettime`, `mpi_wtime`, `cntvct`, `cntvcto` for both `np64` and `np128`.
  - `camd9554n2`: `tsc`, `tsc_asym`, `clock_gettime`, `mpi_wtime` for `np64`; `tsc`, `clock_gettime`, `mpi_wtime` for `np128`.
  - `cgnr6760pn2`: `tsc`, `tsc_asym`, `clock_gettime`, `mpi_wtime` for both `np64` and `np128`.
- Exclusion:
  - `camd9554n2 np128 tsc_asym` is excluded because the original batch hung and lacks complete points for `1024 2048 4096 8192` KiB.
- Visual map:
  - Rows: processor platform.
  - Columns: left `$R_L$`, right `$R_H$`.
  - X-axis: fsize in KiB.
  - Y-axis: `$R_L`/`$R_H$` percentage computed by the code's quantile-window formula.
  - Legend: timer.
