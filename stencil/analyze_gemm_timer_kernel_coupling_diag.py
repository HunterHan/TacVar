#!/usr/bin/env python3
import argparse
import csv
import statistics
from collections import defaultdict
from pathlib import Path

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
except Exception:
    plt = None


def quantile(xs, q):
    xs = sorted(xs)
    if not xs:
        return float("nan")
    k = (len(xs) - 1) * q
    lo = int(k)
    hi = min(lo + 1, len(xs) - 1)
    frac = k - lo
    return xs[lo] * (1 - frac) + xs[hi] * frac


def parse_result_dir(path):
    parts = path.name.split("_")
    if len(parts) < 4:
        return None
    timer, kernel = parts[0], parts[1]
    np_part = next((x for x in parts if x.startswith("np")), None)
    size_part = next((x for x in parts if x.startswith("size")), None)
    if not np_part or not size_part:
        return None
    return {
        "timer": timer,
        "kernel": kernel,
        "np": int(np_part[2:]),
        "size": int(size_part[4:]),
    }


def read_values(path):
    values = []
    checksums = []
    for csv_path in path.glob("*_rank*.csv"):
        with open(csv_path, newline="") as f:
            for row in csv.DictReader(f):
                try:
                    v = float(row["elapsed_ns"])
                    c = float(row["checksum"])
                except Exception:
                    continue
                if v > 0:
                    values.append(v)
                    checksums.append(c)
    return values, checksums


def load_rows(data_root, run_id):
    rows = []
    for run_dir in sorted(Path(data_root).glob(f"*/output_gemm_timer_kernel_coupling_diag/{run_id}")):
        host = run_dir.parts[-3]
        for result_dir in sorted(run_dir.glob("*_*_np*_size*")):
            meta = parse_result_dir(result_dir)
            if not meta:
                continue
            values, checksums = read_values(result_dir)
            if not values:
                continue
            rows.append({
                "host": host,
                **meta,
                "n": len(values),
                "median_ns": statistics.median(values),
                "p10_ns": quantile(values, 0.10),
                "p90_ns": quantile(values, 0.90),
                "checksum_min": min(checksums) if checksums else float("nan"),
                "checksum_max": max(checksums) if checksums else float("nan"),
            })
    return rows


def summarize(rows):
    by = defaultdict(dict)
    for r in rows:
        by[(r["host"], r["np"], r["size"], r["kernel"])][r["timer"]] = r
    out = []
    for key, timers in sorted(by.items()):
        ref = timers.get("tsc")
        if not ref:
            continue
        for timer, r in sorted(timers.items()):
            rec = dict(r)
            rec["ref_median_ns"] = ref["median_ns"]
            rec["ratio_to_tsc"] = r["median_ns"] / ref["median_ns"] if ref["median_ns"] else float("nan")
            out.append(rec)

    by2 = defaultdict(dict)
    for r in out:
        by2[(r["host"], r["np"], r["size"], r["timer"])][r["kernel"]] = r
    for key, kernels in by2.items():
        gemm = kernels.get("gemm")
        dsub = kernels.get("dsub")
        if not gemm or not dsub:
            continue
        interaction = gemm["ratio_to_tsc"] / dsub["ratio_to_tsc"] if dsub["ratio_to_tsc"] else float("nan")
        for r in (gemm, dsub):
            r["interaction_i_timer"] = interaction
            r["gemm_ratio_to_tsc"] = gemm["ratio_to_tsc"]
            r["dsub_ratio_to_tsc"] = dsub["ratio_to_tsc"]
    return out


def write_summary(rows, out_dir):
    fields = [
        "host", "np", "size", "timer", "kernel", "n", "median_ns", "p10_ns", "p90_ns",
        "ref_median_ns", "ratio_to_tsc", "interaction_i_timer",
        "gemm_ratio_to_tsc", "dsub_ratio_to_tsc", "checksum_min", "checksum_max",
    ]
    p = out_dir / "gemm_timer_kernel_coupling_summary.csv"
    with open(p, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields, lineterminator="\n")
        w.writeheader()
        for r in rows:
            w.writerow({k: r.get(k, "") for k in fields})
    return p


