#!/usr/bin/env python3
from pathlib import Path
import csv
import sys

import matplotlib.pyplot as plt
import pandas as pd
from matplotlib.ticker import ScalarFormatter

SCRIPT_DIR = Path("scripts_plot").resolve()
sys.path.insert(0, str(SCRIPT_DIR))
from assess_plot import format_host_name  # noqa: E402

OUT = Path("scripts_plot/outputAssessing/fig6_combined_c920_final_cgnr_camd_repeat2")
OUT.mkdir(parents=True, exist_ok=True)

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


def ordered_timers(values):
    return [t for t in TIMER_ORDER if t in values] + sorted(set(values) - set(TIMER_ORDER))


def read_sources():
    base = pd.read_csv("scripts_plot/outputAssessing/fig6_np64_np128_final/fig6_final_summary_20260520.csv")
    camd = pd.read_csv("scripts_plot/outputAssessing/fig6_repeat2_camd_noguard/all_batches/assess_summary_20260521.csv")
    cgnr = pd.read_csv("scripts_plot/outputAssessing/fig6_repeat2_guarded_compare/all_batches/assess_summary_20260521.csv")

    c920 = base[(base.host == "c920bn3") & base.expr.isin(EXPRS)].copy()
    c920["source_set"] = "c920 final 20260520"

    camd = camd[
        (camd.host == "camd9554n2")
        & camd.expr.isin(["assess.expr1.fsize.np64.repeat2", "assess.expr1.fsize.np128.repeat2"])
    ].copy()
    camd["expr"] = camd["expr"].str.replace(".repeat2", "", regex=False)
    camd["source_set"] = "camd repeat2 no-guard 20260521"

    cgnr = cgnr[
        (cgnr.host == "cgnr6760pn2")
        & cgnr.expr.isin(["assess.expr1.fsize.np64.repeat2", "assess.expr1.fsize.np128.repeat2"])
    ].copy()
    cgnr["expr"] = cgnr["expr"].str.replace(".repeat2", "", regex=False)
    cgnr["source_set"] = "cgnr repeat2 guarded 20260521"

    df = pd.concat([c920, camd, cgnr], ignore_index=True)
    df = df[
        (df.n_measured == 300)
        & (df.n_theory == 300)
        & (df.fsize_kib.isin(FSIZE_EXPECTED))
        & (df.interval_ns == 10000)
        & (df.fkern == "copy")
        & (df.rkern == "none")
    ].copy()
    df = df.sort_values(["host", "expr", "timer", "fsize_kib", "batch"])
    df = df.groupby(["host", "expr", "timer", "fsize_kib"], as_index=False).tail(1)
    return df


def complete_only(df):
    kept = []
    notes = []
    expected = set(FSIZE_EXPECTED)
    for (host, expr, timer), g in df.groupby(["host", "expr", "timer"]):
        present = set(int(x) for x in g.fsize_kib.tolist())
        missing = sorted(expected - present)
        if missing:
            notes.append(f"- excluded `{host}` `{expr}` `{timer}`: missing fsize {missing}")
        else:
            kept.append(g)
    if not kept:
        raise SystemExit("No complete timer lines.")
    return pd.concat(kept, ignore_index=True), notes


def plot_expr(df, expr):
    sub = df[df.expr == expr].copy()
    fig, axes = plt.subplots(len(HOSTS), 2, figsize=(13.6, 8.7), squeeze=False)
    for row_idx, host in enumerate(HOSTS):
        hdf = sub[sub.host == host]
        for col_idx, (metric, ylabel) in enumerate([("r_l_pct", r"$R_L$ (%)"), ("r_h_pct", r"$R_H$ (%)")]):
            ax = axes[row_idx, col_idx]
            for timer in ordered_timers(set(hdf.timer)):
                tdf = hdf[hdf.timer == timer].sort_values("fsize_kib")
                ax.plot(
                    tdf.fsize_kib,
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
    path = OUT / f"{expr}_rl_rh_combined_c920final_cgnr-camd-repeat2_20260521.png"
    fig.savefig(path, dpi=220)
    plt.close(fig)
    return path


def main():
    df, notes = complete_only(read_sources())
    summary = OUT / "fig6_combined_summary_20260521.csv"
    df.to_csv(summary, index=False, quoting=csv.QUOTE_MINIMAL)
    paths = [summary]
    for expr in EXPRS:
        paths.append(plot_expr(df, expr))

    status = OUT / "fig6_combined_status_20260521.md"
    lines = [
        "# Fig6 combined status",
        "",
        "- `c920bn3`: previous final batch from `20260520`.",
        "- `camd9554n2`: repeat2 no-guard batch from `20260521`; high CPU was logged but did not stop the run.",
        "- `cgnr6760pn2`: guarded repeat2 batch from `20260521`.",
        "- Each timer line requires all fsize values `16..8192` KiB and `n_measured=n_theory=300`.",
        "",
        "## Plotted timer lines",
    ]
    for (host, expr), g in df.groupby(["host", "expr"]):
        lines.append(f"- `{host}` `{expr}`: {', '.join(ordered_timers(set(g.timer)))}")
    lines.extend(["", "## Source batches"])
    for (host, expr, timer), g in df.groupby(["host", "expr", "timer"]):
        batches = ", ".join(str(v) for v in sorted(g.batch.unique()))
        source = ", ".join(str(v) for v in sorted(g.source_set.unique()))
        lines.append(f"- `{host}` `{expr}` `{timer}`: {batches}; {source}")
    lines.extend(["", "## Notes"])
    lines.extend(notes or ["- no exclusions"])
    status.write_text("\n".join(lines) + "\n")
    paths.append(status)
    for p in paths:
        print(p)


if __name__ == "__main__":
    main()
