/* Copyright (c) 2013 Freescale Semiconductor, Inc.
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

#include <ppac.h>
#include "ppam_if.h"
#include "ceetm_cfg_parser.h"
#include <ppac_interface.h>

#include <inttypes.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>

#define CEETM_FQID_BASE 0xF00000
#define CEETM_CGR_PERCQ_TD_THRESH  256

/* Override the default command prompt */
const char ppam_prompt[] = "ceetm> ";

static struct ceetm_lni_info *root_lni;

/* Claim a CCGR with congestion management of tail drop and
 * set threshold for tail drop
 */
static int create_ceetm_ccg(struct qm_ceetm_ccg **ccg,
				struct qm_ceetm_channel *channel, int idx)
{
	struct qm_ceetm_ccg_params ccg_params;
	u32 ccg_mask;
	int ret;

	ret = qman_ceetm_ccg_claim(ccg, channel, idx, NULL, NULL);
	if (ret)
		return ret;

	/* Count frames in CCGR, tail drop enabled with specified threshold */
	ccg_mask = QM_CCGR_WE_MODE | QM_CCGR_WE_TD_EN |
		   QM_CCGR_WE_TD_MODE | QM_CCGR_WE_TD_THRES;

	ccg_params.mode = 1;
	ccg_params.td_mode = 1;
	ccg_params.td_en = 1;

	qm_cgr_cs_thres_set64(&ccg_params.td_thres,
	    CEETM_CGR_PERCQ_TD_THRESH, 0);

	ret = qman_ceetm_ccg_set(*ccg, ccg_mask, &ccg_params);
	if (ret)
		return ret;

	return 0;
}

static int set_group_eligibility(struct qm_ceetm_channel *channel,
				 struct ceetm_group_info *group_a,
				 struct ceetm_group_info *group_b)
{
	int ret;

	ret = qman_ceetm_channel_set_group_cr_eligibility(
				channel, 0, group_a->cr_eligible);
	if (ret)
		return ret;
	ret = qman_ceetm_channel_set_group_er_eligibility(
				channel, 0, group_a->er_eligible);
	if (ret)
		return ret;
	if (group_b) {
		ret = qman_ceetm_channel_set_group_cr_eligibility(
					channel, 1, group_b->cr_eligible);
		if (ret)
			return ret;
		ret = qman_ceetm_channel_set_group_er_eligibility(
					channel, 1, group_b->er_eligible);
		if (ret)
			return ret;
	}

	return 0;
}

static int is_wbfs_cq(uint8_t index)
{
	if (index >= 8 && index < 16)
		return 1;
	return 0;
}

static int is_group_a_cq(uint8_t num_group, uint8_t index)
{
	if (num_group == 0 || !is_wbfs_cq(index))
		return 0;

	if (num_group == 1)
		return 1;
	else
		if (index >= 8 && index < 12)
			return 1;

	return 0;
}


static int is_group_b_cq(uint8_t num_group, uint8_t index)
{
	if (is_wbfs_cq(index) && !is_group_a_cq(num_group, index))
		return 1;

	return 0;
}

static int set_cq_eligibility(struct qm_ceetm_channel *channel,
			      struct ceetm_cq_info *cqinfo)
{
	int ret;

	if (!is_wbfs_cq(cqinfo->idx)) {
		ret = qman_ceetm_channel_set_cq_cr_eligibility(
				channel, cqinfo->idx, cqinfo->cr_eligible);
		if (ret)
			return ret;
		ret = qman_ceetm_channel_set_cq_er_eligibility(
				channel, cqinfo->idx, cqinfo->er_eligible);
		if (ret)
			return ret;
	}
	return 0;
}

