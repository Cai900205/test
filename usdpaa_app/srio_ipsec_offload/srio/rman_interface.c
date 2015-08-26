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

#include <usdpaa/compat.h>
#include <rman_fq_interface.h>
#include <rman_interface.h>

#define RMAN_MAX_NUM_OF_IB 4
extern struct srio_dev *sriodev;
extern void teardown_fq(struct qman_fq *fq);

/* This struct holds the default stashing opts for Rx FQ configuration. */
static const struct qm_fqd_stashing default_stash_opts = {
	.annotation_cl = RMAN_STASH_ANNOTATION_CL,
	.data_cl = RMAN_STASH_DATA_CL,
	.context_cl = RMAN_STASH_CONTEXT_CL
};

extern uint32_t bpool_get_size(uint8_t bpid);
struct rman_if {
	struct rman_dev *rmdev;
	struct rman_inbound_block *rmib[RMAN_MAX_NUM_OF_IB];
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
} ____cacheline_aligned;

struct rman_rx {
	struct list_head node;
	struct rman_inbound_block *rmib;
	struct ibcu_cfg cfg;
	struct rman_status *status;
	int fqs_num;
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

uint32_t msg_max_size(enum RIO_TYPE type)
{
	return rmif->msg_size[type];
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

static enum qman_cb_dqrr_result
rman_status_dqrr(struct qman_portal *portal,
		    struct qman_fq *fq,
		    const struct qm_dqrr_entry *dqrr)
{
	struct qm_fd *fd = &dqrr->fd;
    int num = bman_query_free_buffers(pool[fd->bpid]);
    printf("bpid %d avaliable buffers = 0%x\n", fd->bpid, num);
	void *annotations = __dma_mem_ptov(qm_fd_addr(fd));
    if(fd->format == qm_fd_sg) {
        struct qm_sg_entry *sg;
		sg = annotations + fd->offset;
        hexdump(fd, sizeof(struct qm_fd));
    }
	rman_drop_frame(&dqrr->fd);
	rman_if_rxs_disable();
    sleep(1);
	rman_if_rxs_enable();
	return qman_cb_dqrr_consume;
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
	if (!rx)
		return;

	/* Disable inbound block classification unit */
	if (rx->rmib)
		rman_release_ibcu(rx->rmib, rx->cfg.ibcu);

	if (rx->status) {
		teardown_fq(&rx->status->fq);
		__dma_mem_free(rx->status);
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

	rman_init_nonpcd_fq(&status->fq, 0,
			   RMAN_NONHASH_WQ, 0x404,
			   &default_stash_opts, rman_status_dqrr);
	return status;
}

static int rman_rx_ibcu_request(struct rman_rx *rx)
{
	int i;

	if (!rx)
		return -EINVAL;

	for (i = 0; i < RMAN_MAX_NUM_OF_IB; i++) {
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
rman_rx_init(uint32_t fqid, int fq_mode, struct rio_tran *tran)
{
	struct rman_rx *rx;

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

	list_add(&rx->node, &rmif->rman_rx_list);
	return rx;
_err:
	rman_rx_finish(rx);
	return NULL;
}

int rman_rx_listen(struct rman_rx *rx, uint8_t port, uint8_t port_mask,
		   uint16_t sid, uint16_t sid_mask)
{
	int err;
	struct ibcu_cfg *cfg;

	if (!rx)
		return -EINVAL;

	cfg = &rx->cfg;


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
			break;
		default:
			error(0, 0, "Not support SRIO type %d", tran->type);
			goto _err;
		}

#ifdef RMAN_CORE_COPY_MD
		cont_a = 0;
#else
		cont_a = __dma_mem_vtop(md);
#endif
		rman_init_tx_fq(&hash->fq , fqid ? fqid + i : 0,
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

int rman_tx_status_listen(struct rman_tx *tx, int error_flag,  int complete_flag)
{
	int i;
	struct rman_outb_md *md;

	if (!tx || !tx->status)
		return -EINVAL;

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
#ifdef RMAN_VIRTUAL_MULTI_DID
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
		teardown_fq(&tx->status->fq);
		__dma_mem_free(tx->status);
	}

	if (tx->hash) {
		for (i = 0; i < tx->fqs_num; i++)
			teardown_fq(&tx->hash[i].fq);
		__dma_mem_free(tx->hash);
	}

	if (tx->node.next && tx->node.prev)
		list_del(&tx->node);

	free(tx);
}

struct srio_dev *rman_if_get_sriodev(void)
{
	if (!rmif)
		return NULL;
	return rmif->sriodev;
}

int rman_if_port_connect(uint8_t port)
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

int rman_if_init(struct srio_dev *sriodev, const struct rman_cfg *cfg)
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
    
    rmif->sriodev = sriodev;

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

	for (i = 0; i < RMAN_MAX_NUM_OF_IB; i++)
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
