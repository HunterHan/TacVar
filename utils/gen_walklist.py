#!/usr/bin/env python3
import argparse
import csv
import math
import os
import random
import time
from typing import List, Optional


def _seeded_rng(seed: Optional[int]) -> random.Random:
    if seed is not None:
        return random.Random(int(seed))
    # similar spirit to get_terror.c: time-based + extra entropy
    s = int(time.time_ns()) ^ int.from_bytes(os.urandom(8), "little")
    return random.Random(s)


def _gaussian_box_muller(rng: random.Random, sigma: float) -> float:
    # Box-Muller transform (matches get_terror.c intent).
    u1 = rng.random()
    u2 = rng.random()
    z0 = math.sqrt(-2.0 * math.log(u1)) * math.cos(2.0 * math.pi * u2)
    return z0 * sigma


def _pareto_inverse(rng: random.Random, alpha: float, xm: float = 1.0) -> float:
    u = rng.random()
    return xm / (u ** (1.0 / alpha))


def gen_fixed(nwalks: int, fixed_ns: int) -> List[int]:
    return [int(fixed_ns)] * int(nwalks)


def gen_uniform(npass: int, ntest: int, tbase: int, v1: int, v2: int, rng: random.Random) -> List[int]:
    # get_terror.c:
    # ntest_total = NPASS + NTEST * V2
    # first NPASS values = TBASE + V1*V2
    # remaining values are NTEST repeats of bins: TBASE + i*V1 for i in [0, V2)
    warm = [tbase + v1 * v2] * npass
    vals = []
    for _ in range(ntest):
        for i in range(v2):
            vals.append(tbase + i * v1)
    rng.shuffle(vals)
    return warm + vals


def gen_normal(npass: int, ntest: int, tbase: int, sigma: float, lcut: float, hcut: float, rng: random.Random) -> List[int]:
    out: List[int] = []
    for _ in range(npass + ntest):
        while True:
            x = 1.0 + _gaussian_box_muller(rng, sigma)
            if lcut <= x <= hcut:
                out.append(int(x * tbase))
                break
    return out


def gen_pareto(npass: int, ntest: int, tbase: int, alpha: float, hcut: float, rng: random.Random) -> List[int]:
    out: List[int] = []
    for _ in range(npass + ntest):
        while True:
            x = _pareto_inverse(rng, alpha, 1.0)
            if x <= hcut:
                out.append(int(x * tbase))
                break
    return out


def write_walk_csv(path: str, vals_ns: List[int]) -> None:
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["walk_idx", "ta_ns", "tb_ns"])
        for i, ns in enumerate(vals_ns):
            w.writerow([i, int(ns), int(ns)])


def write_meta(path: str, lines: List[str]) -> None:
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        for ln in lines:
            f.write(ln.rstrip("\n") + "\n")


def _rel_to_abs(mu: float, rel: float) -> float:
    return float(mu) * float(rel)


def main() -> int:
    ap = argparse.ArgumentParser(description="Generate ParTES walking list CSV (ta==tb).")
    ap.add_argument("--out", required=True, help="Output CSV path (walk_idx,ta_ns,tb_ns).")
    ap.add_argument("--meta-out", default="", help="Optional meta.txt output path.")
    ap.add_argument("--dist", choices=["fixed", "uniform", "normal", "pareto"], default="normal")

    ap.add_argument("--seed", type=int, default=None)
    ap.add_argument("--nwalks", type=int, default=100)

    # fixed
    ap.add_argument("--fixed-ns", type=int, default=10000)

    # uniform
    ap.add_argument("--mu-ns", type=float, default=10000.0)
    ap.add_argument("--step-ns", type=float, default=50.0)
    ap.add_argument("--nbins", type=int, default=21)

    # normal
    ap.add_argument("--sigma-rel", type=float, default=0.015)
    ap.add_argument("--lcut-rel", type=float, default=0.50)
    ap.add_argument("--hcut-rel", type=float, default=1.50)

    # pareto
    ap.add_argument("--alpha", type=float, default=4.0)

    # Compatibility knobs (old names). If provided, they override the new ones.
    ap.add_argument("--npass", type=int, default=None)
    ap.add_argument("--tbase", type=float, default=None)
    ap.add_argument("--ntest", type=int, default=None)
    ap.add_argument("--v1", type=float, default=None)
    ap.add_argument("--v2", type=int, default=None)
    ap.add_argument("--lcut", type=float, default=None)
    ap.add_argument("--hcut", type=float, default=None)

    args = ap.parse_args()
    rng = _seeded_rng(args.seed)

    if args.dist == "fixed":
        vals = gen_fixed(args.nwalks, args.fixed_ns)
        meta = ["walk_list_type=fixed", f"values={int(args.fixed_ns)}", f"nwalks={int(args.nwalks)}"]
    elif args.dist == "uniform":
        mu = args.mu_ns if args.tbase is None else float(args.tbase)
        step = args.step_ns if args.v1 is None else float(args.v1)
        nbins = args.nbins if args.v2 is None else int(args.v2)
        npass = 0 if args.npass is None else int(args.npass)
        ntest = max(1, int(math.ceil(max(0, args.nwalks - npass) / max(1, nbins)))) if args.ntest is None else int(args.ntest)
        vals = gen_uniform(npass, ntest, int(mu), int(step), nbins, rng)[: args.nwalks]
        meta = [
            "walk_list_type=uniform",
            f"mu_ns={mu}",
            f"step_ns={step}",
            f"nbins={nbins}",
            f"nwalks={int(args.nwalks)}",
            f"npass={npass}",
            f"ntest={ntest}",
        ]
    elif args.dist == "normal":
        mu = args.mu_ns if args.tbase is None else float(args.tbase)
        sigma = _rel_to_abs(mu, args.sigma_rel) if args.v1 is None else float(args.v1) * float(mu)
        lcut = mu * args.lcut_rel if args.lcut is None else float(args.lcut) * float(mu)
        hcut = mu * args.hcut_rel if args.hcut is None else float(args.hcut) * float(mu)
        npass = 0 if args.npass is None else int(args.npass)
        ntest = max(0, int(args.nwalks) - npass)
        vals = gen_normal(npass, ntest, int(mu), float(sigma / mu), float(lcut / mu), float(hcut / mu), rng)[: args.nwalks]
        meta = [
            "walk_list_type=normal",
            f"mu_ns={mu}",
            f"sigma_rel={args.sigma_rel}",
            f"lcut_rel={args.lcut_rel}",
            f"hcut_rel={args.hcut_rel}",
            f"nwalks={int(args.nwalks)}",
            f"npass={npass}",
        ]
        if args.seed is not None:
            meta.append(f"seed={int(args.seed)}")
    else:  # pareto
        mu = args.mu_ns if args.tbase is None else float(args.tbase)
        alpha = args.alpha if args.v1 is None else float(args.v1)
        hcut = mu * args.hcut_rel if args.hcut is None else float(args.hcut) * float(mu)
        npass = 0 if args.npass is None else int(args.npass)
        ntest = max(0, int(args.nwalks) - npass)
        vals = gen_pareto(npass, ntest, int(mu), float(alpha), float(hcut / mu), rng)[: args.nwalks]
        meta = [
            "walk_list_type=pareto",
            f"mu_ns={mu}",
            f"alpha={alpha}",
            f"hcut_rel={args.hcut_rel}",
            f"nwalks={int(args.nwalks)}",
            f"npass={npass}",
        ]
        if args.seed is not None:
            meta.append(f"seed={int(args.seed)}")

    write_walk_csv(args.out, vals)
    if args.meta_out:
        write_meta(args.meta_out, meta)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

