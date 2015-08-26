#!/bin/sh
#
#  Copyright (C) 2011 Freescale Semiconductor, Inc.
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

pid=$2
if [ "$pid" == "" ]
	then
		echo "Give PID to hook up with"
		exit 1
fi

ipsecfwd_config -P $pid -F -a 192.168.10.1 -i 0
ipsecfwd_config -P $pid -F -a 192.168.20.1 -i 1
ipsecfwd_config -P $pid -F -a 192.168.30.1 -i 2
ipsecfwd_config -P $pid -F -a 192.168.40.1 -i 3
ipsecfwd_config -P $pid -F -a 192.168.50.1 -i 4
ipsecfwd_config -P $pid -F -a 192.168.60.1 -i 8

if [ "$1" == "left" ]
then
    i=2
    while [ "$i" -le 11 ]
    do
#set the mac address of the right board here for creating ARP entry
	ipsecfwd_config -P $pid -G -s 192.168.10.$i -m 00:e0:0c:00:cb:00 -r true
	ipsecfwd_config -P $pid -G -s 192.168.20.$i -m 00:e0:0c:00:cb:01 -r true
	ipsecfwd_config -P $pid -G -s 192.168.30.$i -m 00:e0:0c:00:cb:02 -r true
	ipsecfwd_config -P $pid -G -s 192.168.40.$i -m 00:e0:0c:00:cb:03 -r true
	ipsecfwd_config -P $pid -G -s 192.168.50.$i -m 00:e0:0c:00:cb:04 -r true
	ipsecfwd_config -P $pid -G -s 192.168.60.$i -m 00:e0:0c:00:cb:05 -r true
        i=`expr $i + 1`
    done
fi

if [ "$1" == "right" ]
then
    i=2
    while [ "$i" -le 11 ]
    do
#set the mac address of the left board here for creating ARP entry
	ipsecfwd_config -P $pid -G -s 192.168.10.$i -m 00:e0:0c:00:e5:00 -r true
	ipsecfwd_config -P $pid -G -s 192.168.20.$i -m 00:e0:0c:00:e5:01 -r true
	ipsecfwd_config -P $pid -G -s 192.168.30.$i -m 00:e0:0c:00:e5:02 -r true
	ipsecfwd_config -P $pid -G -s 192.168.40.$i -m 00:e0:0c:00:e5:03 -r true
	ipsecfwd_config -P $pid -G -s 192.168.50.$i -m 00:e0:0c:00:e5:04 -r true
	ipsecfwd_config -P $pid -G -s 192.168.60.$i -m 00:e0:0c:00:e5:05 -r true
        i=`expr $i + 1`
    done
fi

