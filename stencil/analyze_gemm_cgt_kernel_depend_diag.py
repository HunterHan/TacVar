#!/usr/bin/env python3
import argparse
import csv
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

FIELDS = ["tm_gemm_ns", "te_dsub_ns", "te_gemm_like_ns", "te_gemm_row_ns"]


def median(xs):
    xs = [float(x) for x in xs if np.isfinite(float(x))]
    return float(np.median(xs)) if xs else float("nan")


def parse_dir(path):
    m = re.match(r"(?P<variant>.+)_np(?P<np>\d+)_size(?P<size>\d+)$", path.name)
    if not m:
        return None
    sm = re.match(r"shuffle(?P<shuffle>\d+)$", path.parent.name)
    if not sm:
        return None
    d = m.groupdict()
    d["np"] = int(d["np"])
    d["size"] = int(d["size"])
    d["shuffle"] = int(sm.group("shuffle"))
    return d


def read_result_dir(path, host):
    meta = parse_dir(path)
    if not meta:
        return None
    vals = {f: [] for f in FIELDS}
    nrows = 0
    for csv_path in path.glob("*_rank*.csv"):
        with open(csv_path, newline="") as f:
            r = csv.DictReader(f)
            for row in r:
                nrows += 1
                for fld in FIELDS:
                    try:
                        v = float(row[fld])
                    except Exception:
                        continue
                    if v > 0:
                        vals[fld].append(v)
    if not vals["tm_gemm_ns"]:
        return None
    out = {"host": host, **meta, "nrows": nrows, "result_dir": str(path)}
    for fld in FIELDS:
        out[fld + "_med"] = median(vals[fld])
    return out


def load_rows(data_root, run_id):
    rows = []
    for run_dir in sorted(Path(data_root).glob(f"*/output_gemm_cgt_kernel_depend_diag/{run_id}")):
        host = run_dir.parts[-3]
        for res_dir in run_dir.glob("shuffle*/*_np*_size*"):
            row = read_result_dir(res_dir, host)
            if row:
                rows.append(row)
    return rows


def reference_variant(host, variants):
    if "cntvcto_current_gemm" in variants:
        return "cntvcto_current_gemm"
    if "tsc_current_gemm" in variants:
        return "tsc_current_gemm"
    raise RuntimeError(f"No reference variant for {host}: {sorted(variants)}")


def summarize(rows):
    by = defaultdict(dict)
    for r in rows:
        by[(r["host"], r["size"], r["shuffle"])][r["variant"]] = r
    out = []
    for (host, size, shuffle), variants in sorted(by.items()):
        ref_name = reference_variant(host, variants)
        ref = variants[ref_name]
        for v, r in sorted(variants.items()):
            rec = {"host": host, "size": size, "shuffle": shuffle, "variant": v, "reference": ref_name, "nrows": r["nrows"]}
            for fld in FIELDS:
                val = r[fld + "_med"]
                rval = ref[fld + "_med"]
                rec[fld + "_med"] = val
                rec[fld + "_ratio"] = val / rval if rval else float("nan")
            # pseudo-TR: remove timer-specific TE overhead relative to reference TE.
            for te in ["te_dsub_ns", "te_gemm_like_ns", "te_gemm_row_ns"]:
                corrected = r["tm_gemm_ns_med"] - (r[te + "_med"] - ref[te + "_med"])
                rec[f"tr_by_{te.replace("_ns", "")}_med"] = corrected
                rec[f"tr_by_{te.replace("_ns", "")}_ratio"] = corrected / ref["tm_gemm_ns_med"] if ref["tm_gemm_ns_med"] else float("nan")
                interaction = (r["tm_gemm_ns_med"] - ref["tm_gemm_ns_med"]) - (r[te + "_med"] - ref[te + "_med"])
                rec[f"interaction_{te.replace("_ns", "")}_ns"] = interaction
                rec[f"interaction_{te.replace("_ns", "")}_pct_ref_tm"] = interaction / ref["tm_gemm_ns_med"] * 100.0 if ref["tm_gemm_ns_med"] else float("nan")
            out.append(rec)
    return out


