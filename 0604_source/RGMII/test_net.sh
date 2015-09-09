#!/bin/bash
#
interface1=fm1-mac5
interface2=fm2-mac5
ip1=192.168.10.1
test_ip1=192.168.10.10
ip2=192.168.10.3
test_ip2=192.168.10.20
passes=10
time=10
interval=1
workers=1
bind=0

show_version()
{
    echo -e "POWERPC-RGMII_TEST-V1.00"
}
show_usage()
{
    echo -e "[option]:  --time        Test time for this program(no use)."
    echo -e "           --passes      Test times for this program(no use)."
    echo -e "           --workers     Worker numbers of this program.(no use)"
    echo -e "           --bind        This program is or not bind.(no use)"
    echo -e "           --interval    Seconds to sleep between two times of test(no use)."
    echo -e "           --interface1  The first interface of this program."
    echo -e "           --interface2  The Second interface of this program."
    echo -e "           --ip1         The ipaddr of the first interface."
    echo -e "           --ip2         The ipaddr of the first interface."
    echo -e "           --test_ip1    The test ipaddr of the first interface."
    echo -e "           --test_ip2    The test ipaddr of the first interface."
    echo -e "           --help        The help of this program."
    echo -e "           --version     The version of this program."
}
#echo -e "$@"
temp=`getopt  -l help,version,workers:,time:,passes:,interval:,ip1:,ip2:,interface1:,interface2:,test_ip1:,test_ip2:,bind  -- -n "$@"`
[ $? != 0 ]&&echo -e "\033[31mERROR:unknown options!\033[0m\n" && show_usage && exit 1
#echo -e "$temp"
eval set -- "$temp"
while true ; 
do 
    [ -z "$1" ] && break;
    case "$1" in 
        --help)
            show_usage;exit 0
            ;;
        --version)
            show_version;exit 0
            ;;
        --bind)
            bind=1; shift
            ;;
        --workers)
            workers=$2; shift 2
            ;;
        --time)
            time=$2; shift 2
            ;;
        --passes)
            passes=$2; shift 2
            ;;
        --interval)
            interval=$2; shift 2
            ;;
        --interface1)
            interface1=$2; shift 2
            ;;
        --interface2)
            interface2=$2; shift 2
            ;;
        --ip1)
            ip1=$2; shift 2
            ;;
        --ip2)
            ip2=$2; shift 2
            ;;
        --test_ip1)
            test_ip1=$2; shift 2
            ;;
        --test_ip2)
            test_ip2=$2; shift 2
            ;;
        --)
            shift
            ;;
        *)
            echo -e "\033[31mERROR:unknown options!\033[0m\n" && show_usage && exit 1
            ;;
        esac
done
MAC0=$interface1
MAC1=$interface2


ETH0_MAC=`ifconfig $MAC0 | grep "HWaddr"|awk -F ' ' '{print $5}'`
ETH1_MAC=`ifconfig $MAC1 | grep "HWaddr"|awk -F ' ' '{print $5}'`


ifconfig $interface1 $ip1 netmask 255.255.255.0
ifconfig $interface2 $ip2 netmask 255.255.255.0

#ip route table all
route add $test_ip1 dev $interface1
route add $test_ip2 dev $interface2

arp -i $interface1 -s $test_ip1 $ETH1_MAC
arp -i $interface2 -s $test_ip2 $ETH0_MAC 

iptables -t nat -F

iptables -t nat -A POSTROUTING -s $ip1 -d $test_ip1 -j SNAT --to-source $test_ip2  
iptables -t nat -A PREROUTING -s $test_ip2 -d $test_ip1 -j DNAT --to-destination $ip2 

iptables -t nat -A POSTROUTING -s $ip2 -d $test_ip2 -j SNAT --to-source $test_ip1  
iptables -t nat -A PREROUTING -s $test_ip1 -d $test_ip2 -j DNAT --to-destination $ip1

ping -I $ip1 $test_ip1 -c 5 -W 5
if [ $? -eq 0 ]
then
	ping -I $ip2 $test_ip2 -c 5 -W 5
	if [ $? -eq 0 ]
	then
		echo -ne "$interface1 AND $interface2 test sucess!"
		ifconfig $interface1 down
		ifconfig $interface2 down
		exit 0
	else
		echo -ne "$interface1 test sucess!,$interface2 test failed!"
		ifconfig $interface1 down
		ifconfig $interface2 down
		exit -1
	fi	
else
	echo -ne "$interface1 test failed!\n"
	ifconfig $interface1 down
	ifconfig $interface2 down
	exit -1	
fi
