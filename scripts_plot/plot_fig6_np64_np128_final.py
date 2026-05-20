#!/usr/bin/env python3
"""Build final Fig.6 RL/RH plots from all assessing batches.

This script is intentionally stricter than ``assess_plot.py``'s quicklook
mode.  It scans every timestamped batch, keeps the newest complete
``(host, expr, timer, fsize)`` row with the expected sample count, and only
plots a timer when all expected fsize points are present.
"""
from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd
from matplotlib.ticker import ScalarFormatter

SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))

from assess_plot import collect, format_host_name  # noqa: E402


HOSTS_DEFAULT = ["c920bn3", "camd9554n2", "cgnr6760pn2"]
EXPRS_DEFAULT = ["assess.expr1.fsize.np64", "assess.expr1.fsize.np128"]
FSIZE_EXPECTED = [16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192]
EXPECTED_SAMPLES = 300
TIMER_ORDER = [
    "tsc",
    "tsc_asym",
    "clock_gettime",
    "mpi_wtime",
    "cntvct",
    "cntvcto",
]
TIMER_COLORS = {
    "tsc": "#1f77b4",
    "tsc_asym": "#17becf",
    "clock_gettime": "#ff7f0e",
    "mpi_wtime": "#2ca02c",
    "cntvct": "#9467bd",
    "cntvcto": "#e377c2",
}


def ordered_timers(values: set[str]) -> list[str]:
    known = [timer for timer in TIMER_ORDER if timer in values]
    extras = sorted(timer for timer in values if timer not in TIMER_ORDER)
    return known + extras


def select_complete_rows(df: pd.DataFrame, exprs: list[str]) -> tuple[pd.DataFrame, list[str]]:
    notes: list[str] = []
    sub = df[df["expr"].isin(exprs)].copy()
    sub = sub[
        (sub["n_measured"] == EXPECTED_SAMPLES)
        & (sub["n_theory"] == EXPECTED_SAMPLES)
        & (sub["fsize_kib"].isin(FSIZE_EXPECTED))
        & (sub["interval_ns"] == 10000)
        & (sub["fkern"] == "copy")
        & (sub["rkern"] == "none")
    ].copy()
    if sub.empty:
        return sub, ["No rows passed the complete-row filter."]

    sub = sub.sort_values(["host", "expr", "timer", "fsize_kib", "batch"])
    sub = sub.groupby(["host", "expr", "timer", "fsize_kib"], as_index=False).tail(1)

    kept = []
    expected = set(FSIZE_EXPECTED)
    for (host, expr, timer), gdf in sub.groupby(["host", "expr", "timer"]):
        present = set(int(v) for v in gdf["fsize_kib"].dropna().tolist())
        missing = sorted(expected - present)
        if missing:
            notes.append(f"- excluded {host} {expr} {timer}: missing fsize {missing}")
            continue
        kept.append(gdf)
    if not kept:
        return pd.DataFrame(columns=sub.columns), notes
    return pd.concat(kept, ignore_index=True), notes


def plot_expr(df: pd.DataFrame, expr: str, hosts: list[str], out_dir: Path, date: str) -> Path | None:
    sub = df[df["expr"] == expr].copy()
    if sub.empty:
        return None
    fig, axes = plt.subplots(len(hosts), 2, figsize=(13.6, max(3.0, 2.9 * len(hosts))), squeeze=False)
    for row_idx, host in enumerate(hosts):
        hdf = sub[sub["host"] == host]
        for col_idx, (metric, ylabel) in enumerate([("r_l_pct", r"$R_L$ (%)"), ("r_h_pct", r"$R_H$ (%)")]):
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
    fig.suptitle(f"{expr}: $R_L$ and $R_H$ vs fsize", y=0.995)
    fig.tight_layout()
    out_path = out_dir / f"{expr}_rl_rh_{date}.png"
    fig.savefig(out_path, dpi=220)
    plt.close(fig)
    return out_path


def write_status(df_all: pd.DataFrame, df_final: pd.DataFrame, notes: list[str], out_dir: Path, date: str) -> Path:
    path = out_dir / f"fig6_final_status_{date}.md"
    lines = [
        "# Fig6 final status",
        "",
        f"- Expected samples per point: `{EXPECTED_SAMPLES}` (`NWALKS=3`, `NTESTS=100`).",
        f"- Expected fsize KiB: `{' '.join(str(v) for v in FSIZE_EXPECTED)}`.",
        "- Selection rule: newest complete row per `(host, expr, timer, fsize)`, then require all fsize points for a timer line.",
        "",
        "## Plotted timer lines",
    ]
    for (host, expr), gdf in df_final.groupby(["host", "expr"]):
        timers = ", ".join(ordered_timers(set(gdf["timer"])))
        lines.append(f"- `{host}` `{expr}`: {timers or 'none'}")
    lines.extend(["", "## Notes"])
    lines.extend(notes or ["- no exclusions"])
    lines.extend(["", "## Source batches"])
    source = (
        df_final.groupby(["host", "expr", "timer"])["batch"]
        .unique()
        .reset_index()
        .sort_values(["host", "expr", "timer"])
    )
    for row in source.itertuples(index=False):
        batches = ", ".join(str(v) for v in row.batch)
        lines.append(f"- `{row.host}` `{row.expr}` `{row.timer}`: {batches}")
    path.write_text("\n".join(lines) + "\n")
    return path


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--data-root", type=Path, default=Path.home() / "code/data")
    ap.add_argument("--date", required=True)
    ap.add_argument("--hosts", nargs="+", default=HOSTS_DEFAULT)
    ap.add_argument("--exprs", nargs="+", default=EXPRS_DEFAULT)
    ap.add_argument("--output", type=Path, required=True)
    args = ap.parse_args()

    args.output.mkdir(parents=True, exist_ok=True)
    df_all = collect(args.data_root, args.date, args.hosts)
    if df_all.empty:
        raise SystemExit("No assessing rows found.")
    df_final, notes = select_complete_rows(df_all, args.exprs)
    if df_final.empty:
        raise SystemExit("No complete fig6 rows found after filtering.")

    summary = args.output / f"fig6_final_summary_{args.date}.csv"
    df_final.to_csv(summary, index=False, quoting=csv.QUOTE_MINIMAL)
    produced: list[Path] = [summary, write_status(df_all, df_final, notes, args.output, args.date)]
    for expr in args.exprs:
        path = plot_expr(df_final, expr, args.hosts, args.output, args.date)
        if path is not None:
            produced.append(path)
    for path in produced:
        print(path)


if __name__ == "__main__":
    main()
