/* Copyright (c) 2010,2012 Freescale Semiconductor, Inc.
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

/* This file should be included by exactly one of the files compiled into the
 * application, after declaring all the required definitions. */

#include <ppac.h>
#include <ppac_interface.h>

/* This struct holds the default stashing opts for Rx FQ configuration. PPAM
 * hooks can override (copies of) it before the configuration occurs. */
static const struct qm_fqd_stashing default_stash_opts = {
	.annotation_cl = PPAC_STASH_ANNOTATION_CL,
	.data_cl = PPAC_STASH_DATA_CL,
	.context_cl = PPAC_STASH_CONTEXT_CL
};

/* Give ppac/main.c access to the interface's configuration (because it doesn't
 * have acccess to the ppac_interface type due to its dependence on ppam). */
const struct fm_eth_port_cfg *ppac_interface_pcfg(struct ppac_interface *i)
{
	return i->port_cfg;
}

/*******************/
/* Packet handling */
/*******************/

#if defined(PPAC_ORDER_PRESERVATION) || \
	defined(PPAC_ORDER_RESTORATION)
#define PRE_DQRR()  local_dqrr = dqrr
#define POST_DQRR() (local_dqrr ? qman_cb_dqrr_consume : qman_cb_dqrr_defer)
#else
#define PRE_DQRR()  do { ; } while (0)
#define POST_DQRR() qman_cb_dqrr_consume
#endif


static enum qman_cb_dqrr_result
cb_dqrr_rx_error(struct qman_portal *qm __always_unused,
		 struct qman_fq *fq,
		 const struct qm_dqrr_entry *dqrr)
{
	struct ppac_rx_error *r = container_of(fq, struct ppac_rx_error, fq);
	struct ppac_interface *_if = container_of(r, struct ppac_interface,
						  rx_error);
	TRACE("Rx_error: fqid=%d\tfd_status = 0x%08x\n",
	      fq->fqid, dqrr->fd.status);
	PRE_DQRR();
	ppam_rx_error_cb(&r->s, &_if->ppam_data, dqrr);
	return POST_DQRR();
}

static enum qman_cb_dqrr_result
cb_dqrr_rx_default(struct qman_portal *qm __always_unused,
		   struct qman_fq *fq,
		   const struct qm_dqrr_entry *dqrr)
{
	struct ppac_rx_default *r = container_of(fq, struct ppac_rx_default,
						 fq);
	struct ppac_interface *_if = r->ppac_if;
	TRACE("Rx_default: fqid=%d\tfd_status = 0x%08x\n", fq->fqid, dqrr->fd.status);
	PRE_DQRR();
	ppam_rx_default_cb(&r->s, &_if->ppam_data, dqrr);
	return POST_DQRR();
}

static enum qman_cb_dqrr_result
cb_dqrr_tx_error(struct qman_portal *qm __always_unused,
		 struct qman_fq *fq,
		 const struct qm_dqrr_entry *dqrr)
{
	struct ppac_tx_error *t = container_of(fq, struct ppac_tx_error, fq);
	struct ppac_interface *_if = container_of(t, struct ppac_interface,
						  tx_error);
	TRACE("Tx_error: fqid=%d\tfd_status = 0x%08x\n", fq->fqid, dqrr->fd.status);
	PRE_DQRR();
	ppam_tx_error_cb(&t->s, &_if->ppam_data, dqrr);
	return POST_DQRR();
}

static enum qman_cb_dqrr_result
cb_dqrr_tx_confirm(struct qman_portal *qm __always_unused,
		   struct qman_fq *fq,
		   const struct qm_dqrr_entry *dqrr)
{
	struct ppac_tx_confirm *t = container_of(fq, struct ppac_tx_confirm,
						 fq);
	struct ppac_interface *_if = container_of(t, struct ppac_interface,
						  tx_confirm);
	TRACE("Tx_confirm: fqid=%d\tfd_status = 0x%08x\n", fq->fqid, dqrr->fd.status);
	PRE_DQRR();
	ppam_tx_confirm_cb(&t->s, &_if->ppam_data, dqrr);
	return POST_DQRR();
}

