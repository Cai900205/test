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

#include "helper_api.h"
#include "helper_interface.h"

/* DMA memory map size */
#define HLP_DMA_MEM_256M    0x10000000
#define HLP_POOL_CHANNELS   1

/* local/global variables */
static u32 helper_sdqcr;
static u32 helper_pool_channels[HLP_POOL_CHANNELS];

__PERCPU u8 curr_cpu_id = 0;
__PERCPU struct bman_pool *helper_pool[HLP_BPOOL_MAX] = { 0 };

static inline struct qman_fq *helper_tx_fq_get(struct packet_desc *pdesc)
{
	int queue = 0;
	struct packet_interface *pif = packet_interface_get(pdesc->port);

	/* get the traffic dest, 0 by default */
	if (likely(pdesc->queue < pif->num_tx_fqs)) {
		queue = pdesc->queue;
	} else if (pdesc->queue >= CEETM_FQID_BASE) {
		/* update fqid for ceetm fq transmission */
		struct qman_fq *fq = local_tx_fq_get();
		fq->fqid = pdesc->queue;
		return fq;
	}

	return &pif->tx[queue].fq;
}

static void helper_pool_channel_init(void)
{
	int ret;
	u16 channel;

	ret = qman_alloc_pool_range(helper_pool_channels,
				    HLP_POOL_CHANNELS, 1, 0);
	if (ret != 1) {
		TRACE("error: pool channel allocation failed\n");
		exit(EXIT_FAILURE);
	}

	channel = helper_pool_channels[0];
	helper_sdqcr = QM_SDQCR_CHANNELS_POOL_CONV(channel);

	TRACE("allocate pool channel 0x%x\n", channel);
}

static void helper_pool_channel_clean(void)
{
	qman_release_pool_range(helper_pool_channels[0], HLP_POOL_CHANNELS);
}

static u32 helper_pool_channel_get(void)
{
	return helper_pool_channels[0];
}

/*
 * The interfaces to application
 */
int fsl_port_loopback_set(u32 port, bool enable)
{
	struct packet_interface *pif = packet_interface_get(port);

	if (pif == NULL || pif->fm_if == NULL) {
		TRACE("port [0x%02x] does NOT support loopback\n", port);
		return ERROR;
	}

	if (enable)
		fman_if_loopback_enable(pif->fm_if);
	else
		fman_if_loopback_disable(pif->fm_if);

	return OK;
}

int fsl_port_promisc_set(u32 port, bool enable)
{
	struct packet_interface *pif = packet_interface_get(port);

	if (pif == NULL || pif->fm_if == NULL) {
		TRACE("port [0x%02x] does NOT support to set loopback\n", port);
		return ERROR;
	}

	if (enable)
		fman_if_promiscuous_enable(pif->fm_if);
	else
		fman_if_promiscuous_disable(pif->fm_if);

	return OK;
}

void *fsl_mem_alloc(u32 mem_size, u32 align_size)
{
	return memalign(align_size, mem_size);
}

void fsl_mem_free(void *addr)
{
	free(addr);
}

void *fsl_buffer_alloc(u32 pool_id)
{
	struct bm_buffer buf;
	int ret = bman_acquire(helper_pool[pool_id], &buf, 1, 0);
	if (ret < 0) {
		TRACE("bman acquire failure, pool %d, ret %d\n", pool_id, ret);
		return NULL;
	}
	return (void *)ptov(buf.addr);
}

void fsl_buffer_free(void *bd, u32 pool_id)
{
	int ret;
	struct bm_buffer buf;
	buf.addr = vtop(bd);

retry:
	ret = bman_release(helper_pool[pool_id], &buf, 1, 0);
	if (unlikely(ret == -EBUSY))
		goto retry;
	else if (ret)
		TRACE("bman release failure, ret %d\n", ret);
}

