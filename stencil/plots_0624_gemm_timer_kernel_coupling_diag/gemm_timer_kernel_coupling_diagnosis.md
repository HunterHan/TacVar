# GEMM/DSUB Timer-Kernel Coupling Diagnosis

`I_timer = (T_timer(GEMM)/T_TSC(GEMM)) / (T_timer(DSub)/T_TSC(DSub))`. Values above 1 indicate timer+GEMM amplification relative to timer+DSub; values below 1 indicate the opposite direction.

| host | timer | GEMM/TSC | DSub/TSC | I_timer | conclusion |
|---|---:|---:|---:|---:|---|
| camd9554n1 | cgt | 0.609 | 1.004 | 0.606 | rejected; GEMM ratio lower than DSub |
| camd9554n1 | papi | 0.822 | 1.204 | 0.682 | kernel-dependent |
| camd9554n1 | wtime | 0.609 | 1.004 | 0.606 | GEMM ratio lower than DSub |
| cgnr6760pn2 | cgt | 0.920 | 1.001 | 0.919 | no material differential |
| cgnr6760pn2 | papi | 0.883 | 1.039 | 0.850 | kernel-dependent |
| cgnr6760pn2 | wtime | 0.858 | 1.003 | 0.856 | GEMM ratio lower than DSub |
