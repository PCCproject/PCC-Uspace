#!/bin/bash
$1 $2 1>$3 2>$4 & PID=$!
if [ "$5" -ne "0" ];
then
    sleep $5
    sudo kill -2 $PID
fi
