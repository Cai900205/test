#!/bin/bash

i=$1
shift

if [ -z "$i" ]; then
    echo "Usage:"
    echo "      $0 <interface>"
    exit 1
fi

# bring up the interface
/sbin/ifup $i

if [ -f /etc/redhat-release ]; then
    NETWORK_CONF_DIR="/etc/sysconfig/network-scripts"
elif [ -f /etc/rocks-release ]; then
    NETWORK_CONF_DIR="/etc/sysconfig/network-scripts"
elif [ -f /etc/SuSE-release ]; then
    NETWORK_CONF_DIR="/etc/sysconfig/network"
else
    if [ -d /etc/sysconfig/network-scripts ]; then
        NETWORK_CONF_DIR="/etc/sysconfig/network-scripts"
    elif [ -d /etc/sysconfig/network ]; then
        NETWORK_CONF_DIR="/etc/sysconfig/network"
    fi
fi

# Bring up child interfaces if configured.
for child_conf in $(/bin/ls -1 ${NETWORK_CONF_DIR}/ifcfg-${i}.???? 2> /dev/null)
do
    ch_i=${child_conf##*-}
    # Skip saved interfaces rpmsave and rpmnew
    if (echo $ch_i | grep rpm > /dev/null 2>&1); then
        continue
    fi

    if [ ! -f /sys/class/net/${i}/create_child ]; then
        continue
    fi

    pkey=0x${ch_i##*.}
    if [ ! -e /sys/class/net/${i}.${ch_i##*.} ] ; then
        echo $pkey > /sys/class/net/${i}/create_child
        /sbin/ifup $ch_i
    fi
done
