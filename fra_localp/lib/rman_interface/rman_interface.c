/* Copyright (c) 2011-2012 Freescale Semiconductor, Inc.
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

#include <usdpaa/compat.h>
#include <fra_fq_interface.h>
#include <rman_interface.h>

#include <stdio.h>

#define FRA_MAX_NUM_OF_IB 4

/* This struct holds the default stashing opts for Rx FQ configuration. */
static const struct qm_fqd_stashing default_stash_opts = {
	.annotation_cl = FRA_STASH_ANNOTATION_CL,
	.data_cl = FRA_STASH_DATA_CL,
	.context_cl = FRA_STASH_CONTEXT_CL
};

struct rman_if {
	struct rman_dev *rmdev;
	struct rman_inbound_block *rmib[FRA_MAX_NUM_OF_IB];
	struct rman_cfg cfg;
	struct srio_dev *sriodev;
	int port_connected;
	u16 tx_channel_id[RMAN_MAX_NUM_OF_CHANNELS];
	uint32_t msg_size[RIO_TYPE_NUM];
	int sg_size;
	struct list_head rman_rx_list;
	struct list_head rman_tx_list;
};

struct rman_status {
	struct qman_fq fq;
	void *pvt;
	nonhash_handler handler;
} ____cacheline_aligned;

struct rman_rx_hash {
	struct qman_fq fq;
	struct hash_opt opt;
	void *pvt;
	hash_handler handler;
} ____cacheline_aligned;

struct rman_rx {
	struct list_head node;
	struct rman_inbound_block *rmib;
	struct ibcu_cfg cfg;
	struct rman_status *status;
	int fqs_num;
	struct rman_rx_hash *hash;
};

struct rman_tx_hash {
	struct rman_outb_md md;
	struct qman_fq fq;
} ____cacheline_aligned;

struct rman_tx {
	struct list_head node;
	struct rio_tran *tran;
	struct rman_status *status;
	int fqs_num;
	struct rman_tx_hash *hash;
};

static struct rman_if *rmif;

static inline void msg_set_fd(struct msg_buf *msg, const struct qm_fd *fd)
{
	msg->flag = USING_FD;
	msg->fd = (struct qm_fd *)fd;
}

static inline void msg_set_bmb(struct msg_buf *msg, const struct bm_buffer *bmb)
{
	msg->flag = USING_BMB;
	msg->bmb = *bmb;
}

uint32_t msg_max_size(enum RIO_TYPE type)
{
	return rmif->msg_size[type];
}

struct msg_buf *msg_alloc(enum RIO_TYPE type)
{
	struct msg_buf *msg;
	struct bm_buffer bmb;
	uint8_t bpid;
	bpid = rmif->cfg.bpid[type];

	if (bpool_buffer_acquire(bpid, &bmb, 1, 0) <= 0) {
		FRA_DBG("RMan:failed to acquire bpool buffer");
		return NULL;
	}
	msg = __dma_mem_ptov(bm_buf_addr(&bmb));
	msg_set_bmb(msg, &bmb);
	FRA_DBG("RMan: get a bman buffer bpid(%d) phy-addr(%llx),"
		"vitraddr(%p)", bmb.bpid, bm_buf_addr(&msg->bmb), msg);

	msg->data = (uint8_t *)msg + RM_DATA_OFFSET;
	msg->len = 0;
	return msg;
}

struct msg_buf *fd_to_msg(const struct qm_fd *fd)
{
	struct msg_buf		*msg;
	struct qm_sg_entry	*sgt;

