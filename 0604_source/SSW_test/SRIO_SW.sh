#!/bin/sh
#
# $1 test times
passes=`expr $1 \* 1000`;
echo -ne  "test times:$passes"
./srio_testc --passes $passes --test_type 2 --data_type swrite 
if [ $? -eq 0 ]
then
	echo "[SRIO-SW-TEST-`date +'%Y-%m-%d %T'`]: SRIO-SW test success."
	exit 0
else
	echo "[SRIO-SW-TEST-`date +'%Y-%m-%d %T'`]: SRIO-SW test failed."
	exit 1
fi
