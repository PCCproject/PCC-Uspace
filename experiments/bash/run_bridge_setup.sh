#!/bin/sh

sudo ipfw delete 100
sudo ipfw pipe 100 config bw 100Mbit/s delay 30ms queue 75KB

echo "${1} pairs of sender/receiver"
L=`jot - 1 $1`
ipPostfix=3
for j in $L
do
  sudo ipfw add 100 pipe 100 all from any to 10.1.1.$ipPostfix
  ipPostfix=$(($ipPostfix + 1))
done
