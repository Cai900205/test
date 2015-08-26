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

#include "helper_interface.h"

#undef  HLP_TX_CONFIRM

#define HLP_FASTPOLL_DQRR           16
#define HLP_SLOWPOLL_BUSY           4
#define HLP_SLOWPOLL_IDLE           400

#define HLP_PKT_QUEUE_SIZE			HLP_FASTPOLL_DQRR

#define HLP_RX_PIC                  1
#define HLP_TX_FQS                  2
#define HLP_TX_FQ_NO_BUF_DEALLOC    0x00000001
#define HLP_TX_FQ_NO_CHECKSUM       0x00000002

#define HLP_PRIORITY_2DROP          3   /* Error/default/etc */
#define HLP_PRIORITY_2FWD           4   /* rx-hash */
#define HLP_PRIORITY_2TX            4   /* Consumed by Fman */

#define HLP_STASH_ANNOTATION_CL     0
#define HLP_STASH_DATA_CL           1
#define HLP_STASH_CONTEXT_CL        0

/* 0->32, 1->64, 2->128, ... 7->4096 */
#define HLP_ORP_WINDOW_SIZE         7
#define HLP_ORP_AUTO_ADVANCE        1
/* 0->no, 3->yes (for 1 & 2->see RM) */
#define HLP_ORP_ACCEPT_LATE         3

/* per-cpu queue to buffer qman polled packet */
struct packet_queue {
	u32 pi;
	u32 ci;
	struct packet_desc *pkt[HLP_PKT_QUEUE_SIZE];
} ____cacheline_aligned;

__PERCPU int helper_slow_poll = 0;
__PERCPU struct packet_queue helper_pkt_queue = {
	.pi = 0,
	.ci = 0
};

/* per-cpu local FQ for transmission usage */
__PERCPU struct qman_fq helper_local_fq;
__PERCPU u32 helper_counter_ern = 0;

/* FQ stashing options */
static const struct qm_fqd_stashing default_stash_opts = {
	.annotation_cl = HLP_STASH_ANNOTATION_CL,
	.data_cl = HLP_STASH_DATA_CL,
	.context_cl = HLP_STASH_CONTEXT_CL
};

/* driver interface table (assume less than 256-entry) */
struct packet_interface *helper_if_table[PACKET_IF_NUM_MAX] = { 0 };

/* default FQ callback function */
static void
cb_dqrr_ern(struct qman_portal *qm __always_unused,
	    struct qman_fq *fq,
	    const struct qm_mr_entry *msg)
{
#ifdef DEBUG_ON
	if ((helper_counter_ern & 0xff) == 0)
		TRACE("Cb_ern: fqid=0x%x\tfd_status = 0x%08x, rc code %d\n",
			  msg->ern.fqid, msg->ern.fd.status, msg->ern.rc);
#endif

	helper_counter_ern++;
	fsl_drop_frame(&msg->ern.fd);
}

static enum qman_cb_dqrr_result
cb_tx_drain(struct qman_portal *qm __always_unused,
	    struct qman_fq *fq,
	    const struct qm_dqrr_entry *dqrr)
{
	TRACE("Tx_drain: fqid=0x%x\tfd_status = 0x%08x\n", fq->fqid,
		  dqrr->fd.status);
	fsl_drop_frame(&dqrr->fd);
	return qman_cb_dqrr_consume;
}

static enum qman_cb_dqrr_result
cb_dqrr_tx_error(struct qman_portal *qm __always_unused,
		 struct qman_fq *fq,
		 const struct qm_dqrr_entry *dqrr)
{
	TRACE("Tx_error: fqid=0x%x\tfd_status = 0x%08x\n", fq->fqid,
		  dqrr->fd.status);
	fsl_drop_frame(&dqrr->fd);
	return qman_cb_dqrr_consume;
}

static enum qman_cb_dqrr_result
cb_dqrr_tx_confirm(struct qman_portal *qm __always_unused,
		   struct qman_fq *fq,
		   const struct qm_dqrr_entry *dqrr)
{
	TRACE("Tx_confirm: fqid=0x%x\tfd_status = 0x%08x\n", fq->fqid,
		  dqrr->fd.status);
	fsl_drop_frame(&dqrr->fd);
	return qman_cb_dqrr_consume;
}

static enum qman_cb_dqrr_result
cb_dqrr_rx_error(struct qman_portal *qm __always_unused,
		 struct qman_fq *fq,
		 const struct qm_dqrr_entry *dqrr)
{
	TRACE("Rx_error: fqid=0x%x\tfd_status = 0x%08x\n", fq->fqid,
		  dqrr->fd.status);
	fsl_drop_frame(&dqrr->fd);
	return qman_cb_dqrr_consume;
}

