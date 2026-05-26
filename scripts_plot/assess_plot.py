#!/usr/bin/env python3
"""Plot TacVar assessing outputs with tex-facing conventions.

The script scans ``~/code/data/<date>/<host>/outputAssessing``. Its primary
metric is the 1-Wasserstein distance between the measured assessing
distribution and the theoretical walking-list distribution for the same runs.
Hosts are kept in separate panels; timer is the within-panel legend where a
legend is needed.
"""
from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path

import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter
from matplotlib.ticker import ScalarFormatter


HOSTS_DEFAULT = ["c920bn3", "camd9554n2", "cgnr6760pn2"]
KERNEL_ORDER = ["copy", "add", "scale", "triad", "pow", "dgemm"]
RL_RH_NQUANT = 1000
RL_RH_DELTA = 5
RL_RH_Q = 900



def format_host_name(host: str) -> str:
    """Format host internal name to display name."""
    host_map = {
        'c920bn3': 'Kunpeng 920B',
        'camd9554n2': 'AMD EPYC 9554',
        'cgnr6760pn2': 'Intel Xeon 6760P',
    }
    return host_map.get(host, host)


def generate_rl_rh_title(expr: str) -> str:
    """Generate title for RL/RH plots."""
    if "fsize" in expr:
        return r"Trends of $R_L$ and $R_H$ of various timers with different flush size"
    elif "interval" in expr:
        return r"Trends of $R_L$ and $R_H$ of various timers with different intervals"
    else:
        return f"{expr}: $R_L$ vs $R_H$"


def format_kernel_name(kernel: str) -> str:
    return kernel.upper()


def ordered_kernels(values: list[str]) -> list[str]:
    present = [kernel for kernel in KERNEL_ORDER if kernel in values]
    extras = sorted(kernel for kernel in values if kernel not in KERNEL_ORDER)
    return present + extras


def latex_escape(text: str) -> str:
    return (
        text.replace("\\", r"\\textbackslash{}")
        .replace("&", r"\&")
        .replace("%", r"\%")
        .replace("$", r"\$")
        .replace("#", r"\#")
        .replace("_", r"\_")
        .replace("{", r"\{")
        .replace("}", r"\}")
        .replace("~", r"\textasciitilde{}")
        .replace("^", r"\textasciicircum{}")
    )


def format_pair_cell(value: object) -> str:
    if pd.isna(value):
        return "--"
    return f"{float(value):.1f}"


def build_frkern_pair_tex(
    rl_table: pd.DataFrame,
    rh_table: pd.DataFrame,
    host: str,
    timer: str,
) -> str:
    row_order = ordered_kernels([str(value) for value in rl_table.index.union(rh_table.index)])
    col_order = ordered_kernels([str(value) for value in rl_table.columns.union(rh_table.columns)])
    n_cols = len(col_order)
    total_metric_cols = 2 * n_cols
    col_spec = "l" + "cc" * n_cols
    aligned_rl = rl_table.reindex(index=row_order, columns=col_order)
    aligned_rh = rh_table.reindex(index=row_order, columns=col_order)

    lines = [
        r"\begin{table*}[htbp]",
        r"\centering",
        rf"\caption{{{latex_escape(format_host_name(host))} {latex_escape(timer)}: $R_L$ and $R_H$ with different flush kernels (\%)}}",
        rf"\label{{tab:frkern-{host}-{timer}}}",
        rf"\begin{{tabular*}}{{\textwidth}}{{@{{\extracolsep{{\fill}}}}{col_spec}}}",
        r"\toprule",
        rf"\multirow{{3}}{{*}}{{Rear Flush}} & \multicolumn{{{total_metric_cols}}}{{c}}{{Front Flush}} \\ \cmidrule(lr){{2-{total_metric_cols + 1}}}",
    ]

    grouped_headers = [rf"\multicolumn{{2}}{{c}}{{{format_kernel_name(kernel)}}}" for kernel in col_order]
    cmidrules = " ".join(rf"\cmidrule(lr){{{2 * idx + 2}-{2 * idx + 3}}}" for idx in range(n_cols))
    lines.append("& " + " & ".join(grouped_headers) + rf" \\ {cmidrules}")
    lines.append("& " + " & ".join([r"$R_L$ & $R_H$" for _ in col_order]) + r" \\ \midrule")

    for row_kernel in row_order:
        row_values: list[str] = []
        for col_kernel in col_order:
            row_values.append(format_pair_cell(aligned_rl.loc[row_kernel, col_kernel]))
            row_values.append(format_pair_cell(aligned_rh.loc[row_kernel, col_kernel]))
        lines.append(f"{format_kernel_name(row_kernel)} & " + " & ".join(row_values) + r" \\")

    lines.extend([
        r"\bottomrule",
        r"\end{tabular*}",
        r"\end{table*}",
    ])
    return "\n".join(lines)

