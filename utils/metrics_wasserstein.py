from __future__ import annotations

from typing import Literal, TypedDict

import numpy as np


DenomPolicy = Literal["inf", "nan"]


class RlRhResult(TypedDict):
    w: float
    rl: float
    rh: float
    wl_uv: float
    wl_u: float
    wh_uv: float
    wh_u: float
    ntiles: int
    tile_max: int
    q_idx: int
    drop: float
    q: float


def _require_1d_nonempty(x: np.ndarray, name: str) -> np.ndarray:
    x = np.asarray(x)
    if x.ndim != 1:
        raise ValueError(f"{name} must be a 1D array, got shape={x.shape}")
    if x.size == 0:
        raise ValueError(f"{name} must be non-empty")
    return x


def effective_tile_max(ntiles: int, drop: float) -> int:
    """
    Number of tiles kept after dropping the upper tail.

    drop=0.995 means keep the first floor(drop * ntiles) tiles (lower quantiles).
    """
    if ntiles < 2:
        raise ValueError("ntiles must be >= 2")
    if not (0.0 < float(drop) <= 1.0):
        raise ValueError("drop must be in (0, 1]")
    tile_max = int(np.floor(float(drop) * int(ntiles)))
    return max(1, min(int(ntiles), tile_max))


def quantiles_from_samples(x: np.ndarray, ntiles: int) -> np.ndarray:
    """
    Discretize a sample array into `ntiles` quantile points using TacVar's tile rule:
      sort(x)
      idx(i) = floor(i/(ntiles-1) * (n-1)), i=0..ntiles-1

    This matches `calc_cdf_i64` in `TacVar/src/partes/stat.c` (index-based quantiles).
    """
    x = _require_1d_nonempty(x, "x")
    if ntiles < 2:
        raise ValueError("ntiles must be >= 2")
    xs = np.sort(x.astype(np.float64, copy=False), kind="quicksort")
    n = xs.size
    if n == 1:
        return np.repeat(xs[0], int(ntiles))
    i = np.arange(int(ntiles), dtype=np.float64)
    idx = np.floor(i / float(ntiles - 1) * float(n - 1)).astype(np.int64)
    return xs[idx]


def w_v00_from_quantiles(Uq: np.ndarray, Vq: np.ndarray, drop: float = 0.995) -> float:
    """
    v0.0 Wasserstein (tiles-based): mean absolute difference on quantile arrays.

    Uses effective tiles only (after `drop`): i in [0, tile_max).
    """
    Uq = _require_1d_nonempty(Uq, "Uq").astype(np.float64, copy=False)
    Vq = _require_1d_nonempty(Vq, "Vq").astype(np.float64, copy=False)
    if Uq.size != Vq.size:
        raise ValueError(f"Uq and Vq must have same length, got {Uq.size} vs {Vq.size}")
    ntiles = int(Uq.size)
    tile_max = effective_tile_max(ntiles, drop)
    return float(np.mean(np.abs(Vq[:tile_max] - Uq[:tile_max])))


def _segment_mean_abs_diff(a: np.ndarray, b: np.ndarray, start: int, end_inclusive: int) -> float:
    if end_inclusive < start:
        return 0.0
    seg = np.abs(a[start : end_inclusive + 1] - b[start : end_inclusive + 1])
    return float(np.mean(seg)) if seg.size else 0.0


def _segment_mean_abs_from_base(a: np.ndarray, base: float, start: int, end_inclusive: int) -> float:
    if end_inclusive < start:
        return 0.0
    seg = np.abs(a[start : end_inclusive + 1] - base)
    return float(np.mean(seg)) if seg.size else 0.0


def rl_rh_from_quantiles(
    Uq: np.ndarray,
    Vq: np.ndarray,
    *,
    q: float = 0.9,
    drop: float = 0.995,
    denom_policy: DenomPolicy = "inf",
) -> RlRhResult:
    """
    Compute v0.0 W plus Rl/Rh on quantile arrays.

    - q splits low/high on the effective tiles (after drop).
    - drop truncates upper tail: keep tiles [0, tile_max).
    - denom_policy controls how to handle zero denominators (segment self-W == 0):
      - "inf": ratio becomes +inf when numerator > 0, else 0
      - "nan": ratio becomes NaN when denominator == 0
    """
    Uq = _require_1d_nonempty(Uq, "Uq").astype(np.float64, copy=False)
    Vq = _require_1d_nonempty(Vq, "Vq").astype(np.float64, copy=False)
    if Uq.size != Vq.size:
        raise ValueError(f"Uq and Vq must have same length, got {Uq.size} vs {Vq.size}")
    if not (0.0 < float(q) < 1.0):
        raise ValueError("q must be in (0, 1)")
    if denom_policy not in ("inf", "nan"):
        raise ValueError("denom_policy must be 'inf' or 'nan'")

    ntiles = int(Uq.size)
    tile_max = effective_tile_max(ntiles, drop)
    eff_last = tile_max - 1

    q_idx = int(np.floor(float(q) * float(eff_last)))
    q_idx = max(0, min(eff_last, q_idx))

    wl_uv = _segment_mean_abs_diff(Vq, Uq, 0, q_idx)
    wl_u = _segment_mean_abs_from_base(Uq, float(Uq[0]), 0, q_idx)

    wh_uv = _segment_mean_abs_diff(Vq, Uq, q_idx + 1, eff_last)
    wh_u = _segment_mean_abs_from_base(Uq, float(Uq[0]), q_idx + 1, eff_last)

    def safe_ratio(num: float, den: float) -> float:
        if den > 0.0:
            return float(num / den)
        if denom_policy == "nan":
            return float("nan")
        # denom_policy == "inf"
        return 0.0 if num == 0.0 else float("inf")

    rl = safe_ratio(wl_uv, wl_u)
    rh = safe_ratio(wh_uv, wh_u)
    w = float(np.mean(np.abs(Vq[:tile_max] - Uq[:tile_max])))

    return {
        "w": w,
        "rl": rl,
        "rh": rh,
        "wl_uv": wl_uv,
        "wl_u": wl_u,
        "wh_uv": wh_uv,
        "wh_u": wh_u,
        "ntiles": ntiles,
        "tile_max": tile_max,
        "q_idx": q_idx,
        "drop": float(drop),
        "q": float(q),
    }


def w_rl_rh_from_samples(
    U: np.ndarray,
    V: np.ndarray,
    *,
    ntiles: int = 100,
    q: float = 0.9,
    drop: float = 0.995,
    denom_policy: DenomPolicy = "inf",
) -> RlRhResult:
    """
    Convenience wrapper that accepts raw samples and applies TacVar's quantile/tiles discretization.
    """
    U = _require_1d_nonempty(U, "U")
    V = _require_1d_nonempty(V, "V")
    Uq = quantiles_from_samples(U, ntiles=int(ntiles))
    Vq = quantiles_from_samples(V, ntiles=int(ntiles))
    return rl_rh_from_quantiles(Uq, Vq, q=float(q), drop=float(drop), denom_policy=denom_policy)

