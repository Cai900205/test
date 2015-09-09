#!/bin/sh
#
# [$1]: Test times to check the function.
# [$2]: Type of the remote board.
# [$3]: System link device.
# [$4]: Size of the test file.
#
echo -ne "POWERPC-USB_TEST-V1.00"
bytes=0
suf=`echo $4| sed 's/\([0-9]*\)\([GgMmKk]\)/\2/g'`
pre=`echo $4| sed 's/\([0-9]*\)\([GgMmKk]\)/\1/g'`
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
if [ ! -f $2-$4-test.file ]
then
dd if=/dev/urandom of=./$2-$4-test.file bs=$4 count=1 >> /dev/null 2>&1
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
while [ $times -ne $1 ]
do
    times=`expr $times + 1`
    mount /dev/$3 /mnt
    cp $2-$4-test.file /mnt
    umount /mnt
	
    if [ $? -ne 0 ]
    then
	echo "[USB-Upload-`date +'%Y-%m-%d %T'`]: Upload the test file failed-$times"
	exit 1
    fi
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
while [ $download_times -ne $1 ]
do
        download_times=`expr $download_times + 1`
        mount /dev/$3 /mnt
        cp /mnt/$2-$4-test.file $2-$4-test.file.tile
        umount /mnt
	if [ $? -ne 0 ]
	then
	echo "[USB-Download-`date +'%Y-%m-%d %T'`]: Download the test file failed-$times"
	exit 1
	fi

	md5host=`md5sum ./$2-$4-test.file`
	md5tile=`md5sum ./$2-$4-test.file.tile`

	if [ "$md5tile" != "$md5tile" ]
	then
	echo "[USB-Download-`date +'%Y-%m-%d %T'`]: Check the date failed-$times"
	exit 1
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

	rm -rf $2-$4-test.file.tile
done
	rm -rf $2-$4-test.file
echo "[USB-TRANSMISSION-`date +'%Y-%m-%d %T'`]: $4-Bytes transmission for $1 times success."
exit 0