def parse_meta(meta_path: Path) -> dict[str, str]:
    meta: dict[str, str] = {}
    if not meta_path.exists():
        return meta
    for line in meta_path.read_text(errors="ignore").splitlines():
        if not line.startswith("|") or "`" not in line:
            continue
        parts = [p.strip() for p in line.strip("|").split("|")]
        if len(parts) >= 2 and parts[0] not in {"Key", "---"}:
            meta[parts[0]] = parts[1].strip("`")
    return meta


def read_values(path: Path) -> list[float]:
    vals: list[float] = []
    with path.open() as f:
        for line in f:
            line = line.strip()
            if line:
                vals.append(float(line))
    return vals


def empirical_w1(a: list[float], b: list[float]) -> float:
    """Return 1D empirical W1. Inputs are expanded to equal sample counts."""
    if not a or not b:
        return math.nan
    a_sorted = sorted(a)
    b_sorted = sorted(b)
    if len(a_sorted) == len(b_sorted):
        return sum(abs(x - y) for x, y in zip(a_sorted, b_sorted)) / len(a_sorted)

    # Quantile interpolation without scipy.
    n = max(len(a_sorted), len(b_sorted))
    total = 0.0
    for i in range(n):
        q = i / (n - 1) if n > 1 else 0.0
        total += abs(sample_quantile(a_sorted, q) - sample_quantile(b_sorted, q))
    return total / n


def local_w_hat(
    theory_sorted: list[float],
    measured_sorted: list[float],
    x_idx: int,
    y_idx: int,
) -> float:
    if not theory_sorted or not measured_sorted or x_idx > y_idx:
        return math.nan
    tau = measured_sorted[0] - theory_sorted[0]
    total = 0.0
    count = 0
    for idx in range(x_idx, y_idx + 1):
        prob = (idx - 1) / (RL_RH_NQUANT - 1)
        total += abs(sample_quantile(measured_sorted, prob) - sample_quantile(theory_sorted, prob) - tau)
        count += 1
    return total / count if count else math.nan


def theory_w_hat(theory_sorted: list[float], x_idx: int, y_idx: int) -> float:
    if not theory_sorted or x_idx > y_idx:
        return math.nan
    tmin = theory_sorted[0]
    total = 0.0
    count = 0
    for idx in range(x_idx, y_idx + 1):
        prob = (idx - 1) / (RL_RH_NQUANT - 1)
        total += abs(sample_quantile(theory_sorted, prob) - tmin)
        count += 1
    return total / count if count else math.nan


def rl_rh_percent(measured: list[float], theory: list[float]) -> tuple[float, float]:
    if not measured or not theory:
        return math.nan, math.nan
    measured_sorted = sorted(measured)
    theory_sorted = sorted(theory)
    low_num = local_w_hat(theory_sorted, measured_sorted, 1, RL_RH_Q)
    low_den = theory_w_hat(theory_sorted, 1, RL_RH_Q)
    high_num = local_w_hat(theory_sorted, measured_sorted, RL_RH_Q + 1, RL_RH_NQUANT - RL_RH_DELTA)
    high_den = theory_w_hat(theory_sorted, RL_RH_Q + 1, RL_RH_NQUANT - RL_RH_DELTA)
    rl = low_num / low_den * 100.0 if low_den else math.nan
    rh = high_num / high_den * 100.0 if high_den else math.nan
    return rl, rh


