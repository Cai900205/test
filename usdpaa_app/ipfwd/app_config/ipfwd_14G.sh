#!/bin/sh
#
#  Copyright (C) 2009, 2011 Freescale Semiconductor, Inc.
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

net_directional_routes()
{
		ipfwd_config -P $pid -B -s 192.168.$1.2 -c $3 \
		-d 192.168.$2.2 -n $4 -g 192.168.$2.2
}

case $(basename $0 .sh) in
   ipfwd_t1040_8G)
	ipfwd_config -P $pid -F -a 192.168.10.1 -i 0
	ipfwd_config -P $pid -F -a 192.168.20.1 -i 1
	ipfwd_config -P $pid -F -a 192.168.30.1 -i 2
	ipfwd_config -P $pid -F -a 192.168.40.1 -i 3
	ipfwd_config -P $pid -F -a 192.168.50.1 -i 4

	ipfwd_config -P $pid -G -s 192.168.10.2 -m 02:00:c0:a8:0a:02 -r true
	ipfwd_config -P $pid -G -s 192.168.20.2 -m 02:00:c0:a8:14:02 -r true
	ipfwd_config -P $pid -G -s 192.168.30.2 -m 02:00:c0:a8:1e:02 -r true
	ipfwd_config -P $pid -G -s 192.168.40.2 -m 02:00:c0:a8:28:02 -r true
	ipfwd_config -P $pid -G -s 192.168.50.2 -m 02:00:c0:a8:32:02 -r true

						# 1280
	net_pair_routes 10 20 16 16		# 2 * 16 * 16 = 512
	net_directional_routes 30 40 16 16	# 16 * 16 = 256
	net_directional_routes 40 50 16 16	# 16 * 16 = 256
	net_directional_routes 50 30 16 16	# 16 * 16 = 256
	;;

   ipfwd_22G)
	ipfwd_config -P $pid -F -a 192.168.60.1	 -i 8
	ipfwd_config -P $pid -F -a 192.168.130.1 -i 12
	ipfwd_config -P $pid -F -a 192.168.140.1 -i 13
	ipfwd_config -P $pid -F -a 192.168.160.1 -i 18

	ipfwd_config -P $pid -G -s 192.168.60.2	 -m 02:00:c0:a8:3c:02 -r true
	ipfwd_config -P $pid -G -s 192.168.130.2 -m 02:00:c0:a8:82:02 -r true
	ipfwd_config -P $pid -G -s 192.168.140.2 -m 02:00:c0:a8:8c:02 -r true
	ipfwd_config -P $pid -G -s 192.168.160.2 -m 02:00:c0:a8:a0:02 -r true

					# 1022
	net_pair_routes 130 140	 7  7	# 2 *  7 *  7 =	 98
	net_pair_routes	 60 160 21 22	# 2 * 21 * 22 = 924
	;;

   ipfwd_20G)
	ipfwd_config -P $pid -F -a 192.168.60.1	 -i 8
	ipfwd_config -P $pid -F -a 192.168.160.1 -i 18

	ipfwd_config -P $pid -G -s 192.168.60.2	 -m 02:00:c0:a8:3c:02 -r true
	ipfwd_config -P $pid -G -s 192.168.160.2 -m 02:00:c0:a8:a0:02 -r true

					# 1012
	net_pair_routes 60 160 22 23	# 2 * 22 * 23 = 1012
	;;

   ipfwd_14G)
	ipfwd_config -P $pid -F -a 192.168.10.1 -i 0
	ipfwd_config -P $pid -F -a 192.168.20.1 -i 1
	ipfwd_config -P $pid -F -a 192.168.40.1 -i 3
	ipfwd_config -P $pid -F -a 192.168.50.1 -i 4
	ipfwd_config -P $pid -F -a 192.168.60.1	-i 8

	ipfwd_config -P $pid -G -s 192.168.10.2 -m 02:00:c0:a8:0a:02 -r true
	ipfwd_config -P $pid -G -s 192.168.20.2 -m 02:00:c0:a8:14:02 -r true
	ipfwd_config -P $pid -G -s 192.168.40.2 -m 02:00:c0:a8:28:02 -r true
	ipfwd_config -P $pid -G -s 192.168.50.2 -m 02:00:c0:a8:32:02 -r true
	ipfwd_config -P $pid -G -s 192.168.60.2 -m 02:00:c0:a8:3c:02 -r true

					# 1000
	net_pair_routes 10 60 10 10	# 2 * 10 * 10 = 200
	net_pair_routes 20 60 10 10	# 2 * 10 * 10 = 200
	net_pair_routes 40 60 10 10	# 2 * 10 * 10 = 200
	net_pair_routes 50 60 10 10	# 2 * 10 * 10 = 200
	;;

   ipfwd_40G)
	ipfwd_config -P $pid -F -a 192.168.50.1 -i 8
	ipfwd_config -P $pid -F -a 192.168.60.1 -i 9
	ipfwd_config -P $pid -F -a 192.168.140.1 -i 18
	ipfwd_config -P $pid -F -a 192.168.150.1 -i 19

	ipfwd_config -P $pid -G -s 192.168.50.2 -m 02:00:c0:a8:0a:02 -r true
	ipfwd_config -P $pid -G -s 192.168.60.2 -m 02:00:c0:a8:14:02 -r true
	ipfwd_config -P $pid -G -s 192.168.140.2 -m 02:00:c0:a8:28:02 -r true
	ipfwd_config -P $pid -G -s 192.168.150.2 -m 02:00:c0:a8:32:02 -r true

					# 1024
	net_pair_routes 50 60 16 16	# 2 * 16 * 16 = 512
	net_pair_routes 140 150 16 16	# 2 * 16 * 16 = 512
	;;

   ipfwd_42G)
	ipfwd_config -P $pid -F -a 192.168.40.1 -i 4
	ipfwd_config -P $pid -F -a 192.168.50.1 -i 8
	ipfwd_config -P $pid -F -a 192.168.60.1 -i 9
	ipfwd_config -P $pid -F -a 192.168.130.1 -i 14
	ipfwd_config -P $pid -F -a 192.168.140.1 -i 18
	ipfwd_config -P $pid -F -a 192.168.150.1 -i 19

	ipfwd_config -P $pid -G -s 192.168.40.2 -m 02:00:c0:a8:da:02 -r true
	ipfwd_config -P $pid -G -s 192.168.50.2 -m 02:00:c0:a8:0a:02 -r true
	ipfwd_config -P $pid -G -s 192.168.60.2 -m 02:00:c0:a8:14:02 -r true
	ipfwd_config -P $pid -G -s 192.168.130.2 -m 02:00:c0:a8:a8:02 -r true
	ipfwd_config -P $pid -G -s 192.168.140.2 -m 02:00:c0:a8:28:02 -r true
	ipfwd_config -P $pid -G -s 192.168.150.2 -m 02:00:c0:a8:32:02 -r true

					# 998
	net_pair_routes 40 130 7 7	# 2 *  7 *  7 = 98
	net_pair_routes 50 60 15 15	# 2 * 15 * 15 = 450
	net_pair_routes 140 150 15 15	# 2 * 15 * 15 = 450
	;;

esac
echo IPFwd Route Creation completed
