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

#include <fra_fman_port.h>
#include <fra_fq_interface.h>
#include <fra_cfg_parser.h>
#include <usdpaa/fman.h>

LIST_HEAD(ifs);

/* This struct holds the default stashing opts for Rx FQ configuration */
static const struct qm_fqd_stashing default_stash_opts = {
	.annotation_cl = FRA_STASH_ANNOTATION_CL,
	.data_cl = FRA_STASH_DATA_CL,
	.context_cl = FRA_STASH_CONTEXT_CL
};

static inline enum fman_mac_type fman_port_type(struct fra_fman_port *port)
{
	return port->port_cfg->fman_if->mac_type;
}

static enum qman_cb_dqrr_result
fman_nonhash_dqrr(struct qman_portal *portal __always_unused,
		  struct qman_fq *fq,
		  const struct qm_dqrr_entry *dqrr)
{
	struct fman_rx_nonhash *r;
	r = container_of(fq, struct fman_rx_nonhash, fq);

	FRA_DBG("%s receives a message fqid=0x%x fd_status = 0x%08x",
		__func__, fq->fqid, dqrr->fd.status);

	PRE_DQRR();
	if (r->handler)
		r->handler(r->pvt, &dqrr->fd);
	else
		fra_drop_frame(&dqrr->fd);
	return POST_DQRR();
}

static enum qman_cb_dqrr_result
fman_hash_dqrr(struct qman_portal *portal __always_unused,
	       struct qman_fq *fq, const struct qm_dqrr_entry *dqrr)
{
	struct fman_rx_hash *r = container_of(fq, struct fman_rx_hash, fq);

	FRA_DBG("%s receives a message fqid=0x%x fd_status = 0x%08x",
		__func__, fq->fqid, dqrr->fd.status);

	PRE_DQRR();
	PRE_ORP(r->orp_id, dqrr->seqnum);
	if (likely(r->handler))
		r->handler(r->pvt, &r->opt, &dqrr->fd);
	else
		fra_drop_frame(&dqrr->fd);
	POST_ORP();
	return POST_DQRR();
}

void fman_port_enable_rx(const struct fra_fman_port *port)
{
	fman_if_enable_rx(port->port_cfg->fman_if);
	FRA_DBG("Interface %d:%d, enabled RX\n",
		port->port_cfg->fman_if->fman_idx,
		port->port_cfg->fman_if->mac_idx);
}

static void fman_port_disable_rx(const struct fra_fman_port *port)
{
	fman_if_disable_rx(port->port_cfg->fman_if);
	FRA_DBG("Interface %d:%d, disabled RX\n",
		port->port_cfg->fman_if->fman_idx,
		port->port_cfg->fman_if->mac_idx);
}

int fman_rx_init(struct fra_fman_port *port, int init_hash, uint8_t wq,
		 u16 channel)
{
	const struct fman_if *fif;
	struct fm_eth_port_fqrange *fqr;
	int loop;

	if (!port)
		return -EINVAL;

	fif = port->port_cfg->fman_if;
	fra_fq_nonpcd_init(&port->rx_error.fq, fif->fqid_rx_err,
		FRA_NONHASH_WQ, FRA_NONHASH_CHANNEL,
			&default_stash_opts, fman_nonhash_dqrr);
	fra_fq_nonpcd_init(&port->rx_default.fq, port->port_cfg->rx_def,
		FRA_NONHASH_WQ, FRA_NONHASH_CHANNEL,
			&default_stash_opts, fman_nonhash_dqrr);

	if (!init_hash)
		return 0;

	list_for_each_entry(fqr, port->port_cfg->list, list) {
		struct fman_pcd_range *pcd_range;
		size_t size = sizeof(struct fman_pcd_range) +
				fqr->count * sizeof(struct fman_rx_hash);
		pcd_range = __dma_mem_memalign(L1_CACHE_BYTES, size);
		if (!pcd_range)
			return -ENOMEM;

		memset(pcd_range, 0, size);
		INIT_LIST_HEAD(&pcd_range->list);
		pcd_range->count = fqr->count;

		for (loop = 0; loop < fqr->count; loop++) {
			struct fman_rx_hash *hash = &pcd_range->hash[loop];
			hash->opt.rx_fqid = fqr->start + loop;
			fra_fq_pcd_init(&hash->fq, hash->opt.rx_fqid,
					wq, channel, &default_stash_opts,
					fman_hash_dqrr);
#ifdef FRA_ORDER_RESTORATION
			fra_orp_init(&hash->orp_id);
			FRA_DBG("FMan port %s, Rx FQID %d associated "
				"with ORP ID %d",
				port->cfg->port_name,
				hash->fq.fqid,
				hash->orp_id);
#endif
		}
		list_add_tail(&pcd_range->list, &port->list);
	}

	return 0;
}

