/*
 * Copyright (c) 2012 Mellanox Technologies, Inc.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <infiniband/opcode.h>

#include "mlx5.h"
#include "wqe.h"
#include "doorbell.h"

enum {
	MLX5_CQ_DOORBELL			= 0x20
};

enum {
	CQ_OK					=  0,
	CQ_EMPTY				= -1,
	CQ_POLL_ERR				= -2
};

#define MLX5_CQ_DB_REQ_NOT_SOL			(1 << 24)
#define MLX5_CQ_DB_REQ_NOT			(0 << 24)

enum {
	MLX5_CQE_SYNDROME_LOCAL_LENGTH_ERR		= 0x01,
	MLX5_CQE_SYNDROME_LOCAL_QP_OP_ERR		= 0x02,
	MLX5_CQE_SYNDROME_LOCAL_PROT_ERR		= 0x04,
	MLX5_CQE_SYNDROME_WR_FLUSH_ERR			= 0x05,
	MLX5_CQE_SYNDROME_MW_BIND_ERR			= 0x06,
	MLX5_CQE_SYNDROME_BAD_RESP_ERR			= 0x10,
	MLX5_CQE_SYNDROME_LOCAL_ACCESS_ERR		= 0x11,
	MLX5_CQE_SYNDROME_REMOTE_INVAL_REQ_ERR		= 0x12,
	MLX5_CQE_SYNDROME_REMOTE_ACCESS_ERR		= 0x13,
	MLX5_CQE_SYNDROME_REMOTE_OP_ERR			= 0x14,
	MLX5_CQE_SYNDROME_TRANSPORT_RETRY_EXC_ERR	= 0x15,
	MLX5_CQE_SYNDROME_RNR_RETRY_EXC_ERR		= 0x16,
	MLX5_CQE_SYNDROME_REMOTE_ABORTED_ERR		= 0x22,
};

enum {
	MLX5_CQE_OWNER_MASK	= 1,
	MLX5_CQE_REQ		= 0,
	MLX5_CQE_RESP_WR_IMM	= 1,
	MLX5_CQE_RESP_SEND	= 2,
	MLX5_CQE_RESP_SEND_IMM	= 3,
	MLX5_CQE_RESP_SEND_INV	= 4,
	MLX5_CQE_RESIZE_CQ	= 5,
	MLX5_CQE_SIG_ERR	= 12,
	MLX5_CQE_REQ_ERR	= 13,
	MLX5_CQE_RESP_ERR	= 14,
	MLX5_CQE_INVALID	= 15,
};

enum {
	MLX5_CQ_MODIFY_RESEIZE = 0,
	MLX5_CQ_MODIFY_MODER = 1,
	MLX5_CQ_MODIFY_MAPPING = 2,
};

struct mlx5_err_cqe {
	uint8_t		rsvd0[32];
	uint32_t	srqn;
	uint8_t		rsvd1[18];
	uint8_t		vendor_err_synd;
	uint8_t		syndrome;
	uint32_t	s_wqe_opcode_qpn;
	uint16_t	wqe_counter;
	uint8_t		signature;
	uint8_t		op_own;
};

struct mlx5_cqe64 {
	uint8_t		rsvd0[17];
	uint8_t		ml_path;
	uint8_t		rsvd20[4];
	uint16_t	slid;
	uint32_t	flags_rqpn;
	uint8_t		rsvd28[4];
	uint32_t	srqn;
	uint32_t	imm_inval_pkey;
	uint8_t		rsvd40[4];
	uint32_t	byte_cnt;
	__be64		timestamp;
	uint32_t	sop_drop_qpn;
	uint16_t	wqe_counter;
	uint8_t		signature;
	uint8_t		op_own;
};

int mlx5_stall_num_loop = 60;
int mlx5_stall_cq_poll_min = 60;
int mlx5_stall_cq_poll_max = 100000;
int mlx5_stall_cq_inc_step = 100;
int mlx5_stall_cq_dec_step = 10;

static void *get_buf_cqe(struct mlx5_buf *buf, int n, int cqe_sz)
{
	return buf->buf + n * cqe_sz;
}

static void *get_cqe(struct mlx5_cq *cq, int n)
{
	return cq->active_buf->buf + n * cq->cqe_sz;
}

static void *get_sw_cqe(struct mlx5_cq *cq, int n)
{
	void *cqe = get_cqe(cq, n & cq->ibv_cq.cqe);
	struct mlx5_cqe64 *cqe64;

	cqe64 = (cq->cqe_sz == 64) ? cqe : cqe + 64;

	if (likely((cqe64->op_own) >> 4 != MLX5_CQE_INVALID) &&
	    !((cqe64->op_own & MLX5_CQE_OWNER_MASK) ^ !!(n & (cq->ibv_cq.cqe + 1)))) {
		return cqe;
	} else {
		return NULL;
	}
}

static void *next_cqe_sw(struct mlx5_cq *cq)
{
	return get_sw_cqe(cq, cq->cons_index);
}

static void update_cons_index(struct mlx5_cq *cq)
{
	cq->dbrec[MLX5_CQ_SET_CI] = htonl(cq->cons_index & 0xffffff);
}

static void handle_good_req(struct ibv_wc *wc, struct mlx5_cqe64 *cqe)
{
	switch (ntohl(cqe->sop_drop_qpn) >> 24) {
	case MLX5_OPCODE_RDMA_WRITE_IMM:
		wc->wc_flags |= IBV_WC_WITH_IMM;
	case MLX5_OPCODE_RDMA_WRITE:
		wc->opcode    = IBV_WC_RDMA_WRITE;
		break;
	case MLX5_OPCODE_SEND_IMM:
		wc->wc_flags |= IBV_WC_WITH_IMM;
	case MLX5_OPCODE_SEND:
	case MLX5_OPCODE_SEND_INVAL:
		wc->opcode    = IBV_WC_SEND;
		break;
	case MLX5_OPCODE_RDMA_READ:
		wc->opcode    = IBV_WC_RDMA_READ;
		wc->byte_len  = ntohl(cqe->byte_cnt);
		break;
	case MLX5_OPCODE_ATOMIC_CS:
		wc->opcode    = IBV_WC_COMP_SWAP;
		wc->byte_len  = 8;
		break;
	case MLX5_OPCODE_ATOMIC_FA:
		wc->opcode    = IBV_WC_FETCH_ADD;
		wc->byte_len  = 8;
		break;
	case MLX5_OPCODE_BIND_MW:
		wc->opcode    = IBV_WC_BIND_MW;
		break;
	case MLX5_OPCODE_UMR:
		wc->opcode    = IBV_EXP_WC_UMR;
		break;

	case MLX5_OPCODE_ATOMIC_MASKED_CS:
		wc->opcode    = IBV_EXP_WC_MASKED_COMP_SWAP;
		break;

	case MLX5_OPCODE_ATOMIC_MASKED_FA:
		wc->opcode    = IBV_EXP_WC_MASKED_FETCH_ADD;
		break;
	}
}

static int handle_responder(struct ibv_wc *wc, struct mlx5_cqe64 *cqe,
			    struct mlx5_qp *qp, struct mlx5_srq *srq,
			    enum mlx5_rsc_type type)
{
	uint16_t	wqe_ctr;
	struct mlx5_wq *wq;
	uint8_t g;
	int err = 0;

	wc->byte_len = ntohl(cqe->byte_cnt);
	if (srq) {
		wqe_ctr = ntohs(cqe->wqe_counter);
		wc->wr_id = srq->wrid[wqe_ctr];
		mlx5_free_srq_wqe(srq, wqe_ctr);
		if (cqe->op_own & MLX5_INLINE_SCATTER_32)
			err = mlx5_copy_to_recv_srq(srq, wqe_ctr, cqe,
						    wc->byte_len);
		else if (cqe->op_own & MLX5_INLINE_SCATTER_64)
			err = mlx5_copy_to_recv_srq(srq, wqe_ctr, cqe - 1,
						    wc->byte_len);
	} else {
		wq	  = &qp->rq;
		wqe_ctr = wq->tail & (wq->wqe_cnt - 1);
		wc->wr_id = wq->wrid[wqe_ctr];
		++wq->tail;
		if (cqe->op_own & MLX5_INLINE_SCATTER_32)
			err = mlx5_copy_to_recv_wqe(qp, wqe_ctr, cqe,
						    wc->byte_len);
		else if (cqe->op_own & MLX5_INLINE_SCATTER_64)
			err = mlx5_copy_to_recv_wqe(qp, wqe_ctr, cqe - 1,
						    wc->byte_len);
	}
	if (err)
		return err;

	wc->byte_len = ntohl(cqe->byte_cnt);

	switch (cqe->op_own >> 4) {
	case MLX5_CQE_RESP_WR_IMM:
		wc->opcode	= IBV_WC_RECV_RDMA_WITH_IMM;
		wc->wc_flags	|= IBV_WC_WITH_IMM;
		wc->imm_data = cqe->imm_inval_pkey;
		break;
	case MLX5_CQE_RESP_SEND:
		wc->opcode   = IBV_WC_RECV;
		break;
	case MLX5_CQE_RESP_SEND_IMM:
		wc->opcode	= IBV_WC_RECV;
		wc->wc_flags	|= IBV_WC_WITH_IMM;
		wc->imm_data = cqe->imm_inval_pkey;
		break;
	}
	wc->slid	   = ntohs(cqe->slid);
	wc->sl		   = (ntohl(cqe->flags_rqpn) >> 24) & 0xf;
	if (srq && (type != MLX5_RSC_TYPE_DCT) &&
	    ((type == MLX5_RSC_TYPE_INVAL) ||
	     ((qp->verbs_qp.qp.qp_type == IBV_QPT_XRC_RECV) ||
	      (qp->verbs_qp.qp.qp_type == IBV_QPT_XRC))))
		wc->src_qp	   = srq->srqn;
	else
		wc->src_qp	   = ntohl(cqe->flags_rqpn) & 0xffffff;


	wc->dlid_path_bits = cqe->ml_path & 0x7f;
	g = (ntohl(cqe->flags_rqpn) >> 28) & 3;
	if (qp && qp->verbs_qp.qp.qp_type == IBV_QPT_UD)
		wc->wc_flags |= g ? IBV_WC_GRH : 0;
	wc->pkey_index     = ntohl(cqe->imm_inval_pkey) & 0xffff;

	return IBV_WC_SUCCESS;
}

static void dump_cqe(FILE *fp, void *buf)
{
	uint32_t *p = buf;
	int i;

	for (i = 0; i < 16; i += 4)
		fprintf(fp, "%08x %08x %08x %08x\n", ntohl(p[i]), ntohl(p[i + 1]),
			ntohl(p[i + 2]), ntohl(p[i + 3]));
}

static void mlx5_set_bad_wc_opcode(struct ibv_exp_wc *wc,
				   struct mlx5_err_cqe *cqe,
				   uint8_t is_req)
{
	if (is_req) {
		switch (ntohl(cqe->s_wqe_opcode_qpn) >> 24) {
		case MLX5_OPCODE_RDMA_WRITE_IMM:
		case MLX5_OPCODE_RDMA_WRITE:
			wc->exp_opcode    = IBV_EXP_WC_RDMA_WRITE;
			break;
		case MLX5_OPCODE_SEND_IMM:
		case MLX5_OPCODE_SEND:
		case MLX5_OPCODE_SEND_INVAL:
			wc->exp_opcode    = IBV_EXP_WC_SEND;
			break;
		case MLX5_OPCODE_RDMA_READ:
			wc->exp_opcode    = IBV_EXP_WC_RDMA_READ;
			break;
		case MLX5_OPCODE_ATOMIC_CS:
			wc->exp_opcode    = IBV_EXP_WC_COMP_SWAP;
			break;
		case MLX5_OPCODE_ATOMIC_FA:
			wc->exp_opcode    = IBV_EXP_WC_FETCH_ADD;
			break;
		case MLX5_OPCODE_BIND_MW:
			wc->exp_opcode    = IBV_EXP_WC_BIND_MW;
			break;
		case MLX5_OPCODE_UMR:
			wc->exp_opcode    = IBV_EXP_WC_UMR;
			break;
		case MLX5_OPCODE_ATOMIC_MASKED_CS:
			wc->exp_opcode    = IBV_EXP_WC_MASKED_COMP_SWAP;
			break;
		case MLX5_OPCODE_ATOMIC_MASKED_FA:
			wc->exp_opcode    = IBV_EXP_WC_MASKED_FETCH_ADD;
			break;
		}
	} else {
		switch (cqe->op_own >> 4) {
		case MLX5_CQE_RESP_WR_IMM:
			wc->exp_opcode	= IBV_EXP_WC_RECV_RDMA_WITH_IMM;
			break;
		case MLX5_CQE_RESP_SEND:
			wc->exp_opcode   = IBV_EXP_WC_RECV;
			break;
		case MLX5_CQE_RESP_SEND_IMM:
			wc->exp_opcode	= IBV_EXP_WC_RECV;
			break;
		}
	}
}

static void mlx5_handle_error_cqe(struct mlx5_err_cqe *cqe,
				  struct ibv_exp_wc *wc)
{
	switch (cqe->syndrome) {
	case MLX5_CQE_SYNDROME_LOCAL_LENGTH_ERR:
		wc->status = IBV_WC_LOC_LEN_ERR;
		break;
	case MLX5_CQE_SYNDROME_LOCAL_QP_OP_ERR:
		wc->status = IBV_WC_LOC_QP_OP_ERR;
		break;
	case MLX5_CQE_SYNDROME_LOCAL_PROT_ERR:
		wc->status = IBV_WC_LOC_PROT_ERR;
		break;
	case MLX5_CQE_SYNDROME_WR_FLUSH_ERR:
		wc->status = IBV_WC_WR_FLUSH_ERR;
		break;
	case MLX5_CQE_SYNDROME_MW_BIND_ERR:
		wc->status = IBV_WC_MW_BIND_ERR;
		break;
	case MLX5_CQE_SYNDROME_BAD_RESP_ERR:
		wc->status = IBV_WC_BAD_RESP_ERR;
		break;
	case MLX5_CQE_SYNDROME_LOCAL_ACCESS_ERR:
		wc->status = IBV_WC_LOC_ACCESS_ERR;
		break;
	case MLX5_CQE_SYNDROME_REMOTE_INVAL_REQ_ERR:
		wc->status = IBV_WC_REM_INV_REQ_ERR;
		break;
	case MLX5_CQE_SYNDROME_REMOTE_ACCESS_ERR:
		wc->status = IBV_WC_REM_ACCESS_ERR;
		break;
	case MLX5_CQE_SYNDROME_REMOTE_OP_ERR:
		wc->status = IBV_WC_REM_OP_ERR;
		break;
	case MLX5_CQE_SYNDROME_TRANSPORT_RETRY_EXC_ERR:
		wc->status = IBV_WC_RETRY_EXC_ERR;
		break;
	case MLX5_CQE_SYNDROME_RNR_RETRY_EXC_ERR:
		wc->status = IBV_WC_RNR_RETRY_EXC_ERR;
		break;
	case MLX5_CQE_SYNDROME_REMOTE_ABORTED_ERR:
		wc->status = IBV_WC_REM_ABORT_ERR;
		break;
	default:
		wc->status = IBV_WC_GENERAL_ERR;
		break;
	}

	wc->vendor_err = cqe->vendor_err_synd;
}

#if defined(__x86_64__) || defined (__i386__)
static inline unsigned long get_cycles()
{
	uint32_t low, high;
	uint64_t val;
	asm volatile ("rdtsc" : "=a" (low), "=d" (high));
	val = high;
	val = (val << 32) | low;
	return val;
}

static void mlx5_stall_poll_cq()
{
	int i;

	for (i = 0; i < mlx5_stall_num_loop; i++)
		(void)get_cycles();
}
static void mlx5_stall_cycles_poll_cq(uint64_t cycles)
{
	while (get_cycles()  <  cycles)
		; /* Nothing */
}
static void mlx5_get_cycles(uint64_t *cycles)
{
	*cycles = get_cycles();
}
#else
static void mlx5_stall_poll_cq()
{
}
static void mlx5_stall_cycles_poll_cq(uint64_t cycles)
{
}
static void mlx5_get_cycles(uint64_t *cycles)
{
}
#endif

