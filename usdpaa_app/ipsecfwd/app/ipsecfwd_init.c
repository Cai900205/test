/**
 \ipsecfwd_init.c
 \brief IPSec Forwarding Application Init
 */
/*
 * Copyright (C) 2011 - 2012 Freescale Semiconductor, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ipsecfwd.h"
#include <usdpaa/fsl_qman.h>
#include "ip/ip_forward.h"
#include "ip/ip_local.h"
#include "arp/arp.h"
#include "ipsec/ipsec_sec.h"
#include "ipsec/ipsec_common.h"
#include "ipsec/ipsec_encap.c"

struct ipsec_context_t *g_ipsec_ctxt[IPSEC_TUNNEL_ENTRIES];
extern struct ipsec_stack_t ipsec_stack;

void init_ppam_ctxt(struct ppam_rx_hash *ppam_ctxt)
{
	ppam_ctxt->stats = ipsec_stack.ip_stack.ip_stats;
	ppam_ctxt->hooks = &ipsec_stack.ip_stack.hooks;
	ppam_ctxt->protos = &ipsec_stack.ip_stack.protos;
	ppam_ctxt->rc = ipsec_stack.ip_stack.rc;
	ppam_ctxt->itt = &ipsec_stack.itt;
}

/**
 \brief Creates and initialized the FQs related to a tunnel
 \param[out] entry IpSec tunnel entry
 \param[out] ctxt IpSec Context
 \param[out] ctxt_a Context A for the FQ
 \param[out] tunnel_id Tunnel Id for the Queue towards SEC
 \return Integer status
 */
int32_t init_sec_fqs(struct ipsec_tunnel_t *entry, bool mode,
			void *ctxt_a, uint32_t tunnel_id)
{
	uint32_t flags;
	struct qman_fq *fq_from_sec;
	struct qman_fq *fq_to_sec;
	struct qm_mcc_initfq opts;
	uint32_t ctx_a_excl;
	uint32_t ctx_a_len;
	int to_sec_index = 0;

	flags = QMAN_FQ_FLAG_NO_ENQUEUE | QMAN_FQ_FLAG_LOCKED |
		QMAN_FQ_FLAG_DYNAMIC_FQID;

	g_ipsec_ctxt[entry->tunnel_id] = __dma_mem_memalign(L1_CACHE_BYTES,
						sizeof(struct ipsec_context_t));
	if (unlikely(NULL == g_ipsec_ctxt[entry->tunnel_id])) {
		pr_err("malloc failed in create_fqs for Tunnel ID: %u\n",
			  tunnel_id);
		return -ENOMEM;
	}

	fq_from_sec = &(g_ipsec_ctxt[entry->tunnel_id]->fq_from_sec);
	/* Rx Callback Handler */
	fq_from_sec->cb = ipsecfwd_rx_cb_pcd;

	if (unlikely(0 != qman_create_fq(0, flags, fq_from_sec))) {
		pr_err("qman_create_fq failed for Tunnel ID: %u\n", tunnel_id);
		return -1;
	}

	flags = QMAN_INITFQ_FLAG_SCHED;
	opts.we_mask =
	    QM_INITFQ_WE_DESTWQ | QM_INITFQ_WE_CONTEXTA | QM_INITFQ_WE_FQCTRL;
	opts.fqd.fq_ctrl = QM_FQCTRL_CTXASTASHING;

#if defined(PPAC_HOLDACTIVE)
	opts.fqd.fq_ctrl |= QM_FQCTRL_HOLDACTIVE;
#elif defined(PPAC_AVOIDBLOCK)
	opts.fqd.fq_ctrl |= QM_FQCTRL_AVOIDBLOCK;
#endif

	ctx_a_excl = (QM_STASHING_EXCL_DATA | QM_STASHING_EXCL_CTX);

	ctx_a_len = (1 << 2) | 1;

	opts.fqd.context_a.hi = (ctx_a_excl << 24) | (ctx_a_len << 16);

	opts.fqd.dest.channel = get_rxc();
	/* Post-CAAM WQ priority changed from 0 to 2 */
	opts.fqd.dest.wq = 2;

	if (unlikely(0 != qman_init_fq(fq_from_sec, flags, &opts))) {
		pr_err("Unable to initialize ingress FQ from sec FQID:%u"
			",tunnel ID: %u\n", fq_from_sec->fqid, tunnel_id);
		return -1;
	}

again:
	flags = QMAN_FQ_FLAG_LOCKED | QMAN_FQ_FLAG_TO_DCPORTAL |
		QMAN_FQ_FLAG_DYNAMIC_FQID;

	fq_to_sec = &(g_ipsec_ctxt[entry->tunnel_id]->fq_to_sec[to_sec_index]);

	/* Tx Callback Handlers */
	fq_to_sec->cb = ipsecfwd_tx_cb;
	if (unlikely(0 != qman_create_fq(0, flags, fq_to_sec))) {
		pr_err("qman_create_fq failed for Tunnel ID:%u\n", tunnel_id);
		return -1;
	}

	flags = QMAN_INITFQ_FLAG_SCHED;
	opts.we_mask = QM_INITFQ_WE_DESTWQ | QM_INITFQ_WE_CONTEXTA |
	    QM_INITFQ_WE_CONTEXTB;
	qm_fqd_context_a_set64(&opts.fqd, __dma_mem_vtop(ctxt_a));
	opts.fqd.context_b = fq_from_sec->fqid;
	opts.fqd.dest.channel = qm_channel_caam;
	opts.fqd.dest.wq = 0;

	if (unlikely(0 != qman_init_fq(fq_to_sec, flags, &opts))) {
		pr_err("Unable to Init CAAM Egress FQ to sec FQID:%u"
			",tunnel ID: %u\n", fq_to_sec->fqid, tunnel_id);
		return -EINVAL;
	}
	entry->qm_fq_to_sec[to_sec_index] = fq_to_sec;

	to_sec_index++;

	if (entry->hb_tunnel && (to_sec_index < NUM_TO_SEC_FQ))
		goto again;

	g_ipsec_ctxt[entry->tunnel_id]->num_fq_to_sec = to_sec_index;

	if (mode == ENCRYPT)
		g_ipsec_ctxt[entry->tunnel_id]->ipsec_handler = &ipsec_encap_cb;
	else
		g_ipsec_ctxt[entry->tunnel_id]->ipsec_handler = &ipsec_decap_cb;

	init_ppam_ctxt(&(g_ipsec_ctxt[entry->tunnel_id]->ppam_ctxt));

	return 0;
}

#if 0
int32_t ip_shutdown_sec_fqs(void)
{
	/* TBD */
}
#endif