static enum qman_cb_dqrr_result
cb_dqrr_rx_default(struct qman_portal *qm __always_unused,
		   struct qman_fq *fq,
		   const struct qm_dqrr_entry *dqrr)
{
#ifdef DEBUG_ON
	if (unlikely(dqrr->fd.format != qm_fd_contig &&
			dqrr->fd.format != qm_fd_sg)) {
		TRACE("Rx_default: unsupported packet format 0x%x, fqid 0x%x\n",
			  dqrr->fd.format, fq->fqid);
		return qman_cb_dqrr_consume;
	}

	if (unlikely(dqrr->fd.status != 0)) {
		TRACE("Rx_default: status error 0x%x\n", dqrr->fd.status);
		fsl_drop_frame(&dqrr->fd);
		return qman_cb_dqrr_consume;
	}
#endif

	return packet_interface_enqueue(fq, (struct qm_dqrr_entry *)dqrr);
}

static enum qman_cb_dqrr_result
cb_dqrr_rx_hash(struct qman_portal *qm __always_unused,
		struct qman_fq *fq,
		const struct qm_dqrr_entry *dqrr)
{
#ifdef DEBUG_ON
	if (unlikely(dqrr->fd.format != qm_fd_contig &&
			dqrr->fd.format != qm_fd_sg)) {
		TRACE("Rx_hash: unsupported packet format 0x%x, fqid 0x%x\n",
			  dqrr->fd.format, fq->fqid);
		return qman_cb_dqrr_consume;
	}

	if (unlikely(dqrr->fd.status != 0)) {
		TRACE("Rx_hash: status error 0x%x\n", dqrr->fd.status);
		fsl_drop_frame(&dqrr->fd);
		return qman_cb_dqrr_consume;
	}
#endif
	return packet_interface_enqueue(fq, (struct qm_dqrr_entry *)dqrr);
}

static void fq_nonpcd_init(struct qman_fq *fq, u32 fqid,
			   u16 channel,
			   const struct qm_fqd_stashing *stashing,
			   qman_cb_dqrr cb)
{
	struct qm_mcc_initfq opts;
	__maybe_unused int ret;

	fq->cb.dqrr = cb;
	ret = qman_create_fq(fqid, QMAN_FQ_FLAG_NO_ENQUEUE, fq);
	BUG_ON(ret);
	/* FIXME: no taildrop/holdactive for "2drop" FQs */
	opts.we_mask = QM_INITFQ_WE_DESTWQ | QM_INITFQ_WE_FQCTRL |
		QM_INITFQ_WE_CONTEXTA;
	opts.fqd.dest.channel = channel;
	opts.fqd.dest.wq = HLP_PRIORITY_2DROP;
	opts.fqd.fq_ctrl = QM_FQCTRL_CTXASTASHING;
	opts.fqd.context_a.stashing = *stashing;
	ret = qman_init_fq(fq, QMAN_INITFQ_FLAG_SCHED, &opts);
	BUG_ON(ret);
}

static void fq_pcd_init(struct qman_fq *fq, u32 fqid,
			u16 fq_type, u16 channel,
			const struct qm_fqd_stashing *stashing,
			int prefer_in_cache)
{
	struct qm_mcc_initfq opts;
	__maybe_unused int ret;

	fq->cb.dqrr = cb_dqrr_rx_hash;

	ret = qman_create_fq(fqid, QMAN_FQ_FLAG_NO_ENQUEUE, fq);
	BUG_ON(ret);

	/* FIXME: no taildrop/holdactive for "2fwd" FQs */
	memset(&opts, 0, sizeof(opts));
	opts.we_mask = QM_INITFQ_WE_DESTWQ | QM_INITFQ_WE_FQCTRL |
		QM_INITFQ_WE_CONTEXTA;
	opts.fqd.dest.channel = channel;
	opts.fqd.dest.wq = HLP_PRIORITY_2FWD;
	opts.fqd.fq_ctrl = QM_FQCTRL_CTXASTASHING;

	/* holdactive and ORP shall not coexist */
	if (fq_type & QM_FQCTRL_HOLDACTIVE) {
		opts.fqd.fq_ctrl |= QM_FQCTRL_HOLDACTIVE;
	} else if (fq_type & QM_FQCTRL_ORP) {
		opts.we_mask |= QM_INITFQ_WE_ORPC;
		opts.fqd.fq_ctrl |= QM_FQCTRL_ORP;
		opts.fqd.orprws = HLP_ORP_WINDOW_SIZE;
		opts.fqd.oa = HLP_ORP_AUTO_ADVANCE;
		opts.fqd.olws = HLP_ORP_ACCEPT_LATE;
	}

	opts.fqd.context_a.stashing = *stashing;
	if (prefer_in_cache)
		opts.fqd.fq_ctrl |= QM_FQCTRL_PREFERINCACHE;

	ret = qman_init_fq(fq, QMAN_INITFQ_FLAG_SCHED, &opts);
	BUG_ON(ret);
}

