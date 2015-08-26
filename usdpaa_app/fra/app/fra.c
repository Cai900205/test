/* Copyright (c) 2010-2012 Freescale Semiconductor, Inc.
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

#include <fra.h>
#include <fra_fq_interface.h>

struct fra *fra;

static inline void tran_status(struct rio_tran *tran)
{
	fprintf(stderr, "\t\trio_tran:%s type:%s\n",
		tran->name, RIO_TYPE_TO_STR[tran->type]);
}

static void dist_info(struct distribution *dist)
{
	struct dist_cfg *cfg;

	if (!dist || !dist->cfg) {
		fprintf(stderr, "a invalid type\n");
		return;
	}
	cfg = dist->cfg;
	fprintf(stderr, "\tdistribution-%d-%s: %s\n",
		cfg->sequence_number,
		DIST_TYPE_STR[cfg->type],
		cfg->name);

	switch (cfg->type) {
	case DIST_TYPE_RMAN_RX:
		fprintf(stderr, "\t\trio port:%d sid:%d "
			"mask:%d queue mode:%s\n",
			cfg->dist_rman_rx_cfg.rio_port,
			cfg->dist_rman_rx_cfg.sid,
			cfg->dist_rman_rx_cfg.sid_mask,
			FQ_MODE_STR[cfg->dist_rman_rx_cfg.fq_mode]);
		tran_status(cfg->dist_rman_rx_cfg.tran);
		fprintf(stderr, "\t\tbase FQID:0x%x count:%d "
			"configured to IB%d CU%d\n",
			cfg->dist_rman_rx_cfg.fqid,
			rman_rx_get_fqs_num(dist->rman_rx),
			rman_rx_get_ib(dist->rman_rx),
			rman_rx_get_cu(dist->rman_rx));
		break;
	case DIST_TYPE_RMAN_TX:
		fprintf(stderr, "\t\tport:%d did:%d\n"
			"\t\tFQID:0x%x count:%d\n",
			cfg->dist_rman_tx_cfg.rio_port,
			cfg->dist_rman_tx_cfg.did,
			cfg->dist_rman_tx_cfg.fqid,
			cfg->dist_rman_tx_cfg.fqs_num);
		tran_status(cfg->dist_rman_tx_cfg.tran);
		break;
	case DIST_TYPE_FMAN_RX:
		fprintf(stderr, "\t\tfman port:%s\n",
			cfg->dist_fman_rx_cfg.fman_port_name);
		break;
	case DIST_TYPE_FMAN_TX:
		fprintf(stderr, "\t\tfman port:%s "
			"fqs num:%d\n",
			cfg->dist_fman_tx_cfg.fman_port_name,
			cfg->dist_fman_tx_cfg.fqs_num);
		break;
	case DIST_TYPE_RMAN_TO_FMAN:
		fprintf(stderr, "\t\trio port:%d - %s\n"
			"\t\tsid:%d mask:%d queue mode:%s\n",
			cfg->dist_rman_to_fman_cfg.rio_port,
			cfg->dist_rman_to_fman_cfg.fman_port_name,
			cfg->dist_rman_to_fman_cfg.sid,
			cfg->dist_rman_to_fman_cfg.sid_mask,
			FQ_MODE_STR[cfg->dist_rman_to_fman_cfg.fq_mode]);
		tran_status(cfg->dist_rman_to_fman_cfg.tran);
		fprintf(stderr, "\t\tbase FQID:0x%x count:%d "
			"configured to IB%d CU%d\n",
			cfg->dist_rman_to_fman_cfg.fqid,
			rman_rx_get_fqs_num(dist->rman_to_fman->rman_rx),
			rman_rx_get_ib(dist->rman_to_fman->rman_rx),
			rman_rx_get_cu(dist->rman_to_fman->rman_rx));
		break;
	case DIST_TYPE_FMAN_TO_RMAN:
		fprintf(stderr, "\t\tFMan:%s - rio port %d\n"
			"\t\tdid:%d\n",
			cfg->dist_fman_to_rman_cfg.fman_port_name,
			cfg->dist_fman_to_rman_cfg.rio_port,
			cfg->dist_fman_to_rman_cfg.did);
		tran_status(cfg->dist_fman_to_rman_cfg.tran);
		break;
	default:
		fprintf(stderr, "an invalid type\n");
	}
}
static int fra_cli_status(int argc, char *argv[])
{
	const struct fra_cfg *fra_cfg;
	struct dist_order *dist_order;
	struct distribution *dist;
	int i = 1;

	if (argc > 2)
		return -EINVAL;

	if (!fra || !fra->cfg) {
		error(0, 0, "Fra is not been configured");
		return -EINVAL;
	}

	fra_cfg = fra->cfg;
	fprintf(stderr, "RMan configuration:\n"
		"\tCreate inbound message descriptor: %s\n"
		"\tOutbound segmentation interleaving disable: %s\n"
		"\tThe algorithmic frame queue bits info:\n"
		"\t\tdata streaming:%d mailbox:%d\n"
		"\tBPID info:\n"
		"\t\tdata streaming:%d mailbox:%d doorbell:%d sg:%d\n"
		"\t\tportwrite:%d\n",
		MD_CREATE_MODE_STR[fra_cfg->rman_cfg.md_create],
		fra_cfg->rman_cfg.osid == 1 ? "yes" : "no",
		fra_cfg->rman_cfg.fq_bits[RIO_TYPE_DSTR],
		fra_cfg->rman_cfg.fq_bits[RIO_TYPE_MBOX],
		fra_cfg->rman_cfg.bpid[RIO_TYPE_DSTR],
		fra_cfg->rman_cfg.bpid[RIO_TYPE_MBOX],
		fra_cfg->rman_cfg.bpid[RIO_TYPE_DBELL],
		fra_cfg->rman_cfg.sgbpid,
		fra_cfg->rman_cfg.bpid[RIO_TYPE_PW]);

	rman_if_status();

	list_for_each_entry(dist_order, &fra->dist_order_list, node) {
		if (i > 1)
			fprintf(stderr, "\n");
		fprintf(stderr, "distribution order-%d:\n", i++);

		dist = dist_order->dist;
		while (dist) {
			dist_info(dist);
			dist = dist->next;
		}
	}
	return 0;
}

cli_cmd(status, fra_cli_status);

static inline void dist_order_handler(struct distribution *dist,
				      struct hash_opt *opt,
				      const struct qm_fd *fd)
{
	enum handler_status status = HANDLER_CONTINUE;

	while (dist && status != HANDLER_DONE) {
		if (dist->handler)
			status = dist->handler(dist, opt, fd);
		dist = dist->next;
	}

	if (status != HANDLER_DONE)
		fra_drop_frame(fd);
}

#ifdef FRA_TX_CONFIRM
static void dist_tx_confirm_cb(void *pvt, const struct qm_fd *fd)
{
#ifdef ENABLE_FRA_DEBUG
	struct distribution *dist = pvt;

	FRA_DBG("DIST(%s) receives a transmit confirm message",
		dist->cfg->name);
#endif
	fra_drop_frame(fd);
}
#endif

#ifdef ENABLE_FRA_DEBUG
static void dist_rman_rx_info(struct distribution *dist,
			      struct hash_opt *opt,
			      const struct qm_fd *fd)
{
	struct msg_buf *msg;

	FRA_DBG("DIST(%s) receives a msg frome fqid(0x%x)",
		dist->cfg->name, opt->rx_fqid);
	msg = fd_to_msg(fd);
	if (msg && !fra->cfg->rman_cfg.md_create) {
		FRA_DBG("This msg is sent by device(%d) using %s",
			msg_get_sid(msg),
			RIO_TYPE_TO_STR[msg_get_type(msg)]);
		switch (msg_get_type(msg)) {
		case RIO_TYPE_MBOX:
			FRA_DBG("type attr: mbox(%d) ltr(%d)",
				mbox_get_mbox(msg), mbox_get_ltr(msg));
			break;
		case RIO_TYPE_DSTR:
			FRA_DBG("type attr: cos(%d) streamid(%d)",
				dstr_get_cos(msg), dstr_get_streamid(msg));
			break;
		default:
			break;
		}
	}
}
#endif

static void dist_rx_error_cb(void *pvt, const struct qm_fd *fd)
{
#ifdef ENABLE_FRA_DEBUG
	struct distribution *dist = pvt;

	FRA_DBG("DIST(%s) receives an error frame with status 0x%x",
		dist->cfg->name, fd->status);
#endif
	fra_drop_frame(fd);
}

static void dist_rman_rx_cb(void *pvt, struct hash_opt *opt,
			    const struct qm_fd *fd)
{
#ifdef ENABLE_FRA_DEBUG
	dist_rman_rx_info(pvt, opt, fd);
#endif

	if (FD_GET_STATUS(fd)) {
		FRA_DBG("This msg has an error status 0x%x",
			FD_GET_STATUS(fd));
		fra_drop_frame(fd);
		return;
	}

	dist_order_handler(pvt, opt, fd);
}

static enum handler_status
dist_rman_tx_handler(struct distribution *dist, struct hash_opt *opt,
		     const struct qm_fd *fd)
{
	FRA_DBG("Dist(%s) will transmit this msg", dist->cfg->name);
	if (rman_send_frame(opt, fd))
		return HANDLER_ERROR;
	return HANDLER_DONE;
}

static void dist_fman_rx_cb(void *pvt, struct hash_opt *opt,
			    const struct qm_fd *fd)
{
#ifdef ENABLE_FRA_DEBUG
	struct distribution *dist = pvt;

	FRA_DBG("DIST(%s) receives a msg from fqid(0x%x)",
		dist->cfg->name, opt->rx_fqid);
#endif
	dist_order_handler(pvt, opt, fd);
}

static enum handler_status
dist_fman_tx_handler(struct distribution *dist, struct hash_opt *opt,
		     const struct qm_fd *fd)
{
	FRA_DBG("Dist(%s) will transmit this msg", dist->cfg->name);
	if (fman_send_frame(opt, fd))
		return HANDLER_ERROR;
	return HANDLER_DONE;
}

static void dist_finish(struct distribution *dist)
{
	struct dist_cfg *cfg;
	struct rman_tx_list *list, *temp;

	if (!dist)
		return;

	cfg = dist->cfg;
	FRA_DBG("release dist %s", dist->cfg->name);
	switch (cfg->type) {
	case DIST_TYPE_RMAN_RX:
		rman_rx_finish(dist->rman_rx);
		dist->rman_rx = NULL;
		break;
	case DIST_TYPE_RMAN_TX:
		rman_tx_finish(dist->rman_tx);
		dist->rman_tx = NULL;
		break;
	case DIST_TYPE_FMAN_RX:
		fman_port_finish_rx(dist->fman_tx);
		dist->fman_rx = NULL;
		break;
	case DIST_TYPE_FMAN_TX:
		fman_port_finish_tx(dist->fman_tx);
		dist->fman_tx = NULL;
		break;
	case DIST_TYPE_RMAN_TO_FMAN:
		if (!dist->rman_to_fman)
			break;
		rman_rx_finish(dist->rman_to_fman->rman_rx);
		fman_port_finish_tx(dist->rman_to_fman->fman_port);
		free(dist->rman_to_fman);
		dist->rman_to_fman = NULL;
		break;
	case DIST_TYPE_FMAN_TO_RMAN:
		if (!dist->fman_to_rman)
			break;
		fman_port_finish_rx(dist->fman_to_rman->fman_port);
		list_for_each_entry_safe(list, temp,
			&dist->fman_to_rman->f2r_tx_list, node) {
			list_del(&list->node);
			rman_tx_finish(list->rman_tx);
			free(list);
		}
		free(dist->fman_to_rman);
		dist->fman_to_rman = NULL;
		break;
	default:
		break;
	}
	__dma_mem_free(dist);
}

static void dist_order_finish(struct dist_order *dist_order)
{
	struct distribution *dist, *dist_temp;

	if (!dist_order)
		return;

	dist = dist_order->dist;
	while (dist) {
		dist_temp = dist;
		dist = dist->next;
		dist_finish(dist_temp);
	}
	if (dist_order->node.prev && dist_order->node.next)
		list_del(&dist_order->node);
	free(dist_order);
}

static void dist_order_list_finish(void)
{
	struct dist_order *dist_order, *temp;

	if (!fra)
		return;
	list_for_each_entry_safe(dist_order, temp,
			&fra->dist_order_list, node)
		dist_order_finish(dist_order);
}

void fra_finish(void)
{
	dist_order_list_finish();
	rman_if_finish();
	fman_ports_finish();
	free(fra);
	fra = NULL;
}

static struct distribution *dist_rman_rx_init(struct dist_cfg *cfg)
{
	struct dist_rman_rx_cfg *rxcfg;
	struct distribution *dist;

	if (!cfg || cfg->type != DIST_TYPE_RMAN_RX)
		return NULL;

	rxcfg = &cfg->dist_rman_rx_cfg;

	if (rman_if_port_connet(rxcfg->rio_port)) {
		error(0, 0, "SRIO port%d is not connected",
		      rxcfg->rio_port);
		return NULL;
	}

	/* Allocate stashable memory for the interface object */
	dist = __dma_mem_memalign(L1_CACHE_BYTES, sizeof(*dist));
	if (!dist)
		return NULL;
	memset(dist, 0, sizeof(*dist));

	dist->cfg = cfg;
	dist->rman_rx = rman_rx_init(1, rxcfg->fqid, rxcfg->fq_mode,
				     rxcfg->wq, 0, rxcfg->tran, dist,
				     dist_rman_rx_cb);

	if (!dist->rman_rx) {
		__dma_mem_free(dist);
		return NULL;
	}

	rman_rx_listen(dist->rman_rx, rxcfg->rio_port, rxcfg->port_mask,
		       rxcfg->sid, rxcfg->sid_mask);

	rman_rx_error_listen(dist->rman_rx, dist, dist_rx_error_cb);

	return dist;
}

