# Tools for ParTES

TacVar/src/partes/tools contains tools for developing ParTES of TacVAR. These tools are mainly functional modules extracted from procedures of ParTES or facilitated to emulate certain noisy parallel measuring. Each tool is coded as simple as possible for the most simplicity. So someone who wants to utilize these tool to characterize the timing fluctuation on their own systems may need to create extra scripts for comprehensive functions.

Tool list:
- **meas_single**: Execute nsub kernels for multiple times and print results to csv files.
- **meas_series_wd**: Measuring sub kernel from ticks to ticke, caculating the Wasserstein Distance of met_cdf vs theoretical time.
- **meas_pair**: Calculate statistical measures (1D Wasserstein Distance and Pearson correlation coefficient) between CDFs of two different nsub kernel measurements. Outputs raw measurements to CSV files. 

## meas_single

meas_single runs the single-substraction kernel for _nrepeat_ times. Each loop step, the kernel will execute the instruction **"a=a-1"** instruction for _nsub_ times. In ParTES, the nsub kernel is used as the basic gauge block for runtime measurement. This tool will generate .csv files for each MPI rank. The first column is nsub, and the second column is the runtime measured on each repeat.

Usage:

``` bash
$ cd TacVar/src/partes/tools
$ make meas_single.x
# Run a 1000-operation nsub kernel for 100 times.
$ mpirun -np <nprocs>./meas_single.x 1000 100
```

## meas_series_wd

meas_series_wd accepts _gpt_ args, and runs the number of nsub kernels from _ticks_ to _ticke_ with _interval_ ticks as interval, each tick runs for _nrepeat_ times. For each tick, the 1D Wasserstein Distance between the cdf (with _ntiles_ tiles) of measured nsub time and standard time calculated with _gpt_ and tick will be printed.

Usage:

``` bash
$ cd TacVar/src/partes/tools
$ make meas_series_wd.x
# Run a 1000-operation nsub kernel for 100 times.
$ mpirun -np <nprocs> ./meas_series_wd.x <gpt> <ticks> <ticke> <interval> <ntiles> <cut_tile>
``` 

## meas_pair

meas_pair accepts <nsub1> <nsub2> args, and runs the number of nsub kernels for <nrepeat> times. Calculating the 1D Wasserstein Distance and Pearson correlation coefficient between the cdf of nsub1 and nsub2. The number of quantiles are set by <ntiles>, calculated from 0 to <cut_tile> in (0, 1]. The output includes both the Wasserstein distance (in nanoseconds) and the Pearson correlation coefficient for each MPI rank. Raw measurement results are saved to CSV files named meas_pair_r<rankid>_ng<nsub1/2>.csv with one measurement per line and no header.

Usage:

``` bash
$ cd TacVar/src/partes/tools
$ make meas_pair.x
$ mpirun -np <nprocs> ./meas_pair.x <nsub1> <nsub2> <nrepeat> <ntiles> <cut_tile>
```