static int is_requestor(uint8_t opcode)
{
	if (opcode == MLX5_CQE_REQ || opcode == MLX5_CQE_REQ_ERR)
		return 1;
	else
		return 0;
}

static int is_responder(uint8_t opcode)
{
	switch (opcode) {
	case MLX5_CQE_RESP_WR_IMM:
	case MLX5_CQE_RESP_SEND:
	case MLX5_CQE_RESP_SEND_IMM:
	case MLX5_CQE_RESP_SEND_INV:
	case MLX5_CQE_RESP_ERR:
		return 1;
	}

	return 0;
}

static int mlx5_poll_one(struct mlx5_cq *cq,
			 struct mlx5_resource **cur_rsc,
			 struct mlx5_srq **cur_srq,
			 struct ibv_exp_wc *wc,
			 uint32_t wc_size)
{
	struct mlx5_cqe64 *cqe64;
	struct mlx5_wq *wq;
	uint16_t wqe_ctr;
	void *cqe;
	uint32_t rsn;
	uint32_t srqn;
	int idx;
	uint8_t opcode;
	struct mlx5_err_cqe *ecqe;
	int err;
	int requestor;
	int responder;
	struct mlx5_context *mctx = to_mctx(cq->ibv_cq.context);
	struct mlx5_qp *mqp = NULL;
	struct mlx5_dct *mdct;
	uint64_t exp_wc_flags = 0;
	enum mlx5_rsc_type type = MLX5_RSC_TYPE_INVAL;

	cqe = next_cqe_sw(cq);
	if (!cqe)
		return CQ_EMPTY;

	cqe64 = (cq->cqe_sz == 64) ? cqe : cqe + 64;

	++cq->cons_index;

	/*
	 * Make sure we read CQ entry contents after we've checked the
	 * ownership bit.
	 */
	rmb();

#ifdef MLX5_DEBUG
	if (mlx5_debug_mask & MLX5_DBG_CQ_CQE) {
		FILE *fp = mctx->dbg_fp;

		mlx5_dbg(fp, MLX5_DBG_CQ_CQE, "dump cqe for cqn 0x%x:\n", cq->cqn);
		dump_cqe(fp, cqe64);
	}
#endif

	((struct ibv_wc *)wc)->wc_flags = 0;
	opcode = cqe64->op_own >> 4;
	requestor = is_requestor(opcode);
	responder = is_responder(opcode);
	if (unlikely(!requestor && !responder))
		return CQ_POLL_ERR;

	srqn = ntohl(cqe64->srqn) & 0xffffff;
	rsn = ntohl(cqe64->sop_drop_qpn) & 0xffffff;
	if (responder && srqn) {
		if (!*cur_srq || (srqn != (*cur_srq)->srqn)) {
			*cur_srq = mlx5_find_srq(mctx, srqn);
			if (unlikely(!*cur_srq))
				return CQ_POLL_ERR;
		}
		if (likely(offsetof(struct ibv_exp_wc, srq) < wc_size)) {
			wc->srq = &(*cur_srq)->vsrq.srq;
			exp_wc_flags |= IBV_EXP_WC_SRQ;
		}
	}

	if (!*cur_rsc || (rsn != (*cur_rsc)->rsn)) {
		*cur_rsc = mlx5_find_rsc(mctx, rsn);
		if (unlikely(!*cur_rsc && !srqn))
			return CQ_POLL_ERR;
	}
	if (*cur_rsc) {
		switch ((*cur_rsc)->type) {
		case MLX5_RSC_TYPE_QP:
			mqp = (struct mlx5_qp *)*cur_rsc;
			if (likely(offsetof(struct ibv_exp_wc, qp) < wc_size)) {
				wc->qp = &mqp->verbs_qp.qp;
				exp_wc_flags |= IBV_EXP_WC_QP;
			}
			break;
		case MLX5_RSC_TYPE_DCT:
			mdct = (struct mlx5_dct *)*cur_rsc;
			if (likely(offsetof(struct ibv_exp_wc, dct) < wc_size)) {
				wc->dct = &mdct->ibdct;
				exp_wc_flags |= IBV_EXP_WC_DCT;
			}
			break;
		default:
			return CQ_POLL_ERR;
		}
		type = (*cur_rsc)->type;
	}

	wc->qp_num = rsn;

	switch (opcode) {
	case MLX5_CQE_REQ:
		if (unlikely(!mqp)) {
			fprintf(stderr, "all requestors are kinds of QPs\n");
			return CQ_POLL_ERR;
		}
		wq = &mqp->sq;
		wqe_ctr = ntohs(cqe64->wqe_counter);
		idx = wqe_ctr & (wq->wqe_cnt - 1);
		handle_good_req((struct ibv_wc *)wc, cqe64);
		if (cqe64->op_own & MLX5_INLINE_SCATTER_32)
			err = mlx5_copy_to_send_wqe(mqp, wqe_ctr, cqe,
						    wc->byte_len);
		else if (cqe64->op_own & MLX5_INLINE_SCATTER_64)
			err = mlx5_copy_to_send_wqe(mqp, wqe_ctr, cqe - 1,
						    wc->byte_len);
		else
			err = 0;

		wc->wr_id = wq->wrid[idx];
		wq->tail = wq->wqe_head[idx] + 1;
		wc->status = err;
		break;
	case MLX5_CQE_RESP_WR_IMM:
	case MLX5_CQE_RESP_SEND:
	case MLX5_CQE_RESP_SEND_IMM:
	case MLX5_CQE_RESP_SEND_INV:
		wc->status = handle_responder((struct ibv_wc *)wc, cqe64, mqp,
					      srqn ? *cur_srq : NULL, type);
		break;
	case MLX5_CQE_RESIZE_CQ:
		break;
	case MLX5_CQE_REQ_ERR:
	case MLX5_CQE_RESP_ERR:
		ecqe = (struct mlx5_err_cqe *)cqe64;
		mlx5_handle_error_cqe(ecqe, wc);
		mlx5_set_bad_wc_opcode(wc, ecqe, (opcode == MLX5_CQE_REQ_ERR));
		if (unlikely(ecqe->syndrome != MLX5_CQE_SYNDROME_WR_FLUSH_ERR &&
			     ecqe->syndrome != MLX5_CQE_SYNDROME_TRANSPORT_RETRY_EXC_ERR)) {
			FILE *fp = mctx->dbg_fp;
			fprintf(fp, PFX "%s: got completion with error:\n",
				mctx->hostname);
			dump_cqe(fp, ecqe);
			if (mlx5_freeze_on_error_cqe) {
				fprintf(fp, PFX "freezing at poll cq...");
				while (1)
					sleep(10);
			}
		}

		if (opcode == MLX5_CQE_REQ_ERR) {
			wq = &mqp->sq;
			wqe_ctr = ntohs(cqe64->wqe_counter);
			idx = wqe_ctr & (wq->wqe_cnt - 1);
			wc->wr_id = wq->wrid[idx];
			wq->tail = wq->wqe_head[idx] + 1;
		} else {
			if (*cur_srq) {
				wqe_ctr = ntohs(cqe64->wqe_counter);
				wc->wr_id = (*cur_srq)->wrid[wqe_ctr];
				mlx5_free_srq_wqe(*cur_srq, wqe_ctr);
			} else {
				wq = &mqp->rq;
				wc->wr_id = wq->wrid[wq->tail & (wq->wqe_cnt - 1)];
				++wq->tail;
			}
		}
		break;
	}

	if (likely(offsetof(struct ibv_exp_wc, exp_wc_flags) < wc_size))
		wc->exp_wc_flags = exp_wc_flags | (uint64_t)((struct ibv_wc *)wc)->wc_flags;

	return CQ_OK;
}