static struct distribution *dist_rman_tx_init(struct dist_cfg *cfg)
{
	struct dist_rman_tx_cfg *txcfg;
	struct distribution *dist;

	if (!cfg || cfg->type != DIST_TYPE_RMAN_TX)
		return NULL;

	txcfg = &cfg->dist_rman_tx_cfg;

	if (rman_if_port_connet(txcfg->rio_port)) {
		error(0, 0, "SRIO port%d is not connected",
		      txcfg->rio_port);
		return NULL;
	}

	dist = __dma_mem_memalign(L1_CACHE_BYTES, sizeof(*dist));
	if (!dist)
		return NULL;
	memset(dist, 0, sizeof(*dist));

	dist->cfg = cfg;

	dist->rman_tx = rman_tx_init(txcfg->rio_port, txcfg->fqid,
				     txcfg->fqs_num, txcfg->wq,
				     txcfg->tran);
	if (!dist->rman_tx) {
		__dma_mem_free(dist);
		return NULL;
	}

#ifdef FRA_TX_CONFIRM
	rman_tx_status_listen(dist->rman_tx, 0, 1, dist, dist_tx_confirm_cb);
#endif
#ifdef FRA_MBOX_MULTICAST
	rman_tx_enable_multicast(dist->rman_tx,
				 txcfg->did >> RM_MBOX_MG_SHIFT,
				 txcfg->did & RM_MBOX_ML_MASK);
#endif
	rman_tx_connect(dist->rman_tx, txcfg->did);
	dist->handler = dist_rman_tx_handler;
	return dist;
}

