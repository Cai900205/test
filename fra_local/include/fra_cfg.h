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

#ifndef _FRA_CFG_H
#define _FRA_CFG_H

/* Application options */
#undef ENABLE_FRA_DEBUG
#undef FRA_CORE_COPY_MD /* core copy the rman descriptor */
#undef FRA_ERROR_INTERRUPT_INFO /* print error interrupt info */

/*
 * This macro is to enable Mailbox multicast mode
 * Note: Messages are limited to one segment and 256 bytes or less
 */
#undef FRA_MBOX_MULTICAST

/*
 * This macro allows fra to create every transmit frame queue using a
 * different virtual DID, this will break the order dependencies between
 * frame queues; the outbound segmentation units can interleaves segments.
 * Fra will achieve a higher transmit rate, get better performance.
 */
#define FRA_VIRTUAL_MULTI_DID

#define FRA_HOLDACTIVE		/* Process each FQ on one portal at a time */
#undef FRA_ORDER_PRESERVATION	/* HOLDACTIVE + enqueue-DCAs */
#undef FRA_ORDER_RESTORATION	/* Use ORP */
#undef FRA_AVOIDBLOCK		/* No full-DQRR blocking of FQs */
#define FRA_PCD_PREFERINCACHE	/* Keep pcd  FQDs in-cache even when empty */
#define FRA_TX_PREFERINCACHE	/* Keep tx FQDs in-cache even when empty */
#define FRA_TX_FORCESFDR	/* Priority allocation of SFDRs to egress */
#undef FRA_DEPLETION		/* Trace depletion entry/exit */
#undef FRA_CGR			/* Track rx and tx fill-levels via CGR */
#undef FRA_CSTD			/* CGR tail-drop */
#undef FRA_CSCN			/* Log CGR state-change notifications */
#define FRA_IDLE_IRQ		/* Block in interrupt-mode when idle */
#define FRA_TX_CONFIRM		/* Use Tx confirmation for all transmits */

/*
 * Application configuration (any modification of these requires an
 * understanding of valid ranges, consequences, etc).
 */
#define FRA_STASH_ANNOTATION_CL	0
#define FRA_STASH_DATA_CL	1
#define FRA_STASH_CONTEXT_CL	0
#define FRA_CGR_RX_PERFQ_THRESH	32
#define FRA_CGR_TX_PERFQ_THRESH	64
#define FRA_BACKOFF_CYCLES	512
#define FRA_ORP_WINDOW_SIZE	7	/* 0->32, 1->64, 2->128, ... 7->4096 */
#define FRA_ORP_AUTO_ADVANCE	1	/* boolean */
#define FRA_ORP_ACCEPT_LATE	3	/* 0->no, 3->yes (for 1 & 2->see RM) */

/* QMan channel settings */
#define FRA_NUM_POOL_CHANNELS 4
#define FRA_NONHASH_WQ 2
#define FRA_DYNAMIC_CHANNEL 0
#define FRA_NONHASH_CHANNEL FRA_DYNAMIC_CHANNEL

/*
 * buffer pool settings
 * bp 10 is used to store doorbell messages
 * bp 11 is used to store data-streaming/mailbox messages
 * bp 12 is used to store s/g tables.
 */
#define DMA_MEM_BP4_BPID	10
#define DMA_MEM_BP4_SIZE	80
#define DMA_MEM_BP4_NUM		0x100 /* 0x100*80==20480 (20KB) */
#define DMA_MEM_BP5_BPID	11
#define DMA_MEM_BP5_SIZE	1600
#define DMA_MEM_BP5_NUM		0x2000 /* 0x2000*1600==13107200 (12.5M) */
#define DMA_MEM_BP6_BPID	12
#define DMA_MEM_BP6_SIZE	64
#define DMA_MEM_BP6_NUM		0x2000 /* 0x2000*64==524288 (0.5MB) */

/* DMA memory size */
#define FRA_DMA_MAP_SIZE	0x4000000 /* 64MB */

#endif /* _FRA_CFG_H */