static int poll_cq(struct ibv_cq *ibcq, int ne, struct ibv_exp_wc *wc,
		   uint32_t wc_size)
{
	struct mlx5_cq *cq = to_mcq(ibcq);
	struct mlx5_resource *rsc = NULL;
	struct mlx5_srq *srq = NULL;
	int npolled;
	int err = CQ_OK;
	void *twc;

	if (cq->stall_enable) {
		if (cq->stall_adaptive_enable) {
			if (cq->stall_last_count)
				mlx5_stall_cycles_poll_cq(cq->stall_last_count + cq->stall_cycles);
		} else if (cq->stall_next_poll) {
			cq->stall_next_poll = 0;
			mlx5_stall_poll_cq();
		}
	}

	mlx5_spin_lock(&cq->lock);

	for (npolled = 0, twc = wc; npolled < ne; ++npolled, twc += wc_size) {
		err = mlx5_poll_one(cq, &rsc, &srq, twc, wc_size);
		if (err != CQ_OK)
			break;
	}

	update_cons_index(cq);

	mlx5_spin_unlock(&cq->lock);

	if (cq->stall_enable) {
		if (cq->stall_adaptive_enable) {
			if (npolled == 0) {
				cq->stall_cycles = max(cq->stall_cycles-mlx5_stall_cq_dec_step,
						       mlx5_stall_cq_poll_min);
				mlx5_get_cycles(&cq->stall_last_count);
			} else if (npolled < ne) {
				cq->stall_cycles = min(cq->stall_cycles+mlx5_stall_cq_inc_step,
						       mlx5_stall_cq_poll_max);
				mlx5_get_cycles(&cq->stall_last_count);
			} else {
				cq->stall_cycles = max(cq->stall_cycles-mlx5_stall_cq_dec_step,
						       mlx5_stall_cq_poll_min);
				cq->stall_last_count = 0;
			}
		} else if (err == CQ_EMPTY) {
			cq->stall_next_poll = 1;
		}
	}

	return err == CQ_POLL_ERR ? err : npolled;
}