def write_csv(summary, out_dir):
    fields = list(summary[0].keys()) if summary else []
    p = out_dir / "gemm_cgt_kernel_depend_summary.csv"
    with open(p, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields, lineterminator="\n")
        w.writeheader()
        for r in summary:
            w.writerow(r)
    return p


def med_by(summary, host, variant, key):
    return median([r[key] for r in summary if r["host"] == host and r["variant"] == variant])


def classify(summary):
    lines = ["# GEMM CGT Kernel-Dependence Diagnosis", "", "| hypothesis | status | evidence |", "|---|---|---|"]
    for host in sorted(set(r["host"] for r in summary)):
        ref = "cntvcto_current_gemm" if any(r["host"] == host and r["variant"] == "cntvcto_current_gemm" for r in summary) else "tsc_current_gemm"
        pre = "cntvcto_after_cgt_prelude_gemm" if ref.startswith("cntvcto") else "tsc_after_cgt_prelude_gemm"
        call = "tsc_after_cgt_call_only_gemm"
        pre_ratio = med_by(summary, host, pre, "tm_gemm_ns_ratio")
        ref_ratio = med_by(summary, host, ref, "tm_gemm_ns_ratio")
        dsub_pre = "tsc_after_cgt_prelude_dsub"
        dsub_ratio = med_by(summary, host, dsub_pre, "te_dsub_ns_ratio") if not ref.startswith("cntvcto") else float("nan")
        call_ratio = med_by(summary, host, call, "tm_gemm_ns_ratio") if not ref.startswith("cntvcto") else float("nan")
        cgt_tm = med_by(summary, host, "cgt_current", "tm_gemm_ns_ratio")
        cgt_tr_d = med_by(summary, host, "cgt_current", "tr_by_te_dsub_ratio")
        cgt_tr_l = med_by(summary, host, "cgt_current", "tr_by_te_gemm_like_ratio")
        cgt_tr_r = med_by(summary, host, "cgt_current", "tr_by_te_gemm_row_ratio")
        papi_tr_d = med_by(summary, host, "papi_time_only", "tr_by_te_dsub_ratio")
        inter_cgt_d = abs(med_by(summary, host, "cgt_current", "interaction_te_dsub_pct_ref_tm"))
        inter_cgt_l = abs(med_by(summary, host, "cgt_current", "interaction_te_gemm_like_pct_ref_tm"))
        inter_cgt_r = abs(med_by(summary, host, "cgt_current", "interaction_te_gemm_row_pct_ref_tm"))
        inter_papi_d = abs(med_by(summary, host, "papi_time_only", "interaction_te_dsub_pct_ref_tm"))
        perturbed = np.isfinite(pre_ratio) and pre_ratio > 1.05
        lines.append(f"| {host}: CGT prelude perturbs GEMM under reference timer | {"confirmed" if perturbed else "rejected"} | {pre}/ref median tm ratio={pre_ratio:.3f}; ref={ref_ratio:.3f}; call_only={call_ratio:.3f}. |")
        if np.isfinite(dsub_ratio):
            diff = abs(pre_ratio - dsub_ratio)
            status = "confirmed" if diff > 0.10 else "rejected"
            lines.append(f"| {host}: CGT prelude perturbs GEMM differently from DSub | {status} | GEMM prelude ratio={pre_ratio:.3f}; DSub prelude TE ratio={dsub_ratio:.3f}; abs gap={diff:.3f}. |")
        reduced = min(inter_cgt_l, inter_cgt_r) < inter_cgt_d * 0.5 if np.isfinite(inter_cgt_d) else False
        lines.append(f"| {host}: GEMM-like/GEMM-row TE reduces CGT residual | {"confirmed" if reduced else "rejected"} | cgt TM={cgt_tm:.3f}; TR dsub={cgt_tr_d:.3f}; TR gemm_like={cgt_tr_l:.3f}; TR gemm_row={cgt_tr_r:.3f}; interactions pct dsub={inter_cgt_d:.1f}, like={inter_cgt_l:.1f}, row={inter_cgt_r:.1f}. |")
        papi_ok = np.isfinite(papi_tr_d) and abs(papi_tr_d - 1.0) < 0.10
        lines.append(f"| {host}: PAPI bias remains DSub-correctable | {"confirmed" if papi_ok else "rejected"} | papi TR by dsub ratio={papi_tr_d:.3f}; papi interaction dsub pct={inter_papi_d:.1f}. |")
    return "\n".join(lines) + "\n"


