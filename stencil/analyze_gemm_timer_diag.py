#!/usr/bin/env python3
import argparse
import csv
import math
import re
from collections import defaultdict
from pathlib import Path

import numpy as np

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
except Exception:
    plt = None


def read_hist(path):
    values = []
    weights = []
    with open(path, newline="") as f:
        rows = list(csv.reader(f))
    if not rows:
        return None
    start = 0
    try:
        float(rows[0][0])
    except Exception:
        start = 1
    for row in rows[start:]:
        if not row:
            continue
        try:
            v = float(row[0])
        except Exception:
            continue
        w = 1.0
        if len(row) > 1:
            try:
                w = float(row[1])
            except Exception:
                w = 1.0
        values.append(v)
        weights.append(w)
    if not values:
        return None
    return np.array(values, dtype=float), np.array(weights, dtype=float)


def weighted_median(hist):
    x, w = hist
    order = np.argsort(x)
    x = x[order]
    w = w[order]
    c = np.cumsum(w)
    return float(x[np.searchsorted(c, c[-1] / 2, side="left")])


def w1(hist_a, hist_b):
    xa, wa = hist_a
    xb, wb = hist_b
    wa = wa / wa.sum()
    wb = wb / wb.sum()
    ia = np.argsort(xa)
    ib = np.argsort(xb)
    xa = xa[ia]
    wa = wa[ia]
    xb = xb[ib]
    wb = wb[ib]
    xs = np.unique(np.concatenate([xa, xb]))
    pa = pb = 0
    ca = cb = 0.0
    total = 0.0
    for i, x in enumerate(xs):
        while pa < len(xa) and xa[pa] == x:
            ca += wa[pa]
            pa += 1
        while pb < len(xb) and xb[pb] == x:
            cb += wb[pb]
            pb += 1
        if i + 1 < len(xs):
            total += abs(ca - cb) * (xs[i + 1] - x)
    return float(total)


def parse_result_dir(path):
    name = path.name
    m = re.match(
        r"gemm_timer_diag_(?P<variant>.+)_np(?P<np>\d+)_size(?P<size>\d+)_nsampRatio(?P<ratio>[^_]+)_nsamp(?P<nsamp>\d+)_filt$",
        name,
    )
    if not m:
        return None
    shuffle = path.parent.name
    sm = re.match(r"shuffle(\d+)$", shuffle)
    if not sm:
        return None
    d = m.groupdict()
    d["shuffle"] = int(sm.group(1))
    d["np"] = int(d["np"])
    d["size"] = int(d["size"])
    d["nsamp"] = int(d["nsamp"])
    return d


def load_rows(data_root, run_id):
    rows = []
    for host_dir in sorted(Path(data_root).glob(f"*/output_gemm_timer_diag/{run_id}")):
        host = host_dir.parts[-3]
        for res_dir in host_dir.glob("shuffle*/gemm_timer_diag_*_filt"):
            meta = parse_result_dir(res_dir)
            if not meta:
                continue
            tm = read_hist(res_dir / "tm_hist.csv")
            tr = read_hist(res_dir / "tr_hist.csv")
            if tm is None or tr is None:
                continue
            rows.append(
                {
                    "host": host,
                    "variant": meta["variant"],
                    "size": meta["size"],
                    "shuffle": meta["shuffle"],
                    "tm_hist": tm,
                    "tr_hist": tr,
                    "tm_med": weighted_median(tm),
                    "tr_med": weighted_median(tr),
                    "res_dir": str(res_dir),
                }
            )
    return rows


def reference_for_host(host, variants):
    if "cntvcto_current" in variants:
        return "cntvcto_current"
    if "tsc_current" in variants:
        return "tsc_current"
    raise RuntimeError(f"No reference timer found for {host}: {sorted(variants)}")


def summarize(rows):
    by = defaultdict(dict)
    for r in rows:
        by[(r["host"], r["size"], r["shuffle"])][r["variant"]] = r

    out = []
    for key, variants in by.items():
        host, size, shuffle = key
        ref_name = reference_for_host(host, variants)
        ref = variants[ref_name]
        denom = max(ref["tm_med"], 1e-12)
        for variant, r in variants.items():
            out.append(
                {
                    "host": host,
                    "size": size,
                    "shuffle": shuffle,
                    "variant": variant,
                    "reference": ref_name,
                    "tm_med": r["tm_med"],
                    "tr_med": r["tr_med"],
                    "tm_ratio": r["tm_med"] / ref["tm_med"],
                    "tr_ratio": r["tr_med"] / ref["tr_med"],
                    "tm_w1_pct": w1(r["tm_hist"], ref["tm_hist"]) / denom * 100.0,
                    "tr_w1_pct": w1(r["tr_hist"], ref["tr_hist"]) / denom * 100.0,
                }
            )
    for r in out:
        r["delta_w1_pct"] = r["tm_w1_pct"] - r["tr_w1_pct"]
        r["tr_gt_tm_w1"] = r["tr_w1_pct"] > r["tm_w1_pct"]
    return out