int mlx5_poll_cq(struct ibv_cq *ibcq, int ne, struct ibv_wc *wc)
{
	return poll_cq(ibcq, ne, (struct ibv_exp_wc *)wc, sizeof(*wc));
}

int mlx5_poll_cq_ex(struct ibv_cq *ibcq, int ne,
		    struct ibv_exp_wc *wc, uint32_t wc_size)
{
	return poll_cq(ibcq, ne, wc, wc_size);
}

int mlx5_arm_cq(struct ibv_cq *ibvcq, int solicited)
{
	struct mlx5_cq *cq = to_mcq(ibvcq);
	struct mlx5_context *ctx = to_mctx(ibvcq->context);
	uint32_t doorbell[2];
	uint32_t sn;
	uint32_t ci;
	uint32_t cmd;

	sn  = cq->arm_sn & 3;
	ci  = cq->cons_index & 0xffffff;
	cmd = solicited ? MLX5_CQ_DB_REQ_NOT_SOL : MLX5_CQ_DB_REQ_NOT;

	cq->dbrec[MLX5_CQ_ARM_DB] = htonl(sn << 28 | cmd | ci);

	/*
	 * Make sure that the doorbell record in host memory is
	 * written before ringing the doorbell via PCI MMIO.
	 */
	wmb();

	doorbell[0] = htonl(sn << 28 | cmd | ci);
	doorbell[1] = htonl(cq->cqn);

	mlx5_write64(doorbell, ctx->uar[0] + MLX5_CQ_DOORBELL, &ctx->lock32);

	wc_wmb();

	return 0;
}