void fman_rx_default_listen(struct fra_fman_port *port,
	void *pvt, nonhash_handler handler)
{
	if (!port)
		return;
	port->rx_default.pvt = pvt;
	port->rx_default.handler = handler;

}

void fman_rx_error_listen(struct fra_fman_port *port,
			  void *pvt, nonhash_handler handler)
{
	if (!port)
		return;
	port->rx_error.pvt = pvt;
	port->rx_error.handler = handler;
}

void fman_rx_hash_listen(struct fra_fman_port *port,
			 void *pvt, hash_handler handler)
{
	struct fman_pcd_range *pcd_range;
	int loop;

	if (!port)
		return;

	list_for_each_entry(pcd_range, &port->list, list) {
		for (loop = 0; loop < pcd_range->count; loop++) {
			pcd_range->hash[loop].pvt = pvt;
			pcd_range->hash[loop].handler = handler;
		}
	}
}

struct hash_opt *
fman_pcd_range_get_opt(struct fman_pcd_range *pcd_range, int idx)
{
	if (!pcd_range || idx >= pcd_range->count)
		return NULL;
	return &pcd_range->hash[idx].opt;
}

int opt_bindto_fman_tx(struct hash_opt *opt, struct fra_fman_port *port,
			int idx)
{
	if (!opt || !port)
		return -EINVAL;

	opt->tx_pvt = port;
	opt->tx_fqid = qman_fq_fqid(&port->tx_fqs[idx % port->txfqs_num]);
	return 0;
}

int fman_tx_init(struct fra_fman_port *port, uint32_t fqid,
		   int fqs_num, uint8_t wq)
{
	const struct fman_if *fif;
	uint64_t context_a;
	uint32_t context_b;
	int loop;

	if (!port)
		return -EINVAL;

	fif = port->port_cfg->fman_if;

	/* Offline ports don't have Tx Error or Tx Confirm FQs */
	if (fif->mac_type != fman_offline) {
		fra_fq_nonpcd_init(&port->tx_error.fq, fif->fqid_tx_err,
		FRA_NONHASH_WQ, FRA_NONHASH_CHANNEL,
			&default_stash_opts, fman_nonhash_dqrr);
		fra_fq_nonpcd_init(&port->tx_confirm.fq, fif->fqid_tx_confirm,
		FRA_NONHASH_WQ, FRA_NONHASH_CHANNEL,
			&default_stash_opts, fman_nonhash_dqrr);
	}

	if (0 == fqs_num)
		return 0;
	/* Allocate and initialize Tx FQs for this interface */
	port->txfqs_num = fqs_num;

	port->tx_fqs = malloc(fqs_num * sizeof(*port->tx_fqs));
	if (!port->tx_fqs)
		return -ENOMEM;
	memset(port->tx_fqs, 0, fqs_num * sizeof(*port->tx_fqs));

#ifdef FRA_TX_CONFIRM
	context_b = fif->fqid_tx_confirm;
	context_a = fman_dealloc_bufs_mask_lo;
#else
	context_b = 0;
	context_a = (((uint64_t) 0x80000000 | fman_dealloc_bufs_mask_hi)
		      << 32) | fman_dealloc_bufs_mask_lo;
#endif
	for (loop = 0; loop < fqs_num; loop++) {
		struct qman_fq *fq = &port->tx_fqs[loop];
		fra_fq_tx_init(fq, fqid ? (fqid + loop) : 0, wq,
			       fif->tx_channel_id, context_a, context_b);
	}
	return 0;
}

