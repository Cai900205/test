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

# $1, $2	- Subnets as in 192.168.$1.* and 192.168.$2.*
# $3		- Number of sources
# $4		- Number of destinations
pid=$1
if [ "$pid" == "" ]
	then
		echo "Give PID to hook up with"
		exit 1
fi

net_pair_routes()
{
net=0
	while [ "$net" -le $5 ]
	do
		lpm_ipfwd_config -P $pid -B -c $2 -d $1.$net.24.2 -n $3 -g \
		192.168.$4.2
		net=`expr $net + 1`
	done
}

lpm_ipfwd_config -P $pid -F -a 192.168.20.1 -i 2
lpm_ipfwd_config -P $pid -F -a 192.168.30.1 -i 3
lpm_ipfwd_config -P $pid -F -a 192.168.80.1 -i 8
lpm_ipfwd_config -P $pid -F -a 192.168.90.1 -i 9

lpm_ipfwd_config -P $pid -G -s 192.168.20.2 -m 02:00:c0:a8:1e:02 -r true
lpm_ipfwd_config -P $pid -G -s 192.168.30.2 -m 02:00:c0:a8:28:02 -r true
lpm_ipfwd_config -P $pid -G -s 192.168.80.2 -m 02:00:c0:a8:32:02 -r true
lpm_ipfwd_config -P $pid -G -s 192.168.90.2 -m 02:00:c0:a8:3c:02 -r true

					# 1024
net_pair_routes 192 1 16 20 255		# 256
net_pair_routes 193 1 16 30 255		# 256
net_pair_routes 194 1 16 80 255		# 256
net_pair_routes 195 1 16 90 255		# 256

echo LPM-IPFwd Route Creation completed
