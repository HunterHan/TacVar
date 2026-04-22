# binw = max (10, (mid - min) / 50)

import os
import numpy as np
import pandas as pd


def read_csvs(dir_path, col, binw_min: int = 10):
    dfs = []
    files = sorted(f for f in os.listdir(dir_path) if f.endswith(".csv"))
    for i in range(0, len(files)):
        df = pd.read_csv(dir_path + '/' + files[i], header=None)
        dfs.append(df)
    df = pd.concat(dfs, ignore_index=True)
    arr = df[col].to_numpy()
    gap = np.quantile(arr, 0.5) - np.quantile(arr, 0)
    binw = max(gap / 50, binw_min) 
    binw = int(binw / binw_min) * binw_min
    return binw


if __name__ == '__main__':
    import sys
    csv_dir = sys.argv[1]
    data_col = int(sys.argv[2])
    binw_min = int(sys.argv[3])
    print(read_csvs(csv_dir, data_col, binw_min))