void mlx5_cq_event(struct ibv_cq *cq)
{
	to_mcq(cq)->arm_sn++;
}

static int is_equal_rsn(struct mlx5_cqe64 *cqe64, uint32_t rsn)
{
	return rsn == (ntohl(cqe64->sop_drop_qpn) & 0xffffff);
}

void __mlx5_cq_clean(struct mlx5_cq *cq, uint32_t rsn, struct mlx5_srq *srq)
{
	uint32_t prod_index;
	int nfreed = 0;
	struct mlx5_cqe64 *cqe64, *dest64;
	void *cqe, *dest;
	uint8_t owner_bit;

	if (!cq)
		return;

	/*
	 * First we need to find the current producer index, so we
	 * know where to start cleaning from.  It doesn't matter if HW
	 * adds new entries after this loop -- the QP we're worried
	 * about is already in RESET, so the new entries won't come
	 * from our QP and therefore don't need to be checked.
	 */
	for (prod_index = cq->cons_index; get_sw_cqe(cq, prod_index); ++prod_index)
		if (prod_index == cq->cons_index + cq->ibv_cq.cqe)
			break;

	/*
	 * Now sweep backwards through the CQ, removing CQ entries
	 * that match our QP by copying older entries on top of them.
	 */
	while ((int) --prod_index - (int) cq->cons_index >= 0) {
		cqe = get_cqe(cq, prod_index & cq->ibv_cq.cqe);
		cqe64 = (cq->cqe_sz == 64) ? cqe : cqe + 64;
		if (is_equal_rsn(cqe64, rsn)) {
			if (srq && (ntohl(cqe64->srqn) & 0xffffff))
				mlx5_free_srq_wqe(srq, ntohs(cqe64->wqe_counter));
			++nfreed;
		} else if (nfreed) {
			dest = get_cqe(cq, (prod_index + nfreed) & cq->ibv_cq.cqe);
			dest64 = (cq->cqe_sz == 64) ? dest : dest + 64;
			owner_bit = dest64->op_own & MLX5_CQE_OWNER_MASK;
			memcpy(dest, cqe, cq->cqe_sz);
			dest64->op_own = owner_bit |
				(dest64->op_own & ~MLX5_CQE_OWNER_MASK);
		}
	}

	if (nfreed) {
		cq->cons_index += nfreed;
		/*
		 * Make sure update of buffer contents is done before
		 * updating consumer index.
		 */
		wmb();
		update_cons_index(cq);
	}
}

