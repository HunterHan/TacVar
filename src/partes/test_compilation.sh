#!/bin/bash

echo "Testing partes compilation..."

# Clean and compile
make clean
make

if [ $? -eq 0 ]; then
    echo "Compilation successful!"
    
    # Test help
    echo "Testing help option..."
    ./partes --help
    
    # Test with no arguments (should work with defaults)
    echo "Testing with default arguments..."
    mpirun -np 2 ./partes
    
    # Test with specific kernels and timing parameters
    echo "Testing with triad kernel and timing parameters..."
    mpirun -np 2 ./partes --fkern triad --fsize 1024 --rkern scale --rsize 2048 --ntests 50 --nsub 500
    
    echo "Testing with copy kernel and timing parameters..."
    mpirun -np 2 ./partes --fkern copy --fsize 512 --rkern add --rsize 1024 --ntests 100 --nsub 1000
    
    echo "Testing with dgemm kernel only..."
    mpirun -np 2 ./partes --fkern dgemm --fsize 4096 --rkern none --ntests 25 --nsub 2000
    
    echo "Testing with MPI broadcast kernel..."
    mpirun -np 2 ./partes --fkern mpi_bcast --fsize 1024 --rkern none --ntests 75 --nsub 750
    
else
    echo "Compilation failed!"
    exit 1
fi