static void fq_tx_init(struct qman_fq *fq, u16 channel,
			   u64 context_a, u32 context_b)
{
	struct qm_mcc_initfq opts;
	__maybe_unused int err;
	/* These FQ objects need to be able to handle DQRR callbacks, when
	 * cleaning up. */
	fq->cb.dqrr = cb_tx_drain;
	err = qman_create_fq(0, QMAN_FQ_FLAG_DYNAMIC_FQID |
				 QMAN_FQ_FLAG_TO_DCPORTAL, fq);
	/* Note: handle errors here, BUG_ON()s are compiled out in performance
	 * builds (ie. the default) and this code isn't even
	 * performance-sensitive. */
	BUG_ON(err);
	opts.we_mask = QM_INITFQ_WE_DESTWQ | QM_INITFQ_WE_FQCTRL |
		QM_INITFQ_WE_CONTEXTB | QM_INITFQ_WE_CONTEXTA;
	opts.fqd.dest.channel = channel;
	opts.fqd.dest.wq = HLP_PRIORITY_2TX;
	opts.fqd.fq_ctrl = 0;
	opts.fqd.fq_ctrl |= QM_FQCTRL_PREFERINCACHE;
	opts.fqd.fq_ctrl |= QM_FQCTRL_FORCESFDR;
	opts.fqd.context_b = context_b;
	qm_fqd_context_a_set64(&opts.fqd, context_a);
	err = qman_init_fq(fq, QMAN_INITFQ_FLAG_SCHED, &opts);
	BUG_ON(err);
}

static void fq_ipc_init(struct qman_fq *fq, u16 channel,
			const struct qm_fqd_stashing *stashing,
			int prefer_in_cache)
{
	struct qm_mcc_initfq opts;
	__maybe_unused int ret;

	fq->cb.dqrr = cb_dqrr_rx_hash;

	ret = qman_create_fq(0, QMAN_FQ_FLAG_DYNAMIC_FQID |
				 QMAN_FQ_FLAG_NO_ENQUEUE, fq);
	BUG_ON(ret);

	/* FQ to dedicated channel */
	memset(&opts, 0, sizeof(opts));
	opts.we_mask = QM_INITFQ_WE_DESTWQ | QM_INITFQ_WE_FQCTRL |
		QM_INITFQ_WE_CONTEXTA;
	opts.fqd.dest.channel = channel;
	opts.fqd.dest.wq = HLP_PRIORITY_2FWD;
	opts.fqd.fq_ctrl = QM_FQCTRL_CTXASTASHING;
	opts.fqd.context_a.stashing = *stashing;
	if (prefer_in_cache)
		opts.fqd.fq_ctrl |= QM_FQCTRL_PREFERINCACHE;

	ret = qman_init_fq(fq, QMAN_INITFQ_FLAG_SCHED, &opts);
	BUG_ON(ret);
}

int local_tx_fq_init(void)
{
	helper_local_fq.cb.ern = cb_dqrr_ern;
	return qman_create_fq(1, QMAN_FQ_FLAG_NO_MODIFY, &helper_local_fq);
}

void local_tx_fq_clean(void)
{
	helper_local_fq.fqid = 1;
	fsl_fq_clean(&helper_local_fq);
}

struct qman_fq *local_tx_fq_get(void)
{
	return &helper_local_fq;
}

struct packet_interface *packet_interface_get(u32 port)
{
	return helper_if_table[port];
}

void packet_interface_set(u32 port, struct packet_interface *i)
{
	if (unlikely(port >= PACKET_IF_NUM_MAX)) {
		TRACE("invalid port %d\n", port);
		return;
	}
	helper_if_table[port] = i;
}

inline int packet_interface_poll(void)
{
	if (!(helper_slow_poll--)) {
		if (qman_poll_slow() || bman_poll_slow())
			helper_slow_poll = HLP_SLOWPOLL_BUSY;
		else
			helper_slow_poll = HLP_SLOWPOLL_IDLE;
	}
	return qman_poll_dqrr(HLP_FASTPOLL_DQRR);
}

inline int packet_interface_enqueue(struct qman_fq *fq,
					struct qm_dqrr_entry *dqrr)
{
	int ret = qman_cb_dqrr_consume;
	struct packet_queue *pqueue = &helper_pkt_queue;
	struct packet_desc *pdesc = (struct packet_desc *)
		fsl_packet_desc_alloc(ptov(dqrr->fd.addr), 0);
	struct interface_fq *p = container_of(fq, struct interface_fq, fq);

	packet_interface_fd_2_pd(&dqrr->fd, pdesc);
	pdesc->port = p->port;
	pdesc->queue = 0;

#if defined(FSL_FQ_HOLDACTIVE_ENABLE) || defined(FSL_FQ_ORP_ENABLE)
	{
		/* assign private info */
		struct packet_desc_private *pdesc_priv =
			(struct packet_desc_private *)pdesc->priv;
		pdesc_priv->fq_type = p->fq_type;
		pdesc_priv->fq = (u32) fq;
		pdesc_priv->dqrr = (u32) dqrr;
		pdesc_priv->resvd = 0;

		if (p->fq_type & QM_FQCTRL_HOLDACTIVE) {
			/* a references to DQRR entry */
			pdesc_priv->dqrr = (u32) dqrr;
			ret = qman_cb_dqrr_defer;
		} else if (p->fq_type & QM_FQCTRL_ORP) {
			/* dqrr sequence number */
			pdesc_priv->seqnum = dqrr->seqnum;
			ret = qman_cb_dqrr_consume;
		}
	}
#endif

