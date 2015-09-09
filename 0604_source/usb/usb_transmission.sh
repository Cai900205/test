#!/bin/sh
#
# [$1]: Test times to check the function.
# [$2]: Type of the remote board.
# [$3]: System link of the device.
# [$4]: Size of the test file.
#


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
mkfs.ext2 /dev/$3 > /dev/null
if [ $? -eq 0  ]
then
    echo "Initialize the disk success."
else
    echo "Initialize the disk failed"
fi
#
# Upload the file and download it in loop.
#
times=0
while [ $times -ne $1 ]
do
	times=`expr $times + 1`
    mount /dev/$3 /mnt
    cp $2-$4-test.file /mnt
    umount /mnt
	if [ $? -ne 0 ]
	then
	echo "[USB-TRANSMISSION-`date +'%Y-%m-%d %T'`]: Upload the test file failed-$times"
	exit 1
	fi

    mount /dev/$3 /mnt
    cp /mnt/$2-$4-test.file test.file.tile
    umount /mnt
	if [ $? -ne 0 ]
	then
	echo "[USB-TRANSMISSION-`date +'%Y-%m-%d %T'`]: Download the test file failed-$times"
	exit 1
	fi

    mount /dev/$3 /mnt
    rm -rf  /mnt/$2-$4-test.file 
    umount /mnt

	md5host=`md5sum ./$2-$4-test.file | awk '{print $1}'`
	md5tile=`md5sum ./$2-$4-test.file.tile | awk '{print $1}'`

	if [ "$md5host" != "$md5tile" ]
	then
	echo "[USB-TRANSMISSION-`date +'%Y-%m-%d %T'`]: Check the date failed-$times"
	exit 1
	fi
	
	echo "[USB-TRANSMISSION-`date +'%Y-%m-%d %T'`]: $4-Bytes transmisson between the host and tile success."
	rm -rf $2-$4-test.file.tile
done
