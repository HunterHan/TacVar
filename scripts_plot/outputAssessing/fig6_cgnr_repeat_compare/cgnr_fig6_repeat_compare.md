# cgnr6760pn2 fig6 repeat comparison

- Base: final fig6 batch in fig6_final_summary_20260520.csv.
- Repeat: assess.expr1.fsize.np64.repeat1 / np128.repeat1, same shared walking lists.
- Same script condition: no core isolation, taskset uses 0..NP-1, NTESTS=100, NWALKS=3.

## Per timer summary

| expr | timer | points | base_rh_mean | repeat_rh_mean | base_rh_median | repeat_rh_median | base_rh_max | repeat_rh_max | rh_delta_abs_mean | rh_corr | base_small_fs_max | repeat_small_fs_max |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| assess.expr1.fsize.np128 | clock_gettime | 10 | 774.009 | 658.059 | 578.578 | 614.392 | 2761.793 | 1222.826 | 449.306 | 0.007 | 2761.793 | 1222.826 |
| assess.expr1.fsize.np128 | mpi_wtime | 10 | 1011.251 | 868.529 | 1013.735 | 633.270 | 1697.217 | 1778.017 | 379.674 | 0.477 | 1697.217 | 1778.017 |
| assess.expr1.fsize.np128 | tsc | 10 | 707.701 | 559.509 | 600.785 | 471.233 | 1323.355 | 1392.300 | 329.559 | 0.366 | 1323.355 | 691.883 |
| assess.expr1.fsize.np128 | tsc_asym | 10 | 630.779 | 703.914 | 548.062 | 555.813 | 1555.601 | 1723.878 | 388.356 | 0.195 | 1555.601 | 1723.878 |
| assess.expr1.fsize.np64 | clock_gettime | 10 | 448.219 | 412.130 | 347.482 | 335.802 | 1038.923 | 837.213 | 299.106 | -0.173 | 1038.923 | 837.213 |
| assess.expr1.fsize.np64 | mpi_wtime | 10 | 569.016 | 610.177 | 524.921 | 515.428 | 855.405 | 1583.866 | 323.672 | 0.469 | 855.405 | 1583.866 |
| assess.expr1.fsize.np64 | tsc | 10 | 572.026 | 780.049 | 535.105 | 590.006 | 1496.871 | 1783.151 | 521.883 | -0.503 | 1496.871 | 1783.151 |
| assess.expr1.fsize.np64 | tsc_asym | 10 | 316.535 | 712.030 | 285.371 | 563.084 | 484.527 | 2548.914 | 410.410 | 0.239 | 484.527 | 2548.914 |

## Largest R_H changes

| expr_base | timer | fsize_kib | r_h_pct_base | r_h_pct_repeat | rh_delta | rh_ratio | r_l_pct_base | r_l_pct_repeat |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| assess.expr1.fsize.np64 | tsc_asym | 256 | 305.307 | 2548.914 | 2243.607 | 8.349 | 17.558 | 20.156 |
| assess.expr1.fsize.np128 | clock_gettime | 16 | 2761.793 | 646.839 | -2114.953 | 0.234 | 35.812 | 39.180 |
| assess.expr1.fsize.np64 | tsc | 16 | 114.543 | 1783.151 | 1668.608 | 15.567 | 14.947 | 17.279 |
| assess.expr1.fsize.np128 | tsc_asym | 64 | 528.989 | 1723.878 | 1194.889 | 3.259 | 35.601 | 56.693 |
| assess.expr1.fsize.np64 | tsc | 256 | 1496.871 | 342.853 | -1154.018 | 0.229 | 18.077 | 16.094 |
| assess.expr1.fsize.np128 | mpi_wtime | 128 | 1697.217 | 689.839 | -1007.378 | 0.406 | 32.997 | 31.053 |
| assess.expr1.fsize.np128 | tsc | 32 | 1323.355 | 369.293 | -954.062 | 0.279 | 27.315 | 31.141 |
| assess.expr1.fsize.np64 | tsc | 32 | 352.277 | 1285.918 | 933.641 | 3.650 | 16.260 | 14.102 |
| assess.expr1.fsize.np128 | clock_gettime | 64 | 394.485 | 1222.826 | 828.341 | 3.100 | 32.231 | 35.961 |
| assess.expr1.fsize.np128 | tsc_asym | 1024 | 948.890 | 146.039 | -802.851 | 0.154 | 57.103 | 34.855 |
| assess.expr1.fsize.np128 | mpi_wtime | 16 | 1175.186 | 393.825 | -781.362 | 0.335 | 30.767 | 33.729 |
| assess.expr1.fsize.np64 | tsc_asym | 16 | 396.592 | 1161.255 | 764.663 | 2.928 | 16.924 | 17.830 |
| assess.expr1.fsize.np64 | mpi_wtime | 32 | 823.338 | 1583.866 | 760.528 | 1.924 | 15.806 | 16.884 |
| assess.expr1.fsize.np64 | clock_gettime | 256 | 98.024 | 837.213 | 739.190 | 8.541 | 15.626 | 17.147 |
| assess.expr1.fsize.np128 | mpi_wtime | 1024 | 516.764 | 1240.194 | 723.430 | 2.400 | 35.054 | 30.847 |
| assess.expr1.fsize.np128 | tsc | 16 | 956.109 | 257.201 | -698.909 | 0.269 | 28.138 | 29.365 |
| assess.expr1.fsize.np128 | tsc_asym | 16 | 1555.601 | 938.851 | -616.750 | 0.604 | 36.569 | 36.483 |
| assess.expr1.fsize.np64 | clock_gettime | 32 | 1038.923 | 426.736 | -612.187 | 0.411 | 16.174 | 16.323 |
| assess.expr1.fsize.np64 | tsc | 128 | 723.028 | 1271.662 | 548.635 | 1.759 | 16.089 | 16.036 |
| assess.expr1.fsize.np64 | mpi_wtime | 64 | 234.657 | 746.400 | 511.744 | 3.181 | 15.182 | 16.236 |

## Notes

- assess.expr1.fsize.np64: small-fsize max R_H base=1496.9%, repeat=2548.9%; all-fsize max base=1496.9%, repeat=2548.9%.
- assess.expr1.fsize.np128: small-fsize max R_H base=2761.8%, repeat=1778.0%; all-fsize max base=2761.8%, repeat=1778.0%.
- If repeat small-fsize high R_H appears at different timer/fsize than base, it supports sparse OS/noise outliers rather than deterministic fsize trend.
