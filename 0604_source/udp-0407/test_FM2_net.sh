#!/bin/bash
#
# $1 Thread_number
# $2 Begin number
# IPADDR
# $3
# $4
# TARGET ADDR
# $5
# $6
# Test times
# $7
# Test type
# $8 
#


#ETH0_MAC=00:11:22:33:44:55
#ETH1_MAC=00:12:33:44:55:66

MAC0=fm2-mac9
MAC1=fm2-mac10
ifconfig $MAC0 down
ifconfig $MAC1 down
tmp_detail1="/tmp/$MAC0-detailse"
tmp_detail10="/tmp/$MAC0-detailre"
tmp_detail2="/tmp/$MAC1-detailse"
tmp_detail20="/tmp/$MAC1-detailre"
detail="$MAC0-$MAC1"

#ifconfig $MAC0 hw ether $ETH0_MAC
#ifconfig $MAC1  hw ether $ETH1_MAC
ETH0_MAC=`ifconfig $MAC0 | grep "HWaddr"|awk -F ' ' '{print $5}'`
ETH1_MAC=`ifconfig $MAC1 | grep "HWaddr"|awk -F ' ' '{print $5}'`

ifconfig $MAC0 $3 netmask 255.255.255.0
ifconfig $MAC1 $4 netmask 255.255.255.0

#ip route table all
route add $5 dev fm2-mac9
route add $6 dev fm2-mac10

arp -i $MAC0 -s $5 $ETH1_MAC
arp -i $MAC1 -s $6 $ETH0_MAC 

iptables -t nat -F

iptables -t nat -A POSTROUTING -s $3 -d $5 -j SNAT --to-source $6  
iptables -t nat -A PREROUTING -s $6 -d $5 -j DNAT --to-destination $4 

iptables -t nat -A POSTROUTING -s $4 -d $6 -j SNAT --to-source $5  
iptables -t nat -A PREROUTING -s $5 -d $6 -j DNAT --to-destination $3
if [ $8 -eq 0 ]
then
    value=speed
else
    if [ $8 -eq 1 ]
    then 
        value=checkdata
    else
        exit 1;
    fi
fi

echo -ne "[FM2_MAC9 AND FM2_MAC10 $value Test-Module]: `date +'%Y-%m-%d %T'`\t0/$7"

ping -I $3 $5 -c 5 -W 5  >> $detail 
if [ $? -eq 0 ]
then
	ping -I $4 $6 -c 5 -W 5  >> $detail
	if [ $? -ne 0 ]
	then
        echo -en  "\r[FM2-MAC9 $value Test-Module]: `date +'%Y-%m-%d %T'` `echo -ne "\t\033[0;31;1merror\033[0m"`/$7 \b\n" 
        exit 1;
	fi	
else
    echo -en  "\r[FM2-MAC9 AND FM2-MAC10 $value  Test-Module]: `date +'%Y-%m-%d %T'` `echo -ne "\t\033[0;31;1merror\033[0m"`/$7 \b\n" 
    exit 1;
fi
#if [$8 -eq 1]
#then
#else
#fi

#./udp-receive --threadnum $1 --startcpu $2 --length 1460 --targetip $6 --sourceip $4 --port 8080  --check 2>&1 | tee -a $tmp_detail10>> $detail &
#./udp-send    --threadnum $1 --startcpu $2 --length 1460 --targetip $5 --sourceip $3 --port 8080  --check 2>&1 | tee -a $tmp_detail10>> $detail &
#number=$1+$2
./udp-receive --threadnum $1 --startcpu $2 --length 1460 --targetip $6 --sourceip $4 --port 8200  --check 2>&1 | tee -a $tmp_detail10 >> $detail &
./udp-send    --threadnum $1 --startcpu $2 --length 1460 --targetip $5 --sourceip $3 --port 8200  --check 2>&1 | tee -a $tmp_detail1 >> $detail &

number=$1+$2
./udp-receive --threadnum $1 --startcpu $number --length 1460 --targetip $5 --sourceip $3 --port 8300  --check 2>&1 | tee -a $tmp_detail20 >> $detail &
./udp-send    --threadnum $1 --startcpu $number --length 1460 --targetip $6 --sourceip $4 --port 8300  --check 2>&1 | tee -a $tmp_detail2 >> $detail &


times=0
lines=0
success=0
while [ $times -ne $7 ]
do
	sleep 10
    sync
	# We can only get dot in detail log if there isn't any error.
	error1=`cat $detail | grep -E "fialed|timeout|Can't|failed|error"`
	if [ "$error1" != "" ]
	then
        	# Wait for the access of the log.

        	failed=`expr $success + 1`
		    error="[error]"
        	echo -en  "\r[FM2-MAC9 AND FM2-MAC10 $value  Test-Module]: `date +'%Y-%m-%d %T'` `echo -ne "\t\033[0;31;1m${failed}${error}\033[0m"`/$7 \b\n" 
		    killall udp-send > /dev/null 2>&1
		    killall udp-receive > /dev/null 2>&1
            rm -rf $detail
            exit 1		
	fi
        
	# Number of the lines is same with the times of success.
	lines_new=`cat $detail | awk '{print NR}' | tail -1`
	if [ "$lines_new" != "" ]
	then
		if [ $lines_new -gt $lines ]
		then
			success=`expr $success + 1`
            echo -en "\r[FM2-MAC9 AND FM-MAC10 $value Test-Module]: `date +'%Y-%m-%d %T'` `echo -ne "\t\033[0;32;1m$success\033[0m"`/$7 \b" 

			lines=$lines_new
			times=`expr $times + 1`
		fi
	fi

done
rm -rf $detail

killall udp-send > /dev/null 2>&1
killall udp-receive > /dev/null 2>&1
echo -ne "\r[FM2-MAC9 AND FM2-MAC10 $value Test-Module]: `date +'%Y-%m-%d %T'``echo -ne "\t\033[0;32;1mFinished\033[0m"`/$7 \b\n" 

exit 0