static struct distribution *dist_rman_to_fman_init(struct dist_cfg *cfg)
{
	struct dist_rman_to_fman_cfg *r2fcfg;
	struct fra_fman_port *fman_port;
	struct distribution *dist;

	if (!cfg || cfg->type != DIST_TYPE_RMAN_TO_FMAN)
		return NULL;

	r2fcfg = &cfg->dist_rman_to_fman_cfg;

	if (rman_if_port_connet(r2fcfg->rio_port)) {
		error(0, 0, "SRIO port%d is not connected",
		      r2fcfg->rio_port);
		return NULL;
	}

	fman_port = get_fra_fman_port(r2fcfg->fman_port_name);
	if (!fman_port) {
		FRA_DBG("can not find fman port %s", r2fcfg->fman_port_name);
		return NULL;
	}
	/* allocate stashable memory for the interface object */
	dist = __dma_mem_memalign(L1_CACHE_BYTES, sizeof(*dist));
	if (!dist)
		return NULL;
	memset(dist, 0, sizeof(*dist));

	dist->cfg = cfg;
	dist->rman_to_fman = malloc(sizeof(struct rman_to_fman));
	if (!dist->rman_to_fman)
		goto _err;
	memset(dist->rman_to_fman, 0, sizeof(struct rman_to_fman));

