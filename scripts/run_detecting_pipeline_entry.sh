#!/bin/bash -x

status=1
echo "Running expr1_timer"
while [ "$status" -ne 0 ]; do
  ./run_detecting_expr1_timer.sh
  status=$?
done

echo "Running expr1_fsize"
status=1
while [ "$status" -ne 0 ]; do
  ./run_detecting_expr1_fsize.sh
  status=$?
done

echo "Running expr2_interval"
status=1
while [ "$status" -ne 0 ]; do
  ./run_detecting_expr2_interval.sh
  status=$?
done

echo "Running expr3_frkern"
status=1
while [ "$status" -ne 0 ]; do
  ./run_detecting_expr3_frkern.sh
  status=$?
done

echo "Finished"