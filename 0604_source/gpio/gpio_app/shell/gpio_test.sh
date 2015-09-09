#!/bin/bash

num=`lsmod | grep "gpio_copy"|wc -l`

if [ $num -eq 1 ]
then
    exit 0
else
    insmod /app/gpio_copy.ko
    mknod /dev/gpio_test c 240 0
fi