def sample_quantile(vals: list[float], q: float) -> float:
    if not vals:
        return math.nan
    if len(vals) == 1:
        return vals[0]
    pos = q * (len(vals) - 1)
    lo = int(math.floor(pos))
    hi = int(math.ceil(pos))
    if lo == hi:
        return vals[lo]
    frac = pos - lo
    return vals[lo] * (1.0 - frac) + vals[hi] * frac


def ta_from_path_or_meta(run_dir: Path, meta: dict[str, str]) -> float:
    name = run_dir.name
    if "_ta" in name:
        try:
            return float(name.rsplit("_ta", 1)[1])
        except ValueError:
            pass
    return float(meta.get("interval_ns", 0) or 0)


def collect_expr(expr_root: Path, host: str) -> pd.DataFrame:
    expr_name = expr_root.parent.name
    batch_id = expr_root.name
    run_log = expr_root / "run_assess.log"
    batch_done = run_log.exists() and "All done. Results under" in run_log.read_text(errors="ignore")

    combos: dict[tuple[str, str, str, int, int], dict[str, object]] = {}
    for cdf in expr_root.rglob("assess_ta_cdf.csv"):
        run_dir = cdf.parent
        combo_dir = run_dir.parents[1]
        meta = parse_meta(combo_dir / "meta.md")
        vals = read_values(cdf)
        if not vals:
            continue
        ta = ta_from_path_or_meta(run_dir, meta)
        timer = meta.get("timer", cdf.parents[3].name)
        fkern = meta.get("fkern", "")
        rkern = meta.get("rkern", "")
        interval_ns = int(float(meta.get("interval_ns", ta or 0) or 0))
        fsize_kib = int(float(meta.get("fsize_kib", 0) or 0))
        key = (timer, fkern, rkern, interval_ns, fsize_kib)
        row = combos.setdefault(
            key,
            {
                "host": host,
                "expr": expr_name,
                "batch": batch_id,
                "batch_root": str(expr_root),
                "batch_done": batch_done,
                "timer": timer,
                "interval_ns": interval_ns,
                "interval_us": interval_ns / 1_000.0,
                "fsize_kib": fsize_kib,
                "rkern": rkern,
                "fkern": fkern,
                "measured": [],
                "theory": [],
                "run_dirs": [],
            },
        )
        row["measured"].extend(vals)  # type: ignore[index, union-attr]
        row["theory"].extend([ta] * len(vals))  # type: ignore[index, union-attr]
        row["run_dirs"].append(str(run_dir))  # type: ignore[index, union-attr]

    rows = []
    for row in combos.values():
        measured = row.pop("measured")  # type: ignore[assignment]
        theory = row.pop("theory")  # type: ignore[assignment]
        row["n_measured"] = len(measured)  # type: ignore[arg-type]
        row["n_theory"] = len(theory)  # type: ignore[arg-type]
        row["w1_ns"] = empirical_w1(measured, theory)  # type: ignore[arg-type]
        rl_pct, rh_pct = rl_rh_percent(measured, theory)  # type: ignore[arg-type]
        row["r_l_pct"] = rl_pct
        row["r_h_pct"] = rh_pct
        mean_ta = sum(theory) / len(theory) if theory else math.nan  # type: ignore[arg-type]
        row["w1_rel"] = row["w1_ns"] / mean_ta if mean_ta else math.nan  # type: ignore[operator]
        row["theory_mean_ns"] = mean_ta
        row["measured_mean_ns"] = sum(measured) / len(measured) if measured else math.nan  # type: ignore[arg-type]
        row["run_dirs"] = ";".join(row["run_dirs"])  # type: ignore[index]
        rows.append(row)
    return pd.DataFrame(rows)


def collect(data_root: Path, date: str, hosts: list[str]) -> pd.DataFrame:
    frames = []
    for host in hosts:
        host_root = data_root / date / host / "outputAssessing"
        for expr_root in sorted(host_root.glob("assess.*/*")):
            df = collect_expr(expr_root, host)
            if not df.empty:
                frames.append(df)
    if not frames:
        return pd.DataFrame()
    return pd.concat(frames, ignore_index=True)


