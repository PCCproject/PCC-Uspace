#!/bin/bash

scp ./src/app/pccserver "$1@sender1.$2.UIUCScheduling.emulab.net:~/pccserver"
scp ./src/app/pccclient "$1@sender1.$2.UIUCScheduling.emulab.net:~/pccclient"