	switch (fd->format) {
	case qm_fd_contig:
		if (fd->offset < RM_DATA_OFFSET)
			return NULL;
		msg = __dma_mem_ptov(qm_fd_addr(fd));
		if (!msg)
			return NULL;
		msg->data = (uint8_t *)msg + fd->offset;
		msg->len = fd->length20;
		msg_set_fd(msg, fd);
		break;
	case qm_fd_sg:
		sgt = __dma_mem_ptov(qm_fd_addr(fd)) + fd->offset;
		FRA_DBG("This message is sg format bpid(%d), e(%d)  f(%d)",
			sgt->bpid, sgt->extension, sgt->final);
		if (sgt->final != 1 || sgt->offset < RM_DATA_OFFSET) {
			error(0, 0,
			      "Unsupported fd sg.final(%d)", sgt->final);
			return NULL;
		}
		msg = __dma_mem_ptov(qm_sg_addr(sgt));
		if (!msg)
			return NULL;
		msg->data = (uint8_t *)msg + sgt->offset;
		msg->len = fd->length20;
		msg_set_fd(msg, fd);
		break;
	default:
		error(0, EINVAL, "Unsupported fd format(%d)",
		      fd->format);
		return NULL;
	}
	return msg;
}

static inline int msg_to_fd(struct qm_fd *fd, const struct msg_buf *msg)
{

	if (!fd || !msg)
		return -EINVAL;

	if (msg->flag == USING_FD) {
		*fd = *msg->fd;
		return 0;
	}

	memset(fd, 0, sizeof(*fd));
	fd->format = qm_fd_contig;
	fd->length20 = msg->len;
	fd->bpid = msg->bmb.bpid;
	qm_fd_addr_set64(fd, bm_buffer_get64(&msg->bmb));
	fd->offset = (void *)msg->data - (void *)msg;
	return 0;
}

int rman_rx_get_fqs_num(struct rman_rx *rman_rx)
{
	if (!rman_rx)
		return 0;
	return rman_rx->fqs_num;
}

int rman_rx_get_ib(struct rman_rx *rman_rx)
{
	if (!rman_rx)
		return -EINVAL;
	return rman_ib_idx(rman_rx->rmib);
}

int rman_rx_get_cu(struct rman_rx *rman_rx)
{
	if (!rman_rx)
		return -EINVAL;
	return rman_rx->cfg.ibcu;
}

struct hash_opt *rman_rx_get_opt(struct rman_rx *rx, int idx)
{
	if (!rx || idx >= rx->fqs_num)
		return NULL;
	return &rx->hash[idx].opt;
}

int opt_bindto_rman_tx(struct hash_opt *opt, struct rman_tx *tx, int idx)
{
	struct rman_tx_hash *hash;
	if (!opt || !tx)
		return -EINVAL;

	hash = &tx->hash[idx % tx->fqs_num];
	opt->tx_pvt = &hash->md;
	opt->tx_fqid = qman_fq_fqid(&hash->fq);

	return 0;
}

static int rman_get_rxfqs_num(enum RMAN_FQ_MODE fq_mode,
			      const struct rio_tran *tran)
{
	int fqs_num, bit_mask;

	if (!rmif || !tran)
		return 0;

	bit_mask = (1 << rmif->cfg.fq_bits[tran->type]) - 1;
	if (fq_mode == ALGORITHMIC) {
		switch (tran->type) {
		case RIO_TYPE_MBOX:
			fqs_num = (tran->mbox.ltr_mask & bit_mask) + 1;
			break;
		case RIO_TYPE_DSTR:
			fqs_num = (tran->dstr.streamid_mask & bit_mask) + 1;
			break;
		default:
			fqs_num = 1;
			break;
		}
	} else
		fqs_num = 1;

	return fqs_num;
}

static int rman_get_rxfqid_offset(enum RMAN_FQ_MODE fq_mode,
				  const struct rio_tran *tran, int i)
{
	int offset, bit_mask;

	if (!rmif || !tran)
		return 0;

	bit_mask = (1 << rmif->cfg.fq_bits[tran->type]) - 1;
	if (fq_mode == ALGORITHMIC) {
		switch (tran->type) {
		case RIO_TYPE_MBOX:
			offset = (tran->mbox.ltr + i)
				 & tran->mbox.ltr_mask
				 & bit_mask ;
			break;
		case RIO_TYPE_DSTR:
			offset = (tran->dstr.streamid + i) &
				 tran->dstr.streamid_mask &
				 bit_mask;
			break;
		default:
			offset = 0;
			break;
		}
	} else
		offset = 0;
	return offset;
}

