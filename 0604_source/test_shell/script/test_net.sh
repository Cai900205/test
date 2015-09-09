#!/bin/bash
#
# Test interface
# $1 
# $2
# IPADDR
# $3
# $4
# TARGET ADDR
# $5
# $6
#

echo -ne "POWERPC_NET_TEST-V1.00"
MAC0=$1
MAC1=$2


ETH0_MAC=`ifconfig $MAC0 | grep "HWaddr"|awk -F ' ' '{print $5}'`
ETH1_MAC=`ifconfig $MAC1 | grep "HWaddr"|awk -F ' ' '{print $5}'`


ifconfig $1 $3 netmask 255.255.255.0
ifconfig $2 $4 netmask 255.255.255.0

#ip route table all
route add $5 dev $1
route add $6 dev $2

arp -i $1 -s $5 $ETH1_MAC
arp -i $2 -s $6 $ETH0_MAC 

iptables -t nat -F

iptables -t nat -A POSTROUTING -s $3 -d $5 -j SNAT --to-source $6  
iptables -t nat -A PREROUTING -s $6 -d $5 -j DNAT --to-destination $4 

iptables -t nat -A POSTROUTING -s $4 -d $6 -j SNAT --to-source $5  
iptables -t nat -A PREROUTING -s $5 -d $6 -j DNAT --to-destination $3

ping -I $3 $5 -c 5 -W 5
if [ $? -eq 0 ]
then
	ping -I $4 $6 -c 5 -W 5
	if [ $? -eq 0 ]
	then
		echo -ne "$1 AND $2 test sucess!"
		ifconfig $1 down
		ifconfig $2 down
		exit 0
	else
		echo -ne "$1 test sucess!,$2 test failed!"
		ifconfig $1 down
		ifconfig $2 down
		exit 1
	fi	
else
	echo -ne "$1 test failed!\n"
	ifconfig $1 down
	ifconfig $2 down
	exit 1	
fi