enum qman_cb_dqrr_result
cb_dqrr_rx_hash(struct qman_portal *qm __always_unused,
		struct qman_fq *fq,
		const struct qm_dqrr_entry *dqrr)
{
	struct ppac_rx_hash *p = container_of(fq, struct ppac_rx_hash, fq);
	TRACE("Rx_hash: fqid=%d\tfd_status = 0x%08x\n", fq->fqid, dqrr->fd.status);
	PRE_DQRR();
	PRE_ORP(p->orp_fq, dqrr->seqnum);
	ppam_rx_hash_cb(&p->s, dqrr);
	POST_ORP();
	return POST_DQRR();
}

void cb_ern(struct qman_portal *qm __always_unused,
	    struct qman_fq *fq,
	    const struct qm_mr_entry *msg)
{
	struct ppac_rx_hash *p __maybe_unused;
	p = container_of(fq, struct ppac_rx_hash, fq);
	TRACE("Tx_ern: fqid=%d\tfd_status = 0x%08x\n", msg->ern.fqid,
	      msg->ern.fd.status);
	PRE_ORP(p->orp_fq, msg->ern.seqnum);
	ppac_drop_frame(&msg->ern.fd);
	POST_ORP();
}

#ifdef PPAC_RX_1G_PREFERINCACHE
#define RX_1G_PIC 1
#else
#define RX_1G_PIC 0
#endif
#ifdef PPAC_RX_10G_PREFERINCACHE
#define RX_10G_PIC 1
#else
#define RX_10G_PIC 0
#endif
#ifdef PPAC_2FWD_RX_OFFLINE_PREFERINCACHE
#define RX_OFFLINE_PIC 1
#else
#define RX_OFFLINE_PIC 0
#endif
#ifdef PPAC_RX_ONIC_PREFERINCACHE
#define RX_ONIC_PIC 1
#else
#define RX_ONIC_PIC 0
#endif

/* This is part of the inlined code due to its dependency on ppam_* types. */
int ppac_interface_init(unsigned idx)
{
	struct ppac_interface *i;
	int err, loop;
	struct qm_fqd_stashing stash_opts;
	const struct fm_eth_port_cfg *port = &netcfg->port_cfg[idx];
	const struct fman_if *fif = port->fman_if;
	size_t size;
	uint64_t context_a = 0;
	uint32_t context_b = 0;
	uint32_t flags = 0;

	if (fif->mac_type == fman_mac_less) {
		size = sizeof(struct ppac_interface) +
			(fif->macless_info.tx_count *
			sizeof(struct ppac_rx_default));
	} else {
		size = sizeof(struct ppac_interface) +
			sizeof(struct ppac_rx_default);
	}
	/* allocate stashable memory for the interface object */
	i = __dma_mem_memalign(L1_CACHE_BYTES, size);
	if (!i)
		return -ENOMEM;
	memset(i, 0, size);
	INIT_LIST_HEAD(&i->list);
	i->size = size;
	i->port_cfg = port;
	/* allocate and initialise Tx FQs for this interface */
	switch (fif->mac_type) {
		case fman_onic:
			i->num_tx_fqs = PPAC_TX_FQS_ONIC;
			break;
		case fman_mac_less:
			i->num_tx_fqs = fif->macless_info.rx_count;
			break;
		case fman_mac_10g:
			i->num_tx_fqs = PPAC_TX_FQS_10G;
			break;
		case fman_mac_1g:
			i->num_tx_fqs = PPAC_TX_FQS_1G;
			break;
		case fman_offline:
			i->num_tx_fqs = PPAC_TX_FQS_OFFLINE;
			break;
	}
	if ((fif->mac_type != fman_mac_less)/* && (fif->mac_type != fman_onic)*/) {
		i->tx_fqs = malloc(sizeof(*i->tx_fqs) * i->num_tx_fqs);
		if (!i->tx_fqs) {
			__dma_mem_free(i);
			return -ENOMEM;
		}
		memset(i->tx_fqs, 0, sizeof(*i->tx_fqs) * i->num_tx_fqs);
	}
	err = ppam_interface_init(&i->ppam_data, port, i->num_tx_fqs, &flags);
	if (err) {
		free(i->tx_fqs);
		__dma_mem_free(i);
		return err;
	}

	if (fif->mac_type == fman_mac_less) {
		uint32_t fqid = fif->macless_info.rx_start;
		for (loop = 0; loop < i->num_tx_fqs; loop++) {
			TRACE("TX FQID %d, count %d\n", fqid, i->num_tx_fqs);
			ppam_interface_tx_fqid(&i->ppam_data, loop, fqid++);
		}
		list_add_tail(&i->node, &ifs);
		return 0;

	}

#ifdef PPAC_TX_CONFIRM
	context_b = fif->fqid_tx_confirm;
#else
	if (fif->mac_type != fman_onic)
		context_a = (uint64_t)1 << 63;

	if (!(flags & PPAM_TX_FQ_NO_BUF_DEALLOC))
		context_a |= ((uint64_t)fman_dealloc_bufs_mask_hi << 32) |
					(uint64_t)fman_dealloc_bufs_mask_lo;
	if (flags & PPAM_TX_FQ_NO_CHECKSUM)
		context_a |= FMAN_CONTEXTA_DIS_CHECKSUM;
        if (flags & PPAM_TX_FQ_SET_OPCODE11)
                context_a |= FMAN_CONTEXTA_SET_OPCODE11;
#endif

	for (loop = 0; loop < i->num_tx_fqs; loop++) {
		struct qman_fq *fq = &i->tx_fqs[loop];
		ppac_fq_tx_init(fq, fif->tx_channel_id, context_a, context_b);
		TRACE("I/F %d, using Tx FQID %d\n", idx, fq->fqid);
		ppam_interface_tx_fqid(&i->ppam_data, loop, fq->fqid);
	}
	/* Offline ports don't have Tx Error or Tx Confirm FQs */
	if (fif->mac_type == fman_offline || fif->mac_type == fman_onic) {
		list_add_tail(&i->node, &ifs);
		return 0;
	}
	/* Note: we should handle errors and unwind */
	stash_opts = default_stash_opts;
	/* For shared MAC, Tx Error and Tx Confirm FQs are created by linux */
	if (fif->shared_mac_info.is_shared_mac != 1) {
		stash_opts = default_stash_opts;
		err = ppam_tx_error_init(&i->tx_error.s, &i->ppam_data, &stash_opts);
		BUG_ON(err);
		ppac_fq_nonpcd_init(&i->tx_error.fq, fif->fqid_tx_err, get_rxc(),
			    &stash_opts, cb_dqrr_tx_error);
		stash_opts = default_stash_opts;
		err = ppam_tx_confirm_init(&i->tx_confirm.s, &i->ppam_data,
				   &stash_opts);
		BUG_ON(err);
		ppac_fq_nonpcd_init(&i->tx_confirm.fq, fif->fqid_tx_confirm, get_rxc(),
			    &stash_opts, cb_dqrr_tx_confirm);
	}

	list_add_tail(&i->node, &ifs);
	return 0;
}