void mlx5_cq_clean(struct mlx5_cq *cq, uint32_t qpn, struct mlx5_srq *srq)
{
	mlx5_spin_lock(&cq->lock);
	__mlx5_cq_clean(cq, qpn, srq);
	mlx5_spin_unlock(&cq->lock);
}

static uint8_t sw_ownership_bit(int n, int nent)
{
	return (n & nent) ? 1 : 0;
}

static int is_hw(uint8_t own, int n, int mask)
{
	return (own & MLX5_CQE_OWNER_MASK) ^ !!(n & (mask + 1));
}

void mlx5_cq_resize_copy_cqes(struct mlx5_cq *cq)
{
	struct mlx5_cqe64 *scqe64;
	struct mlx5_cqe64 *dcqe64;
	void *start_cqe;
	void *scqe;
	void *dcqe;
	int ssize;
	int dsize;
	int i;
	uint8_t sw_own;

	ssize = cq->cqe_sz;
	dsize = cq->resize_cqe_sz;

	i = cq->cons_index;
	scqe = get_buf_cqe(cq->active_buf, i & cq->active_cqes, ssize);
	scqe64 = ssize == 64 ? scqe : scqe + 64;
	start_cqe = scqe;
	if (is_hw(scqe64->op_own, i, cq->active_cqes)) {
		fprintf(stderr, "expected cqe in sw ownership\n");
		return;
	}

	while ((scqe64->op_own >> 4) != MLX5_CQE_RESIZE_CQ) {
		dcqe = get_buf_cqe(cq->resize_buf, (i + 1) & (cq->resize_cqes - 1), dsize);
		dcqe64 = dsize == 64 ? dcqe : dcqe + 64;
		sw_own = sw_ownership_bit(i + 1, cq->resize_cqes);
		memcpy(dcqe, scqe, ssize);
		dcqe64->op_own = (dcqe64->op_own & ~MLX5_CQE_OWNER_MASK) | sw_own;

		++i;
		scqe = get_buf_cqe(cq->active_buf, i & cq->active_cqes, ssize);
		scqe64 = ssize == 64 ? scqe : scqe + 64;
		if (is_hw(scqe64->op_own, i, cq->active_cqes)) {
			fprintf(stderr, "expected cqe in sw ownership\n");
			return;
		}

		if (scqe == start_cqe) {
			fprintf(stderr, "resize CQ failed to get resize CQE\n");
			return;
		}
	}
	++cq->cons_index;
}

