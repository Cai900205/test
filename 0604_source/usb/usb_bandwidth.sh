#!/bin/sh
#
passes=10
device=pp
time=10
interval=1
workers=1
bind=0
SIZE=10M
maxworker=1
show_version()
{
    echo -e "USB_TEST VERSION:POWERPC-USB_TEST-V1.00."
}
show_usage()
{
    echo -e "[option]:  --time        Test time for this program.(no use)"
    echo -e "           --passes      Test times for this program."
    echo -e "           --workers     Worker numbers of this program.(no use)"
    echo -e "           --bind        This program is or not bind.(no use)"
    echo -e "           --interval    Seconds to sleep between two times of test.(no use)"
    echo -e "           --device      Test device number."
    echo -e "           --help        The help of this program."
    echo -e "           --version     The version of this program."
}
#echo -e "$@"
temp=`getopt  -l help,version,workers:,device:,time:,passes:,interval:,bind  -- -n "$@"`
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
            passes=$2; shift 2
            ;;
        --device)
            device=$2; shift 2
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

if [ "$device"  ==  "pp" ]
then
    echo -e "device error!\n"
    exit -1
fi


bytes=0
suf=`echo $SIZE| sed 's/\([0-9]*\)\([GgMmKk]\)/\2/g'`
pre=`echo $SIZE| sed 's/\([0-9]*\)\([GgMmKk]\)/\1/g'`
if [ "$pre" == "" ]
then
	echo "The prefixof the parameter is invalid."
	exit 1
fi

case "$suf" in
"G" | "g")
	bytes=`expr $pre \* 1024 \* 1024 \* 1024`
	;;
"M" | "m")
	bytes=`expr $pre \* 1024 \* 1024`
	;;
"K" | "k")
	bytes=`expr $pre \* 1024`
	;;
"")
	bytes=$pre
	;;
*)
	echo "The suffix of the parameter is invalid."
	exit 1
	;;
esac

#
# Create the test file.
#
if [ ! -f test.file ]
then
dd if=/dev/urandom of=./test.file bs=$SIZE count=1 >> /dev/null 2>&1
fi
#
# mkfs.ext2
#
#
#mkfs.ext2 /dev/$3 > /dev/null
#if [ $? -eq 0  ]
#then
#    echo "Initialize the disk success."
#else
#    echo "Initialize the disk failed"
#fi
# Upload the file and download it in loop.
#
times=0
times_old=0
seconds=$((`date +%s`+2208988800))
temp_seconds=$((`date +%s`+2208988800))
while [ $times -ne $passes ]
do
    times=`expr $times + 1`
    mount /dev/$device /mnt
    cp test.file /mnt
    umount /mnt
	
    if [ $? -ne 0 ]
    then
	echo "[USB-Upload-`date +'%Y-%m-%d %T'`]: Upload the test file failed-$times"
	exit -1
    fi
#    tmp_current=$((`date +%s`+2208988800))
#    if [ `expr $tmp_current - $tmp_seconds` -gt $time ]
#    then
#        echo -e "[SPI-TEST-`date +'%Y-%m-%d %T'`]: $length-Bytes READ $time time success."
#        exit 0
#    fi
    current=$((`date +%s`+2208988800))
    if [ `expr $current - 5` -gt $seconds ]
    then
	interval=`expr $current - $seconds`
	packets=`expr $times - $times_old`
	performance=`awk 'BEGIN {printf '$bytes'*'$packets'*8.0/'$interval'/1000000.0*2}'`
	echo "[USB-Upload-`date +'%Y-%m-%d %T'`]: Transmit $packets packets in $interval seconds, performance: ${performance}Mbps"
	seconds=$((`date +%s`+2208988800))
	times_old=$times
    fi
done

download_times=0
download_times_old=0
download_seconds=$((`date +%s`+2208988800))
while [ $download_times -ne $passes ]
do
        download_times=`expr $download_times + 1`
        mount /dev/$device /mnt
        cp /mnt/test.file test.file.tile
        umount /mnt
	if [ $? -ne 0 ]
	then
	echo "[USB-Download-`date +'%Y-%m-%d %T'`]: Download the test file failed-$times"
	exit -1
	fi

	md5host=`md5sum test.file`
	md5tile=`md5sum test.file.tile`

	if [ "$md5tile" != "$md5tile" ]
	then
	echo "[USB-Download-`date +'%Y-%m-%d %T'`]: Check the date failed-$times"
	exit -1
	fi
	
	download_current=$((`date +%s`+2208988800))
	if [ `expr $download_current - 5` -gt $download_seconds ]
	then
	download_interval=`expr $download_current - $download_seconds`
	download_packets=`expr $download_times - $download_times_old`
	download_performance=`awk 'BEGIN {printf '$bytes'*'$download_packets'*8.0/'$download_interval'/1000000.0*2}'`
	echo "[USB-Download-`date +'%Y-%m-%d %T'`]: Transmit $download_packets packets in $download_interval seconds, performance: ${download_performance}Mbps"
	download_seconds=$((`date +%s`+2208988800))
	download_times_old=$download_times
	fi
	rm -rf test.file.tile
done
	rm -rf test.file
echo "[USB-TRANSMISSION-`date +'%Y-%m-%d %T'`]: $SIZE-Bytes transmission for $passes times success."
exit 0
