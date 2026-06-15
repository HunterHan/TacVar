#!/bin/bash
# Clean up 
for node in camd9554n1 cgnr6760pn2 c920bn3; do
    ssh $node 'pkill -9 -u hpchzy mpirun; pkill -9 -u hpchzy detecing-mpi.0614.x; pkill -9 -f run_entry'
done

sleep 2

# Task 1: expr2 on AMD/GNR
ssh camd9554n1 'cd ~/code/TacVar && (nohup env NUM_WALK=20 bash scripts/run_entry_detecting.expr2Fsize.0614.sh detect all > expr2Fsize.amd.log 2>&1 &)'
ssh cgnr6760pn2 'cd ~/code/TacVar && (nohup env NUM_WALK=20 bash scripts/run_entry_detecting.expr2Fsize.0614.sh detect all > expr2Fsize.gnr.log 2>&1 &)'
# Task 3: expr3 on Kunpeng
ssh c920bn3 'cd ~/code/TacVar && (nohup env NUM_WALK=20 bash scripts/run_entry_detecting.expr3Interval.0614.sh detect all > expr3Interval.c920.log 2>&1 &)'
