/*
 * Copyright 2014 Freescale Semiconductor, Inc.
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

#ifndef _RMAN_CFG_H
#define _RMAN_CFG_H

/* Application options */
#ifdef ENABLE_RMAN_DEBUG
#define RMAN_DBG(fmt, args...) \
	do { \
		fprintf(stderr, fmt"\n", ##args); \
	} while (0)
#else
#define RMAN_DBG(fmt, args...)
#endif


#undef RMAN_CORE_COPY_MD /* core copy the rman descriptor */
#undef RMAN_ERROR_INTERRUPT_INFO /* print error interrupt info */

/*
 * This macro is to enable Mailbox multicast mode
 * Note: Messages are limited to one segment and 256 bytes or less
 */
#undef RMAN_MBOX_MULTICAST

/*
 * This macro allows rman to create every transmit frame queue using a
 * different virtual DID, this will break the order dependencies between
 * frame queues; the outbound segmentation units can interleaves segments.
 * Fra will achieve a higher transmit rate, get better performance.
 */
#define RMAN_VIRTUAL_MULTI_DID

#define RMAN_HOLDACTIVE		/* Process each FQ on one portal at a time */
#undef RMAN_ORDER_PRESERVATION	/* HOLDACTIVE + enqueue-DCAs */
#undef RMAN_ORDER_RESTORATION	/* Use ORP */
#undef RMAN_AVOIDBLOCK		/* No full-DQRR blocking of FQs */
#define RMAN_PCD_PREFERINCACHE	/* Keep pcd  FQDs in-cache even when empty */
#define RMAN_TX_PREFERINCACHE	/* Keep tx FQDs in-cache even when empty */
#define RMAN_TX_FORCESFDR	/* Priority allocation of SFDRs to egress */
#undef RMAN_DEPLETION		/* Trace depletion entry/exit */
#undef RMAN_CGR			/* Track rx and tx fill-levels via CGR */
#undef RMAN_CSTD			/* CGR tail-drop */
#undef RMAN_CSCN			/* Log CGR state-change notifications */
#define RMAN_IDLE_IRQ		/* Block in interrupt-mode when idle */
#undef RMAN_TX_CONFIRM		/* Use Tx confirmation for all transmits */

/*
 * Application configuration (any modification of these requires an
 * understanding of valid ranges, consequences, etc).
 */
#define RMAN_STASH_ANNOTATION_CL	0
#define RMAN_STASH_DATA_CL	1
#define RMAN_STASH_CONTEXT_CL	0
#define RMAN_CGR_RX_PERFQ_THRESH	32
#define RMAN_CGR_TX_PERFQ_THRESH	64
#define RMAN_BACKOFF_CYCLES	512
#define RMAN_ORP_WINDOW_SIZE	7	/* 0->32, 1->64, 2->128, ... 7->4096 */
#define RMAN_ORP_AUTO_ADVANCE	1	/* boolean */
#define RMAN_ORP_ACCEPT_LATE	3	/* 0->no, 3->yes (for 1 & 2->see RM) */

/* QMan channel settings */
#define RMAN_NUM_POOL_CHANNELS 4
#define RMAN_NONHASH_WQ 2
#define RMAN_DYNAMIC_CHANNEL 0
#define RMAN_NONHASH_CHANNEL RMAN_DYNAMIC_CHANNEL
/*Rman Tx and Rx FQIDs*/
#define RMAN_RX_FQID		0x3e80
#define RMAN_TX_FQID		0x3500
#define RMAN_TX_FQID_NUM     1
//#define RMAN_TX_FQID_NUM     4

#endif /* _RMAN_CFG_H */
