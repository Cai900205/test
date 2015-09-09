#!/bin/bash
#
# $1 memsize
# $2 loop
# thread_number
# $3
# begin_cpu
# $4
# Test times
# $5

echo -ne "POWERPC-MEM_TEST-V1.00"
k=$4
detail=memdetail
tmp_detail=/tmp/memdetail$k
begin_cpu=$4
echo -ne "[CPU MEMTEST  Test-Module]: `date +'%Y-%m-%d %T'`\t0/$5"

for((i=0;i<$3;i++));do
./memtester  $1  $2   2>&1 | tee -a $tmp_detail  >> $detail &
begin_cpu=`expr $begin_cpu + 1`
k=`expr $k + 1`
tmp_detail=/tmp/memdetail$k
done


times=0
lines=0

while [ $times -ne $5 ]
do
	sleep 5

	# We can only get dot in detail log if there isn't any error.
	error=`cat $detail | grep -E "FAILURE|timeout|Can't|failed|error"`
	if [ "$error" != "" ]
	then
        	# Wait for the access of the log.

        	failed=`expr $success + 1`
		    error="[error]"
        	echo -en  "\r[CPU MEMTEST Test-Module]: `date +'%Y-%m-%d %T'` `echo -ne "\t\033[0;31;1m${failed}${error}\033[0m"`/$5 \b\n" 
		    killall memtester > /dev/null 2>&1
            rm -rf $detail
            exit 1		
	fi

	# Number of the lines is same with the times of success.
	#lines_new=`cat $detail | awk '{print NR}' | tail -1`
	#if [ "$lines_new" != "" ]
	#then
#		if [ $lines_new -gt $lines ]
#		then
			success=`expr $success + 1`
            echo -en "\r[CPU MEMTEST Test-Module]: `date +'%Y-%m-%d %T'` `echo -ne "\t\033[0;32;1m$success\033[0m"`/$5 \b" 

#			lines=$lines_new
			times=`expr $times + 1`
#		fi
#	fi

done
rm -rf $detail

killall memtester > /dev/null 2>&1
echo -ne "\r[CPU MEMTEST Test-Module]: `date +'%Y-%m-%d %T'``echo -ne "\t\033[0;32;1mFinished\033[0m"`/$5 \b\n" 

exit 0
