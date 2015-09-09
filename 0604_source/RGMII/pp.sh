#!/bin/bash

show_version()
{
    echo -e "PWOERPC-SPI_TEST-V1.00."
}
show_usage()
{
    echo -e "[option]:  --time        Test time for this program."
    echo -e "           --passes      Test times for this program."
    echo -e "           --workers     Worker numbers of this program.(no use)"
    echo -e "           --bind        This program is or not bind.(no use)"
    echo -e "           --interval    Seconds to sleep between two times of test."
    echo -e "           --help        The help of this program."
    echo -e "           --version     The version of this program."
}
#echo -e "$@"
temp=`getopt  -l help,version,workers:,time:,passes:,interval:,bind  -- -n "$@"`
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
        --)
            shift
            ;;
        *)
            echo -e "\033[31mERROR:unknown options!\033[0m\n" && show_usage && exit 1
            ;;
        esac
done
