#!/bin/bash
#
# $1 Thread_number

cpu=T4240

tmp_detail="/tmp/cpu_stress-detail"
detail="/mnt/cpu-stress"

echo -ne "[CPU STRESS Test-Module]: `date +'%Y-%m-%d %T'`\t"

./cpu_stress --workers $1 --passes 0  2>&1 | tee -a $tmp_detail  >> $detail &

times=0
lines=0
failed=0
success=0
while [ 1 ]
do
	sleep 60

	error=`cat $tmp_detail | grep -E "fialed|timeout|Can't|failed|error"`
	if [ "$error" != "" ]
	then
        	# Wait for the access of the log.

        	failed=`expr $failed + 1`
        	echo -en  "\r[CPU STRESS Test-Module]: `date +'%Y-%m-%d %T'` `echo -ne "\t\033[0;31;1m${failed}\033[0m"`/`echo -ne "\033[0;32;1m${success}\033[0m"` \b\n" 
		  #  killall ./cpu_stress > /dev/null 2>&1
          #  rm -rf $detail
          #  exit 1		
	fi

	# Number of the lines is same with the times of success.
	lines_new=`cat $tmp_detail | awk '{print NR}' | tail -1`
	if [ "$lines_new" != "" ]
	then
		if [ $lines_new -gt $lines ]
		then
			success=`expr $success + 1`
        	echo -en  "\r[CPU STRESS Test-Module]: `date +'%Y-%m-%d %T'` `echo -ne "\t\033[0;31;1m${failed}\033[0m"`/`echo -ne "\033[0;32;1m${success}\033[0m"` \b" 

			lines=$lines_new
			times=`expr $times + 1`
		fi
	fi

done
rm -rf $tmp_detail
echo -en  "\r[CPU STRESS Test-Module]: `date +'%Y-%m-%d %T'` `echo -ne "\t\033[0;31;1m${failed}\033[0m"`/`echo -ne "\033[0;32;1m Finished${success}\033[0m"` \b\n" 

killall ./cpu_stress > /dev/null 2>&1
exit 0
