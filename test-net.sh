#!/bin/bash

ETH0_MAC=00:11:22:33:44:55
ETH1_MAC=00:11:22:33:44:66

ifconfig fm1-mac5 hw ether $ETH0_MAC
ifconfig fm2-mac5 hw ether $ETH1_MAC

ifconfig fm1-mac5 192.168.1.1 netmask 255.255.255.0
ifconfig fm2-mac5 192.168.1.2 netmask 255.255.255.0

#ip route table all
route add 192.168.1.11 dev fm1-mac5
route add 192.168.1.22 dev fm2-mac5

arp -i fm1-mac5 -s 192.168.1.11 $ETH1_MAC
arp -i fm2-mac5 -s 192.168.1.22 $ETH0_MAC 

iptables -t nat -F

iptables -t nat -A POSTROUTING -s 192.168.1.1 -d 192.168.1.11 -j SNAT --to-source 192.168.1.22  
iptables -t nat -A PREROUTING -s 192.168.1.22 -d 192.168.1.11 -j DNAT --to-destination 192.168.1.2 

iptables -t nat -A POSTROUTING -s 192.168.1.2 -d 192.168.1.22 -j SNAT --to-source 192.168.1.11  
iptables -t nat -A PREROUTING -s 192.168.1.11 -d 192.168.1.22 -j DNAT --to-destination 192.168.1.1 

