#!/usr/local/bin/bash

sudo ipfw delete 100
sudo ipfw pipe 100 config bw 58Mbit/s delay 9ms queue 43KB plr 0.001

sudo ipfw add 100 pipe 100 all from any to 10.1.1.3