w=1
i=2
while [ "$i" -le 11 ]
do
    j=2
    while [ "$j" -le 11 ]
    do
	if [ "$1" == "left" ]
	then
	   ipsecfwd_config -P $pid -A -s  192.168.60.$i -d  192.168.10.$j \
		-g 192.168.60.2 -G 192.168.10.2 -D 192.168.10.2 -i $w -r out
	   w=`expr $w + 1`
	   ipsecfwd_config -P $pid -A -s  192.168.60.$i -d  192.168.20.$j \
		-g 192.168.60.2 -G 192.168.20.2 -D 192.168.20.2 -i $w -r out
	   w=`expr $w + 1`
	   ipsecfwd_config -P $pid -A -s  192.168.60.$i -d  192.168.30.$j \
		-g 192.168.60.2 -G 192.168.30.2 -D 192.168.30.2 -i $w -r out
	   w=`expr $w + 1`
	   ipsecfwd_config -P $pid -A -s  192.168.60.$i -d  192.168.40.$j \
		-g 192.168.60.2 -G 192.168.40.2 -D 192.168.40.2 -i $w -r out
	   w=`expr $w + 1`
	   ipsecfwd_config -P $pid -A -s  192.168.60.$i -d  192.168.50.$j \
		-g 192.168.60.2 -G 192.168.50.2 -D 192.168.50.2 -i $w -r out
	   w=`expr $w + 1`
	   ipsecfwd_config -P $pid -A -s  192.168.10.$i -d  192.168.60.$j \
		-g 192.168.60.2 -G 192.168.10.2 -D 192.168.60.2 -i $w -r in
	   w=`expr $w + 1`
	   ipsecfwd_config -P $pid -A -s  192.168.20.$i -d  192.168.60.$j \
		-g 192.168.60.2 -G 192.168.20.2 -D 192.168.60.2 -i $w -r in
	   w=`expr $w + 1`
	   ipsecfwd_config -P $pid -A -s  192.168.30.$i -d  192.168.60.$j \
		-g 192.168.60.2 -G 192.168.30.2 -D 192.168.60.2 -i $w -r in
	   w=`expr $w + 1`
	   ipsecfwd_config -P $pid -A -s  192.168.40.$i -d  192.168.60.$j \
		-g 192.168.60.2 -G 192.168.40.2 -D 192.168.60.2 -i $w -r in
	   w=`expr $w + 1`
	   ipsecfwd_config -P $pid -A -s  192.168.50.$i -d  192.168.60.$j \
		-g 192.168.60.2 -G 192.168.50.2 -D 192.168.60.2 -i $w -r in
	   w=`expr $w + 1`
	fi
	if [ "$1" == "right" ]
	then
	   ipsecfwd_config -P $pid -A -s  192.168.60.$i -d  192.168.10.$j \
		-g 192.168.60.2 -G 192.168.10.2 -D 192.168.60.2 -i $w -r in
	   w=`expr $w + 1`
	   ipsecfwd_config -P $pid -A -s  192.168.60.$i -d  192.168.20.$j \
		-g 192.168.60.2 -G 192.168.20.2 -D 192.168.60.2 -i $w -r in
	   w=`expr $w + 1`
	   ipsecfwd_config -P $pid -A -s  192.168.60.$i -d  192.168.30.$j \
		-g 192.168.60.2 -G 192.168.30.2 -D 192.168.60.2 -i $w -r in
	   w=`expr $w + 1`
	   ipsecfwd_config -P $pid -A -s  192.168.60.$i -d  192.168.40.$j \
		-g 192.168.60.2 -G 192.168.40.2 -D 192.168.60.2 -i $w -r in
	   w=`expr $w + 1`
	   ipsecfwd_config -P $pid -A -s  192.168.60.$i -d  192.168.50.$j \
		-g 192.168.60.2 -G 192.168.50.2 -D 192.168.60.2 -i $w -r in
	   w=`expr $w + 1`
	   ipsecfwd_config -P $pid -A -s  192.168.10.$i -d  192.168.60.$j \
		-g 192.168.60.2 -G 192.168.10.2 -D 192.168.10.2 -i $w -r out
	   w=`expr $w + 1`
	   ipsecfwd_config -P $pid -A -s  192.168.20.$i -d  192.168.60.$j \
		-g 192.168.60.2 -G 192.168.20.2 -D 192.168.20.2 -i $w -r out
	   w=`expr $w + 1`
	   ipsecfwd_config -P $pid -A -s  192.168.30.$i -d  192.168.60.$j \
		-g 192.168.60.2 -G 192.168.30.2 -D 192.168.30.2 -i $w -r out
	   w=`expr $w + 1`
	   ipsecfwd_config -P $pid -A -s  192.168.40.$i -d  192.168.60.$j \
		-g 192.168.60.2 -G 192.168.40.2 -D 192.168.40.2 -i $w -r out
	   w=`expr $w + 1`
	   ipsecfwd_config -P $pid -A -s  192.168.50.$i -d  192.168.60.$j \
		-g 192.168.60.2 -G 192.168.50.2 -D 192.168.50.2 -i $w -r out
	   w=`expr $w + 1`
	fi

        j=`expr $j + 1`
    done
    i=`expr $i + 1`
done

echo IPSecFwd CP initialization complete
