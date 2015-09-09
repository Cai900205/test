#!/bin/bash
#
# $1 memsize
# $2 workers

detail=/mnt/memdetail
tmp_detail=/tmp/memdetail

echo -ne "[MEMTEST  Test-Module]: `date +'%Y-%m-%d %T'`\t"
#taskset -c 5-23 
./memtester --passes 0 --memsize $1 --workers $2 2<&1 | tee -a $tmp_detail >> $detail &

lines=0
failed=0
success=0

while [ 1 ]
do
	sleep 60
	error=`cat $tmp_detail | grep -E "FAILURE|timeout|Can't|failed|error"`
	if [ "$error" != "" ]
	then
        	failed=`expr $failed + 1`
        	echo -en  "\r[MEMTEST Test-Module]: `date +'%Y-%m-%d %T'` `echo -ne "\t\033[0;31;1m${failed}\033[0m"`/`echo -ne "\033[0;32;1m${success}\033[0m"` \b\n" 
            killall memtester
            rm -rf $tmp_detail
            sleep 1
            ./memtester --passes 0 --memsize $1 --workers $2 2<&1 | tee -a $tmp_detail >> $detail &
    fi
    # Number of the lines is same with the times of success.
	lines_new=`cat $tmp_detail | awk '{print NR}' | tail -1`
	if [ "$lines_new" != "" ]
	then
		if [ $lines_new -gt $lines ]
		then
			success=`expr $success + 1`
        	echo -en  "\r[MEMTEST Test-Module]: `date +'%Y-%m-%d %T'` `echo -ne "\t\033[0;31;1m${failed}\033[0m"`/`echo -ne "\033[0;32;1m${success}\033[0m"` \b" 
			lines=$lines_new
			times=`expr $times + 1`
		fi
	fi

done
killall memtester > /dev/null 2>&1
echo -en  "\r[MEMTEST Test-Module]: `date +'%Y-%m-%d %T'` `echo -ne "\t\033[0;31;1m${failed}\033[0m"`/`echo -ne "\033[0;32;1m Finished${success}\033[0m"` \b\n" 

exit 0