def keep_latest_batches(df: pd.DataFrame, complete_only: bool = True) -> pd.DataFrame:
    if df.empty:
        return df
    if complete_only:
        done = df[df["batch_done"]].copy()
        if not done.empty:
            df = done
    latest = df.groupby(["host", "expr"])["batch"].transform("max")
    return df[df["batch"] == latest].copy()


def plot_line_expr(
    df: pd.DataFrame,
    expr: str,
    out_dir: Path,
    date: str,
    hosts: list[str],
    metric: str = "w1_ns",
) -> Path | None:
    sub = df[df["expr"] == expr].copy()
    if sub.empty:
        return None
    if "fsize" in expr:
        x_col, x_label = "fsize_kib", "fsize (KiB)"
    elif "interval" in expr:
        x_col, x_label = "interval_us", "interval (us)"
    else:
        return None

    plot_df = (
        sub.groupby(["host", "timer", x_col], as_index=False)[metric]
        .mean()
        .sort_values(["host", "timer", x_col])
    )
    present_hosts = [h for h in hosts if h in set(plot_df["host"])] or sorted(plot_df["host"].unique())
    fig, axes = plt.subplots(len(present_hosts), 1, figsize=(7.2, max(3.0, 2.7 * len(present_hosts))), squeeze=False)
    for ax, host in zip(axes.ravel(), present_hosts):
        hdf = plot_df[plot_df["host"] == host]
        for timer, tdf in hdf.groupby("timer"):
            ax.plot(tdf[x_col], tdf[metric], marker="o", linewidth=1.5, label=timer)
        ax.set_title(format_host_name(host))
        ax.set_xlabel(x_label)
        ax.set_ylabel("Wasserstein (ns)" if metric == "w1_ns" else "Relative Wasserstein")
        ax.grid(True, alpha=0.3)
        ax.legend(title="timer", fontsize=8, title_fontsize=8)
        ax.set_xscale("log", base=2 if x_col == "fsize_kib" else 10)
        if x_col == "fsize_kib":
            ticks = sorted(hdf[x_col].dropna().unique())
            ax.set_xticks(ticks)
            ax.get_xaxis().set_major_formatter(ScalarFormatter())
        else:
            ax.get_xaxis().set_major_formatter(FuncFormatter(lambda value, _pos: f"{value:g}"))
    title_metric = "Wasserstein" if metric == "w1_ns" else "relative Wasserstein"
    fig.suptitle(f"{expr}: measured vs walking-list {title_metric}", y=0.995)
    fig.tight_layout()
    suffix = "wasserstein" if metric == "w1_ns" else "wasserstein_rel"
    out_path = out_dir / f"{expr}_{suffix}_{date}.png"
    fig.savefig(out_path, dpi=180)
    plt.close(fig)
    return out_path


