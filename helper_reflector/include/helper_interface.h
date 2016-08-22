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

#ifndef _HELPER_INTERFACE_H_
#define	_HELPER_INTERFACE_H_

#include "helper_common.h"
#include "helper_api.h"
#include "helper_ceetm.h"

#include <argp.h>
#include <error.h>
#include <stdbool.h>

#define PACKET_IF_NUM_MAX			0x100

/* interface fq & fq list  */
struct interface_fq {
	struct qman_fq fq;
	u32 fq_type;
	u32 port;
} ____cacheline_aligned;

struct interface_fq_list {
	struct list_head list;
	u32 start;
	u32 count;
	struct interface_fq rx_hash[0];
} ____cacheline_aligned;

/* packet interface */
struct packet_interface {
	/* physical port */
	u32 port;

	/* fman interface */
	struct fman_if *fm_if;

	/* default configuration from device tree */
	struct interface_fq tx_error;
	struct interface_fq tx_confirm;
	struct interface_fq rx_error;
	struct interface_fq rx_default;

	/* tx fqs to the physical interface */
	u32 num_tx_fqs;
	struct interface_fq *tx;

	/* ceetm interface */
	struct interface_ceetm *ceetm_if;

	/* rx fq list parsed from rx_fqs_range */
	struct list_head rx_fqs_list;
} ____cacheline_aligned;

/* parameters to configure interface default fqs */
struct interface_param_internal {
	u32 port;
	u32 pool_channel;
	struct fman_if *fm_if;
	u32 num_tx_fqs;
	u32 fqid_rx_default;
};

int packet_interface_init(struct interface_param_internal *param);
int packet_interface_pcd_init(struct interface_pcd_param *param);
int packet_interface_ipc_init(struct interface_ipc_param *param);
int packet_interface_ceetm_init(struct interface_ceetm_param *param);

void packet_interface_clean(u32 port);
void packet_interface_pcd_clean(u32 port);
void packet_interface_ipc_clean(u32 port);
void packet_interface_ceetm_clean(u32 port);
inline struct packet_interface *packet_interface_get(u32 port);

/* local tx fq for transmission */
int local_tx_fq_init(void);
void local_tx_fq_clean(void);
struct qman_fq *local_tx_fq_get(void);

inline int packet_interface_enqueue(struct qman_fq *fq,
				    struct qm_dqrr_entry *dqrr);
inline int packet_interface_dequeue(struct packet_desc **pkt);
static inline void packet_interface_fd_2_pd(struct qm_fd *pfd,
					    struct packet_desc *pdesc)
{
	pdesc->pkt_buffer_addr = (u32) ptov(pfd->addr);
	pdesc->payload_addr = pdesc->pkt_buffer_addr + pfd->offset;
	pdesc->payload_length = pfd->length20;
	pdesc->pool = pfd->bpid;

#ifdef FSL_PKT_SG_ENABLE
	{
		struct packet_desc_private *priv =
		    (struct packet_desc_private *)pdesc->priv;
		priv->pkt_frags_number = 0;

		if (pfd->format == qm_fd_sg) {
			int i = 0;
			struct qm_sg_entry *sg = ptov(pfd->addr + pfd->offset);
			while (1) {
				if (sg[i].extension) {
					TRACE("Don't support ext for SG\n");
					break;
				}

				/* SG buffer details */
				priv->pkt_frags_number++;
				priv->pkt_frags[i].pkt_buffer_addr =
				    (u32) ptov(sg[i].addr);
				priv->pkt_frags[i].payload_addr =
				    priv->pkt_frags[i].pkt_buffer_addr +
				    sg[i].offset;
				priv->pkt_frags[i].payload_length =
				    sg[i].length;
				priv->pkt_frags[i].pool = sg[i].bpid;
				if (sg[i].final)
					break;
				i++;
			}
		}
	}
#endif
}

static inline void packet_interface_pd_2_fd(struct packet_desc *pdesc,
					    struct qm_fd *pfd)
{
	pfd->bpid = pdesc->pool;
	pfd->addr = vtop(pdesc->pkt_buffer_addr);
	pfd->format = qm_fd_contig;
	pfd->offset = pdesc->payload_addr - pdesc->pkt_buffer_addr;
	pfd->length20 = pdesc->payload_length;
	pfd->cmd = 0;

#ifdef FSL_PKT_SG_ENABLE
	{
		struct packet_desc_private *priv =
		    (struct packet_desc_private *)pdesc->priv;

		if (priv->pkt_frags_number > 0) {
			int i;
			struct qm_sg_entry *sg =
			    (struct qm_sg_entry *)pdesc->pkt_buffer_addr;
			memset(sg, 0,
			       sizeof(struct qm_sg_entry) *
			       priv->pkt_frags_number);
			for (i = 0; i < priv->pkt_frags_number; i++) {
				sg[i].addr =
				    vtop(priv->pkt_frags[i].payload_addr);
				sg[i].length =
				    priv->pkt_frags[i].payload_length;
				sg[i].offset =
				    priv->pkt_frags[i].payload_addr -
				    priv->pkt_frags[i].pkt_buffer_addr;
				sg[i].bpid = priv->pkt_frags[i].pool;
			}
			sg[i - 1].final = 1;

			/* format to SG structure */
			pfd->format = qm_fd_sg;
		}
	}
#endif
}

#endif /*  __INTERFACE_H__ */
