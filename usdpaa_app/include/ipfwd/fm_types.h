/** @file
 * Structures for frame manager
 */

/*
 * Copyright (c) 2010 Freescale Semiconductor, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __FM_TYPES_H
#define __FM_TYPES_H

#ifdef __cplusplus
    extern "C" {
#endif
#include <stdint.h>
#include <stdbool.h>

enum FRAME_COLOR {
	FRAME_COLOR_GREEN = 0x0,
	FRAME_COLOR_YELLOW = 0x1,
	FRAME_COLOR_RED = 0x2,
	FRAME_COLOR_OVERRIDE = 0x3
};

enum FRAME_TYPE {
	FRAME_TYPE_UNICAST = 0x0,
	FRAME_TYPE_MULTICAST = 0x2,
	FRAME_TYPE_BROADCAST = 0x3
};

struct l2_results_t {
	/* L2R Result Decode */
	uint32_t ethernet_present:1;
	uint32_t vlan_present:1;
	uint32_t snap_present:1;
	uint32_t mpls_present:1;
	uint32_t pppoe_ppp_present:1;
	uint32_t l2_type_reserved:1;
	uint32_t l2_info_reserved:1;
	uint32_t stacked_vlan_present:1;
	uint32_t unknown_ethertype:1;
	uint32_t frame_type:2;
	uint32_t l2_error_code:5;
} __attribute__ ((__packed__));

struct l3_results_t {
	/* L3R Result Decode */
	uint32_t first_present_ipv4:1;
	uint32_t first_present_ipv6:1;
	uint32_t gre_present:1;
	uint32_t minenc_present:1;
	uint32_t last_present_ipv4:1;
	uint32_t last_present_ipv6:1;
	uint32_t first_result_error:1;
};

struct output_parse_result_t {
	uint32_t pc:2;		/* 0x00 */
	uint32_t port_id:6;
	uint8_t shimr;		/* 0x01 */
	uint16_t l2r;		/* 0x02 */
	uint16_t l3r;		/* 0x04 */
	uint8_t l4r;		/* 0x06 */
	uint8_t classification_planid;	/* 0x07 */
	uint16_t nxt_hdr_type;	/* 0x08 */
	uint16_t checksum;	/* 0x0A */
	uint32_t lineup_confirmation_vector;	/* 0x0c */
	uint8_t shim1O;		/* 0x10 */
	uint8_t shim2O;		/* 0x11 */
	uint8_t shim3O;		/* 0x12 */
	uint8_t ethO;		/* 0x13 */
	uint8_t llc_snapO;	/* 0x14 */
	uint8_t vlan1O;		/* 0x15 */
	uint8_t vlan2O;		/* 0x16 */
	uint8_t lastEtype0;	/* 0x17 */
	uint8_t pppoeO;		/* 0x18 */
	uint8_t mpls1O;		/* 0x19 */
	uint8_t mpls2O;		/* 0x1a */
	uint8_t ipO;		/* 0x1B */
	uint8_t ipO_or_minencapO;	/* 0x1C */
	uint8_t gre;		/* 0x1D */
	uint8_t l4O;		/* 0x1E */
	uint8_t nxtHdr0;	/* 0x1F */
} __attribute__ ((__packed__));

#ifdef __cplusplus
    }
#endif
#endif				/* __FM_TYPES_H */
