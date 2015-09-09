#!/bin/bash
cp ./libaio/libaio* /usr/lib64/ 
cp fio /usr/local/bin/ 
test_path=/home/root/shannon-package-fival-v1/shannon-package-fival-v1
cd $test_path
./install
./ztool --dev=/dev/shannon_cdev mpt -u -c 2400G
./ztool --dev=/dev/shannon_cdev1 mpt -u -c 2400G
rmmod shannon-cdev
#insmod ./shannon702.ko
#./shannon-format -fy -s2400G /dev/scta
#./shannon-format -fy -s2400G /dev/sctb
