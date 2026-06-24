# GEMM CGT Kernel-Dependence Diagnosis

Run note: this is a bounded proof run after the full 32/64/128/256, NSAMP=1000 diagnostic proved impractically slow. The completed run uses three hosts, sizes 32 and 64, NSAMP=200, three shuffles, np=64. It is intended to validate/refute the CGT/DSub-mismatch mechanism, not to replace the formal GEMM full experiment.

| hypothesis | status | evidence |
|---|---|---|
| c920bn3: CGT prelude perturbs GEMM under reference timer | rejected | cntvcto_after_cgt_prelude_gemm/ref median tm ratio=1.000; ref=1.000; call_only=nan. |
| c920bn3: GEMM-like/GEMM-row TE reduces CGT residual | rejected | cgt TM=1.020; TR dsub=1.003; TR gemm_like=0.998; TR gemm_row=0.998; interactions pct dsub=0.3, like=0.2, row=0.2. |
| c920bn3: PAPI bias remains DSub-correctable | confirmed | papi TR by dsub ratio=0.991; papi interaction dsub pct=0.9. |
| camd9554n1: CGT prelude perturbs GEMM under reference timer | rejected | tsc_after_cgt_prelude_gemm/ref median tm ratio=1.000; ref=1.000; call_only=1.000. |
| camd9554n1: CGT prelude perturbs GEMM differently from DSub | rejected | GEMM prelude ratio=1.000; DSub prelude TE ratio=0.998; abs gap=0.002. |
| camd9554n1: GEMM-like/GEMM-row TE reduces CGT residual | rejected | cgt TM=1.003; TR dsub=0.984; TR gemm_like=0.984; TR gemm_row=0.183; interactions pct dsub=1.6, like=1.6, row=81.7. |
| camd9554n1: PAPI bias remains DSub-correctable | confirmed | papi TR by dsub ratio=1.000; papi interaction dsub pct=0.0. |
| cgnr6760pn2: CGT prelude perturbs GEMM under reference timer | rejected | tsc_after_cgt_prelude_gemm/ref median tm ratio=0.977; ref=1.000; call_only=1.024. |
| cgnr6760pn2: CGT prelude perturbs GEMM differently from DSub | rejected | GEMM prelude ratio=0.977; DSub prelude TE ratio=0.999; abs gap=0.023. |
| cgnr6760pn2: GEMM-like/GEMM-row TE reduces CGT residual | rejected | cgt TM=1.069; TR dsub=1.068; TR gemm_like=1.078; TR gemm_row=0.464; interactions pct dsub=6.8, like=7.8, row=53.6. |
| cgnr6760pn2: PAPI bias remains DSub-correctable | confirmed | papi TR by dsub ratio=1.052; papi interaction dsub pct=5.2. |
