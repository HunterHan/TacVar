#!/bin/bash
source ~/code/TacVar/env.bash
mpicc -o timer_probe.x timer_probe.c -O2 -I$PAPI_HOME/include -L$PAPI_HOME/lib -lpapi
echo "--- Running Probe ---"
mpirun -np 1 --map-by core --bind-to core ./timer_probe.x
