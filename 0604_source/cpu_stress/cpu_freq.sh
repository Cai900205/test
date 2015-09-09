#!/bin/sh
./cpu_stress --times 10000000000 --workers 8
if [ $? -ne 0 ]
then
	exit 1
fi
cpu_number=`cat /proc/cpuinfo | grep "cpu" | wc -l`
cpu_freq=`cat /proc/cpuinfo | grep "clock" | awk 'NR==1 {print $3}'`
cpu_freq=`echo $cpu_freq | sed 's/\([0-9]*\).\(0*\)/\1/g'`
cpu_freq=`awk 'BEGIN {printf '$cpu_freq'/1000.0}'`
echo "[CPU-FREQUENCY-`date +'%Y-%m-%d %T'`]: Get the cpu number:$cpu_number Get the cpu frequency: ${cpu_freq}GHz"

#if [ $(echo "$cpu_freq > 0"|bc) = 1 ]
#then
#        exit 0
#else
#        exit 1
#fi
exit 0
 
