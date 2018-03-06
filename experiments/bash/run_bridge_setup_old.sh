#!/usr/local/bin/bash

# has to use /usr/local/bin/bash on FreeBSD node

# $1: number of node pairs
# $2: interval to change pipe
# $3: random seed
# $4: l_bandwidth
# $5: r_bandwidth
# $6: l_delay
# $7: r_delay
# $8: l_queue_size
# $9: r_queue_size
# $10: l_loss_rate
# $11: r_loss_rate
# $12: emulab username

sudo ipfw delete 100
sudo ipfw pipe 100 config bw ${4}Mbit/s delay ${6}ms queue ${8}KB plr ${10}

echo "${1} pairs of sender/receiver"
L=`jot ${1}`
ipPostfix=3
for j in $L
do
  sudo ipfw add 100 pipe 100 all from any to 10.1.1.$ipPostfix
  ipPostfix=$(($ipPostfix + 1))
done

if [ ${2} -eq 0 ]
then
  echo "Static pipe setup"
  exit 1
fi

# store the trace files under user home
echo "Starting pipe setup script"
start_time_ms=$(date +%s)
echo "### Bridge setup trace" >/users/${12}/pcc_log_bridge_${start_time_ms}.txt

# assign a value to ${RANDOM} to guarantee the same sequence of changing pipes
RANDOM=1
while [ 1 ]
do
  # FIXME: pay attention to the maximum range of RANDOM (only up to 32767)
  # bandwidth has the granularity of 1Mbps
  l_bw=$(gawk -v x=${4} 'BEGIN{printf "%d\n", (x)}')
  r_bw=$(gawk -v x=${5} 'BEGIN{printf "%d\n", (x)}')
  bw=$((RANDOM % $((${r_bw}-${l_bw}+1)) + ${l_bw}))

  # delay has the granularity of 1ms
  l_dl=$(gawk -v x=${6} 'BEGIN{printf "%d\n", (x)}')
  r_dl=$(gawk -v x=${7} 'BEGIN{printf "%d\n", (x)}')
  dl=$((RANDOM % $((${r_dl}-${l_dl}+1)) + ${l_dl}))

  # queue size has granularity of 1KB
  l_qs=$(gawk -v x=${8} 'BEGIN{printf "%d\n", (x)}')
  r_qs=$(gawk -v x=${9} 'BEGIN{printf "%d\n", (x)}')
  qs=$((RANDOM % $((${r_qs}-${l_qs}+1)) + ${l_qs}))

  # loss rate has the granularity of 0.01%
  l_lr=$(gawk -v x=${10} 'BEGIN{printf "%d\n", (x * 10000)}')
  r_lr=$(gawk -v x=${11} 'BEGIN{printf "%d\n", (x * 10000)}')
  lr_base=$((RANDOM % $((${r_lr}-${l_lr}+1)) + ${l_lr}))
  lr=$(gawk -v x=${lr_base} 'BEGIN{printf "%.4f\n", (1.0*x/10000.0)}')

  time_ms=$(date +%s)
  echo "${time_ms}: bw ${bw}Mbit/s delay ${dl}ms queue ${qs}KB plr ${lr}" >> \
      /users/${12}/pcc_log_bridge_${start_time_ms}.txt

  sudo ipfw pipe 100 config bw ${bw}Mbit/s delay ${dl}ms queue ${qs}KB plr ${lr}

  sleep ${2}
done
