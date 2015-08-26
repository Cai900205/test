/* Copyright (c) 2011-2013 Freescale Semiconductor, Inc.
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

#ifndef __DPA_CLASSIF_DEMO_H
#define __DPA_CLASSIF_DEMO_H

#define APP_TABLE_KEY_SIZE_IPv4			9  /* bytes */
#define APP_TABLE_KEY_SIZE_IPv6			33 /* bytes */
#define APP_DSCP_TABLE_KEY_SIZE			1  /* bytes */

#define CLS_MBR_SIZE				2

#define IPv4					4
#define IPv6					6

#define IPv4_LEN				4  /* bytes */
#define IPv6_LEN				16 /* bytes */

#define MAX_NUM_OF_IPv4_KEYS			64
#define MAX_NUM_OF_IPv6_KEYS			64
#define NUM_HASH_BUCKETS			32

#define MAC_CHARACTER_LEN			17

#define REM_CMD_ARGC				4
#define ADD_CMD_ARGC				5

#define ENABLE_PROMISC

struct ppam_arguments {
	int	fm;
	int	port;
};

struct eth_counter {
	uint32_t		dropped_pkts;
	uint32_t		bytes;
	uint32_t		pkts;
	uint32_t		broadcast_pkts;
	uint32_t		multicast_pkts;
	uint32_t		crc_align_err_pkts;
	uint32_t		undersized_pkts;
	uint32_t		oversized_pkts;
	uint32_t		frags;
	uint32_t		jabbers;
	uint32_t		pkts_64b;
	uint32_t		pkts_65_127b;
	uint32_t		pkts_128_255b;
	uint32_t		pkts_256_511b;
	uint32_t		pkts_512_1023b;
	uint32_t		pkts_1024_1518b;
	uint32_t		out_pkts;
	uint32_t		out_drop_pkts;
	uint32_t		out_bytes;
	uint32_t		in_errors;
	uint32_t		out_errors;
	uint32_t		in_unicast_pkts;
	uint32_t		out_unicast_pkts;
};

struct key_counter {
	uint32_t bytes;
	uint32_t frames;
};

/* VLAN header definition */
struct vlan_hdr {
	__u16 tci;
	__u16 type;
};

enum dpa_stats_op {
	dpa_classif_stats_get_async = 0,
	dpa_classif_stats_get_sync,
	dpa_traffic_stats_get_async,
	dpa_traffic_stats_get_sync,
	dpa_stats_reset
};

struct counter_data {
	int key_ref; /* DPA Classifier reference for the key */
	int cnt_idx; /* DPA Stats counter id for the key */
	struct list_head node;
};

int		ppam_init(void);
void		ppam_finish(void);

void		print_traffic_dpa_stats_cnts(void);

#endif /* __DPA_CLASSIFIER_DEMO_H */
