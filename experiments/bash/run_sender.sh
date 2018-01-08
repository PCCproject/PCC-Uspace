#!/bin/bash

# $1: sender executable file
# $2: receiver ip addr
# $3: dir_expr_path
# $4: number of node pairs
# $5: bridge pipe bandwidth
# $6: bridge pipe delay
# $7: bridge pipe queue size
# $8: bridge pipe random loss rate
# $9: std out destination
# $10: std err destination
# $11: connection duration

time_ms=$(date +%s)
echo "Timestamp: ${time_ms}"

# TODO: implement changing network experiment in addition to "Simple"
$1 send $2 9000 -DEBUG_RATE_CONTROL -DEBUG_UTILITY_CALC -LOG_RATE_CONTROL_PARAMS -log=$3/pcc_log_${time_ms}.txt -experiments=Simple -npairs=$4 -bandwidth=$5 -delay=$6 -queue=$7 -loss=$8 1>$9 2>${10} & PID=$!

sleep ${11}
sudo kill -2 $PID