/* Configure CEETM as egress traffic management */
static int net_if_ceetm_init(struct ppam_interface *interface,
			     const struct fman_if *fif,
			     uint32_t *txfqid) {
	struct ceetm_channel_info *chinfo;
	struct ceetm_cq_info *cqinfo;
	struct qm_ceetm_lni *lni;
	struct qm_ceetm_sp *sp;
	struct qm_ceetm_rate token_rate;
	struct qm_ceetm_channel *channel;
	struct qm_ceetm_cq *cq;
	struct qm_ceetm_lfq *lfq;
	uint32_t ret, spid;
	uint32_t num_channel = 0;
	uint32_t ceetm_fqbase;
	static uint32_t lni_fqbase;
	static uint32_t fman_idx = -1;
	u64 ctx_a = 0x9200000080000000ull;

	/* Reset counter when goes into another FMan */
	if (fman_idx != fif->fman_idx) {
		lni_fqbase = 0;
		fman_idx = fif->fman_idx;
	}
	ceetm_fqbase = CEETM_FQID_BASE + fif->fman_idx * 0x10000;
	spid = fif->tx_channel_id & 0xF;
	TRACE("Fman %d, spid %d\n", fif->fman_idx, spid);

	*txfqid = ceetm_fqbase + lni_fqbase;
	ret = qman_ceetm_sp_claim(&sp, fif->fman_idx, spid);
	if (ret) {
		printf("Fail to claim sp, err %d\n", ret);
		return ret;
	}

	ret = qman_ceetm_lni_claim(&lni, fif->fman_idx, spid);
	if (ret) {
		printf("Fail to claim lni, err %d\n", ret);
		return ret;
	}

	ret = qman_ceetm_sp_set_lni(sp, lni);
	if (ret) {
		printf("Fail to bundle sp and lni, err %d\n", ret);
		return ret;
	}
	interface->sp = sp;
	interface->lni = lni;

	if (root_lni->is_shaping) {
		ret = qman_ceetm_lni_enable_shaper(lni, 1, 24);
		if (ret) {
			printf("Fail to set shaper to lni, err %d\n", ret);
			return ret;
		}

		qman_ceetm_bps2tokenrate(root_lni->cr, &token_rate, 1);
		ret = qman_ceetm_lni_set_commit_rate(lni, &token_rate, 0x2000);
		if (ret) {
			printf("Fail to set lni commit rate, err %d\n", ret);
			return ret;
		}
		qman_ceetm_bps2tokenrate(root_lni->er, &token_rate, 1);
		ret = qman_ceetm_lni_set_excess_rate(lni, &token_rate, 0x2000);
		if (ret) {
			printf("Fail to set lni excess rate, err %d\n", ret);
			return ret;
		}
	}

	/* Create channels and class queues according to CEETM cfg file */
	list_for_each_entry(chinfo, &root_lni->channel_list, list) {
		int last_cq_idx = 0;

		ret = qman_ceetm_channel_claim(&channel, lni);
		if (ret) {
			printf("Fail to claim channel%d, err %d\n",
				num_channel, ret);
			return ret;
		}
		if (chinfo->is_shaping) {
			ret = qman_ceetm_channel_enable_shaper(channel, 1);
			if (ret) {
				printf("Fail to set shaper for channel%d,"
				       "err %d\n", num_channel, ret);
				return ret;
			}

			qman_ceetm_bps2tokenrate(chinfo->cr, &token_rate, 1);
			ret = qman_ceetm_channel_set_commit_rate(channel,
								 &token_rate,
								 0x1000);
			if (ret) {
				printf("Fail to set channel commit rate,"
				       " err %d\n", ret);
				return ret;
			}
			qman_ceetm_bps2tokenrate(chinfo->er, &token_rate, 1);
			ret = qman_ceetm_channel_set_excess_rate(channel,
								 &token_rate,
								 0x1000);
			if (ret) {
				printf("Fail to set channel excess rate,"
				       " err %d\n", ret);
				return ret;
			}
		}

		if (chinfo->num_group > 0) {
			ret = qman_ceetm_channel_set_group(channel,
				chinfo->num_group - 1, chinfo->group_a->idx,
				chinfo->group_b ? chinfo->group_b->idx : 0);
			if (ret) {
				printf("Fail to set group for cq channel,"
				       " err %d\n", ret);
				return ret;
			}
			if (chinfo->is_shaping) {
				ret = set_group_eligibility(channel,
							    chinfo->group_a,
							    chinfo->group_b);
				if (ret) {
					printf("Fail to set group eligibility,"
					       " err %d\n", ret);
					return ret;
				}
			}
		}

		list_for_each_entry(cqinfo, &chinfo->cq_list, list) {
			struct qm_ceetm_ccg *ccg;

			ret = create_ceetm_ccg(&ccg, channel, cqinfo->idx);
			if (ret) {
				printf("Fail to claim CCGR%d, err %d\n",
					cqinfo->idx, ret);
				return ret;
			}
			if (!is_wbfs_cq(cqinfo->idx)) {
				ret = qman_ceetm_cq_claim(
					&cq, channel, cqinfo->idx, ccg);
			} else if (is_group_a_cq(chinfo->num_group,
						 cqinfo->idx)) {
				ret = qman_ceetm_cq_claim_A(
					&cq, channel, cqinfo->idx, ccg);
			} else {
				ret = qman_ceetm_cq_claim_B(
					&cq, channel, cqinfo->idx, ccg);
			}
			if (ret) {
				printf("Fail to claim CQ%d, err %d\n",
					cqinfo->idx, ret);
				return ret;
			}

			if (is_wbfs_cq(cqinfo->idx)) {
				struct qm_ceetm_weight_code weight_code;

				ret = qman_ceetm_ratio2wbfs(
					cqinfo->weight, 1, &weight_code, 0);
				if (ret) {
					printf("Fail to convert weight code\n");
					return ret;
				}
				ret = qman_ceetm_set_queue_weight(
							cq, &weight_code);
				if (ret) {
					printf("Fail to set weight for cq\n");
					return ret;
				}
			}

			if (chinfo->is_shaping) {
				ret = set_cq_eligibility(channel, cqinfo);
				if (ret) {
					printf("Fail to set eligibility for cq"
					       ", err %d\n", ret);
					return ret;
				}
			}
			/* Number of CQs may be less than 16,
			 * but number of lfqs(FQID) must be 16 in order to
			 * calculate TX FQID conveniently
			 * This part would be more elegant when
			 * CEETM logic FQID can be statically assigned
			 */
			for (; last_cq_idx <= cq->idx; last_cq_idx++) {
				ret = qman_ceetm_lfq_claim(&lfq, cq);
				if (ret) {
					printf("Fail to claim lfq for"
					       "cq%d, err %d\n", cq->idx, ret);
					return ret;
				}
				TRACE("Claim lfq 0x%x to CQ %d, ccg %d\n",
					lfq->idx, cq->idx, ccg->idx);
				ret = qman_ceetm_lfq_set_context(lfq,
								 ctx_a, 0);
				if (ret) {
					printf("Fail to set context for lfq%d,"
					       " err %d\n", lfq->idx, ret);
					return ret;
				}
			}
		}
		/* Have to create 16 lfqs for each channel */
		for (; last_cq_idx < 16; last_cq_idx++) {
			ret = qman_ceetm_lfq_claim(&lfq, cq);
			if (ret) {
				printf("Fail to claim lfq for cq%d, err %d\n",
					cq->idx, ret);
				return ret;
			}
			TRACE("Claim lfq 0x%x to CQ %d\n", lfq->idx, cq->idx);
			ret = qman_ceetm_lfq_set_context(lfq, ctx_a, 0);
			if (ret) {
				printf("Fail to set context for"
				       "lfq%d, err %d\n", lfq->idx, ret);
				return ret;
			}
		}

		num_channel++;
	}

	lni_fqbase += num_channel * 16;
	return 0;
}

