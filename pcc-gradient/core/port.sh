#!/bin/bash

sunifdef -DQUIC_PORT -DQUIC_PORT_LOCAL ./src/pcc_monitor_interval_queue.cpp > "$1/pcc_monitor_interval_queue.cc"
sunifdef -DQUIC_PORT -DQUIC_PORT_LOCAL ./src/pcc_monitor_interval_queue.h > "$1/pcc_monitor_interval_queue.h"
sunifdef -DQUIC_PORT -DQUIC_PORT_LOCAL ./src/pcc_sender.cpp > "$1/pcc_sender.cc"
sunifdef -DQUIC_PORT -DQUIC_PORT_LOCAL ./src/pcc_sender.h > "$1/pcc_sender.h"
