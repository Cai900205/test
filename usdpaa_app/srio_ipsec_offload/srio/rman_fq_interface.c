/*
 * Copyright 2014 Freescale Semiconductor, Inc.
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

#include <rman_fq_interface.h>
#include <ppac.h>

void rman_init_nonpcd_fq(struct qman_fq *fq, uint32_t fqid,
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
	RMAN_DBG("%s creates a fq(0x%x) wq(%d) channel(0x%x)",
		__func__, qman_fq_fqid(fq), opts.fqd.dest.wq,
		opts.fqd.dest.channel);
}

static enum qman_cb_dqrr_result
tx_drain_cb(struct qman_portal *qm __always_unused,
	    struct qman_fq *fq __always_unused,
	    const struct qm_dqrr_entry *dqrr)
{
	RMAN_DBG("Tx_drain: fqid=%d\tfd_status = 0x%08x", fq->fqid,
		dqrr->fd.status);
	rman_drop_frame(&dqrr->fd);
	return qman_cb_dqrr_consume;
}

void rman_init_tx_fq(struct qman_fq *fq,  uint32_t fqid,
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
	fq->cb.dqrr = tx_drain_cb;
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
#ifdef RMAN_TX_PREFERINCACHE
	opts.fqd.fq_ctrl |= QM_FQCTRL_PREFERINCACHE;
#endif
#ifdef RMAN_TX_FORCESFDR
	opts.fqd.fq_ctrl |= QM_FQCTRL_FORCESFDR;
#endif
#ifdef RMAN_CGR
	opts.we_mask |= QM_INITFQ_WE_CGID;
	opts.fqd.cgid = cgr_tx.cgrid;
	opts.fqd.fq_ctrl |= QM_FQCTRL_CGE;
#endif
	qm_fqd_context_a_set64(&opts.fqd, cont_a);
	opts.fqd.context_b = cont_b;
	err = qman_init_fq(fq, QMAN_INITFQ_FLAG_SCHED, &opts);
	BUG_ON(err);
	RMAN_DBG("%s creates a fq(0x%x) wq(%d) channel(0x%x)",
		__func__, qman_fq_fqid(fq), opts.fqd.dest.wq,
		opts.fqd.dest.channel);
}
