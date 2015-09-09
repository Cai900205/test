#!/bin/bash

./bin/srio_link --port 0 --version 

./bin/srio_link  --port 0 --type swrite --test_type 1 --startcpu 2

if [ $? -eq 0 ]
then
	echo "[SRIO-LINK-`date +'%Y-%m-%d %T'`]: Check the srio link status success."
	exit 0
else
	echo "[SRIO-LINK-`date +'%Y-%m-%d %T'`]: Check the srio link status failed."
	exit 1
fi