def plot_rl_rh_expr(df: pd.DataFrame, expr: str, out_dir: Path, date: str, hosts: list[str]) -> Path | None:
    sub = df[df["expr"] == expr].copy()
    if sub.empty:
        return None
    if "fsize" in expr:
        x_col, x_label = "fsize_kib", "fsize (KiB)"
    elif "interval" in expr:
        x_col, x_label = "interval_us", "interval (us)"
    else:
        return None

    rows = []
    for metric, metric_label in [("r_l_pct", "R_L"), ("r_h_pct", "R_H")]:
        tmp = (
            sub.groupby(["host", "timer", x_col], as_index=False)[metric]
            .mean()
            .sort_values(["host", "timer", x_col])
        )
        tmp["metric"] = metric_label
        tmp["value"] = tmp[metric]
        rows.append(tmp[["host", "timer", x_col, "metric", "value"]])
    plot_df = pd.concat(rows, ignore_index=True)
    present_hosts = [h for h in hosts if h in set(plot_df["host"])] or sorted(plot_df["host"].unique())
    fig, axes = plt.subplots(len(present_hosts), 2, figsize=(14.0, max(3.2, 3.0 * len(present_hosts))), squeeze=False)
    for row_idx, host in enumerate(present_hosts):
        hdf = plot_df[plot_df["host"] == host]
        for col_idx, metric in enumerate(["R_L", "R_H"]):
            ax = axes[row_idx, col_idx]
            mdf = hdf[hdf["metric"] == metric]
            for timer, tdf in mdf.groupby("timer"):
                tdf = tdf.sort_values(x_col)
                ax.plot(tdf[x_col], tdf["value"], marker="o", linewidth=1.4, label=timer)
            ax.set_title(format_host_name(host))
            ax.set_xlabel(x_label)
            ax.set_ylabel(metric.replace('_', ''))
            ax.grid(True, alpha=0.3)
            ax.legend(title="timer", fontsize=7, title_fontsize=8)
            ax.set_xscale("log", base=2 if x_col == "fsize_kib" else 10)
            if x_col == "fsize_kib":
                ticks = sorted(mdf[x_col].dropna().unique())
                ax.set_xticks(ticks)
                ax.get_xaxis().set_major_formatter(ScalarFormatter())
            else:
                ax.get_xaxis().set_major_formatter(FuncFormatter(lambda value, _pos: f"{value:g}"))
    fig.suptitle(generate_rl_rh_title(expr), y=0.995)
    fig.tight_layout()
    out_path = out_dir / f"{expr}_rl_rh_{date}.png"
    fig.savefig(out_path, dpi=180)
    plt.close(fig)
    return out_path


def load_timer_detail(batch_root: Path, timer: str) -> dict[str, list[float]]:
    measured: list[float] = []
    theory: list[float] = []
    for cdf in batch_root.glob(f"{timer}/**/assess_ta_cdf.csv"):
        meta = parse_meta(cdf.parent.parents[1] / "meta.md")
        vals = read_values(cdf)
        ta = ta_from_path_or_meta(cdf.parent, meta)
        measured.extend(vals)
        theory.extend([ta] * len(vals))
    return {"measured": measured, "theory": theory}


def ecdf(vals: list[float]) -> tuple[list[float], list[float]]:
    if not vals:
        return [], []
    xs = sorted(vals)
    n = len(xs)
    ys = [(i + 1) / n for i in range(n)]
    return xs, ys


def plot_timer_distributions(df: pd.DataFrame, out_dir: Path, date: str, hosts: list[str]) -> list[Path]:
    sub = df[df["expr"].str.contains("timer", na=False)].copy()
    if sub.empty:
        return []
    paths = []
    for host in [h for h in hosts if h in set(sub["host"])]:
        hdf = sub[sub["host"] == host].sort_values("timer")
        timers = list(hdf["timer"].unique())
        if not timers:
            continue
        fig, axes = plt.subplots(len(timers), 2, figsize=(9.2, max(2.4, 2.05 * len(timers))), squeeze=False)
        for row_idx, timer in enumerate(timers):
            batch_root = Path(hdf[hdf["timer"] == timer]["batch_root"].iloc[0])
            vals = load_timer_detail(batch_root, timer)
            measured = vals["measured"]
            theory = vals["theory"]
            ax_hist, ax_cdf = axes[row_idx]
            ax_hist.hist(measured, bins=40, alpha=0.70, label="measured")
            ax_hist.hist(theory, bins=min(20, max(3, len(set(theory)))), alpha=0.45, label="walking list")
            ax_hist.set_ylabel(timer)
            ax_hist.set_xlabel("time (ns)")
            ax_hist.grid(True, alpha=0.25)
            ax_hist.legend(fontsize=7)

            mx, my = ecdf(measured)
            tx, ty = ecdf(theory)
            ax_cdf.plot(tx, ty, marker="o", linewidth=1.4, label="theoretical")
            ax_cdf.plot(mx, my, linewidth=1.4, label="measured")
            ax_cdf.set_xlabel("time (ns)")
            ax_cdf.set_ylabel("CDF")
            ax_cdf.grid(True, alpha=0.25)
            ax_cdf.legend(fontsize=7)
        axes[0, 0].set_title("Histogram")
        axes[0, 1].set_title("Execution Time Distribution")
        fig.suptitle(f"{format_host_name(host)}", y=0.995)
        fig.tight_layout()
        out_path = out_dir / f"assess.expr1.timer_distribution_{host}_{date}.png"
        fig.savefig(out_path, dpi=180)
        plt.close(fig)
        paths.append(out_path)
    return paths


