#!/bin/bash
# test type
# $1 

cpu=SRIO

if [ $1 -eq 1 ]
then
    value=link
else 
    if [ $1 -eq 2 ]
    then
        value=checkdata
    else    
        if [ $1 -eq 3 ]
        then
            value=speed
        else
            exit 1
        fi
    fi
fi

detail=/mnt/SRIO-$value-detail
tmp_detail=/tmp/SRIO-$value

echo -ne "[SRIO $value Test-Module]: `date +'%Y-%m-%d %T'`\t"

./srio_test --passes 0  --data_type swrite --test_type $1   2>&1 | tee -a $tmp_detail  >> $detail &

times=0
lines=0
success=0
failed=0

while [ 1 ]
do
	sleep 60

	# We can only get dot in detail log if there isn't any error.
	error=`cat $detail | grep -E "fialed|timeout|Can't|failed|error"`
	if [ "$error" != "" ]
	then
        	# Wait for the access of the log.
        	failed=`expr $failed + 1`
        	echo -en  "\r[SRIO $value Test-Module]: `date +'%Y-%m-%d %T'` `echo -ne "\t\033[0;31;1m $failed\033[0m"`/`echo -ne "\033[0;32;1m ${success}\033[0m"` \b\n" 
	fi

	# Number of the lines is same with the times of success.
	lines_new=`cat $detail | awk '{print NR}' | tail -1`
	if [ "$lines_new" != "" ]
	then
		if [ $lines_new -gt $lines ]
		then
			success=`expr $success + 1`
        	echo -en  "\r[SRIO $value Test-Module]: `date +'%Y-%m-%d %T'` `echo -ne "\t\033[0;31;1m $failed\033[0m"`/`echo -ne "\033[0;32;1m ${success}\033[0m"` \b" 
			lines=$lines_new
			times=`expr $times + 1`
		fi
	fi

done
rm -rf $detail
echo -en  "\r[SRIO $value Test-Module]: `date +'%Y-%m-%d %T'` `echo -ne "\t\033[0;31;1m $failed\033[0m"`/`echo -ne "\033[0;32;1m Finished${success}\033[0m"` \b" 
killall srio_test > /dev/null 2>&1

exit 0