	pqueue->pkt[pqueue->pi] = pdesc;
	++pqueue->pi;
	DEBUG("-0->: pi %d, ci %d\n", pqueue->pi, pqueue->ci);

	return ret;
}

/*
 * [eugene]
 * The process is not a safe operation, the caller
 * need copy the returned packet descriptor before
 * the next dequeue.
 */
inline int packet_interface_dequeue(struct packet_desc **pkt)
{
	struct packet_queue *pqueue = &helper_pkt_queue;

	/* return the first buffered packet */
	if (likely(pqueue->ci < pqueue->pi)) {
		*pkt = pqueue->pkt[pqueue->ci];
		++pqueue->ci;
		DEBUG("-1-<: pi %d, ci %d\n", pqueue->pi, pqueue->ci);
		return 1;
	}

	/* reset the index and poll next round */
	pqueue->pi = 0;
	pqueue->ci = 0;
	packet_interface_poll();

	if (pqueue->ci < pqueue->pi) {
		*pkt = pqueue->pkt[pqueue->ci];
		++pqueue->ci;
		DEBUG("-0-<: pi %d, ci %d\n", pqueue->pi, pqueue->ci);
		return 1;
	}

	return 0;
}

/*
 * initialize the default parameters for fman
 * interface
 */
int packet_interface_init(struct interface_param_internal *param)
{
	int loop;
	u64 context_a = 0;
	u32 context_b = 0;
	u32 flags = 0;
	struct qm_fqd_stashing stash_opts = default_stash_opts;
	size_t size = sizeof(struct packet_interface);
	struct packet_interface *i = DMA_MEM_ALLOC(L1_CACHE_BYTES, size);
	if (!i)
		return -ENOMEM;

	TRACE("init interface for port [0x%02x]\n", param->port);

	/* create the interface structure for IPC */
	if (param->fm_if == NULL) {
		memset(i, 0, size);
		i->port = param->port;
		packet_interface_set(param->port, i);
		return 0;
	}

	/* for 1G, 10G and offline ports */
	memset(i, 0, size);
	i->port = param->port;
	i->fm_if = param->fm_if;
	i->tx_error.port = param->port;
	i->tx_confirm.port = param->port;
	i->rx_error.port = param->port;
	i->rx_default.port = param->port;
	i->num_tx_fqs = param->num_tx_fqs;
	INIT_LIST_HEAD(&i->rx_fqs_list);

	/* init tx FQs */
	i->tx = DMA_MEM_ALLOC(L1_CACHE_BYTES, sizeof(*i->tx) * i->num_tx_fqs);
	if (!i->tx) {
		DMA_MEM_FREE(i);
		return -ENOMEM;
	}
	memset(i->tx, 0, sizeof(*i->tx) * i->num_tx_fqs);

#ifdef HLP_TX_CONFIRM
	context_b = i->fm_if->fqid_tx_confirm;
#else
	context_a = (u64) 1 << 63;
	if (!(flags & HLP_TX_FQ_NO_BUF_DEALLOC)) {
		context_a |= ((u64) fman_dealloc_bufs_mask_hi << 32) |
			(u64) fman_dealloc_bufs_mask_lo;
	}
	if (flags & HLP_TX_FQ_NO_CHECKSUM)
		context_a |= FMAN_CONTEXTA_DIS_CHECKSUM;
#endif

	/* clear the context a setting for offline port */
	if (i->fm_if->mac_type == fman_offline) {
		context_a = 0;

		/* enable UDP length update for fman-v3 */
		if (fman_ip_rev >= FMAN_V3) {
#define FMAN_V3_CONTEXTA_A1_VALID   0x20000000
#define FMAN_V3_CONTEXTA_A1_OPCODE  0x1

			context_a = (FMAN_V3_CONTEXTA_A1_VALID |
				 FMAN_V3_CONTEXTA_A1_OPCODE);
			context_a <<= 32;
			DEBUG("context_a setting 0x%llx\n", context_a);
		}
	}

	for (loop = 0; loop < i->num_tx_fqs; loop++) {
		struct qman_fq *fq = &i->tx[loop].fq;
		i->tx[loop].port = param->port;
		fq_tx_init(fq, i->fm_if->tx_channel_id, context_a, context_b);

		TRACE("create tx FQ [0x%x], tx_channel [0x%x]\n",
			  fq->fqid, i->fm_if->tx_channel_id);
		DEBUG("create tx FQ, ctxta 0x%llx, ctxtb 0x%x\n",
			  context_a, context_b);
	}

	/* init error FQ only for offline port */
	if (i->fm_if->mac_type == fman_offline) {
		TRACE("create oh error FQ, [0x%x]\n", i->fm_if->fqid_rx_err);
		fq_nonpcd_init(&i->rx_error.fq, i->fm_if->fqid_rx_err,
				   param->pool_channel, &stash_opts,
				   cb_dqrr_rx_error);
	} else {
		TRACE("create tx error and confirm FQs, [0x%x : 0x%x]\n",
			  i->fm_if->fqid_tx_err, i->fm_if->fqid_tx_confirm);
		fq_nonpcd_init(&i->tx_error.fq, i->fm_if->fqid_tx_err,
				   param->pool_channel, &stash_opts,
				   cb_dqrr_tx_error);
		fq_nonpcd_init(&i->tx_confirm.fq, i->fm_if->fqid_tx_confirm,
				   param->pool_channel, &stash_opts,
				   cb_dqrr_tx_confirm);

		/* init rx FQs */
		TRACE("create rx error and default FQs, [0x%x : 0x%x]\n",
			  i->fm_if->fqid_rx_err, param->fqid_rx_default);
		/* init rx error & default FQs (configured by DTS) */
		fq_nonpcd_init(&i->rx_error.fq, i->fm_if->fqid_rx_err,
				   param->pool_channel, &stash_opts,
				   cb_dqrr_rx_error);
		fq_nonpcd_init(&i->rx_default.fq, param->fqid_rx_default,
				   param->pool_channel, &stash_opts,
				   cb_dqrr_rx_default);
	}

	/* enable rx */
	fman_if_enable_rx(i->fm_if);

	packet_interface_set(param->port, i);

	return 0;
}

