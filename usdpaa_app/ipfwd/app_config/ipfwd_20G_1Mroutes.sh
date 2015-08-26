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

# Script for creating 1M routes
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
	for net in $1
	do
		ipfwd_config -P $pid -B -s 192.168.$net.2 -c $3 \
		-d 192.168.$2.2 -n $4 -g \
		192.168.$2.2
	done
}
ipfwd_config -P $pid -F -a 192.168.24.1 -i 5
ipfwd_config -P $pid -F -a 192.168.29.1 -i 11

ipfwd_config -P $pid -G -s 192.168.24.2 -m 02:00:c0:a8:3c:02 -r true
ipfwd_config -P $pid -G -s 192.168.29.2 -m 02:00:c0:a8:a0:02 -r true

			       # 1048456 total routes

net_pair_routes 24 29 254 254  # 254 * 254 = 64516
net_pair_routes 25 29 254 254  # 254 * 254 = 64516
net_pair_routes 26 29 254 254  # 254 * 254 = 64516
net_pair_routes 27 29 254 254  # 254 * 254 = 64516
net_pair_routes 28 29 254 254  # 254 * 254 = 64516
net_pair_routes 1 29 254 254  # 254 * 254 = 64516
net_pair_routes 18 29 254 254  # 254 * 254 = 64516
net_pair_routes 30 29 254 254  # 254 * 254 = 64516
net_pair_routes 31 29 90 90  # 90 * 90 = 8100
net_pair_routes 29 24 254 254  # 254 * 254 = 64516
net_pair_routes 2 24 254 254  # 254 * 254 = 64516
net_pair_routes 3 24 254 254  # 254 * 254 = 64516
net_pair_routes 4 24 254 254  # 254 * 254 = 64516
net_pair_routes 5 24 254 254  # 254 * 254 = 64516
net_pair_routes 6 24 254 254  # 254 * 254 = 64516
net_pair_routes 17 24 254 254  # 254 * 254 = 64516
net_pair_routes 19 24 254 254  # 254 * 254 = 64516
net_pair_routes 20 24 90 90  # 90 * 90 = 8100

echo IPFwd Route Creation complete
