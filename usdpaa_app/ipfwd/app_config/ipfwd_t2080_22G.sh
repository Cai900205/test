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
	for net in $1 $2
	do
		ipfwd_config -P $pid -B -s 192.168.$net.2 -c $3 \
		-d 192.168.$(expr $1 + $2 - $net).2 -n $4 \
		-g 192.168.$(expr $1 + $2 - $net).2
	done
}

ipfwd_config -P $pid -F -a 192.168.20.1	-i 2
ipfwd_config -P $pid -F -a 192.168.30.1 -i 3
ipfwd_config -P $pid -F -a 192.168.80.1 -i 8
ipfwd_config -P $pid -F -a 192.168.90.1 -i 9

ipfwd_config -P $pid -G -s 192.168.20.2	-m 02:00:c0:a8:3c:02 -r true
ipfwd_config -P $pid -G -s 192.168.30.2 -m 02:00:c0:a8:82:02 -r true
ipfwd_config -P $pid -G -s 192.168.80.2 -m 02:00:c0:a8:8c:02 -r true
ipfwd_config -P $pid -G -s 192.168.90.2 -m 02:00:c0:a8:a0:02 -r true

				# 1022
net_pair_routes  20 30 7  7	# 2 *  7 *  7 =	 98
net_pair_routes	 80 90 21 22	# 2 * 21 * 22 = 924

echo IPFwd Route Creation completed
