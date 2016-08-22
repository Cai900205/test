/*
 * Copyright 2012 Freescale Semiconductor, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor "AS IS" AND ANY
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

#include <fra_fq_interface.h>

/* The SDQCR mask to use (computed from pchannels) */
static uint32_t sdqcr;

/* The forwarding logic uses a per-cpu FQ object for handling enqueues (and
 * ERNs), irrespective of the destination FQID. In this way, cache-locality is
 * more assured, and any ERNs that do occur will show up on the same CPUs they
 * were enqueued from. This works because ERN messages contain the FQID of the
 * original enqueue operation, so in principle any demux that's required by the
 * ERN callback can be based on that. Ie. the FQID set within "local_fq" is from
 * whatever the last executed enqueue was, the ERN handler can ignore it. */
__thread struct qman_fq local_fq;

#if defined(FRA_ORDER_PRESERVATION) || \
	defined(FRA_ORDER_RESTORATION)
__thread const struct qm_dqrr_entry *local_dqrr;
#endif
#ifdef FRA_ORDER_RESTORATION
__thread u32 local_orp_id;
__thread u32 local_seqnum;
#endif

#ifdef FRA_CGR
/* A congestion group to hold Rx FQs (uses netcfg::cgrids[0]) */
struct qman_cgr cgr_rx;
/* Tx FQs go into a separate CGR (uses netcfg::cgrids[1]) */
struct qman_cgr cgr_tx;
#endif

static uint32_t pchannel_idx;
static uint32_t pchannels[FRA_NUM_POOL_CHANNELS];

int init_pool_channels(void)
{
	int ret;

	ret = qman_alloc_pool_range(&pchannels[0], FRA_NUM_POOL_CHANNELS,
					1, 0);
	if (ret != FRA_NUM_POOL_CHANNELS)
		return -ENOMEM;
	for (ret = 0; ret < FRA_NUM_POOL_CHANNELS; ret++) {
		sdqcr |= QM_SDQCR_CHANNELS_POOL_CONV(pchannels[ret]);
		FRA_DBG("Adding pool 0x%x to SDQCR 0x%08x",
			pchannels[ret], sdqcr);
	}
	return 0;
}

void finish_pool_channels(void)
{
	qman_release_pool_range(pchannels[0], FRA_NUM_POOL_CHANNELS);
}

u16 get_rxc(void)
{
	u16 ret = pchannels[pchannel_idx];
	pchannel_idx = (pchannel_idx + 1) % FRA_NUM_POOL_CHANNELS;
	return ret;
}

void fra_fq_nonpcd_init(struct qman_fq *fq, uint32_t fqid,
			uint8_t wq, u16 channel,
			const struct qm_fqd_stashing *stashing,
			qman_cb_dqrr cb)
{
	struct qm_mcc_initfq opts;
	__maybe_unused int ret;
	uint32_t flags;

	flags = QMAN_FQ_FLAG_NO_ENQUEUE;
	if (!fqid)
		flags |= QMAN_FQ_FLAG_DYNAMIC_FQID;

	fq->cb.dqrr = cb;
	ret = qman_create_fq(fqid, flags, fq);
	BUG_ON(ret);

	memset(&opts, 0, sizeof(struct qm_mcc_initfq));
	opts.fqd.dest.channel = channel ? channel : get_rxc();
	opts.fqd.dest.wq = wq;
	opts.we_mask = QM_INITFQ_WE_DESTWQ |
		       QM_INITFQ_WE_FQCTRL |
		       QM_INITFQ_WE_CONTEXTA;

	opts.fqd.fq_ctrl = QM_FQCTRL_CTXASTASHING;
	opts.fqd.context_a.stashing = *stashing;
	ret = qman_init_fq(fq, QMAN_INITFQ_FLAG_SCHED, &opts);
	BUG_ON(ret);
	FRA_DBG("%s creates a fq(0x%x) wq(%d) channel(0x%x)",
		__func__, qman_fq_fqid(fq), opts.fqd.dest.wq,
		opts.fqd.dest.channel);
}

