#!/usr/bin/env python3
import argparse
import csv
from pathlib import Path

import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
import numpy as np
import pandas as pd

plt.rcParams.update({
    "font.size": 8,
    "axes.titlesize": 8,
    "axes.labelsize": 8,
    "legend.fontsize": 7,
    "xtick.labelsize": 7,
    "ytick.labelsize": 7,
})

TIMER_ORDER = ["tsc", "cgt", "wtime", "papi", "papix6", "likwid", "cntvct", "cntvcto"]
TIMER_COLORS = {
    "tsc": "#1f77b4",
    "cgt": "#ff7f0e",
    "wtime": "#2ca02c",
    "papi": "#d62728",
    "papix6": "#9467bd",
    "likwid": "#8c564b",
    "cntvct": "#17becf",
    "cntvcto": "#e377c2",
}
HOST_ORDER = ["c920bn3", "cgnr6760pn2", "camd9554n2"]
NP_ORDER = [64, 128]


def read_summary(path: Path) -> pd.DataFrame:
    frames = []
    for csv_path in sorted(path.glob("*/summary.csv")):
        frames.append(pd.read_csv(csv_path))
    if not frames:
        raise FileNotFoundError(f"no summary.csv files under {path}")
    data = pd.concat(frames, ignore_index=True)
    for col in ["np", "size", "wd", "er", "ep", "binw", "nsamp"]:
        data[col] = pd.to_numeric(data[col], errors="coerce")
    return data


def finite_ok(data: pd.DataFrame) -> pd.DataFrame:
    return data[(data["status"] == "ok") & np.isfinite(data["wd"])].copy()


def timer_legend(data: pd.DataFrame):
    present = [timer for timer in TIMER_ORDER if timer in set(data["timer"].dropna())]
    return [
        Line2D([0], [0], color=TIMER_COLORS[timer], lw=1.5, marker="o", ms=3, label=timer)
        for timer in present
    ]


def subplot_grid():
    fig, axes = plt.subplots(
        len(HOST_ORDER),
        len(NP_ORDER),
        figsize=(7.2, 6.2),
        sharex=True,
        sharey=False,
    )
    fig.subplots_adjust(left=0.075, right=0.985, top=0.965, bottom=0.13, hspace=0.36, wspace=0.14)
    return fig, axes


def plot_wd_summary(data: pd.DataFrame, out: Path):
    ok = finite_ok(data)
    fig, axes = subplot_grid()
    kernel = "jacobi2d5p"
    for i, host in enumerate(HOST_ORDER):
        for j, npv in enumerate(NP_ORDER):
            ax = axes[i, j]
            sub = ok[(ok["host"] == host) & (ok["np"] == npv) & (ok["kernel"] == kernel)]
            for timer in TIMER_ORDER:
                tsub = sub[sub["timer"] == timer].sort_values("size")
                if tsub.empty:
                    continue
                color = TIMER_COLORS[timer]
                ax.plot(tsub["size"], tsub["wd"], marker="o", ms=3.5, lw=1.4, color=color, label=timer)
                bad = tsub[(tsub["er"] > 0.01) | (tsub["ep"] > 0.1)]
                if not bad.empty:
                    ax.scatter(bad["size"], bad["wd"], marker="x", s=38, color=color, zorder=4)
            ax.set_xscale("log", base=2)
            ax.grid(True, alpha=0.25)
            ax.set_title(f"{host}, np={npv}", fontsize=9)
            if i == len(HOST_ORDER) - 1:
                ax.set_xlabel("size")
            if j == 0:
                ax.set_ylabel("Wasserstein")
    fig.legend(handles=timer_legend(ok), loc="lower center", ncol=8, frameon=False, bbox_to_anchor=(0.5, 0.01))
    fig.savefig(out / "expr_filg_erep_wd.png", dpi=300)
    plt.close(fig)


