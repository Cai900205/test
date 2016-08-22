/* Copyright (c) 2011-2012 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
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

#ifndef _FRA_CFG_PARSER_H
#define _FRA_CFG_PARSER_H

#include <fra_common.h>
#include <usdpaa/fsl_rman.h>
#include <usdpaa/compat.h>

#define MAX_LENGTH_OF_NAME 32

enum dist_type {
	DIST_TYPE_UNKNOWN,
	DIST_TYPE_RMAN_RX,
	DIST_TYPE_RMAN_TX,
	DIST_TYPE_FMAN_RX,
	DIST_TYPE_FMAN_TX,
	DIST_TYPE_FMAN_TO_RMAN,
	DIST_TYPE_RMAN_TO_FMAN
};

struct dist_rman_rx_cfg {
	uint8_t rio_port;
	uint8_t port_mask;
	uint16_t sid;
	uint16_t sid_mask;
	uint32_t fqid;
	enum RMAN_FQ_MODE fq_mode;
	uint8_t wq;
	struct rio_tran	*tran;
};

struct dist_rman_tx_cfg {
	uint8_t rio_port;
	uint8_t fqs_num;
	uint8_t wq;
	uint16_t did;
	uint32_t fqid;
	struct rio_tran	*tran;
};

struct fra_fman_port_cfg {
	struct list_head node;
	char name[MAX_LENGTH_OF_NAME];
	uint8_t fman_num; /* 0 => FMAN0, 1 => FMAN1 and so on */
	uint8_t port_type;
	uint8_t port_num; /* 0 onwards */
};

struct dist_fman_rx_cfg {
	char fman_port_name[MAX_LENGTH_OF_NAME];
	uint8_t wq;
};

struct dist_fman_tx_cfg {
	char fman_port_name[MAX_LENGTH_OF_NAME];
	uint32_t fqs_num;
	uint8_t wq;
};

struct dist_fman_to_rman_cfg {
	char fman_port_name[MAX_LENGTH_OF_NAME];
	uint8_t rio_port;
	uint8_t wq;
	uint16_t did;
	struct rio_tran	*tran;
};

struct dist_rman_to_fman_cfg {
	char fman_port_name[MAX_LENGTH_OF_NAME];
	uint8_t rio_port;
	uint8_t port_mask;
	uint16_t sid;
	uint16_t sid_mask;
	uint32_t fqid;
	enum RMAN_FQ_MODE fq_mode;
	uint8_t wq;
	struct rio_tran	*tran;
};

struct dist_cfg {
	struct list_head node;
	struct dist_cfg *next;
	char name[MAX_LENGTH_OF_NAME];
	enum dist_type type;
	uint8_t sequence_number;
	union {
		struct dist_rman_rx_cfg dist_rman_rx_cfg;
		struct dist_rman_tx_cfg dist_rman_tx_cfg;
		struct dist_fman_rx_cfg dist_fman_rx_cfg;
		struct dist_fman_tx_cfg dist_fman_tx_cfg;
		struct dist_fman_to_rman_cfg dist_fman_to_rman_cfg;
		struct dist_rman_to_fman_cfg dist_rman_to_fman_cfg;
	};
};

struct dist_order_cfg {
	struct list_head node;
	struct dist_cfg *dist_cfg;
};

struct policy_cfg {
	struct list_head node;
	char name[MAX_LENGTH_OF_NAME];
	int enable;
	struct list_head dist_order_cfg_list;
};

struct fra_cfg {
	struct list_head dist_order_cfg_list;
	struct list_head fman_port_cfg_list;
	struct list_head trans_list;
	struct list_head dists_list;
	struct list_head policies_list;
	struct policy_cfg *policy_cfg;
	struct rman_cfg rman_cfg;
};

extern const char *DIST_TYPE_STR[];
extern const char *FQ_MODE_STR[];
extern const char *MD_CREATE_MODE_STR[];
extern const char *RIO_TYPE_TO_STR[];

struct fra_cfg *fra_parse_cfgfile(const char *cfg_file);
void fra_cfg_release(struct fra_cfg *fra_cfg);

#endif /*_FRA_CFG_PARSER_H*/