	dist->rman_to_fman->fman_port = fman_port;
	dist->rman_to_fman->rman_rx =
		rman_rx_init(0, r2fcfg->fqid, r2fcfg->fq_mode,
			     0, 0, r2fcfg->tran, NULL, NULL);
	if (!dist->rman_to_fman->rman_rx)
		goto _err;

	fman_tx_init(fman_port, r2fcfg->fqid,
		     rman_rx_get_fqs_num(dist->rman_to_fman->rman_rx),
		     r2fcfg->wq);

#ifdef FRA_TX_CONFIRM
	fman_tx_confirm_listen(dist->rman_to_fman->fman_port,
			       dist, dist_tx_confirm_cb);
#endif

	rman_rx_listen(dist->rman_to_fman->rman_rx, r2fcfg->rio_port,
		       r2fcfg->port_mask, r2fcfg->sid, r2fcfg->sid_mask);

	rman_rx_error_listen(dist->rman_to_fman->rman_rx, dist,
			     dist_rx_error_cb);

	dist->handler = NULL;
	return dist;
_err:
	dist_finish(dist);
	return NULL;
}

static struct distribution *dist_fman_rx_init(struct dist_cfg *cfg)
{
	struct distribution *dist;
	struct dist_fman_rx_cfg *rxcfg;
	struct fra_fman_port *port;