def plot(summary, out_dir):
    if plt is None:
        return []
    paths = []
    for host in sorted(set(r["host"] for r in summary)):
        rows = [r for r in summary if r["host"] == host]
        variants = sorted(set(r["variant"] for r in rows))
        y = np.arange(len(variants))
        fig, ax = plt.subplots(figsize=(10, max(4, 0.4 * len(variants))))
        for key, marker, label in [
            ("tm_gemm_ns_ratio", "o", "TM GEMM"),
            ("tr_by_te_dsub_ratio", "x", "TR by DSub"),
            ("tr_by_te_gemm_like_ratio", "^", "TR by GEMM-like"),
            ("tr_by_te_gemm_row_ratio", "s", "TR by GEMM-row"),
        ]:
            xs = [med_by(summary, host, v, key) for v in variants]
            ax.scatter(xs, y, marker=marker, label=label)
        ax.axvline(1.0, color="k", lw=1, alpha=0.4)
        ax.set_yticks(y); ax.set_yticklabels(variants)
        ax.grid(True, axis="x", alpha=0.3)
        ax.set_xlabel("Median ratio to reference TM")
        ax.set_title(f"GEMM CGT/DSub kernel-dependence diagnostic: {host}")
        ax.legend()
        fig.tight_layout()
        p = out_dir / f"gemm_cgt_kernel_depend_cluster_{host}.png"
        fig.savefig(p, dpi=180); plt.close(fig); paths.append(p)
        # interaction heatmap
        mat = []
        for v in variants:
            mat.append([med_by(summary, host, v, f"interaction_{te}_pct_ref_tm") for te in ["te_dsub", "te_gemm_like", "te_gemm_row"]])
        fig, ax = plt.subplots(figsize=(7, max(4, 0.35 * len(variants))))
        im = ax.imshow(mat, aspect="auto", cmap="coolwarm")
        ax.set_yticks(y); ax.set_yticklabels(variants)
        ax.set_xticks([0,1,2]); ax.set_xticklabels(["DSub", "GEMM-like", "GEMM-row"])
        ax.set_title(f"Interaction (% reference TM): {host}")
        fig.colorbar(im, ax=ax)
        fig.tight_layout()
        p = out_dir / f"gemm_cgt_kernel_depend_interaction_{host}.png"
        fig.savefig(p, dpi=180); plt.close(fig); paths.append(p)
    return paths


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data-root", default="/astrum/home/hpchzy/code/data/20260624_timer_diag")
    ap.add_argument("--run-id", default="20260624-gemm-cgt-kernel-depend")
    ap.add_argument("--out-dir", default="stencil/plots_0624_gemm_cgt_kernel_depend_diag")
    args = ap.parse_args()
    out_dir = Path(args.out_dir); out_dir.mkdir(parents=True, exist_ok=True)
    rows = load_rows(args.data_root, args.run_id)
    if not rows:
        raise SystemExit("no rows found")
    summary = summarize(rows)
    sp = write_csv(summary, out_dir)
    dp = out_dir / "gemm_cgt_kernel_depend_diagnosis.md"
    dp.write_text(classify(summary))
    paths = plot(summary, out_dir)
    print(f"input_rows={len(rows)} summary_rows={len(summary)}")
    print(f"summary={sp}")
    print(f"diagnosis={dp}")
    for p in paths:
        print(f"plot={p}")

if __name__ == "__main__":
    main()