static enum qman_cb_dqrr_result
rman_rx_dqrr(struct qman_portal *portal, struct qman_fq *fq,
	     const struct qm_dqrr_entry *dqrr)
{
	struct rman_rx_hash *hash = container_of(fq, struct rman_rx_hash, fq);

	FRA_DBG("%s receives a msg frame fqid(0x%x) msg format(%d)"
		"bpid(%d) addr(0x%llx) len(%d) offset(%d) "
		" status(0x%x)",
		__func__, fq->fqid, dqrr->fd.format,
		dqrr->fd.bpid, qm_fd_addr_get64(&dqrr->fd),
		dqrr->fd.length20, dqrr->fd.offset,
		dqrr->fd.status);

	PRE_DQRR();
	if (likely(hash->handler))
		hash->handler(hash->pvt, &hash->opt, &dqrr->fd);
	else
		fra_drop_frame(&dqrr->fd);
	return POST_DQRR();
}

static enum qman_cb_dqrr_result
rman_status_dqrr(struct qman_portal *portal,
		    struct qman_fq *fq,
		    const struct qm_dqrr_entry *dqrr)
{
	struct rman_status *status;
	status = container_of(fq, struct rman_status, fq);

	FRA_DBG("%s receives a status frame from fqid(%d) status(0x%08x)",
		__func__, fq->fqid, dqrr->fd.status);

	PRE_DQRR();
	if (status->handler)
		status->handler(status->pvt, &dqrr->fd);
	else
		fra_drop_frame(&dqrr->fd);
	return POST_DQRR();
}

void rman_rx_disable(struct rman_rx *rx)
{
	if (!rx)
		return;

	/* Disable inbound block classification unit */
	rman_disable_ibcu(rx->rmib, rx->cfg.ibcu);
}

void rman_rx_finish(struct rman_rx *rx)
{
	int i;

	if (!rx)
		return;

	/* Disable inbound block classification unit */
	if (rx->rmib)
		rman_release_ibcu(rx->rmib, rx->cfg.ibcu);

	if (rx->status) {
		fra_teardown_fq(&rx->status->fq);
		__dma_mem_free(rx->status);
	}

	if (rx->hash) {
		for (i = 0; i < rx->fqs_num; i++)
			fra_teardown_fq(&rx->hash[i].fq);
		__dma_mem_free(rx->hash);
	}

	if (rx->node.next && rx->node.prev)
		list_del(&rx->node);

	free(rx);
}

static struct rman_status *rman_status_init(void)
{
	struct rman_status *status;

	status = __dma_mem_memalign(L1_CACHE_BYTES, sizeof(*status));
	if (!status) {
		error(0, 0, "failed to allocate dma mem\n");
		return NULL;
	}
	memset(status, 0, sizeof(*status));

	fra_fq_nonpcd_init(&status->fq, 0,
			   FRA_NONHASH_WQ, FRA_NONHASH_CHANNEL,
			   &default_stash_opts, rman_status_dqrr);
	return status;
}

static int rman_rx_ibcu_request(struct rman_rx *rx)
{
	int i;

	if (!rx)
		return -EINVAL;

	for (i = 0; i < FRA_MAX_NUM_OF_IB; i++) {
		/* first initialize inbound block */
		if (!rmif->rmib[i])
			rmif->rmib[i] = rman_ib_init(i);
		if (!rmif->rmib[i])
			continue;

		rx->cfg.ibcu = rman_request_ibcu(rmif->rmib[i]);
		if (rx->cfg.ibcu < 0)
			continue;
		rx->rmib = rmif->rmib[i];
		return 0;
	}
	return -EINVAL;
}

