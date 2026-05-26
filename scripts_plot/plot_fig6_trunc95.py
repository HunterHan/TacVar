#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd
from matplotlib.ticker import ScalarFormatter

import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from assess_plot import format_host_name, parse_meta, read_values, sample_quantile, ta_from_path_or_meta

NQUANT = 1000
Q_SPLIT = 900
TOP_DELTA = 50
FSIZE_EXPECTED = [16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192]
HOSTS = ["c920bn3", "camd9554n2", "cgnr6760pn2"]
EXPRS = ["assess.expr1.fsize.np64", "assess.expr1.fsize.np128"]
TIMER_ORDER = ["tsc", "tsc_asym", "clock_gettime", "mpi_wtime", "cntvct", "cntvcto"]
TIMER_COLORS = {
    "tsc": "#1f77b4",
    "tsc_asym": "#17becf",
    "clock_gettime": "#ff7f0e",
    "mpi_wtime": "#2ca02c",
    "cntvct": "#9467bd",
    "cntvcto": "#e377c2",
}


def ordered_timers(values: set[str]) -> list[str]:
    return [t for t in TIMER_ORDER if t in values] + sorted(values - set(TIMER_ORDER))


def local_w_hat(theory: list[float], measured: list[float], x_idx: int, y_idx: int) -> float:
    if x_idx > y_idx:
        return math.nan
    tau = measured[0] - theory[0]
    total = 0.0
    count = 0
    for idx in range(x_idx, y_idx + 1):
        prob = (idx - 1) / (NQUANT - 1)
        total += abs(sample_quantile(measured, prob) - sample_quantile(theory, prob) - tau)
        count += 1
    return total / count if count else math.nan


def theory_w_hat(theory: list[float], x_idx: int, y_idx: int) -> float:
    if x_idx > y_idx:
        return math.nan
    tmin = theory[0]
    total = 0.0
    count = 0
    for idx in range(x_idx, y_idx + 1):
        prob = (idx - 1) / (NQUANT - 1)
        total += abs(sample_quantile(theory, prob) - tmin)
        count += 1
    return total / count if count else math.nan


def values_from_run_dirs(run_dirs: str) -> tuple[list[float], list[float]]:
    measured: list[float] = []
    theory: list[float] = []
    for rd in str(run_dirs).split(";"):
        if not rd:
            continue
        p = Path(rd)
        vals = read_values(p / "assess_ta_cdf.csv")
        meta = parse_meta(p.parents[1] / "meta.md")
        ta = ta_from_path_or_meta(p, meta)
        measured.extend(vals)
        theory.extend([ta] * len(vals))
    return sorted(measured), sorted(theory)


def recompute_rl_rh(row: pd.Series) -> tuple[float, float]:
    measured, theory = values_from_run_dirs(row["run_dirs"])
    low_num = local_w_hat(theory, measured, 1, Q_SPLIT)
    low_den = theory_w_hat(theory, 1, Q_SPLIT)
    high_num = local_w_hat(theory, measured, Q_SPLIT + 1, NQUANT - TOP_DELTA)
    high_den = theory_w_hat(theory, Q_SPLIT + 1, NQUANT - TOP_DELTA)
    return low_num / low_den * 100.0, high_num / high_den * 100.0


def plot_expr(df: pd.DataFrame, expr: str, out_dir: Path, date: str) -> Path:
    sub = df[df["expr"] == expr].copy()
    fig, axes = plt.subplots(len(HOSTS), 2, figsize=(13.6, 2.9 * len(HOSTS)), squeeze=False)
    for row_idx, host in enumerate(HOSTS):
        hdf = sub[sub["host"] == host]
        for col_idx, (metric, ylabel) in enumerate([("r_l_pct_trunc", r"$R_L$ (%)"), ("r_h_pct_trunc", r"$R_H$ (%)")]):
            ax = axes[row_idx, col_idx]
            for timer in ordered_timers(set(hdf["timer"])):
                tdf = hdf[hdf["timer"] == timer].sort_values("fsize_kib")
                ax.plot(
                    tdf["fsize_kib"],
                    tdf[metric],
                    marker="o",
                    linewidth=1.5,
                    markersize=3.8,
                    label=timer,
                    color=TIMER_COLORS.get(timer),
                )
            ax.set_title(format_host_name(host))
            ax.set_xlabel("fsize (KiB)")
            ax.set_ylabel(ylabel)
            ax.set_xscale("log", base=2)
            ax.set_xticks(FSIZE_EXPECTED)
            ax.get_xaxis().set_major_formatter(ScalarFormatter())
            ax.grid(True, alpha=0.28)
            if not hdf.empty:
                ax.legend(title="timer", fontsize=7, title_fontsize=8, ncols=2)
    fig.suptitle(f"{expr}: $R_L$ and $R_H$ vs fsize (top 5% truncated)", y=0.995)
    fig.tight_layout()
    out = out_dir / f"{expr}_rl_rh_trunc95_{date}.png"
    fig.savefig(out, dpi=220)
    plt.close(fig)
    return out


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--date", default="20260520")
    ap.add_argument("--input", type=Path, default=Path("scripts_plot/outputAssessing/fig6_np64_np128_final/fig6_final_summary_20260520.csv"))
    ap.add_argument("--output", type=Path, default=Path("scripts_plot/outputAssessing/fig6_np64_np128_final_trunc95"))
    args = ap.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    df = pd.read_csv(args.input)
    df = df[df["expr"].isin(EXPRS)].copy()
    rows = []
    for _, row in df.iterrows():
        rl, rh = recompute_rl_rh(row)
        row = row.copy()
        row["r_l_pct_trunc"] = rl
        row["r_h_pct_trunc"] = rh
        row["rh_top_quantile_max"] = (NQUANT - TOP_DELTA - 1) / (NQUANT - 1)
        rows.append(row)
    out_df = pd.DataFrame(rows)
    summary = args.output / f"fig6_final_summary_trunc95_{args.date}.csv"
    out_df.to_csv(summary, index=False, quoting=csv.QUOTE_MINIMAL)

    produced = [summary]
    for expr in EXPRS:
        produced.append(plot_expr(out_df, expr, args.output, args.date))
    for p in produced:
        print(p)


if __name__ == "__main__":
    main()
