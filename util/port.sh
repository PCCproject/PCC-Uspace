#!/bin/bash

sunifdef -DQUIC_PORT -DQUIC_PORT_LOCAL "$1/src/pcc/pcc_monitor_interval_queue.cpp" > "$2/pcc_monitor_interval_queue.cc"
sunifdef -DQUIC_PORT -DQUIC_PORT_LOCAL "$1/src/pcc/pcc_monitor_interval_queue.h" > "$2/pcc_monitor_interval_queue.h"
sunifdef -DQUIC_PORT -DQUIC_PORT_LOCAL "$1/src/pcc/pcc_sender.cpp" > "$2/pcc_sender.cc"
sunifdef -DQUIC_PORT -DQUIC_PORT_LOCAL "$1/src/pcc/pcc_sender.h" > "$2/pcc_sender.h"
