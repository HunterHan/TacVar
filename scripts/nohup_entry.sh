#!/bin/bash -x
nohup ./run_detecting_expr1_fsize.sh > "nohup-$(date +%Y%m%d_%H%M%S).log" 2>&1 &