int ppac_interface_init_rx(struct ppac_interface *i)
{
	__maybe_unused int err;
	int loop;
	struct qm_fqd_stashing stash_opts;
	const struct fman_if *fif = i->port_cfg->fman_if;
	struct fmc_netcfg_fqrange *fqr;

	/* Note: we should handle errors and unwind */
	stash_opts = default_stash_opts;
	if (fif->mac_type == fman_mac_less) {
		uint32_t fqid = fif->macless_info.tx_start;
		for (loop = 0; loop < fif->macless_info.tx_count; loop++) {
			i->rx_default[loop].ppac_if = i;
			err = ppam_rx_default_init(&i->rx_default[loop].s,
				&i->ppam_data, loop, &stash_opts);
			if (err) {
				error(0, err, "%s", __func__);
				return err;
			}
			ppac_fq_nonpcd_init(&i->rx_default[loop].fq,
				fqid++, get_rxc(), &stash_opts,
				cb_dqrr_rx_default);
		}
		ppac_interface_enable_shared_rx(i);
		return 0;
	} else if (fif->mac_type == fman_onic) {
		uint32_t fqid = fif->fqid_rx_def;
		err = ppam_rx_error_init(&i->rx_error.s, &i->ppam_data,
						&stash_opts);
		if (err) {
			error(0, err, "%s", __func__);
			return err;
		}
		ppac_fq_nonpcd_init(&i->rx_error.fq, fif->fqid_rx_err,
				    get_rxc(), &stash_opts, cb_dqrr_rx_error);

		i->rx_default[0].ppac_if = i;
		err = ppam_rx_default_init(&i->rx_default[0].s,
			&i->ppam_data, 0, &stash_opts);
		if (err) {
			error(0, err, "%s", __func__);
			return err;
		}
		ppac_fq_nonpcd_init(&i->rx_default[0].fq,
			fqid++, get_rxc(), &stash_opts,
			cb_dqrr_rx_default);
		ppac_interface_enable_shared_rx(i);
		return 0;
	}
	i->rx_default[0].ppac_if = i;
	if (fif->shared_mac_info.is_shared_mac == 1) {
		struct qm_mcr_queryfq_np np;
		struct qman_fq fq;
		fq.fqid = i->port_cfg->rx_def;
		err = qman_query_fq_np(&fq, &np);
		if (err) {
			error(0, err, "%s(): shared MAC query FQ", __func__);
			return err;
		}
		/* For shared MAC, initialize default FQ only if state is OOS */
		if (np.state == qman_fq_state_oos) {
			err = ppam_rx_default_init(&i->rx_default[0].s,
				&i->ppam_data, 0, &stash_opts);
			if (err) {
				error(0, err, "%s", __func__);
				return err;
			}
			ppac_fq_nonpcd_init(&i->rx_default[0].fq,
				i->port_cfg->rx_def, get_rxc(), &stash_opts,
				cb_dqrr_rx_default);
		}
	} else {
		err = ppam_rx_error_init(&i->rx_error.s, &i->ppam_data,
						&stash_opts);
		if (err) {
			error(0, err, "%s", __func__);
			return err;
		}
		ppac_fq_nonpcd_init(&i->rx_error.fq, fif->fqid_rx_err,
				    get_rxc(), &stash_opts, cb_dqrr_rx_error);
		stash_opts = default_stash_opts;
		err = ppam_rx_default_init(&i->rx_default[0].s, &i->ppam_data,
					   0, &stash_opts);
		if (err) {
			error(0, err, "%s", __func__);
			return err;
		}
		ppac_fq_nonpcd_init(&i->rx_default[0].fq, i->port_cfg->rx_def,
					get_rxc(), &stash_opts,
					cb_dqrr_rx_default);
	}
	list_for_each_entry(fqr, i->port_cfg->list, list) {
		uint32_t fqid = fqr->start;
		struct ppac_pcd_range *pcd_range;
		size_t size = sizeof(struct ppac_pcd_range) +
				fqr->count * sizeof(struct ppac_rx_hash);
		pcd_range = __dma_mem_memalign(L1_CACHE_BYTES, size);
		if (!pcd_range)
			return -ENOMEM;

		memset(pcd_range, 0, size);
		INIT_LIST_HEAD(&pcd_range->list);
		pcd_range->count = fqr->count;

		for (loop = 0; loop < fqr->count; loop++) {
			stash_opts = default_stash_opts;
			err = ppam_rx_hash_init(&pcd_range->rx_hash[loop].s,
				 &i->ppam_data, loop, &stash_opts);
			BUG_ON(err);
#ifdef PPAC_ORDER_RESTORATION
			pcd_range->rx_hash[loop].orp_fq = ppac_orp_init();
			TRACE("I/F %d, Rx FQID %d associated with ORP ID %d\n",
				idx, pcd_range->rx_hash[loop].fq.fqid,
				pcd_range->rx_hash[loop].orp_id);
#endif
			ppac_fq_pcd_init(&pcd_range->rx_hash[loop].fq, fqid++,
				get_rxc(), &stash_opts,
				(fif->mac_type == fman_mac_1g) ? RX_1G_PIC :
				(fif->mac_type == fman_mac_10g) ? RX_10G_PIC :
				(fif->mac_type == fman_offline) ? RX_OFFLINE_PIC:
				(fif->mac_type == fman_onic) ? RX_ONIC_PIC:
				0);
		}
		list_add_tail(&pcd_range->list, &i->list);
	}

	ppac_interface_enable_rx(i);
	if (fif->shared_mac_info.is_shared_mac == 1)
		ppac_interface_enable_shared_rx(i);
	return 0;
}
void ppac_interface_enable_rx(const struct ppac_interface *i)
{
	fman_if_enable_rx(i->port_cfg->fman_if);
	TRACE("Interface %d:%d, enabled RX\n",
	      i->port_cfg->fman_if->fman_idx,
	      i->port_cfg->fman_if->mac_idx);
}