struct rman_rx *
rman_rx_init(int hash_init, uint32_t fqid, int fq_mode, uint8_t wq,
	     u16 channel, struct rio_tran *tran,
	     void *pvt, hash_handler handler)
{
	int i;
	struct rman_rx *rx;
	struct rman_rx_hash *hash;
	size_t size;

	if (!rmif || !fqid || !tran)
		return NULL;

	rx = malloc(sizeof(*rx));
	if (!rx)
		return NULL;
	memset(rx, 0, sizeof(*rx));

	if (rman_rx_ibcu_request(rx))
		goto _err;

	if (!rman_get_ibef(rx->rmib)) {
		rx->status = rman_status_init();
		if (!rx->status)
			goto _err;
		rman_set_ibef(rx->rmib, qman_fq_fqid(&rx->status->fq));
	}

	rx->cfg.tran = tran;
	rx->cfg.fqid = fqid;
	rx->cfg.fq_mode = fq_mode;
	rx->fqs_num = rman_get_rxfqs_num(fq_mode, tran);
	if (rx->fqs_num <= 0)
		goto _err;

	if (!hash_init) {
		list_add(&rx->node, &rmif->rman_rx_list);
		return rx;
	}
	size = rx->fqs_num * sizeof(*hash);

	rx->hash = __dma_mem_memalign(L1_CACHE_BYTES, size);
	if (!rx->hash) {
		error(0, 0, "failed to allocate dma mem\n");
		goto _err;
	}
	memset(rx->hash, 0, size);

	for (i = 0; i < rx->fqs_num; i++) {
		hash = &rx->hash[i];
		hash->opt.rx_fqid = fqid +
			rman_get_rxfqid_offset(fq_mode, tran, i);
		fra_fq_pcd_init(&hash->fq, hash->opt.rx_fqid,
				wq, channel, &default_stash_opts,
				rman_rx_dqrr);
		hash->pvt = pvt;
		hash->handler = handler;
	}

	list_add(&rx->node, &rmif->rman_rx_list);
	return rx;
_err:
	rman_rx_finish(rx);
	return NULL;
}

int rman_rx_error_listen(struct rman_rx *rx, void *pvt,
			     nonhash_handler handler)
{
	if (!rx || !rx->status)
		return -EINVAL;

	rx->status->pvt = pvt;
	rx->status->handler = handler;

	return 0;
}

int rman_rx_listen(struct rman_rx *rx, uint8_t port, uint8_t port_mask,
		   uint16_t sid, uint16_t sid_mask)
{
	int err;
	struct ibcu_cfg *cfg;

	if (!rx)
		return -EINVAL;

	cfg = &rx->cfg;

	FRA_DBG("Bind FQID 0x%x to IBCU %d", cfg->fqid, cfg->ibcu);

	cfg->port = port;
	cfg->port_mask = port_mask;
	cfg->sid = sid;
	cfg->sid_mask = sid_mask;
	cfg->did = 0;
	cfg->did_mask = 0xffff;
	cfg->bpid = rmif->cfg.bpid[cfg->tran->type];
	cfg->sgbpid = rmif->cfg.sgbpid;
	cfg->msgsize = rmif->msg_size[cfg->tran->type];
	cfg->sgsize = rmif->sg_size;
	cfg->data_offset = RM_DATA_OFFSET;
	err = rman_config_ibcu(rx->rmib, cfg);
	return err;
}

void rman_rx_enable(struct rman_rx *rx)
{
	if (!rx)
		return;

	/* Enable inbound block classification unit */
	rman_enable_ibcu(rx->rmib, rx->cfg.ibcu);
}

struct rman_tx *
rman_tx_init(uint8_t port, int fqid, int fqs_num, uint8_t wq,
	     struct rio_tran *tran)
{
	struct rman_tx *tx;
	struct rman_tx_hash *hash;
	struct rman_outb_md *md;
	int i;
	size_t size;
	uint64_t cont_a;

	if (!rmif || !tran || port >= RMAN_MAX_NUM_OF_CHANNELS)
		return NULL;

	tx = malloc(sizeof(*tx));
	if (!tx)
		return NULL;
	memset(tx, 0, sizeof(*tx));

	tx->status = rman_status_init();
	if (!tx->status)
		goto _err;

	tx->tran = tran;
	tx->fqs_num = fqs_num;
	size = tx->fqs_num * sizeof(struct rman_tx_hash);
	tx->hash = __dma_mem_memalign(L1_CACHE_BYTES, size);
	if (!tx->hash) {
		error(0, 0, "failed to allocate dma mem\n");
		goto _err;
	}
	memset(tx->hash, 0, size);

