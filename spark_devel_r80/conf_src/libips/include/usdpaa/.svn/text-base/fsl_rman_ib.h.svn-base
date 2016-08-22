/* Copyright 2012 Freescale Semiconductor, Inc.
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

#ifndef _FSL_RMAN_IB_H
#define _FSL_RMAN_IB_H

#include <limits.h>

enum RIO_TYPE {
	RIO_TYPE0 = 0,
	RIO_TYPE1,
	RIO_TYPE2,
	RIO_TYPE3,
	RIO_TYPE4,
	RIO_TYPE5,
	RIO_TYPE6,
	RIO_TYPE7,
	RIO_TYPE8,
	RIO_TYPE9,
	RIO_TYPE10,
	RIO_TYPE11,
	RIO_TYPE_NUM,
	RIO_TYPE_PW = RIO_TYPE8,
	RIO_TYPE_DSTR = RIO_TYPE9,
	RIO_TYPE_DBELL = RIO_TYPE10,
	RIO_TYPE_MBOX = RIO_TYPE11
};

enum RIO_MBOX_NUM {
	RIO_MBOX_A,
	RIO_MBOX_B,
	RIO_MBOX_C,
	RIO_MBOX_D
};

enum RMAN_FQ_MODE {
	DIRECT,
	ALGORITHMIC
};

struct rio_tran {
	struct list_head node;
	char name[PATH_MAX];
	uint8_t type;
	uint8_t flowlvl;
	uint8_t flowlvl_mask;
	union {
		struct mbox_attr {
			uint8_t mbox;
			uint8_t mbox_mask;
			uint8_t ltr;
			uint8_t ltr_mask;
			uint8_t msglen;
			uint8_t msglen_mask;
		} mbox;

		struct dstr_attr {
			uint16_t streamid;
			uint16_t streamid_mask;
			uint8_t cos;
			uint8_t cos_mask;
		} dstr;
	};
};

struct ibcu_cfg {
	int		ibcu;
	uint8_t		port;
	uint8_t		port_mask;
	uint16_t	sid;
	uint16_t	sid_mask;
	uint16_t	did;
	uint16_t	did_mask;
	int		fqid;
	uint8_t		bpid;
	uint8_t		sgbpid;
	uint32_t	msgsize;
	uint32_t	sgsize;
	uint32_t	data_offset;
	enum RMAN_FQ_MODE	fq_mode;
	struct rio_tran	*tran;
};


struct rman_inbound_block;

/**
 * rman_ib_idx - return index of the inbound index
 * @ib: RMan inbound block info
 */
int rman_ib_idx(struct rman_inbound_block *ib);

/**
 * rman_enable_ibcu - enable the ibcu resource
 * @ib: RMan inbound block info
 * @idx: RMan inbound classification unit index
 *
 * Enable the corresponding ibcu resource
 */
void rman_enable_ibcu(struct rman_inbound_block *ib, int idx);

/**
 * rman_disable_ibcu - disable the ibcu resource
 * @ib: RMan inbound block info
 * @idx: RMan inbound classification unit index
 *
 * Disable the corresponding ibcu resource
 */
void rman_disable_ibcu(struct rman_inbound_block *ib, int idx);

/**
 * rman_release_ibcu - release the ibcu resource
 * @ib: RMan inbound block info
 * @idx: RMan inbound classification unit index
 *
 * This function will disable the specified ibcu and release the resource.
 */
void rman_release_ibcu(struct rman_inbound_block *ib, int idx);

/**
 * rman_request_ibcu - request the ibcu resource
 * @ib: RMan inbound block info
 *
 * Returns an idle ibcu index.
 * If no idle ibcu it returns -EINVAL
 */
int rman_request_ibcu(struct rman_inbound_block *ib);

/**
 * rman_config_ibcu - configure the ibcu resource
 * @ib: RMan inbound block info
 * @cfg: ibcu configuration
 *
 * This function configure the ibcu according to ibcu configuration.
 * Returns %0 on success or %-EINVAL on failure.
 */
int rman_config_ibcu(struct rman_inbound_block *ib, const struct ibcu_cfg *cfg);

/**
 * rman_set_ibef - set the error frame queue
 * @ib: RMan inbound block info
 * @fqid: error frame queue id
 *
 * Error frame queue ID used when MMMR[EFQ]=1. For proper operation,
 * this field should only be modified when the inbound reassembly unit
 * is not enabled.
 */
void rman_set_ibef(struct rman_inbound_block *ib, uint32_t fqid);

/**
 * rman_get_ibef - get the error frame queue
 * @ib: RMan inbound block info
 *
 * Return error frame queue ID
 */
int rman_get_ibef(struct rman_inbound_block *ib);

/**
 * rman_ib_init - initialize the specified inbound block device
 *
 * This function firstly find the specified RMan inbound block , then maps
 * the RMan register space, finally initialize inbound block units.
 * Returns the pointer of rman_inbound_block on success or %NULL on failure.
 */
struct rman_inbound_block *rman_ib_init(int idx);

/**
 * rman_ib_finish - release the RMan inbound block resource
 * @ib: RMan inbound block info
 *
 * Releases All the RMan inbound block resource.
 */
void rman_ib_finish(struct rman_inbound_block *ib);

#endif /*_ FSL_RMAN_IB_H */
