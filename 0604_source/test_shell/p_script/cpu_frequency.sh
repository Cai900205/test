#!/bin/sh
cpu_number=`cat /proc/cpuinfo | grep "cpu" | wc -l`
cpu_freq=`cat /proc/cpuinfo | grep "clock" | awk 'NR==1 {print $3}'`
cpu_freq=`echo $cpu_freq | sed 's/\([0-9]*\).\(0*\)/\1/g'`
cpu_freq=`awk 'BEGIN {printf '$cpu_freq'/1000.0}'`
echo "[CPU-FREQUENCY-`date +'%Y-%m-%d %T'`]: Get the cpu number:$cpu_number Get the cpu frequency: ${cpu_freq}GHz"

./bin/cpu_stress --passes $1 --time $2  --workers $3 --startcpu $4
if [ $? -eq 0 ]
then
	echo "[CPU-STRESS-`date +'%Y-%m-%d %T'`]: CPU stress test success."
	exit 0
else
	echo "[CPU-STRESS-`date +'%Y-%m-%d %T'`]: CPU stress test failed."
	exit 1
fi

 