def write_frkern_tables(df: pd.DataFrame, out_dir: Path, date: str, hosts: list[str]) -> list[Path]:
    sub = df[df["expr"].str.contains("frkern", na=False)].copy()
    if sub.empty:
        return []
    paths = []
    for host in [h for h in hosts if h in set(sub["host"])]:
        for timer in sorted(sub[sub["host"] == host]["timer"].unique()):
            tdf = sub[(sub["host"] == host) & (sub["timer"] == timer)]
            rl_table = tdf.pivot_table(index="fkern", columns="rkern", values="r_l_pct", aggfunc="mean")
            rh_table = tdf.pivot_table(index="fkern", columns="rkern", values="r_h_pct", aggfunc="mean")
            row_order = ordered_kernels([str(k) for k in rl_table.index.union(rh_table.index)])
            col_order = ordered_kernels([str(k) for k in rl_table.columns.union(rh_table.columns)])
            rl_table = rl_table.reindex(index=row_order, columns=col_order).round(3)
            rh_table = rh_table.reindex(index=row_order, columns=col_order).round(3)

            pair_csv_path = out_dir / f"assess.expr3.frkern_pair_table_{host}_{timer}_{date}.csv"
            pair_tex_path = out_dir / f"assess.expr3.frkern_pair_table_{host}_{timer}_{date}.tex"
            pair_df = pd.concat({"R_L": rl_table, "R_H": rh_table}, axis=1)
            pair_df.to_csv(pair_csv_path)
            pair_tex_path.write_text(build_frkern_pair_tex(rl_table, rh_table, host, timer))
            paths.extend([pair_csv_path, pair_tex_path])
    return paths


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--data-root", type=Path, default=Path.home() / "code/data")
    ap.add_argument("--date", required=True)
    ap.add_argument("--hosts", nargs="+", default=HOSTS_DEFAULT)
    ap.add_argument("--all-batches", action="store_true", help="include all timestamped batches instead of the latest host+expr batch")
    ap.add_argument("--allow-incomplete", action="store_true", help="allow latest incomplete batches when --all-batches is not set")
    ap.add_argument("--output", type=Path, default=Path("scripts_plot/outputAssessing"))
    args = ap.parse_args()

    args.output.mkdir(parents=True, exist_ok=True)
    print(f"data_root:{args.data_root} date:{args.date} hosts:{args.hosts} all_batches:{args.all_batches} allow_incomplete:{args.allow_incomplete} output:{args.output}")
    df = collect(args.data_root, args.date, args.hosts)
    print(df['expr'].unique())
    if df.empty:
        raise SystemExit("No assess_ta_cdf.csv files found.")
    if not args.all_batches:
        df = keep_latest_batches(df, complete_only=not args.allow_incomplete)
    print(df['expr'].unique())
    summary_path = args.output / f"assess_summary_{args.date}.csv"
    df.to_csv(summary_path, index=False, quoting=csv.QUOTE_MINIMAL)

    produced: list[Path] = [summary_path]
    for expr in sorted(df["expr"].unique()):
        print(expr)
        out = plot_line_expr(df, expr, args.output, args.date, args.hosts)
        if out is not None:
            produced.append(out)
        if "interval" in expr:
            rel_out = plot_line_expr(df, expr, args.output, args.date, args.hosts, metric="w1_rel")
            if rel_out is not None:
                produced.append(rel_out)
        rh_rl_out = plot_rl_rh_expr(df, expr, args.output, args.date, args.hosts)
        if rh_rl_out is not None:
            produced.append(rh_rl_out)
    produced.extend(plot_timer_distributions(df, args.output, args.date, args.hosts))
    produced.extend(write_frkern_tables(df, args.output, args.date, args.hosts))

    for path in produced:
        print(path)


if __name__ == "__main__":
    main()
