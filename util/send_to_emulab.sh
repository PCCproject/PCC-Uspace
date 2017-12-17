#!/bin/bash

scp ./src/app/pccserver "$1@sender1.$2.UIUCScheduling.emulab.net:~/pccserver"
scp ./src/app/pccclient "$1@sender1.$2.UIUCScheduling.emulab.net:~/pccclient"
scp ./src/core/libudt.so "$1@sender1.$2.UIUCScheduling.emulab.net:~/libudt.so"
scp ./src/pcc_addon.py "$1@sender1.$2.UIUCScheduling.emulab.net:~/pcc_addon.py"
