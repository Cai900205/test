#! /bin/sh 

times=10
time=10
interval=1
workers=1
bind=0
mtd=mtd0
length=20
offset=0
maxworkers=1

show_version()
{
    echo -e "SPI_TEST VERSION:POWERPC-SPI_TEST-V1.00"
}
show_usage()
{
    echo -e "[option]:  --time        Test time for this program."
    echo -e "           --passes      Test times for this program."
    echo -e "           --workers     Worker numbers of this program.(no use)"
    echo -e "           --bind        This program is or not bind.(no use)"
    echo -e "           --interval    Seconds to sleep between two times of test."
    echo -e "           --device      Test device number."
    echo -e "           --length      The length of test data."
    echo -e "           --help        The help of this program."
    echo -e "           --version     The version of this program."
}
#echo -e "$@"
temp=`getopt  -l help,version,length:,device:,workers:,time:,passes:,interval:,bind  -- -n "$@"`
[ $? != 0 ]&&echo -e "\033[31mERROR:unknown options!\033[0m" && show_usage && exit -1
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
            times=$2; shift 2
            ;;
        --length)
            length=$2; shift 2
            ;;
        --device)
            mtd=$2; shift 2
            ;;
        --interval)
            interval=$2; shift 2
            ;;
        --)
            shift
            ;;
        *)
            echo -e "\033[31mERROR:unknown options!\033[0m" && show_usage && exit -1
            ;;
        esac
done
if [ $workers -ne 1 ]
then
    echo -e "\033[31mERROR:workers only one thread!\033[0m" 
    exit -1
fi
temptimes=0;
seconds=$((`date +%s`+2208988800))
while [ $temptimes -ne $times ]
do
    temptimes=`expr $temptimes + 1`
    if [ "$temptimes" -eq 1 ]
    then
        mtd_debug read /dev/$mtd $offset $length test.file
        mv test.file begin.file 
    else
        mtd_debug read /dev/$mtd $offset $length test.file
        diff begin.file test.file
        if [ $? -ne 0 ]
        then 
            echo -e "\033[31mERROR:SPI_TEST Data error!\033[0m" 
            exit -1
        fi
        rm -rf test.file
    fi
    current=$((`date +%s`+2208988800))
    if [ `expr $current - $seconds` -gt $time ]
    then
        echo -e "[SPI-TEST-`date +'%Y-%m-%d %T'`]: $length-Bytes READ $time time success."
        rm -rf begin.file
        exit 0
    fi
    sleep $interval
done
echo -e "[SPI-TEST-`date +'%Y-%m-%d %T'`]: $length-Bytes READ $times times success."
rm -rf begin.file
exit 0