void fra_fq_pcd_init(struct qman_fq *fq, uint32_t fqid,
		     uint8_t wq, u16 channel,
		     const struct qm_fqd_stashing *stashing,
		     qman_cb_dqrr cb)
{
	struct qm_mcc_initfq opts;
	__maybe_unused int ret;
	uint32_t flags;

	flags = QMAN_FQ_FLAG_NO_ENQUEUE;
	if (!fqid)
		flags |= QMAN_FQ_FLAG_DYNAMIC_FQID;

	fq->cb.dqrr = cb;
	ret = qman_create_fq(fqid, flags, fq);
	BUG_ON(ret);

	memset(&opts, 0, sizeof(struct qm_mcc_initfq));
	opts.fqd.dest.channel = channel ? channel : get_rxc();
	opts.fqd.dest.wq = wq;
	opts.we_mask = QM_INITFQ_WE_DESTWQ |
		       QM_INITFQ_WE_FQCTRL |
		       QM_INITFQ_WE_CONTEXTA;
	opts.fqd.fq_ctrl =
#ifdef FRA_HOLDACTIVE
		QM_FQCTRL_HOLDACTIVE |
#endif
#ifdef FRA_AVOIDBLOCK
		QM_FQCTRL_AVOIDBLOCK |
#endif
#ifdef FRA_PCD_PREFERINCACHE
		QM_FQCTRL_PREFERINCACHE |
#endif
		QM_FQCTRL_CTXASTASHING;
#ifdef FRA_CGR
	opts.we_mask |= QM_INITFQ_WE_CGID;
	opts.fqd.cgid = cgr_rx.cgrid;
	opts.fqd.fq_ctrl |= QM_FQCTRL_CGE;
#endif
	opts.fqd.context_a.stashing = *stashing;
	ret = qman_init_fq(fq, QMAN_INITFQ_FLAG_SCHED, &opts);
	BUG_ON(ret);
	FRA_DBG("%s creates a fq(0x%x) wq(%d) channel(0x%x)",
		__func__, qman_fq_fqid(fq), opts.fqd.dest.wq,
		opts.fqd.dest.channel);
}

#ifdef FRA_ORDER_RESTORATION
void fra_orp_init(u32 *orp_id)
{
	struct qm_mcc_initfq opts;
	struct qman_fq tmp_fq;
	int ret = qman_create_fq(0, QMAN_FQ_FLAG_DYNAMIC_FQID, &tmp_fq);
	BUG_ON(ret);

	memset(&opts, 0, sizeof(struct qm_mcc_initfq));
	opts.we_mask = QM_INITFQ_WE_FQCTRL | QM_INITFQ_WE_ORPC;
	opts.fqd.fq_ctrl = QM_FQCTRL_PREFERINCACHE | QM_FQCTRL_ORP;
	opts.fqd.orprws = FRA_ORP_WINDOW_SIZE;
	opts.fqd.oa = FRA_ORP_AUTO_ADVANCE;
	opts.fqd.olws = FRA_ORP_ACCEPT_LATE;
	ret = qman_init_fq(&tmp_fq, QMAN_INITFQ_FLAG_SCHED, &opts);
	BUG_ON(ret);
	*orp_id = tmp_fq.fqid;
}
#endif

static enum qman_cb_dqrr_result
cb_tx_drain(struct qman_portal *qm __always_unused,
	    struct qman_fq *fq __always_unused,
	    const struct qm_dqrr_entry *dqrr)
{
	FRA_DBG("Tx_drain: fqid=%d\tfd_status = 0x%08x", fq->fqid,
		dqrr->fd.status);
	fra_drop_frame(&dqrr->fd);
	return qman_cb_dqrr_consume;
}

