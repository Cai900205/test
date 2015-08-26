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

#ifndef __CEETM_API_H
#define __CEETM_API_H

#include <stdio.h>
#include <error.h>
#include <stdbool.h>

#include <ppac.h>
#include <usdpaa/fman.h>
#include <usdpaa/fsl_qman.h>
#include <usdpaa/fsl_dpa_stats.h>

#include "dpa_classif_demo.h"

#define NUM_OF_QUEUES				3
#define NUM_OF_CHANNELS				4
#define MAX_NUM_OF_DSCP_KEYS			12
					/* NUM_OF_QUEUES * NUM_OF_CHANNELS */

#if defined(B4860)
#define CCSCN_THRESH_IN				64
#define CCSCN_THRESH_OUT			32
#define LNI_EGRESS_SPEED			500000000 /* bps -> 500mbps */
#define IPG_LENGTH				20
#define LNI_BUCKET_LIMIT			0x257F /* MTU = 0x2580 */
#define NET_IF_RX_PRIORITY			4
#define NET_IF_RX_ANNOTATION_STASH		0
#define NET_IF_RX_DATA_STASH			1
#define NET_IF_RX_CONTEXT_STASH			0

/* Alignment to use for cpu-local structs to avoid coherency problems. */
#define MAX_CACHELINE				64

/* Each thread is represented by a "worker" struct. */
struct worker {
	pthread_t				id;
	struct dpa_stats_cnt_request_params	req_params;
	int					*cnts_len;
} __attribute__((aligned(MAX_CACHELINE)));

struct interface {
	struct qm_ceetm_sp		*sp;
	struct qm_ceetm_lni		*lni;
	struct qm_ceetm_channel		*channel[NUM_OF_CHANNELS];
	struct qm_ceetm_ccg		*ccg[NUM_OF_CHANNELS];
	struct qm_ceetm_cq		*cq[NUM_OF_CHANNELS][NUM_OF_QUEUES];
	struct qm_ceetm_lfq		*lfq[NUM_OF_CHANNELS][NUM_OF_QUEUES];
};

struct cq_counter {
	uint32_t			cq_bytes;
	uint32_t			cq_frames;
};

struct cgr_counter {
	uint32_t			cgr_bytes;
	uint32_t			cgr_frames;
};
#endif

enum ceetm_clean {
	clean_sp,
	clean_lni,
	clean_channel,
	clean_ccg,
	clean_tx_fqs
};

struct traffic_counters {
	struct eth_counter eth;
#if defined(B4860)
	struct cq_counter cq[NUM_OF_CHANNELS * NUM_OF_QUEUES];
	struct cgr_counter cgr[NUM_OF_CHANNELS];
#endif
};

int ceetm_init(int fman, int deq_sp);
int ceetm_free(enum ceetm_clean idx);

int create_ceetm_counters(int dpa_stats_id);
#if defined(B4860)
int ceetm_get_counters_sync(struct dpa_stats_cnt_request_params req_params,
		int *cnts_len);
#endif
void print_ceetm_counters(struct traffic_counters *cnts);

#endif /* __CEETM_API_H */