def median(values):
    if not values:
        return float("nan")
    return float(np.median(values))


def write_summary(summary, out_dir):
    out = out_dir / "gemm_timer_diag_summary.csv"
    fields = [
        "host",
        "size",
        "shuffle",
        "variant",
        "reference",
        "tm_med",
        "tr_med",
        "tm_ratio",
        "tr_ratio",
        "tm_w1_pct",
        "tr_w1_pct",
        "delta_w1_pct",
        "tr_gt_tm_w1",
    ]
    with open(out, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields, lineterminator="\n")
        w.writeheader()
        for r in sorted(summary, key=lambda x: (x["host"], x["size"], x["shuffle"], x["variant"])):
            w.writerow({k: r[k] for k in fields})
    return out


def variant_medians(summary, host):
    rows = [r for r in summary if r["host"] == host]
    variants = sorted(set(r["variant"] for r in rows))
    result = {}
    for v in variants:
        arr = [r for r in rows if r["variant"] == v]
        result[v] = {
            "tm_ratio": median([r["tm_ratio"] for r in arr]),
            "tr_ratio": median([r["tr_ratio"] for r in arr]),
            "tm_w1_pct": median([r["tm_w1_pct"] for r in arr]),
            "tr_w1_pct": median([r["tr_w1_pct"] for r in arr]),
            "worse_frac": float(np.mean([r["tr_gt_tm_w1"] for r in arr])) if arr else float("nan"),
        }
    return result


def classify(summary):
    hosts = sorted(set(r["host"] for r in summary))
    lines = []
    lines.append("# GEMM Timer Diagnostic Diagnosis")
    lines.append("")
    lines.append("| hypothesis | status | evidence |")
    lines.append("|---|---|---|")

    # Sanity is assessed outside this CSV if present; analyzer records source-level conclusion conservatively.
    lines.append(
        "| source/compile mismatch | rejected if manifests match | Check `source_manifest.txt` in each run directory; analyzer only uses dirs with expected result shape. |"
    )

    for host in hosts:
        med = variant_medians(summary, host)
        if "tsc_current" in med:
            tsc = med.get("tsc_current", {}).get("tr_ratio", float("nan"))
            tsc_native = med.get("tsc_native_current", {}).get("tr_ratio", float("nan"))
            cgt = med.get("cgt_current", {}).get("tr_ratio", float("nan"))
            wtime = med.get("wtime_current", {}).get("tr_ratio", float("nan"))
            lfence = med.get("tsc_lfence_mem", {}).get("tr_ratio", float("nan"))
            cpuid = med.get("tsc_cpuid_mem", {}).get("tr_ratio", float("nan"))
            pre = med.get("tsc_pre_cgt", {}).get("tr_ratio", float("nan"))
            papi = med.get("papi_time_only", {}).get("tr_ratio", float("nan"))
            px = med.get("papix6_current", {}).get("tr_ratio", float("nan"))
            px_before = med.get("papix6_read_before_ns0", {}).get("tr_ratio", float("nan"))
            px_no = med.get("papix6_no_read", {}).get("tr_ratio", float("nan"))
            high = np.nanmedian([cgt, wtime, tsc_native])
            low = np.nanmedian([tsc, papi])
            gap = abs(high - low)
            has_cluster = bool(np.isfinite(gap) and gap >= 0.15)

            def near(x, target):
                return np.isfinite(x) and np.isfinite(target) and abs(x - target) <= max(0.10, 0.35 * gap)

            if not has_cluster:
                tsc_status = "rejected"
                tsc_evidence = "no material low/high cluster in this host/size aggregate"
            else:
                cpuid_pos = "high" if near(cpuid, high) else "low" if near(cpuid, low) else "middle"
                lfence_pos = "high" if near(lfence, high) else "low" if near(lfence, low) else "middle"
                if lfence_pos == "high" and cpuid_pos == "low":
                    tsc_status = "partial"
                    tsc_evidence = "lfence/rdtsc moves to wall/tsc_native cluster, cpuid+memory stays with current TSC"
                elif lfence_pos == "high" or cpuid_pos == "high":
                    tsc_status = "confirmed"
                    tsc_evidence = f"diagnostic TSC variant moved to high cluster: cpuid={cpuid_pos}, lfence={lfence_pos}"
                else:
                    tsc_status = "rejected"
                    tsc_evidence = f"diagnostic TSC variants stayed low/middle: cpuid={cpuid_pos}, lfence={lfence_pos}"
            lines.append(
                f"| {host}: TSC serialization/barrier causes low/high cluster | {tsc_status} | "
                f"{tsc_evidence}; tr ratios: tsc={tsc:.3f}, cpuid_mem={cpuid:.3f}, "
                f"lfence_mem={lfence:.3f}, tsc_native={tsc_native:.3f}, cgt={cgt:.3f}, wtime={wtime:.3f}. |"
            )
            if not has_cluster:
                pre_status = "rejected"
            else:
                pre_status = "confirmed" if near(pre, high) and not near(pre, low) else "rejected"
            lines.append(
                f"| {host}: dummy clock_gettime before TSC moves TSC to wall-timer cluster | {pre_status} | "
                f"tr ratios: tsc_pre_cgt={pre:.3f}, low_cluster={low:.3f}, wall_cluster={high:.3f}. |"
            )
            if not has_cluster:
                px_status = "rejected"
                px_evidence = "no material cluster to explain"
            elif near(px, high) and near(px_before, low):
                px_status = "confirmed"
                px_evidence = "moving start-side PAPI_read before ns0 moves PAPIX6 from high cluster to low cluster"
            elif near(px_no, low):
                px_status = "partial"
                px_evidence = "removing PAPI_read moves no-read variant to low cluster"
            else:
                px_status = "rejected"
                px_evidence = "PAPIX6 diagnostic variants do not move toward low cluster"
            lines.append(
                f"| {host}: PAPIX6 start-side PAPI_read causes high cluster | {px_status} | "
                f"{px_evidence}; tr ratios: papi={papi:.3f}, papix6_current={px:.3f}, "
                f"read_before={px_before:.3f}, no_read={px_no:.3f}. |"
            )
        else:
            ref = med.get("cntvcto_current", {}).get("tr_ratio", float("nan"))
            worst = max((abs(v["tr_ratio"] - 1.0) for v in med.values()), default=float("nan"))
            status = "confirmed" if worst < 0.05 else "inconclusive"
            lines.append(
                f"| {host}: ARM variants collapse to CNTVCTO | {status} | cntvcto={ref:.3f}, max median |ratio-1|={worst:.3f}. |"
            )
    return "\n".join(lines) + "\n"


