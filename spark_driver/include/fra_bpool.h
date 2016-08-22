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

#ifndef _FRA_BPOOL_H_
#define _FRA_BPOOL_H_

#include <usdpaa/compat.h>
#include <usdpaa/fsl_bman.h>
#include <usdpaa/fsl_qman.h>
#include <usdpaa/dma_mem.h>
#include <usdpaa/compat.h>
#include <error.h>

#undef BPOOL_DEPLETION
#define BPOOL_BACKOFF_CYCLES	512
#define POOL_MAX		64

#ifdef BPOOL_DEBUG
#define BPDBG(fmt, args...) error(0, 0, "BPOOL:"fmt, ##args)
#else
#define BPDBG(fmt, args...) do { ; } while (0)
#endif

struct bpool_config {
	int bpid;
	uint32_t num;
	uint32_t sz;
};

struct bpool_node {
	struct bman_pool	*pool;
	struct bpool_config	cfg;
};

extern struct bpool_node bpool_array[POOL_MAX];

static inline struct bman_pool *bpid_to_bpool(uint8_t bpid)
{
	return bpool_array[bpid].pool;
}

static inline uint32_t bpool_get_size(uint8_t bpid)
{
	return bpool_array[bpid].cfg.sz;
}

static inline void
bm_buffer_free(struct bman_pool *bp, const struct bm_buffer *buf, int count)
{
	while (bman_release(bp, buf, count, 0))
		cpu_spin(BPOOL_BACKOFF_CYCLES);
}

static inline void bpool_fd_free(const struct qm_fd *fd)
{
	struct bm_buffer _bmb, bmb[8];
	const struct qm_sg_entry *sgt;
	struct bman_pool *_bp, *bp;

	if (qm_fd_addr(fd) == 0) {
		BPDBG("Bpool: can not release buffer of address 0x0");
		return;
	}

	if (fd->bpid >= POOL_MAX)
		return;

	bm_buffer_set64(&_bmb, qm_fd_addr(fd));
	_bp = bpool_array[fd->bpid].pool;
	if (!_bp) {
		error(0, ENXIO, "Bpool: bpool(id:%d) is not exist",
		      fd->bpid);
		return;
	}
	if (fd->format == qm_fd_sg) {
		int i, j;
		sgt = __dma_mem_ptov(qm_fd_addr(fd)) + fd->offset;
		i = 0;
		do {
			bp = bpool_array[sgt[i].bpid].pool;
			if (!bp) {
				error(0, ENXIO, "Bpool: bpool(id:%d)"
				      " is not exist", fd->bpid);
				return;
			}
			j = 0;
			do {
				if (sgt[i].extension) {
					error(0, ENOTSUP,
					      "Not support multiple"
					      "s/g frame format");
				}
				bm_buffer_set64(&bmb[j], qm_sg_addr(&sgt[i]));
				j++; i++;
			} while (j < ARRAY_SIZE(bmb) &&
				 !sgt[i-1].final &&
				 sgt[i-1].bpid == sgt[i].bpid);
			BPDBG("Bpool: release %d buffer to bpool(id:%d)",
			      j, sgt[i-1].bpid);
			bm_buffer_free(bp, bmb, j);
		} while (!sgt[i-1].final);
	}
	BPDBG("Bpool: release one buffer to bpool(id:%d)", fd->bpid);
	bm_buffer_free(_bp, &_bmb, 1);
}

static inline int
bpool_buffer_acquire(uint8_t bpid, struct bm_buffer *bufs,
		     uint8_t num, uint32_t flags)
{
	if (bpid >= POOL_MAX)
		return -EINVAL;
	if (!bpool_array[bpid].pool) {
		error(0, ENXIO, "Bpool(%d) is not exist", bpid);
		return -ENXIO;
	}
	return bman_acquire(bpool_array[bpid].pool, bufs, num, flags);
}

static inline void bpool_buffer_free(const struct bm_buffer *bmb)
{
	struct bman_pool *bp;

	if (bmb->bpid >= POOL_MAX)
		return;

	bp = bpool_array[bmb->bpid].pool;
	if (!bp) {
		error(0, ENXIO, "Bpool: bpool(id:%d) dose not exist",
		      bmb->bpid);
		return;
	}
	bm_buffer_free(bp, bmb, 1);
}

void bpools_init(const struct bpool_config *bpcfg, int count);
void bpools_finish(void);

#endif	/* _FRA_BPOOL_H_ */