void ppac_interface_disable_rx(const struct ppac_interface *i)
{
	fman_if_disable_rx(i->port_cfg->fman_if);
	TRACE("Interface %d:%d, disabled RX\n",
	      i->port_cfg->fman_if->fman_idx,
	      i->port_cfg->fman_if->mac_idx);
}

void ppac_interface_enable_shared_rx(const struct ppac_interface *i)
{
	bool if_up = true;
	const struct fman_if *fif = i->port_cfg->fman_if;

	usdpaa_netcfg_enable_disable_shared_rx(i->port_cfg->fman_if,
						if_up);
	if (fif->mac_type == fman_mac_less)
		TRACE("Interface name %s:, enabled RX\n",
			fif->macless_info.macless_name);
	else
		TRACE("Interface name %s:, enabled RX\n",
			fif->shared_mac_info.shared_mac_name);
}

void ppac_interface_disable_shared_rx(const struct ppac_interface *i)
{
	bool if_down = false;
	const struct fman_if *fif = i->port_cfg->fman_if;

	usdpaa_netcfg_enable_disable_shared_rx(i->port_cfg->fman_if,
						if_down);
	if (fif->mac_type == fman_mac_less)
		TRACE("Interface name %s:, disabled RX\n",
			fif->macless_info.macless_name);
	else
		TRACE("Interface name %s:, disabled RX\n",
			fif->shared_mac_info.shared_mac_name);
}