def write_diagnosis(rows, out_dir):
    timers = ["cgt", "papi", "wtime"]
    lines = [
        "# GEMM/DSUB Timer-Kernel Coupling Diagnosis",
        "",
        "`I_timer = (T_timer(GEMM)/T_TSC(GEMM)) / (T_timer(DSub)/T_TSC(DSub))`. Values above 1 indicate timer+GEMM amplification relative to timer+DSub; values below 1 indicate the opposite direction.",
        "",
        "| host | timer | GEMM/TSC | DSub/TSC | I_timer | conclusion |",
        "|---|---:|---:|---:|---:|---|",
    ]
    by = defaultdict(dict)
    for r in rows:
        if r["kernel"] == "gemm":
            by[(r["host"], r["timer"])] = r
    for host in sorted(set(r["host"] for r in rows)):
        for timer in timers:
            r = by.get((host, timer))
            if not r:
                continue
            i = float(r.get("interaction_i_timer", "nan"))
            if timer == "cgt":
                if i >= 1.10:
                    conclusion = "confirmed CGT+GEMM amplification"
                elif i <= 0.90:
                    conclusion = "rejected; GEMM ratio lower than DSub"
                else:
                    conclusion = "no material differential"
            elif timer == "papi":
                conclusion = "kernel-independent-ish" if 0.90 <= i <= 1.10 else "kernel-dependent"
            else:
                if i >= 1.10:
                    conclusion = "tracks CGT amplification"
                elif i <= 0.90:
                    conclusion = "GEMM ratio lower than DSub"
                else:
                    conclusion = "no material differential"
            lines.append(
                f"| {host} | {timer} | {float(r['gemm_ratio_to_tsc']):.3f} | "
                f"{float(r['dsub_ratio_to_tsc']):.3f} | {i:.3f} | {conclusion} |"
            )
    p = out_dir / "gemm_timer_kernel_coupling_diagnosis.md"
    p.write_text("\n".join(lines) + "\n")
    return p


def plot(rows, out_dir):
    if plt is None:
        return []
    paths = []
    for host in sorted(set(r["host"] for r in rows)):
        gemm_rows = [r for r in rows if r["host"] == host and r["kernel"] == "gemm" and r["timer"] in ("tsc", "cgt", "papi", "wtime")]
        timers = [r["timer"] for r in sorted(gemm_rows, key=lambda x: ["tsc", "cgt", "papi", "wtime"].index(x["timer"]))]
        gemm = [float(r["gemm_ratio_to_tsc"]) for r in sorted(gemm_rows, key=lambda x: ["tsc", "cgt", "papi", "wtime"].index(x["timer"]))]
        dsub = [float(r["dsub_ratio_to_tsc"]) for r in sorted(gemm_rows, key=lambda x: ["tsc", "cgt", "papi", "wtime"].index(x["timer"]))]
        inter = [float(r["interaction_i_timer"]) for r in sorted(gemm_rows, key=lambda x: ["tsc", "cgt", "papi", "wtime"].index(x["timer"]))]
        x = range(len(timers))
        width = 0.25
        fig, ax = plt.subplots(figsize=(8, 4))
        ax.bar([i - width for i in x], gemm, width=width, label="GEMM/TSC")
        ax.bar(list(x), dsub, width=width, label="DSub/TSC")
        ax.bar([i + width for i in x], inter, width=width, label="I_timer")
        ax.axhline(1.0, color="k", lw=1, alpha=0.4)
        ax.set_xticks(list(x))
        ax.set_xticklabels(timers)
        ax.set_ylabel("ratio")
        ax.set_title(f"GEMM/DSUB timer-kernel coupling: {host}")
        ax.grid(True, axis="y", alpha=0.3)
        ax.legend()
        fig.tight_layout()
        p = out_dir / f"gemm_timer_kernel_coupling_{host}.png"
        fig.savefig(p, dpi=180)
        plt.close(fig)
        paths.append(p)
    return paths


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data-root", default="/astrum/home/hpchzy/code/data/20260624_timer_kernel_coupling")
    ap.add_argument("--run-id", default="20260624-gemm-timer-kernel-coupling")
    ap.add_argument("--out-dir", default="stencil/plots_0624_gemm_timer_kernel_coupling_diag")
    args = ap.parse_args()
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    rows = load_rows(args.data_root, args.run_id)
    if not rows:
        raise SystemExit("no rows found")
    summary = summarize(rows)
    sp = write_summary(summary, out_dir)
    dp = write_diagnosis(summary, out_dir)
    plots = plot(summary, out_dir)
    print(f"raw_rows={len(rows)} summary_rows={len(summary)}")
    print(f"summary={sp}")
    print(f"diagnosis={dp}")
    for p in plots:
        print(f"plot={p}")


if __name__ == "__main__":
    main()