void fman_tx_confirm_listen(struct fra_fman_port *port,
			    void *pvt, nonhash_handler handler)
{
	if (!port)
		return;
	port->tx_confirm.pvt = pvt;
	port->tx_confirm.handler = handler;

}

void fman_tx_error_listen(struct fra_fman_port *port,
			  void *pvt, nonhash_handler handler)
{
	if (!port)
		return;
	port->tx_error.pvt = pvt;
	port->tx_error.handler = handler;
}

int fman_send_frame(struct hash_opt *opt, const struct qm_fd *fd)
{
	fra_send_frame(opt->tx_fqid, fd);
	return 0;
}

struct fra_fman_port *get_fra_fman_port(const char *name)
{
	struct fra_fman_port *port;

	if (!name)
		return NULL;
	list_for_each_entry(port, &ifs, node) {
		if (!strncmp(name, port->cfg->name, MAX_LENGTH_OF_NAME))
			return port;
	}
	return NULL;
}

void fman_port_finish_rx(struct fra_fman_port *port)
{
	struct fman_pcd_range *pcd_range, *temp;
	int loop;

	if (!port)
		return;

	fman_port_disable_rx(port);
	fra_teardown_fq(&port->rx_default.fq);
	fra_teardown_fq(&port->rx_error.fq);
	list_for_each_entry_safe(pcd_range, temp, &port->list, list) {
		list_del(&pcd_range->list);
		for (loop = 0; loop < pcd_range->count; loop++)
			fra_teardown_fq(&pcd_range->hash[loop].fq);
		__dma_mem_free(pcd_range);
	}
}

void fman_port_finish_tx(struct fra_fman_port *port)
{
	int loop;

	if (!port)
		return;
	/* Offline ports don't have Tx Error or Confirm FQs */
	if (fman_port_type(port) != fman_offline) {
		fra_teardown_fq(&port->tx_confirm.fq);
		fra_teardown_fq(&port->tx_error.fq);
	}

	if (port->tx_fqs) {
		for (loop = 0; loop < port->txfqs_num; loop++) {
			struct qman_fq *fq = &port->tx_fqs[loop];
			fra_teardown_fq(fq);
		}
		free(port->tx_fqs);
	}
}

int fman_port_init(const struct fra_fman_port_cfg *cfg,
		   const struct usdpaa_netcfg_info *netcfg)
{
	struct fra_fman_port *port;
	const struct fman_if *fman_if;
	int err, loop;

	if (!cfg || !netcfg)
		return -EINVAL;

	port = __dma_mem_memalign(L1_CACHE_BYTES, sizeof(*port));
	if (!port)
		return -ENOMEM;
	memset(port, 0, sizeof(*port));

	for (loop = 0; loop < netcfg->num_ethports; loop++) {
		fman_if = netcfg->port_cfg[loop].fman_if;
		if (fman_if->fman_idx == cfg->fman_num &&
		    fman_if->mac_type == cfg->port_type &&
		    fman_if->mac_idx == cfg->port_num) {
			port->port_cfg = &netcfg->port_cfg[loop];
			break;
		}
	}
	if (!port->port_cfg) {
		err = -EINVAL;
		goto _err;
	}

	port->cfg = cfg;
	INIT_LIST_HEAD(&port->list);
	list_add_tail(&port->node, &ifs);
	return 0;
_err:
	__dma_mem_free(port);
	return err;
}

void fman_port_finish(struct fra_fman_port *port)
{
	if (!port)
		return;
	list_del(&port->node);
	__dma_mem_free(port);
}

void fman_ports_finish(void)
{
	struct fra_fman_port *port, *temp;
	list_for_each_entry_safe(port, temp, &ifs, node)
		fman_port_finish(port);
}