void packet_interface_clean(u32 port)
{
	int loop;
	struct packet_interface *i = packet_interface_get(port);

	if (i == NULL) {
		DEBUG("invalid interface, port [0x%02x]\n", port);
		return;
	}

	TRACE("free interface resource for port [0x%02x]\n", port);
	if (i->fm_if) {
		/* release rx & tx memory and FQs */
		fman_if_disable_rx(i->fm_if);

		/* release tx FQs, error, confirm and tx DC */
		if (i->fm_if->mac_type == fman_offline) {
			DEBUG("free oh error FQ\n");
			fsl_fq_clean(&i->rx_error.fq);
		} else {
			DEBUG("free rx default and error FQs\n");
			fsl_fq_clean(&i->rx_default.fq);
			fsl_fq_clean(&i->rx_error.fq);

			DEBUG("free tx confirm and error FQs\n");
			fsl_fq_clean(&i->tx_confirm.fq);
			fsl_fq_clean(&i->tx_error.fq);
		}

		DEBUG("free tx FQs\n");
		for (loop = 0; loop < i->num_tx_fqs; loop++)
			fsl_fq_clean(&i->tx[loop].fq);
		DMA_MEM_FREE(i->tx);
	}

	/* free the interface memory */
	DMA_MEM_FREE(i);
	packet_interface_set(port, NULL);
}

/*
 * initialize PCD parameters for specific interfaces
 */
int packet_interface_pcd_init(struct interface_pcd_param *param)
{
	int ret = 0;
	int idx;
	int loop;
	size_t size;
	struct interface_fq_list *fq_node;
	struct interface_fq_range *fqr = param->rx_fq_range;
	struct qm_fqd_stashing stash_opts = default_stash_opts;
	struct packet_interface *i = packet_interface_get(param->port);

	if (i == NULL) {
		DEBUG("invalid interface, port [0x%02x]\n", param->port);
		return 0;
	}

	/* init rx FQs configured by PCD */
	TRACE("init interface PCD for port [0x%02x]\n", param->port);
	for (idx = 0; idx < param->num_rx_fq_ranges; idx++, fqr++) {
		size = sizeof(struct interface_fq_list) +
			fqr->count * sizeof(struct interface_fq);
		fq_node = DMA_MEM_ALLOC(L1_CACHE_BYTES, size);
		if (!fq_node) {
			ret = -ENOMEM;
			break;
		}

		memset(fq_node, 0, size);
		INIT_LIST_HEAD(&fq_node->list);
		fq_node->start = fqr->start;
		fq_node->count = fqr->count;
		TRACE("create PCD[%d] FQs, start 0x%x, count %d\n",
			idx, fqr->start, fqr->count);
		for (loop = 0; loop < fqr->count; loop++) {
			DEBUG("init fq 0x%x\n", fqr->start + loop);

			fq_node->rx_hash[loop].fq_type = fqr->fq_type;
			fq_node->rx_hash[loop].port = param->port;
			fq_pcd_init(&fq_node->rx_hash[loop].fq,
					(fqr->start + loop), fqr->fq_type,
					fqr->channel, &stash_opts, HLP_RX_PIC);
		}
		list_add_tail(&fq_node->list, &i->rx_fqs_list);
	}

	if (ret < 0) {
		TRACE("init interface failed, ret %d\n", ret);
		packet_interface_pcd_clean(param->port);
		return ret;
	}

	return 0;
}

