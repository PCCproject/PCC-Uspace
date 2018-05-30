#!/bin/bash

varETH="$(/sbin/ifconfig | grep eth[^0] | sed 's/ .*//')"
echo "Operate on interface ${varETH}"

sudo ethtool -K ${varETH} gso off
sudo ethtool -K ${varETH} tso off
sudo ethtool -K ${varETH} gro off
