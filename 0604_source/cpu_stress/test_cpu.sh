#!/bin/bash
#
# $1 Thread_number
# $2 Begin number
# Detail log file
# $3 
# Test times
# $4

cpu=T4240

tmp_detail=/tmp/$cpu-detail
detail="$cpu-$3"

echo -ne "[CPU STRESS Test-Module]: `date +'%Y-%m-%d %T'`\t0/$4"

./cpu_stress --workers $1 --start $2 --times 20  2>&1 | tee -a $tmp_detail  >> $detail &

times=0
lines=0

while [ $times -ne $4 ]
do
	sleep 5

	# We can only get dot in detail log if there isn't any error.
	error=`cat $tmp_detail | grep -E "fialed|timeout|Can't|failed|error"`
	if [ "$error" != "" ]
	then
        	# Wait for the access of the log.

        	failed=`expr $success + 1`
		    error="[error]"
        	echo -en  "\r[CPU STRESS Test-Module]: `date +'%Y-%m-%d %T'` `echo -ne "\t\033[0;31;1m${failed}${error}\033[0m"`/$4 \b\n" 
		    killall ./cpu_stress > /dev/null 2>&1
            rm -rf $detail
            exit 1		
	fi

	# Number of the lines is same with the times of success.
	lines_new=`cat $tmp_detail | awk '{print NR}' | tail -1`
	if [ "$lines_new" != "" ]
	then
		if [ $lines_new -gt $lines ]
		then
			success=`expr $success + 1`
            echo -en "\r[CPU STRESS Test-Module]: `date +'%Y-%m-%d %T'` `echo -ne "\t\033[0;32;1m$success\033[0m"`/$4 \b" 

			lines=$lines_new
			times=`expr $times + 1`
		fi
	fi

done
rm -rf $detail

killall ./cpu_stress > /dev/null 2>&1
echo -ne "\r[CPU STRESS Test-Module]: `date +'%Y-%m-%d %T'``echo -ne "\t\033[0;32;1mFinished\033[0m"`/$4 \b\n" 

exit 0