	if (!cfg || cfg->type != DIST_TYPE_FMAN_RX)
		return NULL;

	rxcfg = &cfg->dist_fman_rx_cfg;
	port = get_fra_fman_port(rxcfg->fman_port_name);
	if (!port)
		return NULL;

	if (fman_rx_init(port, 1, rxcfg->wq, FRA_DYNAMIC_CHANNEL))
		return NULL;

	dist = __dma_mem_memalign(L1_CACHE_BYTES, sizeof(*dist));
	if (!dist)
		return NULL;
	memset(dist, 0, sizeof(*dist));

	dist->cfg = cfg;
	dist->fman_rx = port;

	fman_rx_hash_listen(port, dist, dist_fman_rx_cb);
	return dist;
}

static struct distribution *dist_fman_tx_init(struct dist_cfg *cfg)
{
	struct distribution *dist;
	struct dist_fman_tx_cfg *txcfg;
	struct fra_fman_port *port;

	if (!cfg || cfg->type != DIST_TYPE_FMAN_TX)
		return NULL;

	txcfg = &cfg->dist_fman_tx_cfg;
	port = get_fra_fman_port(txcfg->fman_port_name);
	if (!port)
		return NULL;

	if (fman_tx_init(port, 0, txcfg->fqs_num,  txcfg->wq))
		return NULL;

	dist = __dma_mem_memalign(L1_CACHE_BYTES, sizeof(*dist));
	if (!dist)
		return NULL;
	memset(dist, 0, sizeof(*dist));

	dist->cfg = cfg;
	dist->fman_tx = port;
	dist->handler = dist_fman_tx_handler;

#ifdef FRA_TX_CONFIRM
	fman_tx_confirm_listen(dist->fman_tx, dist, dist_tx_confirm_cb);
#endif
	return dist;
}

static struct distribution *dist_fman_to_rman_init(struct dist_cfg *cfg)
{
	struct distribution *dist;
	struct dist_fman_to_rman_cfg *f2rcfg;
	struct fra_fman_port *port;
	struct fm_eth_port_fqrange *fqr;
	struct rman_tx_list *rman_tx_list;

#ifdef FRA_CORE_COPY_MD
	error(0, 0, "FRA_CORE_COPY_MD mode requires core involvement."
		"So it does not support fman_to_rman distribution");
	return NULL;
#endif

	if (!cfg || cfg->type != DIST_TYPE_FMAN_TO_RMAN)
		return NULL;

	f2rcfg = &cfg->dist_fman_to_rman_cfg;
	port = get_fra_fman_port(f2rcfg->fman_port_name);
	if (!port) {
		FRA_DBG("can not find fman port %s", f2rcfg->fman_port_name);
		return NULL;
	}

	if (rman_if_port_connet(f2rcfg->rio_port)) {
		error(0, 0, "SRIO port%d is not connected",
		      f2rcfg->rio_port);
		return NULL;
	}

	if (fman_rx_init(port, 0, 0, 0))
		return NULL;

	dist = __dma_mem_memalign(L1_CACHE_BYTES, sizeof(*dist));
	if (!dist)
		return NULL;
	memset(dist, 0, sizeof(*dist));

	dist->cfg = cfg;
	dist->fman_to_rman = malloc(sizeof(struct fman_to_rman));
	if (!dist->fman_to_rman)
		goto _err;
	memset(dist->fman_to_rman, 0, sizeof(struct fman_to_rman));
	dist->fman_to_rman->fman_port = port;

	INIT_LIST_HEAD(&dist->fman_to_rman->f2r_tx_list);

