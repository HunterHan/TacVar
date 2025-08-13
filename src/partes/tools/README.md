# Tools for ParTES

TacVar/src/partes/tools contains tools for developing ParTES of TacVAR. These tools are mainly functional modules extracted from procedures of ParTES or facilitated to emulate certain noisy parallel measuring. Each tool is coded as simple as possible for the most simplicity. So someone who wants to utilize these tool to characterize the timing fluctuation on their own systems may need to create extra scripts for comprehensive functions.

Tool list:
- **run_nsub_mpi**: Execute nsub kernels for multiple times and print results to csv files.

## run_nsub_mpi

run_nsub_mpi runs the single-substraction kernel for _nrepeat_ times. Each loop step, the kernel will execute the instruction **"a=a-1"** instruction for _nsub_ times. In ParTES, the nsub kernel is used as the basic gauge block for runtime measurement. This tool will generate .csv files for each MPI rank. The first column is nsub, and the second column is the runtime measured on each repeat.

Usage:

``` bash
$ cd TacVar/src/partes/tools
$ make run_nsub_mpi.x
# Run a 1000-operation nsub kernel for 100 times.
$ mpirun -np ./make_run_nsub_mpi.x 1000 100
```

# 