#!/bin/bash

fm1_mac10_old=0


while [ 1 ]
do
    fm1_mac10_new=`ifconfig fm1-mac10 | grep "RX packets"|awk -F ' ' '{print $2}'|awk -F ':' '{print $2}'`
    if [ $fm1_mac10_new -ne $fm1_mac10_old ]
    then
        fm1_mac10_old=$fm1_mac10_new-$fm1_mac10_old
	    /app/gpio_app 1 1 13 10
        fm1_mac10_old=$fm1_mac10_new
    else 
        if [ $fm1_mac10_new -ne 0 ]
        then
            /app/gpio_app 2 1 13 1
        fi 
    fi

done
