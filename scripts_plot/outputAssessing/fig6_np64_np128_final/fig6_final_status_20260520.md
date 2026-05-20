# Fig6 final status

- Expected samples per point: `300` (`NWALKS=3`, `NTESTS=100`).
- Expected fsize KiB: `16 32 64 128 256 512 1024 2048 4096 8192`.
- Selection rule: newest complete row per `(host, expr, timer, fsize)`, then require all fsize points for a timer line.

## Plotted timer lines
- `c920bn3` `assess.expr1.fsize.np128`: clock_gettime, mpi_wtime, cntvct, cntvcto
- `c920bn3` `assess.expr1.fsize.np64`: clock_gettime, mpi_wtime, cntvct, cntvcto
- `camd9554n2` `assess.expr1.fsize.np128`: tsc, clock_gettime, mpi_wtime
- `camd9554n2` `assess.expr1.fsize.np64`: tsc, tsc_asym, clock_gettime, mpi_wtime
- `cgnr6760pn2` `assess.expr1.fsize.np128`: tsc, tsc_asym, clock_gettime, mpi_wtime
- `cgnr6760pn2` `assess.expr1.fsize.np64`: tsc, tsc_asym, clock_gettime, mpi_wtime

## Notes
- excluded camd9554n2 assess.expr1.fsize.np128 tsc_asym: missing fsize [1024, 2048, 4096, 8192]

## Source batches
- `c920bn3` `assess.expr1.fsize.np128` `clock_gettime`: 20260520_184947
- `c920bn3` `assess.expr1.fsize.np128` `cntvct`: 20260520_184947
- `c920bn3` `assess.expr1.fsize.np128` `cntvcto`: 20260520_184947
- `c920bn3` `assess.expr1.fsize.np128` `mpi_wtime`: 20260520_184947
- `c920bn3` `assess.expr1.fsize.np64` `clock_gettime`: 20260520_184807
- `c920bn3` `assess.expr1.fsize.np64` `cntvct`: 20260520_184807
- `c920bn3` `assess.expr1.fsize.np64` `cntvcto`: 20260520_184807
- `c920bn3` `assess.expr1.fsize.np64` `mpi_wtime`: 20260520_184807
- `camd9554n2` `assess.expr1.fsize.np128` `clock_gettime`: 20260520_194235
- `camd9554n2` `assess.expr1.fsize.np128` `mpi_wtime`: 20260520_194235
- `camd9554n2` `assess.expr1.fsize.np128` `tsc`: 20260520_190124
- `camd9554n2` `assess.expr1.fsize.np64` `clock_gettime`: 20260520_185304
- `camd9554n2` `assess.expr1.fsize.np64` `mpi_wtime`: 20260520_185304
- `camd9554n2` `assess.expr1.fsize.np64` `tsc`: 20260520_185304
- `camd9554n2` `assess.expr1.fsize.np64` `tsc_asym`: 20260520_185304
- `cgnr6760pn2` `assess.expr1.fsize.np128` `clock_gettime`: 20260520_185639
- `cgnr6760pn2` `assess.expr1.fsize.np128` `mpi_wtime`: 20260520_185639
- `cgnr6760pn2` `assess.expr1.fsize.np128` `tsc`: 20260520_185639
- `cgnr6760pn2` `assess.expr1.fsize.np128` `tsc_asym`: 20260520_185639
- `cgnr6760pn2` `assess.expr1.fsize.np64` `clock_gettime`: 20260520_185305
- `cgnr6760pn2` `assess.expr1.fsize.np64` `mpi_wtime`: 20260520_185305
- `cgnr6760pn2` `assess.expr1.fsize.np64` `tsc`: 20260520_185423, 20260520_185305
- `cgnr6760pn2` `assess.expr1.fsize.np64` `tsc_asym`: 20260520_185305