def read_hist(path: Path):
    if not path.exists():
        return None
    arr = np.loadtxt(path, delimiter=",")
    if arr.ndim == 1:
        arr = arr.reshape(1, -1)
    if arr.shape[1] < 2:
        return None
    x = arr[:, 0]
    y = arr[:, 1]
    finite = np.isfinite(x) & np.isfinite(y) & (y >= 0)
    x = x[finite]
    y = y[finite]
    if x.size == 0 or y.sum() <= 0:
        return None
    cdf = np.cumsum(y) / y.sum()
    icdf = 1.0 - cdf
    return x, np.maximum(icdf, 1e-8)


def select_hist_rows(data: pd.DataFrame, kernel: str, hist_root: Path) -> pd.DataFrame:
    ok = finite_ok(data)
    rows = []
    for host in HOST_ORDER:
        for npv in NP_ORDER:
            sub = ok[(ok["host"] == host) & (ok["np"] == npv) & (ok["kernel"] == kernel)]
            if sub.empty:
                continue
            size_counts = sub.groupby("size")["timer"].nunique().sort_values(ascending=False)
            for size in size_counts.index:
                cand = sub[sub["size"] == size].copy()
                usable = []
                for _, row in cand.iterrows():
                    rel = Path(row["rel_dir"])
                    hdir = hist_root / host / "hist" / host / rel
                    if (hdir / "tm_hist.csv").exists() and (hdir / "tr_hist.csv").exists():
                        usable.append(row)
                if usable:
                    rows.extend(usable)
                    break
    if not rows:
        return pd.DataFrame()
    return pd.DataFrame(rows)


def plot_icdf(data: pd.DataFrame, hist_root: Path, kernel: str, out_file: str, out: Path):
    rows = select_hist_rows(data, kernel, hist_root)
    if rows.empty:
        raise FileNotFoundError(f"no hist rows for {kernel}")
    fig, axes = subplot_grid()
    for i, host in enumerate(HOST_ORDER):
        for j, npv in enumerate(NP_ORDER):
            ax = axes[i, j]
            sub = rows[(rows["host"] == host) & (rows["np"] == npv)]
            title_size = None
            for timer in TIMER_ORDER:
                tsub = sub[sub["timer"] == timer]
                if tsub.empty:
                    continue
                row = tsub.iloc[0]
                title_size = int(row["size"])
                hdir = hist_root / host / "hist" / host / Path(row["rel_dir"])
                color = TIMER_COLORS[timer]
                tm = read_hist(hdir / "tm_hist.csv")
                tr = read_hist(hdir / "tr_hist.csv")
                if tm is not None:
                    ax.plot(tm[0], tm[1], ls="--", lw=1.0, color=color, alpha=0.75)
                if tr is not None:
                    ax.plot(tr[0], tr[1], ls="-", lw=1.4, color=color, label=timer)
            ax.set_yscale("log")
            ax.grid(True, alpha=0.25)
            suffix = f", size={title_size}" if title_size is not None else ""
            ax.set_title(f"{host}, np={npv}{suffix}", fontsize=9)
            if i == len(HOST_ORDER) - 1:
                ax.set_xlabel("time (ns)")
            if j == 0:
                ax.set_ylabel("ICDF")
    fig.legend(handles=timer_legend(rows), loc="lower center", ncol=8, frameon=False, bbox_to_anchor=(0.5, 0.01))
    fig.savefig(out / out_file, dpi=300)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary-root", required=True, type=Path)
    parser.add_argument("--hist-root", required=True, type=Path)
    parser.add_argument("--out-dir", required=True, type=Path)
    args = parser.parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    data = read_summary(args.summary_root)
    data.to_csv(args.out_dir / "stencil_filter_summary.csv", index=False, quoting=csv.QUOTE_MINIMAL)
    plot_wd_summary(data, args.out_dir)
    plot_icdf(data, args.hist_root, "jacobi2d5p", "expr_filt_jacobi_draft.png", args.out_dir)
    plot_icdf(data, args.hist_root, "tl_f90_cg_calc_w", "expr_filt_tl_draft.png", args.out_dir)


if __name__ == "__main__":
    main()