void fsl_buffer_pool_fill(struct packet_pool_cfg *pool_cfg)
{
	int err;
	int loop;
	struct bman_pool *p;
	struct packet_pool_cfg *bp = pool_cfg;

	for (; bp->bpid != -1; bp++) {
		struct bm_buffer bufs[8];
		int num_bufs = 0;

		p = helper_pool[bp->bpid];
		err = 0;

		/* Drain the pool of anything already in it. */
		if (bp->num > 0) {
			do {
				if (err != 1)
					err = bman_acquire(p, bufs, 8, 0);
				if (err < 8)
					err = bman_acquire(p, bufs, 1, 0);
				if (err > 0)
					num_bufs += err;
			} while (err > 0);
		}
		if (num_bufs)
			TRACE("warn: drained %u bufs from BPID %d\n",
			      num_bufs, bp->bpid);

		/* Fill the pool */
		for (num_bufs = 0; num_bufs < bp->num;) {
			int rel = (bp->num - num_bufs) > 8 ? 8 :
			    (bp->num - num_bufs);
			for (loop = 0; loop < rel; loop++) {
				void *ptr = DMA_MEM_ALLOC(L1_CACHE_BYTES,
							  bp->size);
				if (!ptr) {
					TRACE("error: no space for bpid %d\n",
					      bp->bpid);
					return;
				}
				bm_buffer_set64(&bufs[loop], vtop(ptr));
			}

			do {
				err = bman_release(p, bufs, rel, 0);
			} while (err == -EBUSY);
			if (err)
				TRACE("error: release failure\n");

			num_bufs += rel;
		}

		TRACE("Release %u bufs to BPID %d\n", num_bufs, bp->bpid);
	}
}

void fsl_buffer_pool_init(int global, struct packet_pool_cfg *pool_cfg)
{
	struct bman_pool_params params = {
		.bpid = 0,
		.flags = BMAN_ACQUIRE_FLAG_STOCKPILE,
		.cb = 0,
		.cb_ctx = 0
	};
	struct packet_pool_cfg *bp = pool_cfg;

	for (; bp->bpid != -1; bp++) {
		if (helper_pool[bp->bpid] == NULL) {
			params.bpid = bp->bpid;
			helper_pool[bp->bpid] = bman_new_pool(&params);
			if (helper_pool[bp->bpid] == NULL) {
				TRACE("error: bman_new_pool(%d) failed\n",
				      bp->bpid);
				continue;
			}
		}
	}

	if (global)
		fsl_buffer_pool_fill(pool_cfg);
}

void fsl_buffer_pool_clean(void)
{
	int j;
	struct bman_pool *p;
	for (j = 0; j < HLP_BPOOL_MAX; j++) {
		p = helper_pool[j];
		if (p) {
			bman_flush_stockpile(p, 0);
			bman_free_pool(p);
		}
	}
}
/* init the interfaces listed in DTS */
int fsl_interface_init(struct interface_param *param)
{
	static u32 param_buffer[64] = { 0 };
	const char *if_desc[] = { "offline", "1G", "10G", "mac-less" };

	int ret;
	u32 channel = helper_pool_channel_get();
	struct fman_if *pfif;
	struct interface_param_internal *if_param =
	    (struct interface_param_internal *)param_buffer;

	/* configure port mapping */
	if (param->port >= PACKET_IF_NUM_MAX)
		TRACE("invalid port range, %s-port%d\n",
		      param->name, param->port);

	/* skip the IPC port init, which is not in DTS */
	if (param->type == fman_ipc) {
		memset(if_param, 0, sizeof(struct interface_param_internal));
		if_param->port = param->port;
		TRACE("init port [0x%02x] to thread-%d for IPC\n",
		      param->port, param->id);
		ret = packet_interface_init(if_param);
		if (ret < 0) {
			TRACE("init port [0x%02x] failed\n", param->port);
			return ERROR;
		}
		return OK;
	}

	/* init driver interfaces, if it's depicted in DTS */
	list_for_each_entry(pfif, fman_if_list, node) {
		if (pfif->mac_type == param->type &&
		    pfif->fman_idx == param->fman &&
		    pfif->mac_idx == param->id) {

			if_param->port = param->port;
			if_param->pool_channel = channel;
			if_param->fm_if = pfif;
			if_param->num_tx_fqs = 1;
			if_param->fqid_rx_default = pfif->fqid_rx_err + 1;
			TRACE("init port [0x%02x] to fm%d-mac%d-%s\n",
			      if_param->port, pfif->fman_idx, pfif->mac_idx,
			      if_desc[pfif->mac_type]);
			ret = packet_interface_init(if_param);
			if (ret < 0) {
				TRACE("init port [0x%02x] failed\n",
				      param->port);
				return ERROR;
			}
			break;
		}
	}

	return OK;
}

