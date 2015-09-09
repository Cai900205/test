#!/bin/sh
#
# $1 test size
# $2 test times
cd ./bin/memtester-4.3.0/
make && make install
cd -
memtester $1 $2
if [ $? -eq 0 ]
then
	echo "[MEMTEST-`date +'%Y-%m-%d %T'`]: memory test success."
	exit 0
else
	echo "[MEMTEST-`date +'%Y-%m-%d %T'`]: memory test failed."
	exit 1
fi
