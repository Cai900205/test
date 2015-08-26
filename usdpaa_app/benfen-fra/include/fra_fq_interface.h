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

#ifndef _FRA_FQ_INTERFACE_H
#define _FRA_FQ_INTERFACE_H

#include <fra_common.h>
#include <usdpaa/compat.h>
#include <stdio.h>

/* sanity check the application options for basic conflicts */
#if defined(FRA_HOLDACTIVE) && defined(FRA_AVOIDBLOCK)
#error "HOLDACTIVE and AVOIDBLOCK options are mutually exclusive"
#endif
#if defined(FRA_ORDER_PRESERVATION) && !defined(FRA_HOLDACTIVE)
#error "ORDER_PRESERVATION requires HOLDACTIVE"
#endif

/* The forwarding logic uses a per-cpu FQ object for handling enqueues (and
 * ERNs), irrespective of the destination FQID. In this way, cache-locality is
 * more assured, and any ERNs that do occur will show up on the same CPUs they
 * were enqueued from. This works because ERN messages contain the FQID of the
 * original enqueue operation, so in principle any demux that's required by the
 * ERN callback can be based on that. Ie. the FQID set within "local_fq" is from
 * whatever the last executed enqueue was, the ERN handler can ignore it. */
extern __thread struct qman_fq local_fq;

#if defined(FRA_ORDER_PRESERVATION) || \
	defined(FRA_ORDER_RESTORATION)
extern __thread const struct qm_dqrr_entry *local_dqrr;
#endif

#ifdef FRA_ORDER_RESTORATION
extern __thread u32 local_orp_id;
extern __thread u32 local_seqnum;
#endif

#ifdef FRA_2FWD_ORDER_PRESERVATION
/* Similarly, FRA APIs to send/drop a frame use this state in order to support
 * order preservation. */
extern __PERCPU const struct qm_dqrr_entry *local_dqrr;
#endif

#ifdef FRA_CGR
/* A congestion group to hold Rx FQs (uses netcfg::cgrids[0]) */
extern struct qman_cgr cgr_rx;
/* Tx FQs go into a separate CGR (uses netcfg::cgrids[1]) */
extern struct qman_cgr cgr_tx;
#endif

#if defined(FRA_ORDER_PRESERVATION) || \
	defined(FRA_ORDER_RESTORATION)
#define PRE_DQRR()  local_dqrr = dqrr
#define POST_DQRR() (local_dqrr ? qman_cb_dqrr_consume : qman_cb_dqrr_defer)
#else
#define PRE_DQRR()  do { ; } while (0)
#define POST_DQRR() qman_cb_dqrr_consume
#endif

#ifdef FRA_ORDER_RESTORATION
#define PRE_ORP(orpid, seqnum) \
	do { \
		local_orp_id = orpid; \
		local_seqnum = seqnum; \
	} while (0)

#define POST_ORP() \
	do { \
		local_orp_id = 0; \
	} while (0)
#else
#define PRE_ORP(orpid, seqnum) do { ; } while (0)
#define POST_ORP()             do { ; } while (0)
#endif

/*****************************/
/* Packet-handling APIs */
/****************************/
static inline void fra_drop_frame(const struct qm_fd *fd)
{
#ifdef FRA_ORDER_RESTORATION
	int ret;
	/*
	 * The "ORP object" passed to qman_enqueue_orp() is only used to extract
	 * the ORPID, so declare a temporary object to provide that.
	 */
	struct qman_fq tmp_orp = {
		.fqid = local_orp_id
	};
	local_fq.fqid = local_orp_id;
#endif

	bpool_fd_free(fd);
	FRA_DBG("drop: bpid %d <-- 0x%llx", fd->bpid, qm_fd_addr(fd));

#ifdef FRA_ORDER_RESTORATION
	/*
	 * Perform a "HOLE" enqueue so that the ORP doesn't wait for the
	 * sequence number that we're dropping.
	 */
	if (!local_orp_id)
		return;
retry_orp:
	ret = qman_enqueue_orp(&local_fq, fd, QMAN_ENQUEUE_FLAG_HOLE,
			       &tmp_orp, local_seqnum);
	if (ret) {
		cpu_spin(FRA_BACKOFF_CYCLES);
		goto retry_orp;
	}
	FRA_DBG("drop: fqid %d <-- 0x%x (HOLE)",
		local_fq.fqid, local_seqnum);
#endif
}

#ifdef FRA_ORDER_PRESERVATION
#define EQ_FLAGS() (QMAN_ENQUEUE_FLAG_DCA | \
		    QMAN_ENQUEUE_FLAG_DCA_PTR(local_dqrr))
#else
#define EQ_FLAGS() 0
#endif

static inline void fra_send_frame(u32 fqid, const struct qm_fd *fd)
{
	int ret;
	local_fq.fqid = fqid;
retry:
#ifdef FRA_ORDER_RESTORATION
	if (local_orp_id) {
		/*
		 * The "ORP object" passed to qman_enqueue_orp() is only used to
		 * extract the ORPID, so declare a temporary object to provide
		 * that.
		 */
		struct qman_fq tmp_orp = {
			.fqid = local_orp_id
		};
		ret = qman_enqueue_orp(&local_fq, fd, EQ_FLAGS(), &tmp_orp,
					local_dqrr->seqnum);
		FRA_DBG("send ORP: fqid %d, orpid %d, "
			"seqnum %d <-- 0x%llx (%d)",
			local_fq.fqid, tmp_orp.fqid,
			local_dqrr->seqnum, qm_fd_addr(fd), ret);
	} else
#endif
	{
		ret = qman_enqueue(&local_fq, fd, EQ_FLAGS());
		FRA_DBG("send: fqid 0x%x <-- 0x%llx (%d)",
			local_fq.fqid, qm_fd_addr(fd), ret);
	}
	if (ret) {
		cpu_spin(FRA_BACKOFF_CYCLES);
		goto retry;
	}
#ifdef FRA_ORDER_PRESERVATION
	/*
	 * NULLing this ensures the driver won't consume the ring entry
	 * explicitly (ie. FRA's callback will return qman_cb_dqrr_defer).
	 */
	local_dqrr = NULL;
#endif
}

#ifdef FRA_CGR
int fra_cgr_ids_init(void);
uint32_t fra_cgr_rx_id(void);
uint32_t fra_cgr_tx_id(void);
int fra_cgr_rx_init(uint32_t numrxfqs);
int fra_cgr_tx_init(uint32_t numrxfqs);
void dump_cgr(const struct qm_mcr_querycgr *res);
void fra_cgr_ids_release(void);
#endif

#ifdef FRA_ORDER_RESTORATION
void fra_orp_init(u32 *orp_id);
#endif

void local_fq_init(void);

/* Initialize pcd frame queue */
void fra_fq_pcd_init(struct qman_fq *fq, uint32_t fqid,
		     uint8_t wq, u16 channel,
		     const struct qm_fqd_stashing *stashing,
		     qman_cb_dqrr cb);

/* Initialize nonpcd frame queue */
void fra_fq_nonpcd_init(struct qman_fq *fq, uint32_t fqid,
			uint8_t wq, u16 channel,
			const struct qm_fqd_stashing *stashing,
			qman_cb_dqrr cb);

/* Initialize tx frame queue */
void fra_fq_tx_init(struct qman_fq *fq,  uint32_t fqid,
		    uint8_t wq, u16 channel,
		    uint64_t cont_a, uint32_t cont_b);

/* Tear down frame queue */
void fra_teardown_fq(struct qman_fq *fq);

int init_pool_channels(void);
void finish_pool_channels(void);

#endif /* _FRA_FQ_INTERFACE_H */
