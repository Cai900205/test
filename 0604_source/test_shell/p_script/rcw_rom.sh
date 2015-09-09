#!/bin/sh
#  [POWERPC-TEST]: RCW-ROM	Write and readback to check the user rom.
#		1. passes	Test times.
#		2. interval 	Seconds between two times of test.
#               3. offset       offset address
#		4. i2c		I2c bus index.
#		5. slave	Slave address.
#		6. count	count.
#               7. word_offset  16bit or 8bit read
if [ $7 -eq 1 ]
then
./bin/rom_read --passes $1 --interval $2 --offset $3 --i2c $4 --slave $5 --count $6 --word_offset 
else
./bin/rom_read --passes $1 --interval $2 --offset $3 --i2c $4 --slave $5 --count $6 
fi
if [ $? -eq 0 ]
then
	echo "[RCW_ROM-`date +'%Y-%m-%d %T'`]: RCW_ROM test success."
	exit 0
else
	echo "[RCW_ROM-`date +'%Y-%m-%d %T'`]: RCW_ROM test failed."
	exit 1
fi
