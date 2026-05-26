# Fig6 combined status

- `c920bn3`: previous final batch from `20260520`.
- `camd9554n2`: repeat2 no-guard batch from `20260521`; high CPU was logged but did not stop the run.
- `cgnr6760pn2`: guarded repeat2 batch from `20260521`.
- Each timer line requires all fsize values `16..8192` KiB and `n_measured=n_theory=300`.

## Plotted timer lines
- `c920bn3` `assess.expr1.fsize.np128`: clock_gettime, mpi_wtime, cntvct, cntvcto
- `c920bn3` `assess.expr1.fsize.np64`: clock_gettime, mpi_wtime, cntvct, cntvcto
- `camd9554n2` `assess.expr1.fsize.np128`: tsc, tsc_asym, clock_gettime, mpi_wtime
- `camd9554n2` `assess.expr1.fsize.np64`: tsc, tsc_asym, clock_gettime, mpi_wtime
- `cgnr6760pn2` `assess.expr1.fsize.np128`: tsc, tsc_asym, clock_gettime, mpi_wtime
- `cgnr6760pn2` `assess.expr1.fsize.np64`: tsc, tsc_asym, clock_gettime, mpi_wtime

## Source batches
- `c920bn3` `assess.expr1.fsize.np128` `clock_gettime`: 20260520_184947; c920 final 20260520
- `c920bn3` `assess.expr1.fsize.np128` `cntvct`: 20260520_184947; c920 final 20260520
- `c920bn3` `assess.expr1.fsize.np128` `cntvcto`: 20260520_184947; c920 final 20260520
- `c920bn3` `assess.expr1.fsize.np128` `mpi_wtime`: 20260520_184947; c920 final 20260520
- `c920bn3` `assess.expr1.fsize.np64` `clock_gettime`: 20260520_184807; c920 final 20260520
- `c920bn3` `assess.expr1.fsize.np64` `cntvct`: 20260520_184807; c920 final 20260520
- `c920bn3` `assess.expr1.fsize.np64` `cntvcto`: 20260520_184807; c920 final 20260520
- `c920bn3` `assess.expr1.fsize.np64` `mpi_wtime`: 20260520_184807; c920 final 20260520
- `camd9554n2` `assess.expr1.fsize.np128` `clock_gettime`: 20260521_185512; camd repeat2 no-guard 20260521
- `camd9554n2` `assess.expr1.fsize.np128` `mpi_wtime`: 20260521_185512; camd repeat2 no-guard 20260521
- `camd9554n2` `assess.expr1.fsize.np128` `tsc`: 20260521_185512; camd repeat2 no-guard 20260521
- `camd9554n2` `assess.expr1.fsize.np128` `tsc_asym`: 20260521_185512; camd repeat2 no-guard 20260521
- `camd9554n2` `assess.expr1.fsize.np64` `clock_gettime`: 20260521_184802; camd repeat2 no-guard 20260521
- `camd9554n2` `assess.expr1.fsize.np64` `mpi_wtime`: 20260521_184802; camd repeat2 no-guard 20260521
- `camd9554n2` `assess.expr1.fsize.np64` `tsc`: 20260521_184802; camd repeat2 no-guard 20260521
- `camd9554n2` `assess.expr1.fsize.np64` `tsc_asym`: 20260521_184802; camd repeat2 no-guard 20260521
- `cgnr6760pn2` `assess.expr1.fsize.np128` `clock_gettime`: 20260521_175448; cgnr repeat2 guarded 20260521
- `cgnr6760pn2` `assess.expr1.fsize.np128` `mpi_wtime`: 20260521_175448; cgnr repeat2 guarded 20260521
- `cgnr6760pn2` `assess.expr1.fsize.np128` `tsc`: 20260521_175448; cgnr repeat2 guarded 20260521
- `cgnr6760pn2` `assess.expr1.fsize.np128` `tsc_asym`: 20260521_175448; cgnr repeat2 guarded 20260521
- `cgnr6760pn2` `assess.expr1.fsize.np64` `clock_gettime`: 20260521_175232; cgnr repeat2 guarded 20260521
- `cgnr6760pn2` `assess.expr1.fsize.np64` `mpi_wtime`: 20260521_175232; cgnr repeat2 guarded 20260521
- `cgnr6760pn2` `assess.expr1.fsize.np64` `tsc`: 20260521_175232; cgnr repeat2 guarded 20260521
- `cgnr6760pn2` `assess.expr1.fsize.np64` `tsc_asym`: 20260521_175232; cgnr repeat2 guarded 20260521

## Notes
- no exclusions
