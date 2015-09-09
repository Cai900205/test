#!/bin/bash
#
# $1 port number
# $2 Begin cpu
# test type
# $3 
# Test times
# $4

cpu=SRIO

if [ $3 -eq 1 ]
then
    value=link
else 
    if [ $3 -eq 2 ]
    then
        value=checkdata
    else    
        if [ $3 -eq 3 ]
        then
            value=speed
        else
            exit 1
        fi
    fi
fi
tmp_detail=/mnt/SRIO-$value-detail
detail=/tmp/SRIO-$value

echo -ne "[SRIO $value Test-Module]: `date +'%Y-%m-%d %T'`\t0/$4"

./srio_check --port $1  --type swrite --test_type $3 --startcpu $2   2>&1 | tee -a $tmp_detail  >> $detail &

times=0
lines=0

while [ 1 ]
do
	sleep 60

	# We can only get dot in detail log if there isn't any error.
	error=`cat $detail | grep -E "fialed|timeout|Can't|failed|error"`
	if [ "$error" != "" ]
	then
        	# Wait for the access of the log.

        	failed=`expr $success + 1`
		    error="[error]"
        	echo -en  "\r[SRIO $value Test-Module]: `date +'%Y-%m-%d %T'` `echo -ne "\t\033[0;31;1m${failed}${error}\033[0m"`/$4 \b\n" 
#		    killall srio_check > /dev/null 2>&1
#            rm -rf $detail
#            exit 1		
	fi

	# Number of the lines is same with the times of success.
	lines_new=`cat $detail | awk '{print NR}' | tail -1`
	if [ "$lines_new" != "" ]
	then
		if [ $lines_new -gt $lines ]
		then
			success=`expr $success + 1`
            echo -en "\r[SRIO $value Test-Module]: `date +'%Y-%m-%d %T'` `echo -ne "\t\033[0;32;1m$success\033[0m"`/$4 \b" 

			lines=$lines_new
			times=`expr $times + 1`
		fi
	fi

done
rm -rf $detail

echo -ne "\r[SRIO $value Test-Module]: `date +'%Y-%m-%d %T'``echo -ne "\t\033[0;32;1mFinished\033[0m"`/$4 \b\n" 
killall srio_speed > /dev/null 2>&1

exit 0
