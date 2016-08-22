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

#ifndef _HELPER_COMMON_H_
#define _HELPER_COMMON_H_

#include <usdpaa/of.h>
#include <usdpaa/fsl_usd.h>
#include <usdpaa/fsl_qman.h>
#include <usdpaa/fsl_bman.h>
#include <usdpaa/dma_mem.h>
#include <internal/compat.h>
#include <usdpaa/fman.h>

#include <netinet/ip.h>

/* The common definition for helper function */
#undef DEBUG_ON
#ifdef DEBUG_ON
#define DEBUG               TRACE
#define DBPKT               fsl_dump
#define DBG_MEMSET          memset
#define TRACE(fmt, args...) \
	printf("[cpu %d]%s: "fmt, get_cpu_id(), \
		__func__, ##args)

#else
#define DEBUG(args...) do {} while (0)
#define DBPKT(args...) do {} while (0)
#define DBG_MEMSET(args...) do {} while (0)
#define TRACE(fmt, args...) \
	printf("[cpu %d]"fmt, get_cpu_id(), ##args)
#endif

#define vtop(v) __dma_mem_vtop((void *)v)
#define ptov(p) __dma_mem_ptov(p)
#define DMA_MEM_ALLOC(algn, sz) __dma_mem_memalign(algn, sz)
#define DMA_MEM_FREE(ptr)  \
	do {                   \
		if (ptr != NULL) { \
			__dma_mem_free(ptr);\
			ptr = NULL;    \
		}                  \
	} while (0)
#define OK     0
#define ERROR -1

/* common decalration */
void fsl_dump(const char *comments, u8 *pkt, u32 len);
void fsl_free_bman_buffer(struct bman_pool *bp, struct bm_buffer *buf,
			  int count, int flags);
u16 fsl_ip_cksum(u8 *pkt, u8 len);
void fsl_iphdr_create(struct iphdr *iph, u32 sip, u32 dip, u8 proto);
void fsl_fq_clean(struct qman_fq *fq);
void fsl_drop_frame(const struct qm_fd *fd);

static inline int get_cpu_id(void)
{
	extern __PERCPU u8 curr_cpu_id;
	return curr_cpu_id;
}

static inline struct bman_pool *get_bman_pool(int bpid)
{
	extern __PERCPU struct bman_pool *helper_pool[];
	return helper_pool[bpid];
}

static inline int fsl_send_frame(struct qman_fq *fq, struct qm_fd *fd,
				 int flags)
{
	int ret;
retry:
	ret = qman_enqueue(fq, fd, flags);
	if (unlikely(ret == -EBUSY))
		goto retry;
	else if (unlikely(ret != 0))
		TRACE("qman enqueue, fqid 0x%x, ret = %d\n", fq->fqid, ret);

	return ret;
}

static inline int fsl_send_frame_orp(struct qman_fq *fq,
					 struct qm_fd *fd, int flags,
					 struct qman_fq *orp, u32 seqnum)
{
	int ret;
retry:
	ret = qman_enqueue_orp(fq, fd, flags, orp, seqnum);
	if (unlikely(ret == -EBUSY))
		goto retry;
	else if (unlikely(ret != 0))
		TRACE("qman enqueue orp, fqid 0x%x, ret = %d\n", fq->fqid, ret);

	return ret;
}

#endif
