#!/bin/sh
SPS_NODES=`lspci -n | grep 1cb0 | wc -l`
EXEC=syslk
EXECAGT=sysagt
if [ "$SPS_NODES" != "2" ]
then
    echo =========== Can not find SPS card!
    echo =========== System will reboot now.
    reboot
    exit
fi

while true; do
EXEC_INST=`ps aux | grep $EXEC | grep -v grep | wc -l`
if [ "$EXEC_INST" == "0" ]
then
    ./$EXEC &
fi
EXECAGT_INST=`ps aux | grep $EXECAGT | grep -v grep | wc -l`
if [ "$EXEC_INST" == "0" ]
then
    ./$EXECAGT &
fi
sleep 10
done