def plot_clusters(summary, out_dir):
    if plt is None:
        return []
    paths = []
    for host in sorted(set(r["host"] for r in summary)):
        med = variant_medians(summary, host)
        variants = sorted(med)
        y = np.arange(len(variants))
        tm = [med[v]["tm_ratio"] for v in variants]
        tr = [med[v]["tr_ratio"] for v in variants]
        fig, ax = plt.subplots(figsize=(9, max(4, 0.35 * len(variants))))
        ax.scatter(tm, y, label="Tm ratio", marker="o")
        ax.scatter(tr, y, label="Tr ratio", marker="x")
        ax.axvline(1.0, color="k", lw=1, alpha=0.4)
        ax.set_yticks(y)
        ax.set_yticklabels(variants)
        ax.set_xlabel("Median runtime ratio to reference")
        ax.set_title(f"GEMM timer diagnostic clusters: {host}")
        ax.grid(True, axis="x", alpha=0.3)
        ax.legend()
        fig.tight_layout()
        p = out_dir / f"gemm_timer_diag_cluster_{host}.png"
        fig.savefig(p, dpi=180)
        plt.close(fig)
        paths.append(p)
    return paths


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data-root", default="/astrum/home/hpchzy/code/data/20260623_timer_diag")
    ap.add_argument("--run-id", default="20260623-gemm-timer-diag-v2")
    ap.add_argument("--out-dir", default="stencil/plots_0623_gemm_timer_diag")
    args = ap.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    rows = load_rows(args.data_root, args.run_id)
    if not rows:
        raise SystemExit("No diagnostic rows found")
    summary = summarize(rows)
    summary_path = write_summary(summary, out_dir)
    diag = classify(summary)
    diag_path = out_dir / "gemm_timer_diag_diagnosis.md"
    diag_path.write_text(diag)
    plot_paths = plot_clusters(summary, out_dir)
    print(f"rows={len(rows)} summary_rows={len(summary)}")
    print(f"summary={summary_path}")
    print(f"diagnosis={diag_path}")
    for p in plot_paths:
        print(f"plot={p}")


if __name__ == "__main__":
    main()
