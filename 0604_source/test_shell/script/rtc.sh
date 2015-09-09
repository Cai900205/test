#!/bin/sh
#
# $1 interval
# $2 test times
./bin/rtc --version
./bin/rtc --interval $1 --passes $2
if [ $? -eq 0 ]
then
	echo "[RTC-TEST-`date +'%Y-%m-%d %T'`]: RTC test success."
	exit 0
else
	echo "[RTC-TEST-`date +'%Y-%m-%d %T'`]: RTC test failed."
	exit 1
fi