void fsl_interface_clean(u32 port)
{
	packet_interface_clean(port);
}

/* init user-configured PCD for the interfaces indexed by logic
 * port. The pre-condition is fsl_interface_init.
 */
int fsl_interface_pcd_init(struct interface_pcd_param *param)
{
	int j;
	int ret;
	u32 channel = helper_pool_channel_get();

	/* allocate fixed pool channel for PCD configuration */
	for (j = 0; j < param->num_rx_fq_ranges; j++) {
		if (param->rx_fq_range[j].channel == 0)
			param->rx_fq_range[j].channel = channel;
	}

	ret = packet_interface_pcd_init(param);
	if (ret < 0) {
		TRACE("init interface pcd failed, port [0x%02x]\n",
		      param->port);
		return ret;
	}

	return OK;
}

void fsl_interface_pcd_clean(u32 port)
{
	packet_interface_pcd_clean(port);
}

/* Init IPC interface to specific core, since dedicated
 * channel is used for the FQ, the caller shall be under
 * different threads.
 */
int fsl_interface_ipc_init(struct interface_ipc_param *param)
{
	int ret;

	ret = packet_interface_ipc_init(param);
	if (ret < 0) {
		TRACE("init interface ipc failed, port [0x%02x]\n",
		      param->port);
		return ret;
	}

	return OK;
}

void fsl_interface_ipc_clean(u32 port)
{
	packet_interface_ipc_clean(port);
}

int fsl_interface_ceetm_init(struct interface_ceetm_param *param)
{
	int ret;

	ret = packet_interface_ceetm_init(param);
	if (ret < 0) {
		TRACE("init interface ceetm failed, port [0x%02x]\n",
		      param->port);
		return ret;
	}

	return OK;
}

void fsl_interface_ceetm_clean(u32 port)
{
	packet_interface_ceetm_clean(port);
}

u32 fsl_interface_ceetm_fqid_base(u32 port)
{
	struct packet_interface *pif = packet_interface_get(port);
	if (pif == NULL || pif->ceetm_if == NULL) {
		TRACE("no ceetm configuration on port [0x%02x]\n", port);
		return 0;
	}

	return pif->ceetm_if->fqid_base;
}

/* Receive packet from eth, offline, or IPC interfaces. It's
 * a non-pending call, will return a new allocated packet
 * descriptor to caller, or NULL if no packet coming.
 */
int fsl_pkt_recv(struct packet_desc **ppdesc)
{
	if (packet_interface_dequeue(ppdesc) > 0)
		return OK;

	return ERROR;
}

/* Send the packet defined by the descriptor to the destination
 * port. The private info in packet descriptor will impact the
 * enqueue behavior.
 * The packet decriptor and buffer will be released.
 */
int fsl_pkt_send(struct packet_desc *pdesc)
{
	int ret;
	int flags = 0;
	struct qm_fd fd;
	struct qman_fq *txfq = helper_tx_fq_get(pdesc);

	packet_interface_pd_2_fd(pdesc, &fd);
	DEBUG("port 0x%02x, queue 0x%x, cmd %d, bpid %d, off %d, len %d\n",
	      pdesc->port, pdesc->queue, fd.cmd,
	      fd.bpid, fd.offset, fd.length20);

#if defined(FSL_FQ_HOLDACTIVE_ENABLE) || defined(FSL_FQ_ORP_ENABLE)
	{
		/* send frame to specified tx fq */
		struct packet_desc_private *pdesc_priv =
		    (struct packet_desc_private *)pdesc->priv;
		if (pdesc_priv->fq_type & FQ_TYPE_ORP)
			ret = fsl_send_frame_orp(txfq, &fd, flags,
				((struct qman_fq *)pdesc_priv->fq),
				pdesc_priv->seqnum);
		else if (pdesc_priv->fq_type & FQ_TYPE_HOLDACTIVE) {
			flags = QMAN_ENQUEUE_FLAG_DCA_PTR(pdesc_priv->dqrr) |
			    QMAN_ENQUEUE_FLAG_DCA;
			ret = fsl_send_frame(txfq, &fd, flags);
		} else
			ret = fsl_send_frame(txfq, &fd, flags);
	}
#else
	ret = fsl_send_frame(txfq, &fd, flags);
#endif
	if (ret < 0) {
		TRACE("send frame failure, ret %d\n", ret);
		return ret;
	}
#if 0
	fsl_packet_desc_free(pdesc, HLP_PKT_DESC_BPID);
#endif
	return OK;
}