void packet_interface_pcd_clean(u32 port)
{
	int loop;
	struct interface_fq_list *fq_curr;
	struct interface_fq_list *fq_next;
	struct packet_interface *i = packet_interface_get(port);

	if (i == NULL) {
		DEBUG("invalid interface, port [0x%02x]\n", port);
		return;
	}

	TRACE("free interface pcd resource for port [0x%02x]\n", port);
	list_for_each_entry_safe(fq_curr, fq_next, &i->rx_fqs_list, list) {
		DEBUG("free %d rx FQs\n", fq_curr->count);
		for (loop = 0; loop < fq_curr->count; loop++)
			fsl_fq_clean(&fq_curr->rx_hash[loop].fq);
		DMA_MEM_FREE(fq_curr);
	}
}

/* [eugene]
 *
 * The IPC interface is inited with dedicated channel.
 *
 *  [thread_a : port_b]       [thread_x .. thread_y]
 *       ^                              |
 *       |                              |
 *  [dedicated_channel]                 |
 *       |                              |
 *       | (if.tx[0].fq)                |
 *       \--------------- < ------------/
 */
int packet_interface_ipc_init(struct interface_ipc_param *param)
{
	int loop;
	struct qm_fqd_stashing stash_opts = default_stash_opts;
	struct packet_interface *i = packet_interface_get(param->port);
	if (i == NULL) {
		DEBUG("invalid interface, port [0x%02x]\n", param->port);
		return 0;
	}

	TRACE("init IPC interface to thread [%d], port [0x%02x]\n",
		  param->thread_id, param->port);
	memset(i, 0, sizeof(struct packet_interface));
	i->port = param->port;
	i->num_tx_fqs = param->num_tx_fqs;
	i->tx = DMA_MEM_ALLOC(L1_CACHE_BYTES, sizeof(*i->tx) * i->num_tx_fqs);
	if (!i->tx) {
		DMA_MEM_FREE(i);
		return -ENOMEM;
	}

	/* default FQ type */
	memset(i->tx, 0, sizeof(*i->tx) * i->num_tx_fqs);
	for (loop = 0; loop < i->num_tx_fqs; loop++) {
		i->tx[loop].port = param->port;
		fq_ipc_init(&i->tx[loop].fq,
				qman_affine_channel(param->thread_id),
				&stash_opts, HLP_RX_PIC);
		DEBUG("create FQ [0x%x] to thread [%d], port [0x%02x]\n",
			  i->tx[loop].fq.fqid, param->thread_id, param->port);
	}

	return 0;
}

void packet_interface_ipc_clean(u32 port)
{
	int loop;
	struct packet_interface *i = packet_interface_get(port);

	if (i == NULL) {
		DEBUG("invalid interface, port [0x%02x]\n", port);
		return;
	}

	TRACE("free interface ipc resource for port [0x%02x]\n", port);
	for (loop = 0; loop < i->num_tx_fqs; loop++)
		fsl_fq_clean(&i->tx[loop].fq);
	DMA_MEM_FREE(i->tx);
}

/*
 * Channel & FQID mapping
 *
 *  - FM0
 *    lni A0
 *    channel A00 - 0xf00000, CQ0-CQ15
 *    channel A01 - 0xf00010, CQ0-CQ15
 *    channel A02 - 0xf00020, CQ0-CQ15
 *    lni A1
 *    channel A10 - 0xf00030, CQ0-CQ15
 *    channel A11 - 0xf00040, CQ0-CQ15
 *    lni A2
 *    channel A20 - 0xf00050, CQ0-CQ15
 *
 *  - FM1
 *    lni B0
 *    channel B00 - 0xf10000, CQ0-CQ15
 *    channel B01 - 0xf10010, CQ0-CQ15
 *    lni B1
 *    channel B10 - 0xf10020, CQ0-CQ15
 *    channel B11 - 0xf10030, CQ0-CQ15
 *    channel B12 - 0xf10040, CQ0-CQ15
 *
 */