void fra_fq_tx_init(struct qman_fq *fq,  uint32_t fqid,
		uint8_t wq, u16 channel,
		uint64_t cont_a, uint32_t cont_b)
{
	struct qm_mcc_initfq opts;
	uint32_t flags;
	__maybe_unused int err;

	/* These FQ objects need to be able to handle DQRR callbacks, when
	 * cleaning up. */
	flags = QMAN_FQ_FLAG_TO_DCPORTAL;
	if (!fqid)
		flags |= QMAN_FQ_FLAG_DYNAMIC_FQID;
	fq->cb.dqrr = cb_tx_drain;
	err = qman_create_fq(fqid, flags, fq);
	/* Note: handle errors here, BUG_ON()s are compiled out in performance
	 * builds (ie. the default) and this code isn't even
	 * performance-sensitive. */
	BUG_ON(err);

	memset(&opts, 0, sizeof(struct qm_mcc_initfq));
	opts.fqd.dest.channel = channel;
	opts.fqd.dest.wq = wq;
	opts.we_mask = QM_INITFQ_WE_DESTWQ |
		       QM_INITFQ_WE_FQCTRL |
		       QM_INITFQ_WE_CONTEXTB |
		       QM_INITFQ_WE_CONTEXTA;
	opts.fqd.fq_ctrl = 0;
#ifdef FRA_TX_PREFERINCACHE
	opts.fqd.fq_ctrl |= QM_FQCTRL_PREFERINCACHE;
#endif
#ifdef FRA_TX_FORCESFDR
	opts.fqd.fq_ctrl |= QM_FQCTRL_FORCESFDR;
#endif
#ifdef FRA_CGR
	opts.we_mask |= QM_INITFQ_WE_CGID;
	opts.fqd.cgid = cgr_tx.cgrid;
	opts.fqd.fq_ctrl |= QM_FQCTRL_CGE;
#endif
	qm_fqd_context_a_set64(&opts.fqd, cont_a);
	opts.fqd.context_b = cont_b;
	err = qman_init_fq(fq, QMAN_INITFQ_FLAG_SCHED, &opts);
	BUG_ON(err);
	FRA_DBG("%s creates a fq(0x%x) wq(%d) channel(0x%x)",
		__func__, qman_fq_fqid(fq), opts.fqd.dest.wq,
		opts.fqd.dest.channel);
}

static void cb_ern(struct qman_portal *qm __always_unused,
		   struct qman_fq *fq, const struct qm_mr_entry *msg)
{
	FRA_DBG("cb_ern: fqid=%d\tfd_status = 0x%08x\n", msg->ern.fqid,
		msg->ern.fd.status);
	PRE_ORP(msg->ern.orp, msg->ern.seqnum);
	fra_drop_frame(&msg->ern.fd);
	POST_ORP();
}

void local_fq_init(void)
{
	__maybe_unused int s;

	/* Initialise the enqueue-only FQ object for this cpu/thread. Note, the
	 * fqid argument ("1") is superfluous, the point is to mark the object
	 * as ready for enqueuing and handling ERNs, but unfit for any FQD
	 * modifications. The forwarding logic will substitute in the required
	 * FQID. */
	local_fq.cb.ern = cb_ern;
	s = qman_create_fq(1, QMAN_FQ_FLAG_NO_MODIFY, &local_fq);
	BUG_ON(s);

	/* Set the qman portal's SDQCR mask */
	qman_static_dequeue_add(sdqcr);
}

void fra_teardown_fq(struct qman_fq *fq)
{
	u32 flags;
	int s;

	if (!fq || fq->fqid < 1)
		return;

	s = qman_retire_fq(fq, &flags);
	if (s == 1) {
		/* Retire is non-blocking, poll for completion */
		enum qman_fq_state state;
		do {
			qman_poll();
			qman_fq_state(fq, &state, &flags);
		} while (state != qman_fq_state_retired);
		if (flags & QMAN_FQ_STATE_NE) {
			/* FQ isn't empty, drain it */
			s = qman_volatile_dequeue(fq, 0,
				QM_VDQCR_NUMFRAMES_TILLEMPTY);
			BUG_ON(s);
			/* Poll for completion */
			do {
				qman_poll();
				qman_fq_state(fq, &state, &flags);
			} while (flags & QMAN_FQ_STATE_VDQCR);
		}
	}
	s = qman_oos_fq(fq);
	BUG_ON(s);
	qman_destroy_fq(fq, 0);
}

#ifdef FRA_CGR
static void cgr_rx_cb(struct qman_portal *qm, struct qman_cgr *c, int congested)
{
	BUG_ON(c != &cgr_rx);

	error(0, 0, "%s: rx CGR -> congestion %s", __func__,
		congested ? "entry" : "exit");
}

static void cgr_tx_cb(struct qman_portal *qm, struct qman_cgr *c, int congested)
{
	BUG_ON(c != &cgr_tx);

	error(0, 0, "%s: tx CGR -> congestion %s", __func__,
		congested ? "entry" : "exit");
}

int fra_cgr_ids_init(void)
{
	uint32_t cgrids[2];
	int err;

	err = qman_alloc_cgrid_range(&cgrids[0], 2, 1, 0);
	if (err != 2) {
		cgr_rx.cgrid = -1;
		cgr_tx.cgrid = -1;
		fprintf(stderr, "error: insufficient CGRIDs available\n");
		return -EINVAL;
	}

	cgr_rx.cgrid = cgrids[0];
	cgr_tx.cgrid = cgrids[1];
	return 0;
}