	list_for_each_entry(fqr, port->port_cfg->list, list) {
		uint32_t fqid = fqr->start;
		rman_tx_list = malloc(sizeof(struct rman_tx_list));
		if (!rman_tx_list)
			goto _err;
		memset(rman_tx_list, 0, sizeof(struct rman_tx_list));

		rman_tx_list->rman_tx = rman_tx_init(f2rcfg->rio_port, fqid,
						     fqr->count, f2rcfg->wq,
						     f2rcfg->tran);

		if (!rman_tx_list->rman_tx)
			goto _err;

#ifdef FRA_TX_CONFIRM
		rman_tx_status_listen(rman_tx_list->rman_tx, 0, 1, dist,
				      dist_tx_confirm_cb);
#endif
#ifdef FRA_MBOX_MULTICAST
		rman_tx_enable_multicast(rman_tx_list->rman_tx,
					 f2rcfg->did >> RM_MBOX_MG_SHIFT,
					 f2rcfg->did & RM_MBOX_ML_MASK);
#endif

		rman_tx_connect(rman_tx_list->rman_tx, f2rcfg->did);
		list_add_tail(&rman_tx_list->node,
			      &dist->fman_to_rman->f2r_tx_list);
	}
	return dist;
_err:
	dist_finish(dist);
	return NULL;

}

static int dist_rx_tx_mapping(struct dist_order *dist_order)
{
	struct distribution *first, *second;
	struct hash_opt *opt;
	struct fra_fman_port *port;
	int i;

	if (!dist_order || !dist_order->dist || !dist_order->dist->next)
		return 0;


	first = dist_order->dist;
	second = first->next;

	FRA_DBG("Mapping %s--%s", first->cfg->name, second->cfg->name);
	if (first->cfg->type == DIST_TYPE_RMAN_RX) {
		int fqs_num = rman_rx_get_fqs_num(first->rman_rx);
		if (second->cfg->type == DIST_TYPE_RMAN_TX) {
			for (i = 0; i < fqs_num; i++) {
				opt = rman_rx_get_opt(first->rman_rx, i);
				opt_bindto_rman_tx(opt, second->rman_tx, i);
				FRA_DBG("\tRX FQID-0x%x --> TX FQID-0x%x",
					opt->rx_fqid, opt->tx_fqid);
			}
		} else if (second->cfg->type == DIST_TYPE_FMAN_TX) {
			for (i = 0; i < fqs_num; i++) {
				opt = rman_rx_get_opt(first->rman_rx, i);
				opt_bindto_fman_tx(opt, second->fman_tx, i);
				FRA_DBG("\tRX FQID-0x%x --> TX FQID-0x%x",
					opt->rx_fqid, opt->tx_fqid);
			}
		} else
			return -EINVAL;
	} else if (first->cfg->type == DIST_TYPE_FMAN_RX) {
		struct fman_pcd_range *pcd_range;
		port = first->fman_rx;
		list_for_each_entry(pcd_range, &port->list, list) {
			for (i = 0; i < pcd_range->count ; i++) {
				opt = fman_pcd_range_get_opt(pcd_range, i);
				if (second->cfg->type == DIST_TYPE_RMAN_TX)
					opt_bindto_rman_tx(opt,
						second->rman_tx,
						i);
				else if (second->cfg->type ==
							DIST_TYPE_FMAN_TX)
					opt_bindto_fman_tx(opt,
						second->fman_tx,
						i);
				else
					continue;
				FRA_DBG("\tRX FQID-0x%x --> TX FQID-0x%x",
					opt->rx_fqid, opt->tx_fqid);
			}
		}
	} else
		return -EINVAL;
	return 0;
}

void dist_order_start(struct dist_order *dist_order)
{
	struct distribution *dist;
	struct dist_cfg *cfg;

	if (!dist_order)
		return;

	dist = dist_order->dist;
	while (dist) {
		cfg = dist->cfg;
		fprintf(stderr, "Start dist(%s)\n", cfg->name);
		switch (cfg->type) {
		case DIST_TYPE_RMAN_RX:
			rman_rx_enable(dist->rman_rx);
		break;
		case DIST_TYPE_FMAN_RX:
			fman_port_enable_rx(dist->fman_tx);
		break;
		case DIST_TYPE_RMAN_TO_FMAN:
			if (!dist->rman_to_fman)
				break;
			rman_rx_enable(dist->rman_to_fman->rman_rx);
		break;
		case DIST_TYPE_FMAN_TO_RMAN:
			if (!dist->fman_to_rman)
				break;
			fman_port_enable_rx(dist->fman_to_rman->fman_port);
			break;
		default:
			break;
		}
		dist = dist->next;
	}
}

struct dist_order *dist_order_init(struct dist_order_cfg  *dist_order_cfg)
{

	struct dist_order *dist_order;
	struct dist_cfg *dist_cfg;
	struct distribution *dist, *next_dist;
	int err;

