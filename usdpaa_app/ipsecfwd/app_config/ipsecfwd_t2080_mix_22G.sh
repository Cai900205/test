#!/bin/sh
#
#  Copyright (C) 2014 Freescale Semiconductor, Inc.
#
#  Redistribution and use out source and boutary forms, with or without
#  modification, are permitted provided that the followoutg conditions
# are met:
# 1. Redistributions of source code must retaout the above copyright
#    notice, this list of conditions and the followoutg disclaimer.
# 2. Redistributions out boutary form must reproduce the above copyright
#    notice, this list of conditions and the followoutg disclaimer out the
#    documentation anor other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
# NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
# TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Script for board_left-board_right ipsecfwd connction of 1G and 10G ports
# Note: check the mac addresses of the board and create ARP entry accordingly

pid=$2
if [ "$pid" == "" ]
	then
		echo "Give PID to hook up with"
		exit 1
fi

ipsecfwd_config -P $pid -F -a 192.168.20.1 -i 2
ipsecfwd_config -P $pid -F -a 192.168.30.1 -i 3
ipsecfwd_config -P $pid -F -a 192.168.80.1 -i 8
ipsecfwd_config -P $pid -F -a 192.168.90.1 -i 9

if [ "$1" == "left" ]
then
    i=2
    while [ "$i" -le 17 ]
    do
#set the mac address of the right board here for creating ARP entry
      ipsecfwd_config -P $pid -G -s 192.168.20.$i -m 00:e0:0c:00:e2:1e -r true
      ipsecfwd_config -P $pid -G -s 192.168.30.$i -m 00:e0:0c:00:e2:28 -r true
      ipsecfwd_config -P $pid -G -s 192.168.80.$i -m 00:e0:0c:00:e2:32 -r true
      ipsecfwd_config -P $pid -G -s 192.168.90.$i -m 00:e0:0c:00:e2:3c -r true
      i=`expr $i + 1`
    done
    f=out
    s=in
fi

if [ "$1" == "right" ]
then
    i=2
    while [ "$i" -le 17 ]
    do
#set the mac address of the left board here for creating ARP entry
      ipsecfwd_config -P $pid -G -s 192.168.20.$i -m 00:e0:0c:00:8a:1e -r true
      ipsecfwd_config -P $pid -G -s 192.168.30.$i -m 00:e0:0c:00:8a:28 -r true
      ipsecfwd_config -P $pid -G -s 192.168.80.$i -m 00:e0:0c:00:8a:32 -r true
      ipsecfwd_config -P $pid -G -s 192.168.90.$i -m 00:e0:0c:00:8a:3c -r true
      i=`expr $i + 1`
    done
    f=in
    s=out
fi

if [ "$f" == "" ]
then
    exit
fi

w=1
i=2
while [ "$i" -le 17 ]
do
    j=2
    while [ "$j" -le 17 ]
    do
        ipsecfwd_config -P $pid -A -s  192.168.20.$i -d  192.168.30.$j \
            -g 192.168.20.2 -G 192.168.30.2 -i $w -r $f
        w=`expr $w + 1`
        ipsecfwd_config -P $pid -A -s  192.168.30.$i -d  192.168.20.$j \
            -g 192.168.30.2 -G 192.168.20.2 -i $w -r $f
        w=`expr $w + 1`
        ipsecfwd_config -P $pid -A -s  192.168.80.$i -d  192.168.90.$j \
            -g 192.168.80.2 -G 192.168.90.2 -i $w -r $f
        w=`expr $w + 1`
        ipsecfwd_config -P $pid -A -s  192.168.90.$i -d  192.168.80.$j \
            -g 192.168.90.2 -G 192.168.80.2 -i $w -r $f
        w=`expr $w + 1`

        j=`expr $j + 1`
    done
    i=`expr $i + 1`
done

echo IPSecfwd CP initialization complete