	for (i = 0; i < tx->fqs_num; i++) {
		hash = &tx->hash[i];
		md = &hash->md;
		md->ftype = tran->type;
		md->br = 1;
		md->cs = 0;
		md->es = 0;
		md->status_fqid = qman_fq_fqid(&tx->status->fq);
		md->did = 0;
		md->count = 0;
		md->flowlvl = tran->flowlvl;
		md->tint = port;
		switch (tran->type) {
		case RIO_TYPE_MBOX:
			md->retry = 255;
			md->dest = (tran->mbox.ltr +
				    (i & tran->mbox.ltr_mask))
				    << 6 | tran->mbox.mbox;
			break;
		case RIO_TYPE_DSTR:
			md->dest = tran->dstr.streamid +
				   (i & tran->dstr.streamid_mask);
			md->other_attr = tran->dstr.cos;
			break;
		case RIO_TYPE_DBELL:
		case RIO_TYPE_PW:
			break;
		default:
			error(0, 0, "Not support SRIO type %d", tran->type);
			goto _err;
		}

#ifdef FRA_CORE_COPY_MD
		cont_a = 0;
#else
		cont_a = __dma_mem_vtop(md);
#endif
		fra_fq_tx_init(&hash->fq , fqid ? fqid + i : 0,
				wq, rmif->tx_channel_id[port],
				cont_a, 0);
	}

	list_add(&tx->node, &rmif->rman_tx_list);
	return tx;
_err:
	rman_tx_finish(tx);
	return NULL;
}

int rman_tx_enable_multicast(struct rman_tx *tx, int mg, int ml)
{
	int i;
	struct rman_outb_md *md;

	if (!tx || !tx->tran)
		return -EINVAL;

	if (tx->tran->type != RIO_TYPE_MBOX) {
		error(0, 0, "Multicast only support Mailbox transaction");
		return -EINVAL;
	}

	for (i = 0; i < tx->fqs_num; i++) {
		md = &tx->hash[i].md;
		md->mm = 1;
		md->message_group = mg;
		md->message_list = ml;
	}
	return 0;
}

int rman_tx_status_listen(struct rman_tx *tx, int error_flag,
			  int complete_flag, void *pvt,
			  nonhash_handler handler)
{
	int i;
	struct rman_outb_md *md;

	if (!tx || !tx->status)
		return -EINVAL;

	tx->status->pvt = pvt;
	tx->status->handler = handler;

	for (i = 0; i < tx->fqs_num; i++) {
		md = &tx->hash[i].md;
		if (complete_flag)
			md->cs = 1;
		if (error_flag)
			md->es = 1;
	}
	return 0;
}

int rman_tx_connect(struct rman_tx *tx, int did)
{
	int i;

	if (!tx)
		return -EINVAL;

	for (i = 0; i < tx->fqs_num; i++) {
#ifdef FRA_VIRTUAL_MULTI_DID
		tx->hash[i].md.did = did + i;
#else
		tx->hash[i].md.did = did;
#endif
	}
	return 0;
}

void rman_tx_finish(struct rman_tx *tx)
{
	int i;

	if (!tx)
		return;

	if (tx->status) {
		fra_teardown_fq(&tx->status->fq);
		__dma_mem_free(tx->status);
	}

	if (tx->hash) {
		for (i = 0; i < tx->fqs_num; i++)
			fra_teardown_fq(&tx->hash[i].fq);
		__dma_mem_free(tx->hash);
	}

	if (tx->node.next && tx->node.prev)
		list_del(&tx->node);

	free(tx);
}

