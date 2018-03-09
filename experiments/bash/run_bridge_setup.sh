#!/usr/local/bin/bash

sudo ipfw delete 100
sudo ipfw pipe 100 config bw 10Mbit/s delay 10ms queue 75KB plr 0.000

sudo ipfw add 100 pipe 100 all from any to 10.1.1.3
sudo ipfw add 100 pipe 100 all from any to 10.1.1.4
sudo ipfw add 100 pipe 100 all from any to 10.1.1.5
sudo ipfw add 100 pipe 100 all from any to 10.1.1.6