void fsl_pkt_drop(struct packet_desc *pdesc)
{
	struct qm_fd fd;

	packet_interface_pd_2_fd(pdesc, &fd);
#if defined(FSL_FQ_HOLDACTIVE_ENABLE) || defined(FSL_FQ_ORP_ENABLE)
	{
		struct packet_desc_private *pdesc_priv =
		    (struct packet_desc_private *)pdesc->priv;
		if (pdesc_priv->fq_type & FQ_TYPE_ORP) {
			struct qman_fq *orp = (struct qman_fq *)pdesc_priv->fq;

			DEBUG("orpfq 0x%x, seqnum %d\n", orp->fqid,
				pdesc_priv->seqnum);
			fsl_send_frame_orp(orp, &fd, QMAN_ENQUEUE_FLAG_HOLE,
				orp, pdesc_priv->seqnum);
		} else if (pdesc_priv->fq_type & FQ_TYPE_HOLDACTIVE) {
			struct qm_dqrr_entry *dqrr =
			    (struct qm_dqrr_entry *)pdesc_priv->dqrr;
			DEBUG("hold active fq\n");
			qman_dca(dqrr, 0);
		}
	}
#endif

	fsl_drop_frame(&fd);
	fsl_packet_desc_free(pdesc, HLP_PKT_DESC_BPID);
}

void fsl_cpu_init(int cpu, int global, void_func sync_all)
{
	int ret;

	curr_cpu_id = cpu;

	if (global) {
		ret = of_init();
		if (ret) {
			TRACE("of_init() failed\n");
			exit(EXIT_FAILURE);
		}

		ret = fman_init();
		if (ret) {
			TRACE("fman_init() failed\n");
			exit(EXIT_FAILURE);
		}

		ret = bman_global_init();
		if (ret) {
			TRACE("bman_global_init() failed, ret=%d\n", ret);
			exit(EXIT_FAILURE);
		}

		ret = qman_global_init();
		if (ret) {
			TRACE("qman_global_init() failed, ret=%d\n", ret);
			exit(EXIT_FAILURE);
		}

		dma_mem_generic = dma_mem_create(DMA_MAP_FLAG_ALLOC,
						 NULL, HLP_DMA_MEM_256M);
		if (!dma_mem_generic) {
			TRACE("dma_mem_create() failed\n");
			exit(EXIT_FAILURE);
		}
	}

	/* sync until the init done for 'fman, qman, bman,
	   and dma' components */
	if (sync_all != NULL)
		sync_all();

	DEBUG("sync the 1st time\n");

	/* Initialise bman/qman portals */
	ret = bman_thread_init();
	if (ret) {
		TRACE("bman_thread_init(%d) failed, ret=%d\n", cpu, ret);
		exit(EXIT_FAILURE);
	}

	ret = qman_thread_init();
	if (ret) {
		TRACE("qman_thread_init(%d) failed, ret=%d\n", cpu, ret);
		exit(EXIT_FAILURE);
	}

	if (global)
		helper_pool_channel_init();

	/* sync all cores to wait for buffer pool and FQs init done */
	if (sync_all != NULL)
		sync_all();

	/* init application tx fq, the fqid need be modified during run-time */
	local_tx_fq_init();

	/* dequeue rx pool channel for current thread */
	qman_static_dequeue_add(helper_sdqcr);
	DEBUG("sync the 2nd time\n");
}

void fsl_cpu_exit(int global)
{
	/* clean the local tx fq */
	local_tx_fq_clean();

	/* clear rx pool channel dequeue */
	qman_static_dequeue_del(helper_sdqcr);

	/* release the allocated pool channel */
	if (global) {
		helper_pool_channel_clean();
		of_finish();
	}
}
