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

#include <unistd.h>
#include "ceetm_api.h"

int				tmg_cnt_id = DPA_OFFLD_INVALID_OBJECT_ID;
int				cng_cnt_id = DPA_OFFLD_INVALID_OBJECT_ID;

#if defined(B4860)
struct interface		intf;
static struct worker		*worker;

static void ceetm_congestion_cb(struct qm_ceetm_ccg *ccg, void *cb_ctx,
		int congested)
{
	printf("%s,  CEETM CCGR -> congestion %s\n", __func__,
					congested ? "entry" : "exit");
}
#endif

int ceetm_free(enum ceetm_clean idx)
{
	int err = 0;
#if defined(B4860)
	int i = 0, j = 0;

	switch (idx) {
	case clean_tx_fqs:
		for (i = 0; i < NUM_OF_CHANNELS; i++) {
			for (j = 0; j < NUM_OF_QUEUES; j++) {
				err = qman_ceetm_lfq_release(intf.lfq[i][j]);
				if (err)
					error(0, -err, "Cannot release lfq %d\n",
							intf.lfq[i][j]->idx);
				err = qman_ceetm_cq_release(intf.cq[i][j]);
				if (err)
					error(0, -err, "Cannot release cq %d\n",
							intf.cq[i][j]->idx);
			}
		}
	case clean_ccg:
		for (i = 0; i < NUM_OF_CHANNELS; i++) {
			err = qman_ceetm_ccg_release(intf.ccg[i]);
			if (err)
				error(0, -err, "Cannot release ccg %d\n",
						intf.ccg[i]->idx);
		}
	case clean_channel:
		for (i = 0; i < NUM_OF_CHANNELS; i++) {
			err = qman_ceetm_channel_release(intf.channel[i]);
			if (err)
				error(0, -err, "Cannot release channel%d\n",
						intf.channel[i]->idx);
		}
	case clean_lni:
		err = qman_ceetm_lni_release(intf.lni);
		if (err)
			error(0, -err, "Cannot release lni %d\n", intf.lni->idx);
	case clean_sp:
		err = qman_ceetm_sp_release(intf.sp);
		if (err)
			error(0, -err, "Cannot release sp %d\n", intf.sp->idx);
		break;
	}
#endif

	return err;
}