/* Release CEETM resource when app quits */
static void release_ceetm_resource(struct ppam_interface *interface)
{
	struct qm_ceetm_lni *lni = interface->lni;
	struct qm_ceetm_sp *sp = interface->sp;
	struct qm_ceetm_channel *channel;

	list_for_each_entry(channel, &lni->channels, node) {
		struct qm_ceetm_cq *cq;
		struct qm_ceetm_ccg *ccg;

		list_for_each_entry(ccg, &channel->ccgs, node) {
			qman_ceetm_ccg_release(ccg);
		}

		list_for_each_entry(cq, &channel->class_queues, node) {
			struct qm_ceetm_lfq *lfq;

			list_for_each_entry(lfq, &cq->bound_lfqids, node) {
				qman_ceetm_lfq_release(lfq);
			}

			qman_ceetm_cq_release(cq);
		}
		qman_ceetm_channel_release(channel);
	}
	qman_ceetm_lni_release(lni);
	qman_ceetm_sp_release(sp);
}

/* Initialize CEETM queues here  */
static int ppam_interface_init(struct ppam_interface *p,
			       const struct fm_eth_port_cfg *cfg,
			       unsigned int num_tx_fqs,
			       uint32_t *flags __maybe_unused)
{
	struct fman_if *fif = cfg->fman_if;
	return net_if_ceetm_init(p, fif, &p->tx_fqid_base);
}
/* Recycle CEETM resource */
static void ppam_interface_finish(struct ppam_interface *p)
{
	release_ceetm_resource(p);
}
static void ppam_interface_tx_fqid(struct ppam_interface *p, unsigned idx,
				   uint32_t fqid)
{
}

static int ppam_rx_error_init(struct ppam_rx_error *p,
			      struct ppam_interface *_if,
			      struct qm_fqd_stashing *stash_opts)
{
	return 0;
}
static void ppam_rx_error_finish(struct ppam_rx_error *p,
				 struct ppam_interface *_if)
{
}
static inline void ppam_rx_error_cb(struct ppam_rx_error *p,
				    struct ppam_interface *_if,
				    const struct qm_dqrr_entry *dqrr)
{
	const struct qm_fd *fd = &dqrr->fd;
	ppac_drop_frame(fd);
}

