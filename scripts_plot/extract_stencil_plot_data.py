#!/usr/bin/env python3
import argparse
import math
import shutil
from pathlib import Path

import pandas as pd


PREFERRED_SIZES = [256, 512, 128, 1024, 64]


def read_meta(path: Path):
    meta = {}
    for line in path.read_text(errors="ignore").splitlines():
        if "=" in line:
            k, v = line.split("=", 1)
            meta[k.strip()] = v.strip()
    return meta


def read_float(path: Path):
    try:
        text = path.read_text(errors="ignore").strip().split()
        return float(text[0]) if text else math.nan
    except Exception:
        return math.nan


def build_summary(base: Path) -> pd.DataFrame:
    rows = []
    for meta_path in sorted(base.glob("np*/*_filt/metadata.env")):
        filt_dir = meta_path.parent
        meta = read_meta(meta_path)
        row = {
            "host": meta.get("host", ""),
            "np": meta.get("np", ""),
            "kernel": meta.get("kernel", ""),
            "size": meta.get("size", ""),
            "timer": meta.get("timer", ""),
            "status": meta.get("status", ""),
            "nt": meta.get("nt", ""),
            "nsamp": meta.get("nsamp", ""),
            "binw": meta.get("binw", ""),
            "git_commit": meta.get("git_commit", ""),
            "rel_dir": str(filt_dir.relative_to(base)),
            "wd": read_float(filt_dir / "wd.out"),
            "er": read_float(filt_dir / "er.out"),
            "ep": read_float(filt_dir / "ep.out"),
        }
        rows.append(row)
    data = pd.DataFrame(rows)
    if data.empty:
        return data
    for col in ["np", "size", "wd", "er", "ep"]:
        data[col] = pd.to_numeric(data[col], errors="coerce")
    return data


def choose_hist_rows(data: pd.DataFrame):
    ok = data[(data["status"] == "ok") & data["wd"].notna()].copy()
    rows = []
    for (kernel, npv), sub in ok.groupby(["kernel", "np"]):
        chosen = None
        for size in PREFERRED_SIZES:
            cand = sub[sub["size"] == size]
            if not cand.empty:
                chosen = cand
                break
        if chosen is None:
            size_counts = sub.groupby("size")["timer"].nunique().sort_values(ascending=False)
            if size_counts.empty:
                continue
            chosen = sub[sub["size"] == size_counts.index[0]]
        rows.append(chosen)
    if not rows:
        return pd.DataFrame()
    return pd.concat(rows, ignore_index=True)


def copy_hists(base: Path, out: Path, rows: pd.DataFrame):
    for _, row in rows.iterrows():
        src = base / row["rel_dir"]
        dst = out / "hist" / row["host"] / row["rel_dir"]
        dst.mkdir(parents=True, exist_ok=True)
        for name in ["metadata.env", "tm_hist.csv", "tr_hist.csv", "sim_cdf.csv", "wd.out", "er.out", "ep.out"]:
            if (src / name).exists():
                shutil.copy2(src / name, dst / name)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--base", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    data = build_summary(args.base)
    data.to_csv(args.out / "summary.csv", index=False)
    if not data.empty:
        copy_hists(args.base, args.out, choose_hist_rows(data))


if __name__ == "__main__":
    main()
