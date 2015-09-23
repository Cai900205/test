#!/bin/bash

i=$1
shift

if [ -z "$i" ]; then
    echo "Usage:"
    echo "      $0 <interface>"
    exit 1
fi

# bring up the interface
/sbin/ifup --force $i

# Bring up child interfaces if configured.
while read _line
do
    if [[ "$_line" =~ $i\.[0-9]* && "$_line" =~ "iface" ]]
    then
        ifname=$(echo $_line | cut -f2 -d" ")

        if [ ! -f /sys/class/net/$i/create_child ]; then
            continue
        fi

        pkey=0x${ifname##*.}

        if [ ! -e /sys/class/net/$ifname ] ; then
            {
            local retry_cnt=0
            echo $pkey > /sys/class/net/$i/create_child
            while [[ $? -ne 0 && $retry_cnt -lt 10 ]]; do
                sleep 1
                let retry_cnt++
                echo $pkey > /sys/class/net/$i/create_child
            done
            } > /dev/null 2>&1
            /sbin/ifup --force $ifname
        fi
    fi
done < "/etc/network/interfaces"