int ceetm_init(int fman, int deq_sp)
{
	int err = 0;
#if defined(B4860)
	int i = 0, j = 0;
	uint8_t prio_a = 1, prio_b = 2;
	uint16_t we_mask;
	uint32_t cr_speed[NUM_OF_CHANNELS] = {	80000000,   /* bps AF1x */
						90000000,   /* bps AF2x */
						100000000,  /* bps AF3x */
						110000000}; /* bps AF4x */
	uint32_t er_speed[NUM_OF_CHANNELS] = {	85000000,   /* bps AF1x */
						95000000,   /* bps AF2x */
						105000000,  /* bps AF3x */
						115000000}; /* bps AF4x */
	uint64_t context_a = 0x9200000080000000ull, context_b = 0;

	struct qm_ceetm_ccg_params c_params;
	struct qm_ceetm_rate lni_egress_rate, ch_cr_rate, ch_er_rate;

	err = qman_ceetm_sp_claim(&intf.sp, fman, deq_sp);
	if (err) {
		error(0, -err, "Cannot claim sub-portal %d\n", deq_sp);
		ceetm_free(clean_sp);
	} else {
		TRACE("Claimed sub-portal %d successfully\n", deq_sp);
	}

	err = qman_ceetm_lni_claim(&intf.lni, fman, deq_sp);
	if (err) {
		error(0, -err, "Cannot claim LNI %d\n", deq_sp);
		ceetm_free(clean_lni);
	} else {
		TRACE("Claimed LNI %d successfully\n", deq_sp);
	}

	err = qman_ceetm_sp_set_lni(intf.sp, intf.lni);
	if (err) {
		error(0, -err, "Cannot map sub-portal %d with LNI %d\n",
				deq_sp, deq_sp);
		ceetm_free(clean_lni);
	} else {
		TRACE("Mapped sub-portal %d with LNI %d successfully\n",
				deq_sp, deq_sp);
	}

	err = qman_ceetm_bps2tokenrate(LNI_EGRESS_SPEED, &lni_egress_rate, 0);
	if (err) {
		error(0, -err, "Cannot get token rate from the given bps\n");
		ceetm_free(clean_lni);
	} else {
		TRACE("LNI Egress Rate Whole = %x, fraction = %x\n",
				lni_egress_rate.whole,
				lni_egress_rate.fraction);
	}

	err = qman_ceetm_lni_enable_shaper(intf.lni, 0, IPG_LENGTH);
	if (err) {
		error(0, -err, "Cannot enable lni shaper\n");
		ceetm_free(clean_lni);
	} else {
		TRACE("LNI shaper enabled successfully\n");
	}

	err = qman_ceetm_lni_set_commit_rate(intf.lni, &lni_egress_rate,
			LNI_BUCKET_LIMIT);
	if (err) {
		error(0, -err, "Cannot set lni commit rate\n");
		ceetm_free(clean_lni);
	} else {
		TRACE("Commit rate set to LNI successfully\n");
	}

	err = qman_ceetm_lni_set_excess_rate(intf.lni, &lni_egress_rate,
			LNI_BUCKET_LIMIT);
	if (err) {
		error(0, -err, "Cannot set lni excess rate\n");
		ceetm_free(clean_lni);
	} else {
		TRACE("Excess rate set to LNI successfully\n");
	}

	we_mask = QM_CCGR_WE_CS_THRES_IN |
			QM_CCGR_WE_CS_THRES_OUT |
			QM_CCGR_WE_CSCN_EN |
			QM_CCGR_WE_TD_EN |
			QM_CCGR_WE_MODE;
	memset(&c_params, 0, sizeof(struct qm_ceetm_ccg_params));
	c_params.mode = 1;
	c_params.td_en = 1;
	c_params.cscn_en = 1;
	qm_cgr_cs_thres_set64(&c_params.cs_thres_in, CCSCN_THRESH_IN, 0);
	qm_cgr_cs_thres_set64(&c_params.cs_thres_out, CCSCN_THRESH_OUT, 0);

	for (i = 0; i < NUM_OF_CHANNELS; i++) {
		err = qman_ceetm_channel_claim(&intf.channel[i], intf.lni);
		if (err) {
			error(0, -err, "Cannot claim channel %d\n",
					i);
			ceetm_free(clean_channel);
		} else {
			TRACE("Claimed channel %d successfully\n",
					i);
		}

		err = qman_ceetm_channel_enable_shaper(intf.channel[i], 0);
		if (err) {
			error(0, -err, "Cannot enable channel%d shaper\n",
					i);
			ceetm_free(clean_channel);
		} else {
			TRACE("Channel%d shaper enabled successfully\n",
					i);
		}

		err = qman_ceetm_bps2tokenrate(cr_speed[i], &ch_cr_rate, 0);
		if (err) {
			error(0, -err, "Cannot acquire token rate from the channel cr bps\n");
			ceetm_free(clean_channel);
		} else {
			TRACE("Token rate from channel cr bps acquired successfully\n");
		}

		err = qman_ceetm_bps2tokenrate(er_speed[i], &ch_er_rate, 0);
		if (err) {
			error(0, -err, "Cannot acquire token rate from the channel cr bps\n");
			ceetm_free(clean_channel);
		} else {
			TRACE("Token rate from channel cr bps acquired successfully\n");
		}

		err = qman_ceetm_channel_set_commit_rate(intf.channel[i],
				&ch_cr_rate, LNI_BUCKET_LIMIT);
		if (err) {
			error(0, -err, "Cannot set channel%d CR\n",
					i);
			ceetm_free(clean_channel);
		} else {
			TRACE("Commit rate set successfully for channel %d\n",
					i);
		}

		err = qman_ceetm_channel_set_excess_rate(intf.channel[i],
				&ch_er_rate, LNI_BUCKET_LIMIT);
		if (err) {
			error(0, -err, "Cannot set channel%d ER\n",
					i);
			ceetm_free(clean_channel);
		} else {
			TRACE("Channel Excess Rate set successfully for channel %d\n",
					i);
		}

		err = qman_ceetm_channel_set_group(intf.channel[i], 1, prio_a,
				prio_b);
		if (err) {
			error(0, -err, "Cannot set group for channel %d\n",
					i);
			ceetm_free(clean_channel);
		} else {
			TRACE("Set group for channel %d successfully\n",
					i);
		}

		err = qman_ceetm_ccg_claim(&intf.ccg[i], intf.channel[i], 0,
				ceetm_congestion_cb, NULL);
		if (err) {
			error(0, -err, "Cannot claim ccg%d\n",
					i);
			ceetm_free(clean_ccg);
		} else {
			TRACE("Claimed ccg%d successfully\n",
					i);
		}

		err = qman_ceetm_ccg_set(intf.ccg[i], we_mask, &c_params);
		if (err) {
			error(0, -err, "Cannot set ccg%d\n",
					i);
			ceetm_free(clean_ccg);
		} else {
			TRACE("Set ccg%d successfully\n",
					i);
		}
		/* Initialise Tx CQs */
		for (j = 0; j < NUM_OF_QUEUES; j++) {
			err = qman_ceetm_cq_claim(&intf.cq[i][j],
					intf.channel[i], j, intf.ccg[i]);
			if (err) {
				error(0, -err, "Failed to claim CQ %d in channel %d\n",
						j, intf.channel[i]->idx);
				ceetm_free(clean_tx_fqs);
			} else {
				TRACE("Claimed CQ %d in channel %d successfully\n",
						j, intf.channel[i]->idx);
			}

			err = qman_ceetm_lfq_claim(&intf.lfq[i][j],
					intf.cq[i][j]);
			if (err) {
				error(0, -err, "Failed to claim LFQ %d in channel %d\n",
						j, intf.channel[i]->idx);
				ceetm_free(clean_tx_fqs);
			} else {
				TRACE("Claimed LFQ %d in channel %d successfully\n",
						j, intf.channel[i]->idx);
			}

			err = qman_ceetm_lfq_set_context(intf.lfq[i][j],
					context_a, context_b);
			if (err) {
				error(0, -err, "Cannot set context_a and context_b for lfq %d\n",
						intf.lfq[i][j]->idx);
				ceetm_free(clean_tx_fqs);
			} else {
				TRACE("Set context_a and context_b successfully\n");
			}

			err = qman_ceetm_channel_set_cq_cr_eligibility(
					intf.channel[i], intf.cq[i][j]->idx, 1);
			if (err) {
				error(0, -err, "Cannot set cr eligibility for cq#%d\n",
						intf.cq[i][j]->idx);
				ceetm_free(clean_tx_fqs);
			} else {
				TRACE("Set cr eligibility for cq#%d successfully\n",
						intf.cq[i][j]->idx);
			}

			err = qman_ceetm_channel_set_cq_er_eligibility(
					intf.channel[i], intf.cq[i][j]->idx, 1);
			if (err) {
				error(0, -err, "Cannot set er eligibility for cq#%d\n",
						intf.cq[i][j]->idx);
				ceetm_free(clean_tx_fqs);
			} else {
				TRACE("Set er eligibility for cq#%d successfully\n",
						intf.cq[i][j]->idx);
			}
		}
	}

	err = posix_memalign((void *)&worker, MAX_CACHELINE, sizeof(*worker));
	if (err) {
		error(0, -err, "Cannot allocate memory for get_stats thread\n");
		return -ENOMEM;
	}
#endif

	return err;
}