int packet_interface_ceetm_init(struct interface_ceetm_param *param)
{
	static u32 ceetm_fqid_base[2] = {
		CEETM_FQID_BASE,    /* fman0 */
		CEETM_FQID_BASE + 0x10000   /* fman1 */
	};
	u64 ctx_a = 0x9200000080000000ull;

	int ret, j, k;
	u32 spid, lnid, fmid;
	struct qm_ceetm_rate token_rate;
	struct qm_ceetm_channel *channel = NULL;
	struct qm_ceetm_cq *cq = NULL;
	struct qm_ceetm_lfq *lfq = NULL;
	struct ceetm_channel_config *chan_cfg = NULL;
	struct packet_interface *i = packet_interface_get(param->port);
	struct interface_ceetm *cif;

	if (i == NULL) {
		DEBUG("invalid interface, port [0x%02x]\n", param->port);
		return 0;
	}

	TRACE("init ceetm on port [0x%02x]\n", param->port);
	if (i->fm_if->mac_type != fman_mac_10g &&
		i->fm_if->mac_type != fman_mac_1g) {
		TRACE("NO ceetm support on port 0x%x\n", param->port);
		return 0;
	}

	cif = DMA_MEM_ALLOC(L1_CACHE_BYTES, sizeof(struct interface_ceetm));
	if (cif == NULL)
		return -ENOMEM;
	memset(cif, 0, sizeof(struct interface_ceetm));

	/* calculate fqid base */
	fmid = i->fm_if->fman_idx;
	cif->fqid_base = ceetm_fqid_base[fmid];

	/* claim the sp & lni */
	spid = i->fm_if->tx_channel_id & 0xF;
	ret = qman_ceetm_sp_claim(&cif->sp, fmid, spid);
	if (ret) {
		TRACE("fail to claim sp %d, err 0x%x\n", spid, ret);
		return ret;
	}

	/* lni 0/1 is optimized for 10G port */
	lnid = (i->fm_if->mac_type == fman_mac_10g) ? 0 : 2;
	for (; lnid < CEETM_LNI_NUM; lnid++) {
		ret = qman_ceetm_lni_claim(&cif->lni, fmid, lnid);
		if (ret)
			continue;
	}
	if (ret) {
		TRACE("fail to claim lni %d, err 0x%x\n", lnid, ret);
		return ret;
	}

	ret = qman_ceetm_sp_set_lni(cif->sp, cif->lni);
	if (ret) {
		TRACE("fail to bundle sp and lni, err 0x%x\n", ret);
		return ret;
	}

	DEBUG("initialize CEETM for fm%d-mac%d, spid %d, lni %d\n",
		  fmid, i->fm_if->mac_idx, spid, lnid);
	/* configure lni shaper */
	if (param->lni.shaper.enabled) {
		ret =
			qman_ceetm_lni_enable_shaper(cif->lni,
						 param->lni.shaper.coupled,
						 CEETM_LNI_OAL);
		if (ret) {
			TRACE("fail to set shaper to lni, err 0x%x\n", ret);
			return ret;
		}
	}

	qman_ceetm_bps2tokenrate(param->lni.shaper.cr, &token_rate,
				 CEETM_BPS_ROUNDING);
	ret = qman_ceetm_lni_set_commit_rate(cif->lni, &token_rate,
					   param->lni.shaper.cr_token_limit);
	if (ret) {
		TRACE("fail to set lni commit rate, err 0x%x\n", ret);
		return ret;
	}
	qman_ceetm_bps2tokenrate(param->lni.shaper.er, &token_rate,
				 CEETM_BPS_ROUNDING);
	ret = qman_ceetm_lni_set_excess_rate(cif->lni, &token_rate,
					   param->lni.shaper.er_token_limit);
	if (ret) {
		TRACE("fail to set lni excess rate, err 0x%x\n", ret);
		return ret;
	}

	DEBUG("configure the ceetm channels\n");

	/* Create channels and class queues according to CEETM cfg file */
	for (j = 0; j < param->num_of_channels; j++) {
		int last_cq_idx = 0;

		chan_cfg = &param->channel[j];

		/* claim and configure channel */
		ret = qman_ceetm_channel_claim(&channel, cif->lni);
		if (ret) {
			TRACE("Fail to claim channel-%d, err 0x%x\n", j, ret);
			return ret;
		}

		if (chan_cfg->shaper.enabled) {
			ret = qman_ceetm_channel_enable_shaper(channel,
					chan_cfg->shaper.coupled);
			if (ret) {
				TRACE("Fail to set shaper for channel%d\n", j);
				return ret;
			}

			qman_ceetm_bps2tokenrate(chan_cfg->shaper.cr,
						&token_rate,
						CEETM_BPS_ROUNDING);
			ret = qman_ceetm_channel_set_commit_rate(channel,
					&token_rate,
					chan_cfg->shaper.cr_token_limit);
			if (ret) {
				TRACE("fail to set channel commit rate\n");
				return ret;
			}
			qman_ceetm_bps2tokenrate(chan_cfg->shaper.er,
						&token_rate,
						CEETM_BPS_ROUNDING);
			ret = qman_ceetm_channel_set_excess_rate(channel,
					&token_rate,
					chan_cfg->shaper.er_token_limit);
			if (ret) {
				TRACE("fail to set channel excess rate\n");
				return ret;
			}
		}

		/* configure the groups */
		if (chan_cfg->num_of_groups == 1) {
			ret = qman_ceetm_channel_set_group(channel, 0,
				chan_cfg->group[0].index, 0);
		} else if (chan_cfg->num_of_groups == 2) {
			ret = qman_ceetm_channel_set_group(channel, 1,
				chan_cfg->group[0].index,
				chan_cfg->group[1].index);
		}
		if (ret) {
			TRACE("fail to set group for cq channel\n");
			return ret;
		}

		/* configure class queue */
		DEBUG("configure the ceetm CQs for channel %d\n", j);
		for (k = 0; k < chan_cfg->num_of_cqs; k++) {
			struct ceetm_cq_config *cq_cfg = &chan_cfg->cq[k];
			struct qm_ceetm_ccg *ccg = 0;
			struct qm_ceetm_weight_code wcode;

			/* configure CCGR */
			if (cq_cfg->ccg_params) {
				ret = qman_ceetm_ccg_claim(&ccg, channel,
							 cq_cfg->index, NULL,
							 NULL);
				if (ret) {
					TRACE("Fail to claim CCGR%d\n",
						 cq_cfg->index);
					return ret;
				}

				ret = qman_ceetm_ccg_set(ccg, cq_cfg->ccg_mask,
							   cq_cfg->ccg_params);
				if (ret) {
					TRACE("Fail to set CCGR%d\n",
						  cq_cfg->index);
					return ret;
				}
			}

			/* configure class queue, supports 1 group with
			 * 8 CQs, or 2 groups 4CQs each
			 */
			if (cq_cfg->index < 8) {
				ret = qman_ceetm_cq_claim(&cq, channel,
							cq_cfg->index, ccg);
			} else if (cq_cfg->index < 12) {
				ret = qman_ceetm_cq_claim_A(&cq, channel,
							  cq_cfg->index, ccg);
			} else {
				if (chan_cfg->num_of_groups == 2)
					ret = qman_ceetm_cq_claim_B(&cq,
							channel, cq_cfg->index,
							ccg);
				else
					ret = qman_ceetm_cq_claim_A(&cq,
							channel, cq_cfg->index,
							ccg);
			}
			if (ret) {
				TRACE("Fail to claim CQ%d\n",
					cq_cfg->index);
				return ret;
			}

			/* configure weight code for grouped class */
			if (cq_cfg->index >= 8) {
				wcode.x = cq_cfg->weightx;
				wcode.y = cq_cfg->weighty;
				ret = qman_ceetm_set_queue_weight(cq, &wcode);
				if (ret) {
					TRACE("Fail to set weight for cq%d\n",
						k);
					return ret;
				}
			}

			/* Number of CQs may be less than 16, but number
			 * of lfqs(FQID) is set to 16 in order to calculate
			 * TX FQID conveniently
			 */
			for (; last_cq_idx <= cq->idx; last_cq_idx++) {
				ret = qman_ceetm_lfq_claim(&lfq, cq);
				if (ret) {
					TRACE("fail to claim lfq for cq%d\n",
						cq->idx);
					return ret;
				}

				ret = qman_ceetm_lfq_set_context(lfq, ctx_a, 0);
				if (ret) {
					TRACE("fail to set context for lfq%d\n",
						lfq->idx);
					return ret;
				}

				TRACE("Claim lfq 0x%x to CQ %d\n", lfq->idx,
					  cq->idx);
			}
		}

		/* create 16 lfqs for each channel */
		for (; last_cq_idx < CEETM_CQ_NUM; last_cq_idx++) {
			ret = qman_ceetm_lfq_claim(&lfq, cq);
			if (ret) {
				TRACE("fail to claim lfq for cq%d\n",
					cq->idx);
				return ret;
			}

			ret = qman_ceetm_lfq_set_context(lfq, ctx_a, 0);
			if (ret) {
				TRACE("fail to set context for lfq%d\n",
					lfq->idx);
				return ret;
			}

			DEBUG("Claim lfq 0x%x to CQ %d as reserved\n",
				lfq->idx, cq->idx);
		}
	}

	/* next fqid base */
	ceetm_fqid_base[fmid] += param->num_of_channels * CEETM_CQ_NUM;

	i->ceetm_if = cif;

	DEBUG("ceetm configuration done\n");

	return 0;
}

void packet_interface_ceetm_clean(u32 port)
{
	struct interface_ceetm *cif;
	struct qm_ceetm_channel *channel;
	struct packet_interface *i = packet_interface_get(port);

	if (i == NULL || i->ceetm_if == NULL) {
		DEBUG("invalid interface, port [0x%02x]\n", port);
		return;
	}

	/* clean up ceetm configuration */
	TRACE("clear ceetm resources on port [0x%02x]\n", port);
	cif = i->ceetm_if;
	list_for_each_entry(channel, &cif->lni->channels, node) {
		struct qm_ceetm_cq *cq;
		struct qm_ceetm_ccg *ccg;

		list_for_each_entry(ccg, &channel->ccgs, node) {
			qman_ceetm_ccg_release(ccg);
		}

		list_for_each_entry(cq, &channel->class_queues, node) {
			struct qm_ceetm_lfq *lfq;

			list_for_each_entry(lfq, &cq->bound_lfqids, node) {
				qman_ceetm_lfq_release(lfq);
			}

			qman_ceetm_cq_release(cq);
		}
		qman_ceetm_channel_release(channel);
	}

	qman_ceetm_lni_release(cif->lni);
	qman_ceetm_sp_release(cif->sp);

	DMA_MEM_FREE(cif);
}
