#!/bin/bash

/app/gpio_app 0 1 26 1
while [ 1 ]
do
    /app/gpio_app 3 1 27 1
    if [ $? -eq 0 ]
    then
        usleep 10000
        /app/gpio_app 3 1 27 1
        if [ $? -eq 0 ]
        then
        #killall        
            /app/gpio_app 2 1 26 1
        fi
    fi
    usleep 10000
done