void ppac_interface_finish(struct ppac_interface *i)
{
	int loop;
	const struct fman_if *fif = i->port_cfg->fman_if;

	/* Cleanup in the opposite order of ppac_interface_init() */
	list_del(&i->node);

	if (ppac_interface_type(i) == fman_mac_less) {
		ppam_interface_finish(&i->ppam_data);
		__dma_mem_free(i);
		return;
	}

	/* Offline and shared-mac ports don't have Tx Error or Confirm FQs */
	if (ppac_interface_type(i) != fman_offline &&
	    ppac_interface_type(i) != fman_onic &&
	    fif->shared_mac_info.is_shared_mac != 1) {
		ppam_tx_confirm_finish(&i->tx_confirm.s, &i->ppam_data);
		teardown_fq(&i->tx_confirm.fq);
		ppam_tx_error_finish(&i->tx_error.s, &i->ppam_data);
		teardown_fq(&i->tx_error.fq);
	}
	for (loop = 0; loop < i->num_tx_fqs; loop++) {
		struct qman_fq *fq = &i->tx_fqs[loop];
		TRACE("I/F %d, destroying Tx FQID %d\n",
		      i->port_cfg->fman_if->fman_idx, fq->fqid);
		teardown_fq(fq);
	}

	ppam_interface_finish(&i->ppam_data);
	free(i->tx_fqs);
	__dma_mem_free(i);
}
void ppac_interface_finish_rx(struct ppac_interface *i)
{
	int loop;
	struct ppac_pcd_range *pcd_range;
	const struct fman_if *fif = i->port_cfg->fman_if;

	if (fif->mac_type == fman_mac_less) {
		ppac_interface_disable_shared_rx(i);
		for (loop = 0; loop < fif->macless_info.tx_count; loop++) {
			ppam_rx_default_finish(&i->rx_default[loop].s,
				&i->ppam_data);
			teardown_fq(&i->rx_default[loop].fq);
		}
		return;
	}
	/* Cleanup in the opposite order of ppac_interface_init_rx() */
	if (fif->shared_mac_info.is_shared_mac == 1)
		ppac_interface_disable_shared_rx(i);
	else
		ppac_interface_disable_rx(i);

	/* Cleanup if default Rx FQ is non-zero */
	if (qman_fq_fqid(&i->rx_default[0].fq)) {
		ppam_rx_default_finish(&i->rx_default[0].s, &i->ppam_data);
		teardown_fq(&i->rx_default[0].fq);
	}

	/* In case of shared-mac, error FQ is owned by Linux */
	if (fif->shared_mac_info.is_shared_mac != 1) {
		ppam_rx_error_finish(&i->rx_error.s, &i->ppam_data);
		teardown_fq(&i->rx_error.fq);
	}

	list_for_each_entry(pcd_range, &i->list, list) {
		for (loop = 0; loop < pcd_range->count; loop++) {
			ppam_rx_hash_finish(&pcd_range->rx_hash[loop].s,
				 &i->ppam_data, loop);
			teardown_fq(&pcd_range->rx_hash[loop].fq);
#ifdef PPAC_ORDER_RESTORATION
			teardown_fq(pcd_range->rx_hash[loop].orp_fq);
			__dma_mem_free(pcd_range->rx_hash[loop].orp_fq);
#endif
		}
	}
}
