#!/bin/bash



insmod /app/gpio_copy.ko
mknod /dev/gpio_test c 240 0
/app/gpio_app 1 3 5 100