int create_ceetm_counters(int dpa_stats_id)
{
#if defined(B4860)
	int err = 0, i = 0, j = 0;
	struct dpa_stats_cls_cnt_params cls_params;
	void **cq_array;
	void **ccg_array;

	/* Create Traffic Manager Class Queue class counter */
	cls_params.type = DPA_STATS_CNT_TRAFFIC_MNG;
	cls_params.class_members = NUM_OF_CHANNELS * NUM_OF_QUEUES;
	cq_array = calloc(sizeof(*cq_array), cls_params.class_members);

	if (cq_array) {
		for (i = 0; i < NUM_OF_CHANNELS; i++) {
			for (j = 0; j < NUM_OF_QUEUES; j++)
				cq_array[i * NUM_OF_QUEUES + j] = intf.cq[i][j];
		}
	}

	cls_params.traffic_mng_params.traffic_mng = cq_array;
	cls_params.traffic_mng_params.cnt_sel = DPA_STATS_CNT_NUM_ALL;
	cls_params.traffic_mng_params.src = DPA_STATS_CNT_TRAFFIC_CLASS;

	err = dpa_stats_create_class_counter(dpa_stats_id, &cls_params,
			&tmg_cnt_id);
	if (err < 0) {
		error(0, -err, "Failed to create DPA Stats class counter\n");
		return err;
	}
	printf("Successfully created DPA Stats counter: %d\n", tmg_cnt_id);

	/* Create Traffic Manager Class Congestion Group single counter */
	cls_params.type = DPA_STATS_CNT_TRAFFIC_MNG;
	cls_params.class_members = NUM_OF_CHANNELS;

	ccg_array = malloc(sizeof(*ccg_array) * cls_params.class_members);

	if (ccg_array) {
		for (i = 0; i < NUM_OF_CHANNELS; i++)
				ccg_array[i] = intf.ccg[i];
	}

	cls_params.traffic_mng_params.src = DPA_STATS_CNT_TRAFFIC_CG;
	cls_params.traffic_mng_params.traffic_mng = ccg_array;
	cls_params.traffic_mng_params.cnt_sel = DPA_STATS_CNT_NUM_ALL;

	err = dpa_stats_create_class_counter(dpa_stats_id, &cls_params,
			&cng_cnt_id);
	if (err < 0) {
		error(0, -err, "Failed to create DPA Stats class counter\n");
		return err;
	}
	printf("Successfully created DPA Stats counter: %d\n", cng_cnt_id);
#endif
	return 0;
}

