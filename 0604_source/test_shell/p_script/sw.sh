#!/bin/sh
#  [POWERPC-TEST]: RCW-ROM	Write and readback to check the user rom.
#		1. passes	Test times.
#		2. interval 	Seconds between two times of test.
#		3. i2c		I2c bus index.
#		4. slave	Slave address.

./bin/sw_read --passes $1 --interval $2 --i2c $3 --slave $4 2>&1
if [ $? -eq 0 ]
then
	echo "[SW_TEST-`date +'%Y-%m-%d %T'`]: SW test success."
	exit 0
else
	echo "[SW_TEST-`date +'%Y-%m-%d %T'`]: SW test failed."
	exit 1
fi
