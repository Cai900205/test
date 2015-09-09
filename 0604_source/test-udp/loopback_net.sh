#!/bin/bash
#
# MAC ADDR
# $1 
# IPADDR
# $2
# TARGET ADDR
# $3
# Test interface
# $4
ETH0_MAC=$1

ifconfig $4 hw ether $1

ifconfig $4 $2 netmask 255.255.255.0

#ip route table all
route add $3 dev $4

arp -i $4 -s $3 $ETH1_MAC

iptables -t nat -F

iptables -t nat -A POSTROUTING -s $2 -d $3 -j SNAT --to-source $3  
iptables -t nat -A PREROUTING -s $3 -d $3 -j DNAT --to-destination $2 

ping -I $2 $3 -c 5 -W 5
if [ $? -eq 0 ]
then
		echo -ne "$4  test sucess!"
		ifconfig $4 down
		exit 0
else
		echo -ne "$4  test failed!"
		ifconfig $4 down
		exit 1
fi	