	if (!dist_order_cfg || !dist_order_cfg->dist_cfg)
		return NULL;

	/* Check whether dist has a valid network configuration */
	dist_cfg = dist_order_cfg->dist_cfg;
	while (dist_cfg) {
		char *fman_port_name = NULL;
		switch (dist_cfg->type) {
		case DIST_TYPE_FMAN_RX:
			fman_port_name = dist_cfg->
				dist_fman_rx_cfg.fman_port_name;
			break;
		case DIST_TYPE_FMAN_TX:
			fman_port_name = dist_cfg->
				dist_fman_tx_cfg.fman_port_name;
			break;
		case DIST_TYPE_FMAN_TO_RMAN:
			fman_port_name = dist_cfg->
				dist_fman_tx_cfg.fman_port_name;
			break;
		case DIST_TYPE_RMAN_TO_FMAN:
			fman_port_name = dist_cfg->
				dist_fman_tx_cfg.fman_port_name;
			break;
		default:
			break;
		}

		if (fman_port_name && !get_fra_fman_port(fman_port_name)) {
			FRA_DBG("Dist(%s): can not find fman port %s",
				dist_cfg->name, fman_port_name);
			return NULL;
		}

		dist_cfg = dist_cfg->next;
	}

	dist_order = malloc(sizeof(*dist_order));
	if (!dist_order) {
		error(0, errno, "failed to allocate dist_order memory");
		return NULL;
	}
	memset(dist_order, 0, sizeof(*dist_order));
	dist_cfg = dist_order_cfg->dist_cfg;
	dist = dist_order->dist;
	err = 0;
	while (dist_cfg && !err) {
		FRA_DBG("\nTo initialize distribution(%s)", dist_cfg->name);
		switch (dist_cfg->type) {
		case DIST_TYPE_RMAN_RX:
			next_dist = dist_rman_rx_init(dist_cfg);
			break;
		case DIST_TYPE_RMAN_TX:
			next_dist = dist_rman_tx_init(dist_cfg);
			break;
		case DIST_TYPE_FMAN_RX:
			next_dist = dist_fman_rx_init(dist_cfg);
			break;
		case DIST_TYPE_FMAN_TX:
			next_dist = dist_fman_tx_init(dist_cfg);
			break;
		case DIST_TYPE_FMAN_TO_RMAN:
			next_dist = dist_fman_to_rman_init(dist_cfg);
			break;
		case DIST_TYPE_RMAN_TO_FMAN:
			next_dist = dist_rman_to_fman_init(dist_cfg);
			break;
		default:
			next_dist = NULL;
			break;
		}
		if (!next_dist) {
			FRA_DBG("dist(%s) is not been initialized",
				dist_cfg->name);
			err = 1;
			break;
		}
		if (!dist)
			dist_order->dist = next_dist;
		else
			dist->next = next_dist;
		dist = next_dist;
		dist_cfg = dist_cfg->next;
	}
	if (err || !dist_order->dist)
		goto _err;

	err = dist_rx_tx_mapping(dist_order);
	if (err)
		goto _err;

	dist_order_start(dist_order);
	return dist_order;

_err:
	dist_order_finish(dist_order);
	return NULL;
}

int dist_order_list_init(void)
{
	struct dist_order_cfg  *dist_order_cfg;
	struct dist_order  *dist_order;

	if (!fra || !fra->cfg)
		return -EINVAL;

	INIT_LIST_HEAD(&fra->dist_order_list);
	list_for_each_entry(dist_order_cfg,
		&fra->cfg->policy_cfg->dist_order_cfg_list, node) {
		dist_order = dist_order_init(dist_order_cfg);
		if (dist_order)
			list_add_tail(&dist_order->node,
				&fra->dist_order_list);
	}
	return 0;
}

