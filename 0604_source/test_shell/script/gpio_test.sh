#!/bin/sh
#
#$1 test_cmd
#$2 test_gpionumber
#$3 test_bitnumber
#
#
flag=1;
./bin/gpio_test  --version 
./bin/gpio_test --cmd $1 --gpionum $2 --bitnum $3  --passes 10 
if [ $? -eq 0 ]
then
	echo "[GPIO-TEST`date +'%Y-%m-%d %T'`]: gpio test success."
	exit 0
else
	echo "[GPIO-TEST`date +'%Y-%m-%d %T'`]: gpio test failed."
	exit 1
fi
