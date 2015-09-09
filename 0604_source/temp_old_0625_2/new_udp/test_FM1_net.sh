#!/bin/bash
#
# $1 Thread_number
#



MAC0=fm1-mac9
MAC1=fm1-mac10

tmp_detail="/tmp/$MAC0-$MAC1-detail"
detail="/mnt/$MAC0-$MAC1"

ifconfig $MAC0 down
ifconfig $MAC1 down

./udp-receive1 --passes 0 --workers $1 --interface_s $MAC0 --interface_c $MAC1>&1 | tee -a $tmp_detail >> $detail &

echo -en  "[FM1-MAC9 AND FM1-MAC10 Test-Module]: `date +'%Y-%m-%d %T'`\t" 

times=0
lines=0
success=0
failed=0
while [ 1 ]
do
	sleep 60
	error1=`cat $tmp_detail | grep -E "fialed|timeout|Can't|failed|error"`
	if [ "$error1" != "" ]
	then
        	# Wait for the access of the log.

        	failed=`expr $failed + 1`
        	echo -en  "\r[FM1-MAC9 AND FM1-MAC10 Test-Module]: `date +'%Y-%m-%d %T'` `echo -ne "\t\033[0;31;1m${failed}\033[0m"`/`echo -ne "\033[0;32;1m${success}\033[0m"` \b\n" 
	        killall udp-receive1 
            rm -rf $tmp_detail
            sleep 1
            ./udp-receive1 --passes 0 --workers $1 --interface_s $MAC0 --interface_c $MAC1>&1 | tee -a $tmp_detail >> $detail &
    fi
        
	# Number of the lines is same with the times of success.
	lines_new=`cat $tmp_detail | awk '{print NR}' | tail -1`
	if [ "$lines_new" != "" ]
	then
		if [ $lines_new -gt $lines ]
		then
			success=`expr $success + 1`
        	echo -en  "\r[FM1-MAC9 AND FM1-MAC10 Test-Module]: `date +'%Y-%m-%d %T'` `echo -ne "\t\033[0;31;1m${failed}\033[0m"`/`echo -ne "\033[0;32;1m${success}\033[0m"` \b" 
			lines=$lines_new
			times=`expr $times + 1`
		fi
	fi

done
rm -rf $tmp_detail
killall udp-receive1 > /dev/null 2>&1
echo -en  "\r[FM1-MAC9 AND FM1-MAC10 Test-Module]: `date +'%Y-%m-%d %T'` `echo -ne "\t\033[0;31;1m${failed}\033[0m"`/`echo -ne "\033[0;32;1m${success}\033[0m"` \b\n" 
exit 0
