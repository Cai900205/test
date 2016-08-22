/* Copyright (c) 2011-2012 Freescale Semiconductor, Inc.
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

#ifndef _FRA_H
#define _FRA_H

#include <fra_common.h>
#include <fra_cfg_parser.h>
#include <usdpaa/usdpaa_netcfg.h>
#include <rman_interface.h>
#include <fra_fman_port.h>
#include <usdpaa/fsl_dma.h>
#include <usdpaa/fsl_srio.h>
#include <unistd.h>
#include <app_conf.h>

struct rman_tx_list {
	struct list_head node;
	struct rman_tx *rman_tx;
};

struct fman_to_rman {
	struct fra_fman_port *fman_port;
	struct list_head f2r_tx_list;
};

struct rman_to_fman {
	struct rman_rx *rman_rx;
	struct fra_fman_port *fman_port;
};

struct distribution {
	struct distribution *next;
	struct dist_cfg *cfg;
	enum handler_status (*handler)(struct distribution *dist,
				       struct hash_opt *opt,
				       const struct qm_fd *fd);
	union {
		struct rman_rx *rman_rx;
		struct rman_tx *rman_tx;
		struct fra_fman_port *fman_rx;
		struct fra_fman_port *fman_tx;
		struct fman_to_rman *fman_to_rman;
		struct rman_to_fman *rman_to_fman;
	};
} ____cacheline_aligned;

struct dist_order {
	struct list_head node;
	struct distribution *dist;
};

struct fra {
	struct list_head dist_order_list;
	const struct fra_cfg *cfg;
};

enum handler_status {
	HANDLER_DONE,
	HANDLER_CONTINUE,
	HANDLER_ERROR
};

extern struct fra *fra;
#if 1
struct dma_pool {
   dma_addr_t dma_phys_base;
   void *dma_virt_base;
};
extern struct dma_ch *dmadev;
extern struct dma_pool *swrite_dmapool;
#endif
int fra_init(const struct usdpaa_netcfg_info *netcfg,
	     const struct fra_cfg *fra_cfg);

void fra_reset(void);

int msg_do_reset(void);

void fra_finish(void);

#endif /* _FRA_H */