int rman_send_frame(struct hash_opt *opt, const struct qm_fd *fd)
{
#ifdef ENABLE_FRA_DEBUG
	struct rman_outb_md *stdmd = opt->tx_pvt;
#endif

#ifdef FRA_CORE_COPY_MD
	struct rman_outb_md *md;
	if (fd->format == qm_fd_contig) {
		if (fd->offset < RM_DATA_OFFSET)
			return -EINVAL;
		md = __dma_mem_ptov(qm_fd_addr(fd));
	} else if (fd->format == qm_fd_sg) {
		const struct qm_sg_entry *sgt;
		sgt = __dma_mem_ptov(qm_fd_addr(fd)) + fd->offset;
		if (sgt->offset < RM_DATA_OFFSET)
			return -EINVAL;
		md = __dma_mem_ptov(qm_sg_addr(sgt));
	} else
		return -EINVAL;

	memcpy(md, opt->tx_pvt, sizeof(*md));
	md->count = fd->length20;
#endif

#ifdef ENABLE_FRA_DEBUG
	switch (stdmd->ftype) {
	case RIO_TYPE_MBOX:
		FRA_DBG("sends to device(%d) a msg using mailbox"
			" mbox(%d) ltr(%d) fq(0x%x)",
			stdmd->did, stdmd->dest & 3, (stdmd->dest >> 6) & 3,
			opt->tx_fqid);
			break;
	case RIO_TYPE_DSTR:
		FRA_DBG("sends to device(%d) a msg using"
			" data-streaming"
			" cos(0x%x) streamid(0x%x) fq(0x%x)",
			stdmd->did, stdmd->other_attr, stdmd->dest,
			opt->tx_fqid);
		break;
	case RIO_TYPE_PW:
		FRA_DBG(
			"sends to device(%d) a msg using port-write"
			" dest(0x%x) other_attr(0x%x) fq(0x%x)",
			stdmd->did, stdmd->dest,
			stdmd->other_attr, opt->tx_fqid);
		break;
	default:
		FRA_DBG(
			"sends to device(%d) a msg using %d"
			" dest(0x%x) other_attr(0x%x) fq(0x%x)",
			stdmd->did, stdmd->ftype,
			stdmd->dest, stdmd->other_attr, opt->tx_fqid);
		break;
	}
#endif

	fra_send_frame(opt->tx_fqid, fd);
	return 0;
}

int rman_send_msg(struct rman_tx *tx, int hash_idx, struct msg_buf *msg)
{
	struct qm_fd fd;
	struct rman_tx_hash *hash;
	struct hash_opt opt;

	if (!tx || !msg)
		return -EINVAL;

	if (msg_to_fd(&fd, msg))
		return -EINVAL;

	hash = &tx->hash[hash_idx % tx->fqs_num];
	opt.tx_pvt = &hash->md;
	opt.tx_fqid = qman_fq_fqid(&hash->fq);

	return rman_send_frame(&opt, &fd);
}

struct srio_dev *rman_if_get_sriodev(void)
{
	if (!rmif)
		return NULL;
	return rmif->sriodev;
}

int rman_if_port_connet(uint8_t port)
{
	int port_num;

	if (!rmif || !rmif->sriodev)
		return -EINVAL;

	if (rmif->port_connected & (1 << port))
		return 0;

	port_num = fsl_srio_get_port_num(rmif->sriodev);
	if (port >= port_num)
		return -EINVAL;

	if (fsl_srio_connection(rmif->sriodev, port))
		return -EINVAL;

	rmif->port_connected |= 1 << port;

	return 0;
}

void rman_if_ports_stop(void)
{
	int i, port_num;

	port_num = fsl_srio_get_port_num(rmif->sriodev);
	for (i = 0; i < port_num; i++) {
		if (rmif->port_connected & (1 << i)) {
			fsl_srio_drain_enable(rmif->sriodev, i);
			fsl_srio_port_disable(rmif->sriodev, i);
		}
	}
}

void rman_if_ports_start(void)
{
	int i, port_num;

	port_num = fsl_srio_get_port_num(rmif->sriodev);
	for (i = 0; i < port_num; i++) {
		if (rmif->port_connected & (1 << i)) {
			fsl_srio_port_enable(rmif->sriodev, i);
			fsl_srio_drain_disable(rmif->sriodev, i);
		}
	}
}

