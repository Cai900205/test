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

#ifndef _HELPER_API_H_
#define _HELPER_API_H_

#include "helper_common.h"
#include "helper_ceetm.h"

#undef FSL_FQ_HOLDACTIVE_ENABLE
#undef FSL_FQ_ORP_ENABLE
#undef FSL_PKT_SG_ENABLE

#define fman_ipc			0xff

/* These 2 buffer pools need be configured when app
 * has reference to individual DESC & SEC buffer
 */
#define HLP_BPOOL_MAX       64
#define HLP_PKT_DESC_BPID	HLP_BP0_BPID
#define HLP_PKT_SEC_BPID	HLP_BP1_BPID
#define HLP_BP0_BPID		0
#define HLP_BP0_SIZE		256
#define HLP_BP0_NUM			0x400
#define HLP_BP1_BPID		1
#define HLP_BP1_SIZE		2048
#define HLP_BP1_NUM			0x1000

typedef void (*void_func) (void);

/* a set of APIs defined for usdpaa helper */
enum interface_fq_type {
	FQ_TYPE_DEFAULT = 0,
	FQ_TYPE_IPC = FQ_TYPE_DEFAULT,
	FQ_TYPE_HOLDACTIVE = QM_FQCTRL_HOLDACTIVE,
	FQ_TYPE_ORP = QM_FQCTRL_ORP,
	FQ_TYPE_CGE = QM_FQCTRL_CGE,
};

/* packet descriptor to application */
struct packet_desc {
	u32 pkt_buffer_addr;
	u32 payload_addr;
	u32 payload_length;
	u32 pool;

	u32 port;
	u32 queue;

	/* reserved for freescale internal use to
	 * support ORP, holdactive, etc
	 */
	u32 priv[0];
} __packed;

/* packet fragments descriptor for SG structure */
struct packet_desc_frag {
	u32 pkt_buffer_addr;
	u32 payload_addr;
	u32 payload_length;
	u32 pool;
} __packed;

/* fresscale private packet descriptor structure */
struct packet_desc_private {
	u32 fq_type;
	u32 fq;
	union {
		u32 dqrr;
		u32 seqnum;
		u32 resvd;
	};

	/* used for SG structure */
	u32 pkt_frags_number;
	struct packet_desc_frag pkt_frags[0];
} __packed;

/* configure packet buffer pool */
struct packet_pool_cfg {
	u32 bpid;
	u32 num;
	u32 size;
} __packed;

/* interface parameters to configure pcd */
struct interface_param {
	int port;		/* logic port */
	int type;		/* port type */
	int fman;
	int id;
	const char *name;
} __packed;

struct interface_fq_range {
	u32 fq_type;
	u32 start;
	u32 count;
	u32 channel;
} __packed;

struct interface_pcd_param {
	u32 port;		/* logical port */
	u32 num_rx_fq_ranges;
	struct interface_fq_range rx_fq_range[0];
} __packed;

struct interface_ipc_param {
	u32 port;
	u32 thread_id;
	u32 num_tx_fqs;
} __packed;

int fsl_port_loopback_set(u32 logic_port, bool enable);
int fsl_port_promisc_set(u32 logic_port, bool enable);
void *fsl_mem_alloc(u32 mem_size, u32 align_size);
void fsl_mem_free(void *addr);
void *fsl_buffer_alloc(u32 pool_id);
void fsl_buffer_free(void *bd, u32 pool_id);
void fsl_buffer_pool_init(int global, struct packet_pool_cfg *pool_cfg);
void fsl_buffer_pool_clean(void);
int fsl_interface_init(struct interface_param *param);
void fsl_interface_clean(u32 port);
int fsl_interface_pcd_init(struct interface_pcd_param *param);
void fsl_interface_pcd_clean(u32 port);
int fsl_interface_ipc_init(struct interface_ipc_param *param);
void fsl_interface_ipc_clean(u32 port);
int fsl_interface_ceetm_init(struct interface_ceetm_param *param);
void fsl_interface_ceetm_clean(u32 port);
u32 fsl_interface_ceetm_fqid_base(u32 port);

int fsl_pkt_generator(struct packet_desc **ppdesc);
int fsl_pkt_recv(struct packet_desc **ppdesc);
int fsl_pkt_send(struct packet_desc *pdesc);
void fsl_pkt_drop(struct packet_desc *pdesc);
void fsl_cpu_init(int cpu, int global, void_func sync_all);
void fsl_cpu_exit(int global);

static inline void *fsl_packet_desc_alloc(void *annotation, u32 pool_id)
{
	/* packet descriptor is at the annotation */
	if (annotation != NULL)
		return annotation;

	return fsl_buffer_alloc(pool_id);
}

static inline void fsl_packet_desc_free(struct packet_desc *desc, u32 pool_id)
{
	/* free buffer only for non-annotation case */
	if ((u32) desc == desc->pkt_buffer_addr)
		return;
	fsl_buffer_free(desc, pool_id);
}

#endif /* _HELPER_API_H_ */