int mlx5_alloc_cq_buf(struct mlx5_context *mctx, struct mlx5_cq *cq,
		      struct mlx5_buf *buf, int nent, int cqe_sz)
{
	struct mlx5_cqe64 *cqe;
	int i;
	struct mlx5_device *dev = to_mdev(mctx->ibv_ctx.device);
	int ret;
	enum mlx5_alloc_type type;
	enum mlx5_alloc_type default_type = MLX5_ALLOC_TYPE_PREFER_CONTIG;

	if (mlx5_use_huge("HUGE_CQ"))
		default_type = MLX5_ALLOC_TYPE_HUGE;

	mlx5_get_alloc_type(MLX5_CQ_PREFIX, &type, default_type);

	ret = mlx5_alloc_prefered_buf(mctx, buf,
				      align(nent * cqe_sz, dev->page_size),
				      dev->page_size,
				      type,
				      MLX5_CQ_PREFIX);

	if (ret)
		return -1;

	memset(buf->buf, 0, nent * cqe_sz);

	for (i = 0; i < nent; ++i) {
		cqe = buf->buf + i * cqe_sz;
		cqe += cqe_sz == 128 ? 1 : 0;
		cqe->op_own = MLX5_CQE_INVALID << 4;
	}

	return 0;
}

int mlx5_free_cq_buf(struct mlx5_context *ctx, struct mlx5_buf *buf)
{
	return mlx5_free_actual_buf(ctx, buf);
}
