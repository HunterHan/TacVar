# cgnr6760pn2 guarded repeat2 comparison

- repeat2 passed precheck and completed for np64/np128.
- camd9554n2 repeat2 did not run because guarded precheck found a high-CPU process.
- Values below compare base, repeat1, and guarded repeat2.

## Per timer stats

expr,timer,points,base_small_rh_max,r1_small_rh_max,r2_small_rh_max,base_rh_median,r1_rh_median,r2_rh_median,base_rh_max,r1_rh_max,r2_rh_max
assess.expr1.fsize.np128,clock_gettime,10,2761.79,1222.83,1090.65,578.58,614.39,460.34,2761.79,1222.83,1090.65
assess.expr1.fsize.np128,mpi_wtime,10,1697.22,1778.02,4056.35,1013.73,633.27,628.11,1697.22,1778.02,4056.35
assess.expr1.fsize.np128,tsc,10,1323.35,691.88,684.0,600.78,471.23,531.76,1323.35,1392.3,1026.93
assess.expr1.fsize.np128,tsc_asym,10,1555.6,1723.88,2370.68,548.06,555.81,709.93,1555.6,1723.88,2370.68
assess.expr1.fsize.np64,clock_gettime,10,1038.92,837.21,784.45,347.48,335.8,432.82,1038.92,837.21,784.45
assess.expr1.fsize.np64,mpi_wtime,10,855.4,1583.87,1279.72,524.92,515.43,405.13,855.4,1583.87,1279.72
assess.expr1.fsize.np64,tsc,10,1496.87,1783.15,1481.43,535.11,590.01,498.2,1496.87,1783.15,1481.43
assess.expr1.fsize.np64,tsc_asym,10,484.53,2548.91,622.94,285.37,563.08,316.19,484.53,2548.91,622.94


## Key summary
- assess.expr1.fsize.np64: small-fsize max RH base=1496.9%, repeat1=2548.9%, repeat2=1481.4%.
- assess.expr1.fsize.np128: small-fsize max RH base=2761.8%, repeat1=1778.0%, repeat2=4056.3%.

## Worst repeat2 RH rows

expr_base,timer,fsize_kib,r_h_pct_base,r_h_pct_r1,r_h_pct_r2,r_l_pct_base,r_l_pct_r1,r_l_pct_r2
assess.expr1.fsize.np128,mpi_wtime,32,1321.24,1392.14,4056.35,32.17,34.89,31.25
assess.expr1.fsize.np128,tsc_asym,128,398.01,307.82,2370.68,34.02,56.37,37.98
assess.expr1.fsize.np64,tsc,16,114.54,1783.15,1481.43,14.95,17.28,16.1
assess.expr1.fsize.np64,mpi_wtime,32,823.34,1583.87,1279.72,15.81,16.88,14.96
assess.expr1.fsize.np128,clock_gettime,32,902.48,731.04,1090.65,35.95,31.24,32.92
assess.expr1.fsize.np128,tsc,8192,1159.55,1392.3,1026.93,142.33,157.45,148.38
assess.expr1.fsize.np128,mpi_wtime,2048,430.52,533.63,999.11,67.77,90.26,108.08
assess.expr1.fsize.np128,mpi_wtime,8192,1506.26,1204.34,984.92,390.07,356.6,311.37
assess.expr1.fsize.np128,clock_gettime,16,2761.79,646.84,942.69,35.81,39.18,34.36
assess.expr1.fsize.np128,clock_gettime,4096,630.59,348.38,894.52,46.36,47.41,46.41
assess.expr1.fsize.np128,clock_gettime,128,590.78,581.94,848.49,33.92,31.76,41.03
assess.expr1.fsize.np128,mpi_wtime,16,1175.19,393.82,815.73,30.77,33.73,31.53
assess.expr1.fsize.np64,mpi_wtime,16,795.08,1104.74,798.48,14.63,15.99,14.88
assess.expr1.fsize.np64,clock_gettime,128,713.87,216.51,784.45,19.57,18.21,18.79
assess.expr1.fsize.np64,tsc,256,1496.87,342.85,766.35,18.08,16.09,16.51
assess.expr1.fsize.np128,tsc_asym,64,528.99,1723.88,758.07,35.6,56.69,36.33
assess.expr1.fsize.np128,tsc_asym,32,769.8,1187.26,743.95,36.2,37.93,37.23
assess.expr1.fsize.np64,clock_gettime,16,237.73,236.01,738.92,15.52,16.25,16.45
assess.expr1.fsize.np128,tsc_asym,16,1555.6,938.85,733.83,36.57,36.48,35.8
assess.expr1.fsize.np128,tsc_asym,8192,567.13,563.18,722.19,46.51,45.67,48.37
