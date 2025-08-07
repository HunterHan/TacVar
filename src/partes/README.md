# Partes - Parallel Timing Error Sensor

## Overview

Partes is a parallel timing error sensor that measures timing deviations between theoretical and measured execution times. It generates a series of gauge measurements using subtraction operations and compares them with theoretical expectations to detect timing anomalies.

## Structure

### Kernel Files

All kernels are located in the `kernels/` directory:

- `none.c` - No operation kernel
- `triad.c` - Triad operation: `a[i] = 0.42 * b[i] + c[i]`
- `scale.c` - Scale operation: `a[i] = 1.0001 * b[i]; b[i] = 1.0001 * a[i]`
- `copy.c` - Copy operation: `a[i] = b[i]`
- `add.c` - Add operation: `a[i] = b[i] + c[i]`
- `pow.c` - Power operation: `a[i] = pow(b[i], 1.0001)`
- `dgemm.c` - Matrix multiplication (simplified)
- `bcast.c` - MPI broadcast operation

### Each Kernel Implements

1. **Init function** - Initialize kernel data structures with memory size in KiB
2. **Run function** - Execute the kernel operation
3. **Cleanup function** - Free allocated memory

## Usage

### Command Line Options

```bash
./partes [options]
```

Options:
- `--fkern <kernel>` - Front kernel (none, triad, scale, copy, add, pow, dgemm, mpi_bcast)
- `--fsize <size>` - Front kernel memory size in KiB
- `--rkern <kernel>` - Rear kernel (none, triad, scale, copy, add, pow, dgemm, mpi_bcast)
- `--rsize <size>` - Rear kernel memory size in KiB
- `--timer <timer>` - Timer method (clock_gettime, mpi_wtime)
- `--ntests <num>` - Number of gauge measurements (default: 100)
- `--nsub <num>` - Number of subtractions per gauge (default: 1000)
- `--help, -h` - Show help message

### Examples

```bash
# Default run (no kernels, 100 tests, 1000 subtractions each)
mpirun -np 2 ./partes

# Run with triad front kernel and scale rear kernel
mpirun -np 2 ./partes --fkern triad --fsize 1024 --rkern scale --rsize 2048 --ntests 50 --nsub 500

# Run with copy front kernel and add rear kernel
mpirun -np 2 ./partes --fkern copy --fsize 512 --rkern add --rsize 1024 --ntests 100 --nsub 1000

# Run with dgemm front kernel only
mpirun -np 2 ./partes --fkern dgemm --fsize 4096 --rkern none --ntests 25 --nsub 2000

# Run with MPI broadcast kernel
mpirun -np 2 ./partes --fkern mpi_bcast --fsize 1024 --rkern none --ntests 75 --nsub 750
```

## Building

```bash
make clean
make
```

## Testing

```bash
./test_compilation.sh
```


## Output

- **Console**: Timing parameters, kernel information, and Wasserstein distance
- **wd.out**: Wasserstein distance value for external processing
- **Logging**: Detailed timing measurements and statistical analysis