static int ppam_rx_default_init(struct ppam_rx_default *p,
				struct ppam_interface *_if,
				unsigned idx,
				struct qm_fqd_stashing *stash_opts)
{
	return 0;
}
static void ppam_rx_default_finish(struct ppam_rx_default *p,
				   struct ppam_interface *_if)
{
}
static inline void ppam_rx_default_cb(struct ppam_rx_default *p,
				      struct ppam_interface *_if,
				      const struct qm_dqrr_entry *dqrr)
{
	const struct qm_fd *fd = &dqrr->fd;
	ppac_drop_frame(fd);
}

static int ppam_tx_error_init(struct ppam_tx_error *p,
			      struct ppam_interface *_if,
			      struct qm_fqd_stashing *stash_opts)
{
	return 0;
}
static void ppam_tx_error_finish(struct ppam_tx_error *p,
				 struct ppam_interface *_if)
{
}
static inline void ppam_tx_error_cb(struct ppam_tx_error *p,
				    struct ppam_interface *_if,
				    const struct qm_dqrr_entry *dqrr)
{
	const struct qm_fd *fd = &dqrr->fd;
	ppac_drop_frame(fd);
}

static int ppam_tx_confirm_init(struct ppam_tx_confirm *p,
				struct ppam_interface *_if,
				struct qm_fqd_stashing *stash_opts)
{
	return 0;
}
static void ppam_tx_confirm_finish(struct ppam_tx_confirm *p,
				   struct ppam_interface *_if)
{
}
static inline void ppam_tx_confirm_cb(struct ppam_tx_confirm *p,
				      struct ppam_interface *_if,
				      const struct qm_dqrr_entry *dqrr)
{
	const struct qm_fd *fd = &dqrr->fd;
	ppac_drop_frame(fd);
}

static int ppam_rx_hash_init(struct ppam_rx_hash *p,
			     struct ppam_interface *_if,
			     unsigned idx,
			     struct qm_fqd_stashing *stash_opts)
{
	/* tx fqid is base fqid for the interface plus Tos value */
	p->tx_fqid = _if->tx_fqid_base;

	TRACE("Mapping Rx FQ %p:%d --> Tx FQID %d\n", p, idx, p->tx_fqid);
	return 0;
}

static void ppam_rx_hash_finish(struct ppam_rx_hash *p,
				struct ppam_interface *_if,
				unsigned idx)
{
}

static inline void ppam_rx_hash_cb(struct ppam_rx_hash *p,
				   const struct qm_dqrr_entry *dqrr)
{
	struct ether_header *prot_eth;
	const struct qm_fd *fd = &dqrr->fd;
	uint32_t tx_fqid;

	BUG_ON(fd->format != qm_fd_contig);
	prot_eth = __dma_mem_ptov(qm_fd_addr(fd)) + fd->offset;
	/* Eliminate ethernet broadcasts. */
	if (likely(!(prot_eth->ether_dhost[0] & 0x01) &&
				(prot_eth->ether_type == ETHERTYPE_IP))) {
		struct iphdr *iphdr = (typeof(iphdr))(prot_eth + 1);

		TRACE("LFQID base 0x%x, IP ver=%d,tos=0x%x,len=%d,id=%d\n",
			p->tx_fqid, iphdr->version, iphdr->tos, iphdr->tot_len,
			iphdr->id);

		/* The 4msb of TOS is CQ channel id, 4lsb is CQ id */
		tx_fqid = p->tx_fqid + iphdr->tos;

		ppac_send_frame(tx_fqid, fd);
		return;
	}
	ppac_drop_frame(fd);
}

/* We implement no arguments, these are the minimal stubs */
struct ppam_arguments {
	const char *ceetm_cfg;
};

struct ppam_arguments ppam_args;

/* ppam global init hook used to retrieve ceetm configuration */
int ppam_init(void)
{
	const char *envp;

	if (ppam_args.ceetm_cfg == NULL) {
		envp = getenv("DEF_CEETM_CFG_PATH");
		if (envp != NULL)
			ppam_args.ceetm_cfg = envp;
	}
	root_lni = ceetm_cfg_parse(ppam_args.ceetm_cfg);
	if (root_lni == NULL) {
		fprintf(stderr, "%s:%hu:%s(): ceetm cfg parser failed ",
				__FILE__, __LINE__, __func__);
		return -1;
	}

	return 0;
}

void ppam_finish(void)
{
	ceetm_cfg_clean();
}

static error_t ceetm_parser(int key, char *arg, struct argp_state *state)
{
	switch (key) {
	case 'f':
		ppam_args.ceetm_cfg = arg;
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

const char ppam_doc[] = "CEETM";
static const struct argp_option argp_opts[] = {
	{"ceetm-on-egress", 'f', "FILE", 0, "CEETM configuration file"},
	{}
};
const struct argp ppam_argp = {argp_opts, ceetm_parser, 0, ppam_doc};

/* Inline the PPAC machinery */
#include <ppac.c>

