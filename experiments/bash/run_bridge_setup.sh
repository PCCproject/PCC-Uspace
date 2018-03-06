#!/usr/local/bin/bash

sudo ipfw delete 100
sudo ipfw pipe 100 config bw 10Mbit/s delay 10ms queue 75KB plr 0.000

sudo ipfw add 100 pipe 100 all from any to 10.1.1.3

sudo ipfw pipe 100 config bw 98Mbit/s delay 91ms queue 713KB plr 0.040
sleep 5

sudo ipfw pipe 100 config bw 28Mbit/s delay 88ms queue 458KB plr 0.051
sleep 5

sudo ipfw pipe 100 config bw 55Mbit/s delay 26ms queue 138KB plr 0.075
sleep 5

sudo ipfw pipe 100 config bw 54Mbit/s delay 19ms queue 447KB plr 0.031
sleep 5

sudo ipfw pipe 100 config bw 27Mbit/s delay 43ms queue 379KB plr 0.019
sleep 5

sudo ipfw pipe 100 config bw 19Mbit/s delay 18ms queue 533KB plr 0.005
sleep 5

sudo ipfw pipe 100 config bw 96Mbit/s delay 36ms queue 271KB plr 0.041
sleep 5

sudo ipfw pipe 100 config bw 90Mbit/s delay 47ms queue 689KB plr 0.057
sleep 5

sudo ipfw pipe 100 config bw 12Mbit/s delay 27ms queue 224KB plr 0.017
sleep 5

sudo ipfw pipe 100 config bw 10Mbit/s delay 99ms queue 505KB plr 0.094
sleep 5

sudo ipfw pipe 100 config bw 51Mbit/s delay 48ms queue 285KB plr 0.075
sleep 5

sudo ipfw pipe 100 config bw 92Mbit/s delay 92ms queue 469KB plr 0.063
sleep 5

sudo ipfw pipe 100 config bw 97Mbit/s delay 25ms queue 274KB plr 0.073
sleep 5

sudo ipfw pipe 100 config bw 62Mbit/s delay 65ms queue 672KB plr 0.039
sleep 5

sudo ipfw pipe 100 config bw 81Mbit/s delay 17ms queue 117KB plr 0.035
sleep 5

sudo ipfw pipe 100 config bw 67Mbit/s delay 56ms queue 593KB plr 0.045
sleep 5

sudo ipfw pipe 100 config bw 22Mbit/s delay 95ms queue 124KB plr 0.011
sleep 5

sudo ipfw pipe 100 config bw 41Mbit/s delay 91ms queue 189KB plr 0.041
sleep 5

sudo ipfw pipe 100 config bw 48Mbit/s delay 41ms queue 556KB plr 0.059
sleep 5

sudo ipfw pipe 100 config bw 39Mbit/s delay 49ms queue 524KB plr 0.041
sleep 5

