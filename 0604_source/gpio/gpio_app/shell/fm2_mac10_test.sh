#!/bin/bash

fm1_mac9_old=0


while [ 1 ]
do
    fm1_mac9_new=`ifconfig fm2-mac10 | grep "RX packets"|awk -F ' ' '{print $2}'|awk -F ':' '{print $2}'`
    if [ $fm1_mac9_new -ne $fm1_mac9_old ]
    then
        fm1_mac9_old=$fm1_mac9_new-$fm1_mac9_old
	    /app/gpio_app 1 1 15 10
        fm1_mac9_old=$fm1_mac9_new
    else
        if [ $fm1_mac9_new -ne 0 ]
        then 
            /app/gpio_app 2 1 15 1
        fi
    fi

done
