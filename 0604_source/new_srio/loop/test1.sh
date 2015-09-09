#!/bin/bash 
times=0
passes=0
while [ $passes -ne $1 ]
do
  #  ./SW --i2c 2 --slave 0x67 --offset 0x3C80C0 --data 0x8000007e --type 1
    sleep 1
   # ./srio_new_passes_256 --port 0 --type swrite --test_type 2 --startcpu 4 --passes 1000 > test_log
    ./srio_check_test61 --port 0 --type swrite --test_type 2 --startcpu 4 --passes 1000 > test_log 
   # sleep 30
   # killall srio_loop_256
    sleep 1
    error=`cat test_log | grep "ERROR"|wc -l`
    if [ $error -ne 0 ]
    then
        times=`expr $times + 1`
        echo -ne "test times $times"
        cp test_log error_print6$times
    else
	echo  "sucessful"
    fi
    rm -rf test_log
    passes=`expr $passes + 1`
done