#if defined(B4860)
static void *worker_fn(void *args)
{
	int err = 0;
	long cpu = -1;
	struct worker *wrk = args;
	cpu_set_t cpuset;

	/* Set CPU affinity */
	CPU_ZERO(&cpuset);
	/* If possible start the thread on the last cpu available */
	cpu = sysconf(_SC_NPROCESSORS_ONLN) - 1;
	if (cpu >= 0)
		CPU_SET(cpu, &cpuset);
	else
		/* Otherwise use the default USDPAA cpu */
		CPU_SET(1, &cpuset);

	err = pthread_setaffinity_np(wrk->id, sizeof(cpu_set_t), &cpuset);
	if (err) {
		error(0, -err, "Failed: pthread_setaffinity_np()");
		pthread_exit((void *) args);
	}

	err = qman_thread_init();
	if (err) {
		error(0, -err, "Failed: qman_thread_init()");
		qman_thread_finish();
		pthread_exit((void *) args);
	}

	/* Call DPA Stats get counters function */
	err = dpa_stats_get_counters(wrk->req_params, wrk->cnts_len, NULL);
	if (err < 0) {
		error(0, -err, "Failed to create DPA Stats request\n");
		qman_thread_finish();
		pthread_exit((void *) args);
	}
	printf("\nSuccessfully created DPA Stats sync request\n");
	print_traffic_dpa_stats_cnts();

	/* Release resources */
	qman_thread_finish();
	pthread_exit((void *) args);
}

int ceetm_get_counters_sync(struct dpa_stats_cnt_request_params req_params,
		int *cnts_len)
{
	int err = 0;
	worker->req_params = req_params;
	worker->cnts_len = cnts_len;

	err = pthread_create(&worker->id, NULL, worker_fn, worker);
	if (err) {
		error(0, -err, "Cannot create get_stats thread\n");
		return err;
	}

	err = pthread_join(worker->id, NULL);
	if (err) {
		error(0, -err, "Return code from pthread_join(id:%ld) is %d\n",
				(long)worker->id, err);
		return err;
	}
	return 0;
}
#endif

void print_ceetm_counters(struct traffic_counters *cnts)
{
#if defined(B4860)
	int i = 0;
	printf("\n\nCLASS QUEUE\n");
	printf("CQ::      BYTES   FRAMES\n");
	for (i = 0; i < NUM_OF_CHANNELS * NUM_OF_QUEUES; i++)
		printf("%2d %12d %8d\n", i, cnts->cq[i].cq_bytes,
						cnts->cq[i].cq_frames);

	printf("\n\nCLASS CONGESTION GROUP\n");
	printf("CGR::      BYTES   FRAMES\n");
	for (i = 0; i < NUM_OF_CHANNELS; i++)
		printf("%3d %12d %8d\n", i, cnts->cgr[i].cgr_bytes,
						cnts->cgr[i].cgr_frames);
#endif
}
