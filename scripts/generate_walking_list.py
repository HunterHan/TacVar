import pandas as pd
import numpy as np
import argparse
import os
from pathlib import Path


def generate_walking_list(tbase, dist, num_walk):
    rng = np.random.default_rng(42)
    if dist == 0:
        walking_list = rng.normal(loc=0, scale=0.015, size=num_walk)
    elif dist == 1:
        # Todo
        rng.uniform(low=0.0,high=1.0, size=num_walk)
    elif dist == 2:
        # Todo
        alpha = 2.0
        x = rng.pareto(a=alpha, size=num_walk) + 1.0
    else:
        raise ValueError(f"Unsupported distribution: {dist}")
    # walking_list = tbase + walking_list
    # walking_list = np.rint(walking_list).astype(np.int64)
    walking_list = walking_list * tbase * 0.1 + tbase
    walking_list = np.rint(walking_list).astype(np.int64)
    return pd.DataFrame({"ns": walking_list})

def main():
    parser = argparse.ArgumentParser()

    parser.add_argument("--tbase", required=True, type=int)
    parser.add_argument("--dist", required=True, type=int)
    parser.add_argument("--num_walk", required=True, type=int)
    parser.add_argument("--output_file", required=True, type=Path, help="Path to the output file")

    args = parser.parse_args()

    df = generate_walking_list(args.tbase, args.dist, args.num_walk)

    print(f"Walking list save to {args.output_file}")
    df.to_csv(args.output_file, index=False, header=False)


if __name__ == "__main__":
    main()