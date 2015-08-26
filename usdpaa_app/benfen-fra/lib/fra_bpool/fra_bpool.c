/* Copyright (c) 2011 Freescale Semiconductor, Inc.
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

#include "fra_bpool.h"
#include <usdpaa/conf.h>

#include <assert.h>
#include <stdio.h>

struct bpool_node bpool_array[POOL_MAX];

#ifdef BPOOL_DEPLETION
static void bp_depletion(struct bman_portal *bm __always_unused,
			 struct bman_pool *p,
			 void *cb_ctx __maybe_unused,
			 int depleted)
{
	uint8_t bpid = bman_get_params(p)->bpid;
	BUG_ON(p != *(typeof(&p))cb_ctx);

	error(0, 0, "%s: BP%u -> %s", __func__, bpid,
	      depleted ? "entry" : "exit");
}
#endif

static int bpool_init(uint8_t bpid)
{
	struct bman_pool_params params = {
		.bpid	= bpid,
#ifdef BPOOL_DEPLETION
		.flags	= BMAN_POOL_FLAG_DEPLETION,
		.cb	= bp_depletion,
		.cb_ctx	= &(bpool_array[bpid].pool),
#endif
	};

	BUG_ON(bpid >= POOL_MAX);
	if (bpool_array[bpid].pool)
		/* this BPID is already handled */
		return 0;

	bpool_array[bpid].pool = bman_new_pool(&params);
	if (!bpool_array[bpid].pool) {
		error(0, ENOMEM,
		      "error: bman_new_pool(%d) failed", bpid);
		return -ENOMEM;
	}
	return 0;
}

/* Initialise and see any BPIDs we've been configured to set up */
static void bpool_node_init(const struct bpool_config *cfg)
{
	struct bman_pool	*pool;
	struct bm_buffer	bufs[8];
	uint32_t		num_bufs = 0;
	uint8_t			bpid;
	uint32_t		rel;
	int			err, loop;

	bpid = cfg->bpid;
	err = bpool_init(bpid);
	if (err < 0) {
		error(0, -err,
		      "BPOOL error: failed to initialize bpool (%d)",
		      bpid);
		return;
	}
	pool = bpool_array[bpid].pool;
	bpool_array[bpid].cfg = *cfg;
	BPDBG("init bpool: the size is %d number is %d", cfg->sz, cfg->num);

	/* Drain the pool of anything already in it. */
	do {
		/* Acquire is all-or-nothing, so we drain in 8s, then in
		 * 1s for the remainder. */
		if (err != 1)
			err = bman_acquire(pool, bufs, 8, 0);
		if (err < 8)
			err = bman_acquire(pool, bufs, 1, 0);
		if (err > 0)
			num_bufs += err;
	} while (err > 0);
	if (num_bufs)
		error(0, 0, "warn: drained %u bufs from BPID %d",
		      num_bufs, bpid);
	/* Fill the pool */
	for (num_bufs = 0; num_bufs < cfg->num; ) {
		rel = (cfg->num - num_bufs) > 8 ? 8 : (cfg->num - num_bufs);
		for (loop = 0; loop < rel; loop++) {
			void *ptr = __dma_mem_memalign(L1_CACHE_BYTES, cfg->sz);
			if (!ptr) {
				fprintf(stderr, "error: insufficient memory\n");
				break;
			}
			bm_buffer_set64(&bufs[loop],
					__dma_mem_vtop(ptr));
		}
		do {
			err = bman_release(pool, bufs, rel, 0);
		} while (err == -EBUSY);
		if (err)
			error(0, -err, "%s", __func__);
		num_bufs += rel;
	}
	error(0, 0, "BPOOL: Release %u bufs to BPID %d",
	      num_bufs, bpid);
}

/* Initialise and see any BPIDs we've been configured to set up */
void bpools_init(const struct bpool_config *bpcfg, int count)
{
	int loop;
	for (loop = 0; loop < count; loop++)
		bpool_node_init(&bpcfg[loop]);
}

void bpools_finish(void)
{
	int loop;
	for (loop = 0; loop < POOL_MAX; loop++) {
		if (bpool_array[loop].pool) {
			bman_free_pool(bpool_array[loop].pool);
			bpool_array[loop].pool = NULL;
		}
	}
}
