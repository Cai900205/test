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
#ifndef __APP_CONFIG_H
#define __APP_CONFIG_H

#include <stdbool.h>
#include <net/if.h>

/* IP fragmentation scratch buffer pool */
#define DMA_MEM_IPF_SIZE	1600
#define DMA_MEM_IPF_NUM		0x0
/* IP reassembly buffer pool */
#define IPR_BPID		5
#define DMA_MEM_IPR_SIZE	1600
#define DMA_MEM_IPR_NUM		0x2000
/* Interfaces and OP buffer pool */
#define IF_BPID			16
#define DMA_MEM_IF_SIZE		1728
#define DMA_MEM_IF_NUM		0x4000
#define OP_BPID			9
#define DMA_MEM_OP_SIZE		1728
#define DMA_MEM_OP_NUM		0x2000
/* used by SEC for output frames */
#define DMA_MAP_SIZE	0x4000000 /*64M*/
#define SEC_WQ_ID	7
#define SEC_DATA_OFF_BURST	1
#define SEC_ERA_5_DATA_OFF_BURST	3

/* The following FQIDs are static because they are
 * used in PCD definitions */
/* Tx FQIDs on outbound direction */
#define IB_TX_FQID			0x3e83
#define OB_OH_POST_TX_FQID		0x3e82

/*
 * Array containing the FQ IDs for PRE SEC Offline port
 * Policy PCD XML will be updated by ipsec_offload usecase with corresponding FQ
 * ID before applying
 * Iterate in this array based on the FMAN ID, not DPA IPSec instance ID
 */
#define MAX_DPA_IPSEC_INSTANCES		2
static const int ob_oh_pre_tx_fqid[MAX_DPA_IPSEC_INSTANCES] = {0x3e81, 0x4e81};

/* Tx FQIDs on inbound direction */
#define OB_TX_FQID			0x2e80
#define IB_OH_TX_FQID			0x2e81

/* virtual storage profile used on the IB OH (only in case of ONIC) */
#define VSP_ID					1
#define VSP_BP_SIZE				1728

/* application configuration data */
struct app_conf {
	/* FMAN index */
	int fm;
	/* outbound Fman interface */
	struct fman_if *ob_eth;
	/* inbound Fman interface */
	struct fman_if *ib_eth;
	/* DPA IPsec inbound offline Fman interface*/
	struct fman_if *ib_oh;
	/* DPA IPsec pre SEC outbound offline Fman interface*/
	struct fman_if *ob_oh_pre;
	/* DPA IPsec post SEC outbound offline Fman interface*/
	struct fman_if *ob_oh_post;
	/* Max number of SA pairs */
	int max_sa;
	/* IP fragmentation scratch bpid*/
	u32 ipf_bpid;
	/* MTU pre encryption */
	int mtu_pre_enc;
	/* outer header TOS field */
	int outer_tos;
	/* enable inbound ECN tunneling*/
	bool ib_ecn;
	/* enable outbound ECN tunneling */
	bool ob_ecn;
	/* inbound loopback set */
	bool ib_loop;
	/* Virtual inbound interface name */
	char vif[IFNAMSIZ];
	/* Virtual outbound interface name */
	char vof[IFNAMSIZ];
	/* IPsec interface name */
	char vipsec[IFNAMSIZ];
	/* Aggregate inbound tunnels. Inbound decrypted traffic
	 * from multiple SAs will be enqueued to oh_pre_sec port,
	 * encrypted on one SA and sent to Tx port.(single port case)
	 */
	bool ib_aggreg;
};
extern struct app_conf app_conf;
#endif