static void rman_interrupt_handler(void)
{
	int status;

	status = rman_interrupt_status();
	if (!status)
		return;

	if (status & RMAN_OTE_ERROR_MASK) {
		error(0, 0, "gets rman outbound transaction error");
		msg_do_reset();
		return;
	}
	if (status & RMAN_ITE_ERROR_MASK) {
		error(0, 0, "gets rman inbound transaction error");
		msg_do_reset();
		return;
	}

	/* A workaround to avoid PFDR low watermark error interrupt */
	if (status & RMAN_BAE_ERROR_MASK) {
		rman_if_rxs_disable();
		/* Waiting to release the buffer */
		sleep(1);
		rman_if_rxs_enable();
	}

#ifdef FRA_ERROR_INTERRUPT_INFO
	if (status & RMAN_OFER_ERROR_MASK)
		error(0, 0, "Outbound frame queue enqueue rejection error");
	if (status & RMAN_IFER_ERROR_MASK)
		error(0, 0, "Inbound frame queue enqueue rejection error");
	if (status & RMAN_BAE_ERROR_MASK)
		error(0, 0, "RMan buffer allocation error");
	if (status & RMAN_T9IC_ERROR_MASK)
		error(0, 0, "Type9 interrupt coalescing drop threshold exceed");
	if (status & RMAN_T8IC_ERROR_MASK)
		error(0, 0, "Type8 interrupt coalescing drop threshold exceed");
	if (status & RMAN_MFE_ERROR_MASK)
		error(0, 0, "RMan message format error");
#endif

	rman_interrupt_clear();
	rman_interrupt_enable();
}

static void *interrupt_handler(void *data)
{
	int s, rman_fd, srio_fd, nfds;
	fd_set readset;
	uint32_t junk;

	rman_fd = rman_global_fd();
	srio_fd = fsl_srio_fd(rman_if_get_sriodev());

	if (rman_fd > srio_fd)
		nfds = rman_fd + 1;
	else
		nfds = srio_fd + 1;

	rman_interrupt_clear();
	rman_interrupt_enable();
	fsl_srio_clr_bus_err(rman_if_get_sriodev());
	fsl_srio_irq_enable(rman_if_get_sriodev());

	while (1) {
		FD_ZERO(&readset);
		FD_SET(rman_fd, &readset);
		FD_SET(srio_fd, &readset);
		s = select(nfds, &readset, NULL, NULL, NULL);
		if (s < 0) {
			error(0, 0, "RMan&SRIO select error");
			break;
		}
		if (s) {
			if (FD_ISSET(rman_fd, &readset)) {
				read(rman_fd, &junk, sizeof(junk));
				rman_interrupt_handler();
			}
			if (FD_ISSET(srio_fd, &readset)) {
				read(srio_fd, &junk, sizeof(junk));
				fsl_srio_irq_handler(rman_if_get_sriodev());
			}
		}
	}

	pthread_exit(NULL);
}

void fra_interrupt_handler_start(void)
{
	int ret;
	pthread_t interrupt_handler_id;

	ret = pthread_create(&interrupt_handler_id, NULL,
			     interrupt_handler, NULL);
	if (ret)
		error(0, errno, "Create interrupt handler thread error");
}

int fra_init(const struct usdpaa_netcfg_info *netcfg,
	     const struct fra_cfg *fra_cfg)
{
	struct fra_fman_port_cfg *fra_fman_port_cfg;
	int err;

	if (!fra_cfg) {
		error(0, 0, "Fra is not been configured");
		return -EINVAL;
	}

	fra = malloc(sizeof(struct fra));
	if (!fra) {
		error(0, errno, "failed to allocate fra memory");
		return -errno;
	}
	memset(fra, 0, sizeof(*fra));
	INIT_LIST_HEAD(&fra->dist_order_list);
	fra->cfg = fra_cfg;

	if (rman_if_init(&fra_cfg->rman_cfg)) {
		error(0, 0, "failed to initialize rman if");
		err = -EINVAL;
		goto _err;
	}

	list_for_each_entry(fra_fman_port_cfg,
		&fra_cfg->fman_port_cfg_list, node)
		fman_port_init(fra_fman_port_cfg, netcfg);

	err = dist_order_list_init();
	if (err)
		goto _err;

	fra_interrupt_handler_start();

	return 0;
_err:
	fra_finish();
	return err;
}

static void fra_rman_reset(void)
{
	/* Disbale fman and rman rx and remove all fqs */
	dist_order_list_finish();

	/* Wait until RMan inbound message manager idle */
	while (rman_rx_busy())
		;

	/* Stop SRIO ports */
	rman_if_ports_stop();

	/* Wait until RMan outbound message manager idle */
	while (rman_tx_busy())
		;

	/* Reset RMan */
	rman_reset();

	/* Start SRIO ports */
	rman_if_ports_start();

	/* Reconfigure RMan */
	rman_if_reconfig(&fra->cfg->rman_cfg);

	/* Initialize all the fqs and enable FMan and RMan rx */
	dist_order_list_init();
}

void fra_reset(void)
{
	fra_rman_reset();
	rman_interrupt_enable();
}
