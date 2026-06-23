# GEMM Timer Diagnostic Diagnosis

| hypothesis | status | evidence |
|---|---|---|
| source/compile mismatch | rejected if manifests match | Check `source_manifest.txt` in each run directory; analyzer only uses dirs with expected result shape. |
| c920bn3: ARM variants collapse to CNTVCTO | confirmed | cntvcto=1.000, max median |ratio-1|=0.007. |
| camd9554n1: TSC serialization/barrier causes low/high cluster | partial | lfence/rdtsc moves to wall/tsc_native cluster, cpuid+memory stays with current TSC; tr ratios: tsc=1.000, cpuid_mem=1.000, lfence_mem=1.678, tsc_native=1.709, cgt=1.694, wtime=1.695. |
| camd9554n1: dummy clock_gettime before TSC moves TSC to wall-timer cluster | rejected | tr ratios: tsc_pre_cgt=0.995, low_cluster=0.999, wall_cluster=1.695. |
| camd9554n1: PAPIX6 start-side PAPI_read causes high cluster | confirmed | moving start-side PAPI_read before ns0 moves PAPIX6 from high cluster to low cluster; tr ratios: papi=0.998, papix6_current=1.693, read_before=0.998, no_read=1.696. |
| cgnr6760pn2: TSC serialization/barrier causes low/high cluster | rejected | no material low/high cluster in this host/size aggregate; tr ratios: tsc=1.000, cpuid_mem=1.000, lfence_mem=1.115, tsc_native=1.133, cgt=1.217, wtime=1.128. |
| cgnr6760pn2: dummy clock_gettime before TSC moves TSC to wall-timer cluster | rejected | tr ratios: tsc_pre_cgt=0.997, low_cluster=0.991, wall_cluster=1.133. |
| cgnr6760pn2: PAPIX6 start-side PAPI_read causes high cluster | rejected | no material cluster to explain; tr ratios: papi=0.982, papix6_current=1.097, read_before=0.982, no_read=1.098. |