uint32_t fra_cgr_rx_id(void)
{
	return cgr_rx.cgrid;
}

uint32_t fra_cgr_tx_id(void)
{
	return cgr_tx.cgrid;
}

int fra_cgr_rx_init(uint32_t numrxfqs)
{
	struct qm_mcc_initcgr opts = {
		.we_mask = QM_CGR_WE_CS_THRES |
#ifdef FRA_CSCN
			   QM_CGR_WE_CSCN_EN |
#endif
#ifdef FRA_CSTD
			   QM_CGR_WE_CSTD_EN |
#endif
			   QM_CGR_WE_MODE,
		.cgr = {
#ifdef FRA_CSCN
			.cscn_en = QM_CGR_EN,
#endif
#ifdef FRA_CSTD
			.cstd_en = QM_CGR_EN,
#endif
			.mode = QMAN_CGR_MODE_FRAME
		}
	};

	if (-1 == cgr_rx.cgrid)
		return -EINVAL;
	/* Set up Rx CGR */
	qm_cgr_cs_thres_set64(&opts.cgr.cs_thres,
			      numrxfqs * FRA_CGR_RX_PERFQ_THRESH, 0);
	cgr_rx.cb = cgr_rx_cb;

	return qman_create_cgr(&cgr_rx, QMAN_CGR_FLAG_USE_INIT, &opts);
}

int fra_cgr_tx_init(uint32_t numtxfqs)
{
	struct qm_mcc_initcgr opts = {
		.we_mask = QM_CGR_WE_CS_THRES |
#ifdef FRA_CSCN
			   QM_CGR_WE_CSCN_EN |
#endif
#ifdef FRA_CSTD
			   QM_CGR_WE_CSTD_EN |
#endif
			   QM_CGR_WE_MODE,
		.cgr = {
#ifdef FRA_CSCN
			.cscn_en = QM_CGR_EN,
#endif
#ifdef FRA_CSTD
			.cstd_en = QM_CGR_EN,
#endif
			.mode = QMAN_CGR_MODE_FRAME
		}
	};

	if (-1 == cgr_tx.cgrid)
		return -EINVAL;

	/* Set up Tx CGR */
	qm_cgr_cs_thres_set64(&opts.cgr.cs_thres,
			      numtxfqs * FRA_CGR_TX_PERFQ_THRESH, 0);
	cgr_tx.cb = cgr_tx_cb;

	return qman_create_cgr(&cgr_tx, QMAN_CGR_FLAG_USE_INIT, &opts);
}

void dump_cgr(const struct qm_mcr_querycgr *res)
{
	uint64_t val64;

	printf("\tcscn_en: %d\n", res->cgr.cscn_en);
	printf("\tcscn_targ: 0x%08x\n", res->cgr.cscn_targ);
	printf("\tcstd_en: %d\n", res->cgr.cstd_en);
	printf("\tcs: %d\n", res->cgr.cs);
	val64 = qm_cgr_cs_thres_get64(&res->cgr.cs_thres);
	printf("\tcs_thresh: 0x%02x_%04x_%04x\n", (uint32_t)(val64 >> 32),
		(uint32_t)(val64 >> 16) & 0xffff, (uint32_t)val64 & 0xffff);
	printf("\tmode: %d\n", res->cgr.mode);
	val64 = qm_mcr_querycgr_i_get64(res);
	printf("\ti_bcnt: 0x%02x_%04x_%04x\n", (uint32_t)(val64 >> 32),
		(uint32_t)(val64 >> 16) & 0xffff, (uint32_t)val64 & 0xffff);
	val64 = qm_mcr_querycgr_a_get64(res);
	printf("\ta_bcnt: 0x%02x_%04x_%04x\n", (uint32_t)(val64 >> 32),
		(uint32_t)(val64 >> 16) & 0xffff, (uint32_t)val64 & 0xffff);
}

void fra_cgr_ids_release(void)
{
	qman_release_cgrid_range(cgr_rx.cgrid, 1);
	qman_release_cgrid_range(cgr_tx.cgrid, 1);
	qman_delete_cgr(&cgr_rx);
	qman_delete_cgr(&cgr_tx);
}

#endif
