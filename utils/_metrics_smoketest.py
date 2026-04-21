from __future__ import annotations

import numpy as np

from metrics_wasserstein import (
    quantiles_from_samples,
    rl_rh_from_quantiles,
    w_rl_rh_from_samples,
)


def _assert_close(name: str, got: float, want: float, atol: float = 1e-9) -> None:
    if not np.isfinite(got) or abs(got - want) > atol:
        raise AssertionError(f"{name}: got {got}, want {want} (atol={atol})")


def main() -> None:
    rng = np.random.default_rng(0)

    # 1) Identical distributions => w ~= 0, rl ~= 0, rh ~= 0
    x = rng.normal(loc=0.0, scale=1.0, size=10000).astype(np.float64)
    res = w_rl_rh_from_samples(x, x, ntiles=100, q=0.9, drop=0.995)
    _assert_close("w(identical)", res["w"], 0.0, atol=0.0)
    _assert_close("rl(identical)", res["rl"], 0.0, atol=0.0)
    _assert_close("rh(identical)", res["rh"], 0.0, atol=0.0)

    # 2) Pure shift => w > 0 (v0.0 does NOT remove shift)
    y = x + 5.0
    res2 = w_rl_rh_from_samples(x, y, ntiles=100, q=0.9, drop=0.995)
    if not (res2["w"] > 0):
        raise AssertionError(f"w(shift) expected > 0, got {res2['w']}")

    # 3) Drop affects tile_max but averages are over effective tiles
    Uq = quantiles_from_samples(x, ntiles=100)
    Vq = quantiles_from_samples(y, ntiles=100)
    r_drop1 = rl_rh_from_quantiles(Uq, Vq, q=0.9, drop=1.0)
    r_drop2 = rl_rh_from_quantiles(Uq, Vq, q=0.9, drop=0.995)
    if not (r_drop2["tile_max"] <= r_drop1["tile_max"]):
        raise AssertionError("tile_max should not increase when drop decreases")

    print("OK")


if __name__ == "__main__":
    main()

