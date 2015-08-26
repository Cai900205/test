/* Copyright (c) 2014 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *	 notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *	 notice, this list of conditions and the following disclaimer in the
 *	 documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *	 names of its contributors may be used to endorse or promote products
 *	 derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __IPSEC_SIZING_H
#define __IPSEC_SIZING_H

/* These valuses must be set according to PCD model*/
#if defined P4080
#define NUM_SETS		2
#define NUM_WAYS		8
#elif defined B4860
#define NUM_SETS		8
#define NUM_WAYS		8
#elif defined B4420
#define NUM_SETS		8
#define NUM_WAYS		8
#else
#define NUM_SETS		2
#define NUM_WAYS		8
#endif

#define SETS			0
#define WAYS			1

#define IN_SA_PCD_HASH_OFF	0
#define IPSEC_START_IN_FLOW_ID	0

/* Number of sets and ways per inbound pre-sec CC node type */
#define IPSEC_IN_SA_HASH_ENTRIES { \
	[DPA_IPSEC_SA_IPV4][SETS] = NUM_SETS, \
	[DPA_IPSEC_SA_IPV4][WAYS] = NUM_WAYS, \
	[DPA_IPSEC_SA_IPV4_NATT][SETS] = NUM_SETS, \
	[DPA_IPSEC_SA_IPV4_NATT][WAYS] = NUM_WAYS, \
	[DPA_IPSEC_SA_IPV6][SETS] = NUM_SETS, \
	[DPA_IPSEC_SA_IPV6][WAYS] = NUM_WAYS, \
}

/* Max number of keys per outbound pre-sec CC node type
   0 - not used
*/

#define IPSEC_OUT_POL_CC_NODE_KEYS { \
	0, /* DPA_IPSEC_PROTO_TCP_IPV4 */ \
	0, /* DPA_IPSEC_PROTO_TCP_IPV6 */ \
	0, /* DPA_IPSEC_PROTO_UDP_IPV4 */ \
	0, /* DPA_IPSEC_PROTO_UDP_IPV6 */ \
	NUM_SETS * NUM_WAYS, \
	/* DPA_IPSEC_PROTO_ICMP_IPV4 */ \
	NUM_SETS * NUM_WAYS, \
	/* DPA_IPSEC_PROTO_ICMP_IPV6 */ \
	0, /* DPA_IPSEC_PROTO_SCTP_IPV4 */ \
	0, /* DPA_IPSEC_PROTO_SCTP_IPV6 */ \
	NUM_SETS * NUM_WAYS, \
	/* DPA_IPSEC_PROTO_ANY_IPV4 */ \
	NUM_SETS * NUM_WAYS, \
	/* DPA_IPSEC_PROTO_ANY_IPV6 */ \
}

 /* Key sizes per inbound pre-sec CC node type */
#define IPSEC_PRE_DEC_TBL_KEY_SIZE \
	{ \
	/* IPV4 SA */ \
	(DPA_OFFLD_IPv4_ADDR_LEN_BYTES + \
	IP_PROTO_FIELD_LEN + \
	ESP_SPI_FIELD_LEN), \
	/* IPV4 SA w/ NATT*/ \
	(DPA_OFFLD_IPv4_ADDR_LEN_BYTES + \
	IP_PROTO_FIELD_LEN + \
	2 * PORT_FIELD_LEN + \
	ESP_SPI_FIELD_LEN), \
	/* IPV6 SA */ \
	(DPA_OFFLD_IPv6_ADDR_LEN_BYTES + \
	IP_PROTO_FIELD_LEN + \
	ESP_SPI_FIELD_LEN) \
	}
/* Key sizes per outbound pre-sec CC node type
   0 - not used
*/
#define IPSEC_OUT_PRE_ENC_TBL_KEY_SIZE \
	{ \
	0,     \
	0,     \
	0,     \
	0,     \
	(2 * DPA_OFFLD_IPv4_ADDR_LEN_BYTES +	 \
	IP_PROTO_FIELD_LEN), \
	(2 * DPA_OFFLD_IPv6_ADDR_LEN_BYTES +	 \
	IP_PROTO_FIELD_LEN), \
	0,     \
	0,     \
	(2 * DPA_OFFLD_IPv4_ADDR_LEN_BYTES + \
	IP_PROTO_FIELD_LEN + \
	2 * PORT_FIELD_LEN), \
	(2 * DPA_OFFLD_IPv6_ADDR_LEN_BYTES + \
	IP_PROTO_FIELD_LEN + \
	2 * PORT_FIELD_LEN) \
	}

/* Packet fields for outbound pre-sec traffic selector */
#define IPSEC_OUT_POL_TCPUDP_KEY_FIELDS \
	(DPA_IPSEC_KEY_FIELD_SIP | \
	DPA_IPSEC_KEY_FIELD_DIP | \
	DPA_IPSEC_KEY_FIELD_PROTO | \
	DPA_IPSEC_KEY_FIELD_SPORT | \
	DPA_IPSEC_KEY_FIELD_DPORT)

#define IPSEC_OUT_POL_ICMP_KEY_FIELDS \
	(DPA_IPSEC_KEY_FIELD_SIP | \
	DPA_IPSEC_KEY_FIELD_DIP | \
	DPA_IPSEC_KEY_FIELD_PROTO)

static inline int get_out_pol_num(int dpa_ipsec_proto)
{
	int out_pol_cc_node_keys[] = IPSEC_OUT_POL_CC_NODE_KEYS;
	if (dpa_ipsec_proto < 0 ||
	    dpa_ipsec_proto >= DPA_IPSEC_MAX_SUPPORTED_PROTOS)
		return -1;
	return out_pol_cc_node_keys[dpa_ipsec_proto];
}

static inline int get_outb_key_size(int dpa_ipsec_proto)
{
	int outb_key_size[] = IPSEC_OUT_PRE_ENC_TBL_KEY_SIZE;
	if (dpa_ipsec_proto < 0 ||
	    dpa_ipsec_proto >= DPA_IPSEC_MAX_SUPPORTED_PROTOS)
		return -1;
	return outb_key_size[dpa_ipsec_proto];
}

static inline int get_inb_key_size(int dpa_ipsec_proto)
{
	int inb_key_size[] = IPSEC_PRE_DEC_TBL_KEY_SIZE;
	if (dpa_ipsec_proto < 0 ||
	    dpa_ipsec_proto >= DPA_IPSEC_MAX_SUPPORTED_PROTOS)
		return -1;
	return inb_key_size[dpa_ipsec_proto];
}

static inline int get_in_sa_hash_ways(int dpa_ipsec_sa_type)
{
	int num_entries[][2] = IPSEC_IN_SA_HASH_ENTRIES;
	if (dpa_ipsec_sa_type < 0 ||
	    dpa_ipsec_sa_type >= DPA_IPSEC_MAX_SA_TYPE)
		return -1;
	return num_entries[dpa_ipsec_sa_type][WAYS];
}
static inline int get_in_sa_hash_sets(int dpa_ipsec_sa_type)
{
	int num_entries[][2] = IPSEC_IN_SA_HASH_ENTRIES;
	if (dpa_ipsec_sa_type < 0 ||
	    dpa_ipsec_sa_type >= DPA_IPSEC_MAX_SA_TYPE)
		return -1;
	return num_entries[dpa_ipsec_sa_type][SETS];
}
#endif /*__IPSEC_SIZING_H*/
