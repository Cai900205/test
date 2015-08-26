#!/bin/sh
#
#  Copyright (C) 2013 Freescale Semiconductor, Inc.
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

pid=$1
if [ "$pid" == "" ]
	then
		echo "Give PID to hook up with"
		exit 1
fi


net_pair_routes()
{
	for dst in $(seq 2 $(expr $4 + 1))
	do
		ipsecfwd_config -P $pid -G -s 192.168.$1.$dst \
		-m 02:00:c0:a8:3c:02 -r true
	done

	for dst in $(seq 2 $(expr $4 + 1))
	do
		ipsecfwd_config -P $pid -G -s 192.168.$2.$dst \
		-m 02:00:c0:a8:a0:02 -r true
	done

	i=$5
	for net in $1 $2
	do
		for src in $(seq 2 $(expr $3 + 1))
		do
			for dst in $(seq 2 $(expr $4 + 1))
			do
				ipsecfwd_config -P $pid -A \
				-s 192.168.$net.$src \
				-d 192.168.$(expr $1 + $2 - $net).$dst \
				-g 192.168.$net.2 \
				-G 192.168.$net.1 \
				-i $i -r in

				i=$((i+1))
			done
		done
	done
}

case $(basename $0 .sh) in
	ipsecfwd_t2080_dec_42G)
		ipsecfwd_config -P $pid -F -a 192.168.40.1 -i 2
		ipsecfwd_config -P $pid -F -a 192.168.50.1 -i 8
		ipsecfwd_config -P $pid -F -a 192.168.60.1 -i 9
		ipsecfwd_config -P $pid -F -a 192.168.130.1 -i 3
		ipsecfwd_config -P $pid -F -a 192.168.140.1 -i 10
		ipsecfwd_config -P $pid -F -a 192.168.150.1 -i 11

							# 998
		net_pair_routes 40 130 7 7 1		# 2 * 7  * 7  =	98
		net_pair_routes 50 60 15 15 100		# 2 * 15 * 15 =	450
		net_pair_routes 140 150 15 15 600	# 2 * 15 * 15 = 450
		;;

esac
echo IPSecFwd CP initialization complete