void rman_if_status(void)
{
	int socket_counts, fq_counts;
	struct rman_rx *rx;
	struct rman_tx *tx;
	int i, port_num, port_width;

	if (!rmif) {
		fprintf(stderr, "RMan interface has not been initialized\n");
		return;
	}

	port_num = fsl_srio_get_port_num(rmif->sriodev);
	for (i = 0; i < port_num; i++) {
		if (rmif->port_connected & (1 << i)) {
			fprintf(stderr, "\tUse SRIO port %d: ", i);
			port_width = fsl_srio_port_width(rmif->sriodev, i);
			switch (port_width) {
			case 0:
				fprintf(stderr, "using lane 0\n");
				break;
			case 1:
				fprintf(stderr, "using lane 2\n");
				break;
			case 2:
				fprintf(stderr, "using lane 0-3\n");
				break;
			case 3:
				fprintf(stderr, "using lane 0-1\n");
				break;
			default:
				fprintf(stderr, "cannot get valid width\n");
			}

		}
	}

	socket_counts = fq_counts = 0;
	list_for_each_entry(rx, &rmif->rman_rx_list, node) {
		socket_counts++;
		fq_counts += rx->fqs_num;
	}
	fprintf(stderr, "\tCreate %d RX sockets and %d frame queues\n",
		socket_counts, fq_counts);

	socket_counts = fq_counts = 0;
	list_for_each_entry(tx, &rmif->rman_tx_list, node) {
		socket_counts++;
		fq_counts += tx->fqs_num;
	}
	fprintf(stderr, "\tCreate %d TX sockets and %d frame queues\n",
		socket_counts, fq_counts);
}

void rman_if_reconfig(const struct rman_cfg *cfg)
{
	int i;

	if (!rmif || !cfg)
		return;

	rmif->cfg = *cfg;

	for (i = RIO_TYPE0; i < RIO_TYPE_NUM; i++)
		rmif->msg_size[i] = bpool_get_size(rmif->cfg.bpid[i]);
	rmif->sg_size = bpool_get_size(rmif->cfg.sgbpid);

	rman_dev_config(rmif->rmdev, cfg);
}

int rman_if_init(const struct rman_cfg *cfg)
{
	int err, i;

	if (!cfg)
		return -EINVAL;

	rmif = malloc(sizeof(*rmif));
	if (!rmif) {
		error(0, errno, "malloc()");
		return -errno;
	}
	memset(rmif, 0, sizeof(*rmif));

	err = fsl_srio_uio_init(&rmif->sriodev);
	if (err < 0) {
		error(0, -err, "srio_uio_init()");
		return err;
	}

	rmif->rmdev = rman_dev_init();
	if (!rmif->rmdev) {
		error(0, ENODEV, "rman_dev_init()");
		err = -EINVAL;
		goto _err;
	}
	rman_if_reconfig(cfg);

	for (i = 0; i < RMAN_MAX_NUM_OF_CHANNELS; i++)
		rmif->tx_channel_id[i] = rman_get_channel_id(rmif->rmdev, i);

	INIT_LIST_HEAD(&rmif->rman_rx_list);
	INIT_LIST_HEAD(&rmif->rman_tx_list);
	return 0;
_err:
	rman_if_finish();
	return err;
}

void rman_if_finish(void)
{
	struct rman_rx *rx, *rx_tmp;
	struct rman_tx *tx, *tx_tmp;
	int i;

	if (!rmif)
		return;

	list_for_each_entry_safe(rx, rx_tmp,
			&rmif->rman_rx_list, node)
		rman_rx_finish(rx);

	list_for_each_entry_safe(tx, tx_tmp,
			&rmif->rman_tx_list, node)
		rman_tx_finish(tx);

	if (rmif->sriodev)
		fsl_srio_uio_finish(rmif->sriodev);

	if (rmif->rmdev)
		rman_dev_finish(rmif->rmdev);

	for (i = 0; i < FRA_MAX_NUM_OF_IB; i++)
		rman_ib_finish(rmif->rmib[i]);

	free(rmif);
	rmif = NULL;
}

void rman_if_rxs_disable(void)
{
	struct rman_rx *rx;

	if (!rmif)
		return;

	list_for_each_entry(rx, &rmif->rman_rx_list, node)
		rman_rx_disable(rx);
}

void rman_if_rxs_enable(void)
{
	struct rman_rx *rx;

	if (!rmif)
		return;

	list_for_each_entry(rx, &rmif->rman_rx_list, node)
		rman_rx_enable(rx);
}
