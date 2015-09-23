/*
 * Copyright (c) 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2005 Mellanox Technologies Ltd.  All rights reserved.
 * Copyright (c) 2006, 2007 Cisco Systems.  All rights reserved.
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

#include <infiniband/opcode.h>

#include "mlx4.h"
#include "doorbell.h"

int mlx4_stall_num_loop = 300;

enum {
	MLX4_CQ_DOORBELL			= 0x20
};

enum {
	CQ_OK					=  0,
	CQ_EMPTY				= -1,
	CQ_POLL_ERR				= -2
};

#define MLX4_CQ_DB_REQ_NOT_SOL			(1 << 24)
#define MLX4_CQ_DB_REQ_NOT			(2 << 24)

enum {
	MLX4_CQE_VLAN_PRESENT_MASK		= 1 << 29,
	MLX4_CQE_QPN_MASK			= 0xffffff,
};

enum {
	MLX4_CQE_OWNER_MASK			= 0x80,
	MLX4_CQE_IS_SEND_MASK			= 0x40,
	MLX4_CQE_INL_SCATTER_MASK		= 0x20,
	MLX4_CQE_OPCODE_MASK			= 0x1f
};

enum {
	MLX4_CQE_SYNDROME_LOCAL_LENGTH_ERR		= 0x01,
	MLX4_CQE_SYNDROME_LOCAL_QP_OP_ERR		= 0x02,
	MLX4_CQE_SYNDROME_LOCAL_PROT_ERR		= 0x04,
	MLX4_CQE_SYNDROME_WR_FLUSH_ERR			= 0x05,
	MLX4_CQE_SYNDROME_MW_BIND_ERR			= 0x06,
	MLX4_CQE_SYNDROME_BAD_RESP_ERR			= 0x10,
	MLX4_CQE_SYNDROME_LOCAL_ACCESS_ERR		= 0x11,
	MLX4_CQE_SYNDROME_REMOTE_INVAL_REQ_ERR		= 0x12,
	MLX4_CQE_SYNDROME_REMOTE_ACCESS_ERR		= 0x13,
	MLX4_CQE_SYNDROME_REMOTE_OP_ERR			= 0x14,
	MLX4_CQE_SYNDROME_TRANSPORT_RETRY_EXC_ERR	= 0x15,
	MLX4_CQE_SYNDROME_RNR_RETRY_EXC_ERR		= 0x16,
	MLX4_CQE_SYNDROME_REMOTE_ABORTED_ERR		= 0x22,
};

struct mlx4_cqe {
	uint32_t	vlan_my_qpn;
	uint32_t	immed_rss_invalid;
	uint32_t	g_mlpath_rqpn;
	union {
		struct {
			union {
				struct {
					uint16_t  sl_vid;
					uint16_t  rlid;
				};
				uint32_t  timestamp_16_47;
			};
			uint16_t  reserved1;
			uint8_t   reserved2;
			uint8_t   reserved3;
		};
		struct {
			uint16_t reserved4;
			uint8_t  smac[6];
		};
	};
	uint32_t	byte_cnt;
	uint16_t	wqe_index;
	uint16_t	checksum;
	uint8_t		reserved5[1];
	uint16_t	timestamp_0_15;
	uint8_t		owner_sr_opcode;
}  __attribute__((packed));

struct mlx4_err_cqe {
	uint32_t	vlan_my_qpn;
	uint32_t	reserved1[5];
	uint16_t	wqe_index;
	uint8_t		vendor_err;
	uint8_t		syndrome;
	uint8_t		reserved2[3];
	uint8_t		owner_sr_opcode;
};

static struct mlx4_cqe *get_cqe(struct mlx4_cq *cq, int entry)
{
	return cq->buf.buf + entry * cq->cqe_size;
}

static void *get_sw_cqe(struct mlx4_cq *cq, int n)
{
	struct mlx4_cqe *cqe = get_cqe(cq, n & cq->ibv_cq.cqe);
	struct mlx4_cqe *tcqe = cq->cqe_size == 64 ? cqe + 1 : cqe;

	return (!!(tcqe->owner_sr_opcode & MLX4_CQE_OWNER_MASK) ^
		!!(n & (cq->ibv_cq.cqe + 1))) ? NULL : cqe;
}

static struct mlx4_cqe *next_cqe_sw(struct mlx4_cq *cq)
{
	return get_sw_cqe(cq, cq->cons_index);
}

static void update_cons_index(struct mlx4_cq *cq)
{
	*cq->set_ci_db = htonl(cq->cons_index & 0xffffff);
}

static void mlx4_handle_error_cqe(struct mlx4_err_cqe *cqe, struct ibv_wc *wc)
{
	if (cqe->syndrome == MLX4_CQE_SYNDROME_LOCAL_QP_OP_ERR)
		printf(PFX "local QP operation err "
		       "(QPN %06x, WQE index %x, vendor syndrome %02x, "
		       "opcode = %02x)\n",
		       htonl(cqe->vlan_my_qpn), htonl(cqe->wqe_index),
		       cqe->vendor_err,
		       cqe->owner_sr_opcode & ~MLX4_CQE_OWNER_MASK);

	switch (cqe->syndrome) {
	case MLX4_CQE_SYNDROME_LOCAL_LENGTH_ERR:
		wc->status = IBV_WC_LOC_LEN_ERR;
		break;
	case MLX4_CQE_SYNDROME_LOCAL_QP_OP_ERR:
		wc->status = IBV_WC_LOC_QP_OP_ERR;
		break;
	case MLX4_CQE_SYNDROME_LOCAL_PROT_ERR:
		wc->status = IBV_WC_LOC_PROT_ERR;
		break;
	case MLX4_CQE_SYNDROME_WR_FLUSH_ERR:
		wc->status = IBV_WC_WR_FLUSH_ERR;
		break;
	case MLX4_CQE_SYNDROME_MW_BIND_ERR:
		wc->status = IBV_WC_MW_BIND_ERR;
		break;
	case MLX4_CQE_SYNDROME_BAD_RESP_ERR:
		wc->status = IBV_WC_BAD_RESP_ERR;
		break;
	case MLX4_CQE_SYNDROME_LOCAL_ACCESS_ERR:
		wc->status = IBV_WC_LOC_ACCESS_ERR;
		break;
	case MLX4_CQE_SYNDROME_REMOTE_INVAL_REQ_ERR:
		wc->status = IBV_WC_REM_INV_REQ_ERR;
		break;
	case MLX4_CQE_SYNDROME_REMOTE_ACCESS_ERR:
		wc->status = IBV_WC_REM_ACCESS_ERR;
		break;
	case MLX4_CQE_SYNDROME_REMOTE_OP_ERR:
		wc->status = IBV_WC_REM_OP_ERR;
		break;
	case MLX4_CQE_SYNDROME_TRANSPORT_RETRY_EXC_ERR:
		wc->status = IBV_WC_RETRY_EXC_ERR;
		break;
	case MLX4_CQE_SYNDROME_RNR_RETRY_EXC_ERR:
		wc->status = IBV_WC_RNR_RETRY_EXC_ERR;
		break;
	case MLX4_CQE_SYNDROME_REMOTE_ABORTED_ERR:
		wc->status = IBV_WC_REM_ABORT_ERR;
		break;
	default:
		wc->status = IBV_WC_GENERAL_ERR;
		break;
	}

	wc->vendor_err = cqe->vendor_err;
}

static int mlx4_poll_one(struct mlx4_cq *cq,
			 struct mlx4_qp **cur_qp,
			 struct ibv_exp_wc *wc,
			 uint32_t wc_size, int is_exp)
{
	struct mlx4_wq *wq;
	struct mlx4_cqe *cqe;
	struct mlx4_srq *srq;
	uint32_t qpn;
	uint32_t g_mlpath_rqpn;
	uint16_t wqe_index;
	int is_error;
	int is_send;
	int size;
	int left;
	int list_len;
	int i;
	struct mlx4_inlr_rbuff *rbuffs;
	uint8_t *sbuff;
	int timestamp_en = !!(cq->creation_flags &
			      IBV_EXP_CQ_TIMESTAMP);
	uint64_t exp_wc_flags = 0;
	uint64_t wc_flags = 0;
	cqe = next_cqe_sw(cq);
	if (!cqe)
		return CQ_EMPTY;

	if (cq->cqe_size == 64)
		++cqe;

	++cq->cons_index;

	VALGRIND_MAKE_MEM_DEFINED(cqe, sizeof *cqe);

	/*
	 * Make sure we read CQ entry contents after we've checked the
	 * ownership bit.
	 */
	rmb();

	qpn = ntohl(cqe->vlan_my_qpn) & MLX4_CQE_QPN_MASK;
	wc->qp_num = qpn;

	is_send  = cqe->owner_sr_opcode & MLX4_CQE_IS_SEND_MASK;

	/* include checksum as work around for calc opcode */
	is_error = (cqe->owner_sr_opcode & MLX4_CQE_OPCODE_MASK) ==
		MLX4_CQE_OPCODE_ERROR && (cqe->checksum & 0xff);

	if ((qpn & MLX4_XRC_QPN_BIT) && !is_send) {
		/*
		 * We do not have to take the XSRQ table lock here,
		 * because CQs will be locked while SRQs are removed
		 * from the table.
		 */
		*cur_qp = NULL;
		srq = mlx4_find_xsrq(&to_mctx(cq->ibv_cq.context)->xsrq_table,
				     ntohl(cqe->g_mlpath_rqpn) & MLX4_CQE_QPN_MASK);
		if (!srq)
			return CQ_POLL_ERR;
	} else {
		if (unlikely(!*cur_qp || (qpn != (*cur_qp)->verbs_qp.qp.qp_num))) {
			/*
			 * We do not have to take the QP table lock here,
			 * because CQs will be locked while QPs are removed
			 * from the table.
			 */
			*cur_qp = mlx4_find_qp(to_mctx(cq->ibv_cq.context), qpn);
			if (unlikely(!*cur_qp))
				return CQ_POLL_ERR;
		}
		if (is_exp) {
			wc->qp = &((*cur_qp)->verbs_qp.qp);
			exp_wc_flags |= IBV_EXP_WC_QP;
		}
		srq = ((*cur_qp)->verbs_qp.qp.srq) ? to_msrq((*cur_qp)->verbs_qp.qp.srq) : NULL;
	}

	if (is_send) {
		wq = &(*cur_qp)->sq;
		wqe_index = ntohs(cqe->wqe_index);
		wq->tail += (uint16_t) (wqe_index - (uint16_t) wq->tail);
		wc->wr_id = wq->wrid[wq->tail & (wq->wqe_cnt - 1)];
		++wq->tail;
	} else if (srq) {
		wqe_index = htons(cqe->wqe_index);
		wc->wr_id = srq->wrid[wqe_index];
		mlx4_free_srq_wqe(srq, wqe_index);
		if (is_exp) {
			wc->srq = &(srq->verbs_srq.srq);
			exp_wc_flags |= IBV_EXP_WC_SRQ;
		}
	} else {
		wq = &(*cur_qp)->rq;
		wqe_index = wq->tail & (wq->wqe_cnt - 1);
		wc->wr_id = wq->wrid[wqe_index];
		++wq->tail;
	}

	if (unlikely(is_error)) {
		mlx4_handle_error_cqe((struct mlx4_err_cqe *)cqe,
				      (struct ibv_wc *)wc);
		return CQ_OK;
	}

	wc->status = IBV_WC_SUCCESS;

	if (timestamp_en && offsetof(struct ibv_exp_wc, timestamp) < wc_size)  {
		/* currently, only CQ_CREATE_WITH_TIMESTAMPING_RAW is
		 * supported. CQ_CREATE_WITH_TIMESTAMPING_SYS isn't
		 * supported */
		if (cq->creation_flags &
		    IBV_EXP_CQ_TIMESTAMP_TO_SYS_TIME)
			wc->timestamp = 0;
		else {
			wc->timestamp =
				(uint64_t)(ntohl(cqe->timestamp_16_47) +
					   !cqe->timestamp_0_15) << 16
				| (uint64_t)ntohs(cqe->timestamp_0_15);
			exp_wc_flags |= IBV_EXP_WC_WITH_TIMESTAMP;
		}
	}

	if (is_send) {
		switch (cqe->owner_sr_opcode & MLX4_CQE_OPCODE_MASK) {
		case MLX4_OPCODE_CALC_RDMA_WRITE_IMM:
		case MLX4_OPCODE_RDMA_WRITE_IMM:
			wc_flags |= IBV_WC_WITH_IMM;
		case MLX4_OPCODE_RDMA_WRITE:
			wc->exp_opcode    = IBV_EXP_WC_RDMA_WRITE;
			break;
		case MLX4_OPCODE_SEND_IMM:
			wc_flags |= IBV_WC_WITH_IMM;
		case MLX4_OPCODE_SEND:
			wc->exp_opcode    = IBV_EXP_WC_SEND;
			break;
		case MLX4_OPCODE_RDMA_READ:
			wc->exp_opcode    = IBV_EXP_WC_RDMA_READ;
			wc->byte_len  = ntohl(cqe->byte_cnt);
			break;
		case MLX4_OPCODE_ATOMIC_CS:
			wc->exp_opcode    = IBV_EXP_WC_COMP_SWAP;
			wc->byte_len  = 8;
			break;
		case MLX4_OPCODE_ATOMIC_FA:
			wc->exp_opcode    = IBV_EXP_WC_FETCH_ADD;
			wc->byte_len  = 8;
			break;
		case MLX4_OPCODE_ATOMIC_MASK_CS:
			wc->exp_opcode    = IBV_EXP_WC_MASKED_COMP_SWAP;
			break;
		case MLX4_OPCODE_ATOMIC_MASK_FA:
			wc->exp_opcode    = IBV_EXP_WC_MASKED_FETCH_ADD;
			break;
		case MLX4_OPCODE_LOCAL_INVAL:
			if (unlikely(!is_exp))
				return CQ_POLL_ERR;
			wc->exp_opcode    = IBV_EXP_WC_LOCAL_INV;
			break;
		case MLX4_OPCODE_SEND_INVAL:
			wc->exp_opcode    = IBV_EXP_WC_SEND;
			break;
		case MLX4_OPCODE_BIND_MW:
			wc->exp_opcode    = IBV_EXP_WC_BIND_MW;
			break;
		default:
			/* assume it's a send completion */
			wc->exp_opcode    = IBV_EXP_WC_SEND;
			break;
		}
	} else {
		wc->byte_len = ntohl(cqe->byte_cnt);
		if ((*cur_qp) && (*cur_qp)->max_inlr_sg &&
		    (cqe->owner_sr_opcode & MLX4_CQE_INL_SCATTER_MASK)) {
			rbuffs = (*cur_qp)->inlr_buff.buff[wqe_index].sg_list;
			list_len = (*cur_qp)->inlr_buff.buff[wqe_index].list_len;
			sbuff = mlx4_get_recv_wqe((*cur_qp), wqe_index);
			left = wc->byte_len;
			for (i = 0; (i < list_len) && left; i++) {
				size = min(rbuffs->rlen, left);
				memcpy(rbuffs->rbuff, sbuff, size);
				left -= size;
				rbuffs++;
				sbuff += size;
			}
			if (left) {
				wc->status = IBV_WC_LOC_LEN_ERR;
				return CQ_OK;
			}
		}

		switch (cqe->owner_sr_opcode & MLX4_CQE_OPCODE_MASK) {
		case MLX4_RECV_OPCODE_RDMA_WRITE_IMM:
			wc->exp_opcode   = IBV_EXP_WC_RECV_RDMA_WITH_IMM;
			wc_flags = IBV_WC_WITH_IMM;
			wc->imm_data = cqe->immed_rss_invalid;
			break;
		case MLX4_RECV_OPCODE_SEND_INVAL:
			if (unlikely(!is_exp))
				return CQ_POLL_ERR;
			wc->exp_opcode   = IBV_EXP_WC_RECV;
			exp_wc_flags |= IBV_EXP_WC_WITH_INV;
			wc->imm_data = ntohl(cqe->immed_rss_invalid);
			break;
		case MLX4_RECV_OPCODE_SEND:
			wc->exp_opcode   = IBV_EXP_WC_RECV;
			wc_flags = 0;
			break;
		case MLX4_RECV_OPCODE_SEND_IMM:
			wc->exp_opcode   = IBV_EXP_WC_RECV;
			wc_flags = IBV_WC_WITH_IMM;
			wc->imm_data = cqe->immed_rss_invalid;
			break;
		}

		if (!timestamp_en) {
			exp_wc_flags |= IBV_EXP_WC_WITH_SLID;
			wc->slid = ntohs(cqe->rlid);
		}
		g_mlpath_rqpn	   = ntohl(cqe->g_mlpath_rqpn);
		wc->src_qp	   = g_mlpath_rqpn & 0xffffff;
		wc->dlid_path_bits = (g_mlpath_rqpn >> 24) & 0x7f;
		wc_flags	  |= g_mlpath_rqpn & 0x80000000 ? IBV_WC_GRH : 0;
		wc->pkey_index     = ntohl(cqe->immed_rss_invalid) & 0x7f;
		/* When working with xrc srqs, don't have qp to check link layer.
		  * Using IB SL, should consider Roce. (TBD)
		*/
		/* sl is invalid when timestamp is used */
		if (!timestamp_en) {
			if ((*cur_qp) && (*cur_qp)->link_layer ==
			    IBV_LINK_LAYER_ETHERNET)
				wc->sl = ntohs(cqe->sl_vid) >> 13;
			else
				wc->sl = ntohs(cqe->sl_vid) >> 12;
			exp_wc_flags |= IBV_EXP_WC_WITH_SL;
		}
	}

	if (is_exp)
		wc->exp_wc_flags = exp_wc_flags | (uint64_t)wc_flags;

	((struct ibv_wc *)wc)->wc_flags = wc_flags;

	return CQ_OK;
}

#if defined(__x86_64__) || defined(__i386__)
static inline unsigned long get_cycles()
{
	unsigned low, high;
	unsigned long long val;
	asm volatile ("rdtsc" : "=a" (low), "=d" (high));
	val = high;
	val = (val << 32) | low;
	return val;
}
#else
static inline unsigned long get_cycles()
{
	return 0;
}
#endif

static void mlx4_stall_poll_cq()
{
	int i;

	for (i = 0; i < mlx4_stall_num_loop; i++)
		(void)get_cycles();
}

int mlx4_poll_cq(struct ibv_cq *ibcq, int ne, struct ibv_exp_wc *wc,
		 uint32_t wc_size, int is_exp)
{
	struct mlx4_cq *cq = to_mcq(ibcq);
	struct mlx4_qp *qp = NULL;
	int npolled;
	int err = CQ_OK;

	if (unlikely(cq->stall_next_poll)) {
		cq->stall_next_poll = 0;
		mlx4_stall_poll_cq();
	}
	mlx4_spin_lock(&cq->lock);
	
	for (npolled = 0; npolled < ne; ++npolled) {
		err = mlx4_poll_one(cq, &qp, ((void *)wc) + npolled * wc_size,
				    wc_size, is_exp);
		if (unlikely(err != CQ_OK))
			break;
	}

	if (likely(npolled || err == CQ_POLL_ERR))
		update_cons_index(cq);

	mlx4_spin_unlock(&cq->lock);

	if (unlikely(cq->stall_enable && err == CQ_EMPTY))
		cq->stall_next_poll = 1;
	
	return err == CQ_POLL_ERR ? err : npolled;
}

int mlx4_exp_poll_cq(struct ibv_cq *ibcq, int num_entries,
		     struct ibv_exp_wc *wc, uint32_t wc_size)
{
	return mlx4_poll_cq(ibcq, num_entries, wc, wc_size, 1);
}

int mlx4_poll_ibv_cq(struct ibv_cq *ibcq, int ne, struct ibv_wc *wc)
{
	return mlx4_poll_cq(ibcq, ne, (struct ibv_exp_wc *)wc, sizeof(*wc), 0);
}

int mlx4_arm_cq(struct ibv_cq *ibvcq, int solicited)
{
	struct mlx4_cq *cq = to_mcq(ibvcq);
	uint32_t doorbell[2];
	uint32_t sn;
	uint32_t ci;
	uint32_t cmd;

	sn  = cq->arm_sn & 3;
	ci  = cq->cons_index & 0xffffff;
	cmd = solicited ? MLX4_CQ_DB_REQ_NOT_SOL : MLX4_CQ_DB_REQ_NOT;

	*cq->arm_db = htonl(sn << 28 | cmd | ci);

	/*
	 * Make sure that the doorbell record in host memory is
	 * written before ringing the doorbell via PCI MMIO.
	 */
	wmb();

	doorbell[0] = htonl(sn << 28 | cmd | cq->cqn);
	doorbell[1] = htonl(ci);

	mlx4_write64(doorbell, to_mctx(ibvcq->context), MLX4_CQ_DOORBELL);

	return 0;
}

void mlx4_cq_event(struct ibv_cq *cq)
{
	to_mcq(cq)->arm_sn++;
}

void __mlx4_cq_clean(struct mlx4_cq *cq, uint32_t qpn, struct mlx4_srq *srq)
{
	struct mlx4_cqe *cqe, *dest;
	uint32_t prod_index;
	uint8_t owner_bit;
	int nfreed = 0;
	int cqe_inc = cq->cqe_size == 64 ? 1 : 0;

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
		cqe += cqe_inc;
		if (srq && srq->ext_srq &&
		    ntohl(cqe->g_mlpath_rqpn & MLX4_CQE_QPN_MASK) == srq->verbs_srq.srq_num &&
		    !(cqe->owner_sr_opcode & MLX4_CQE_IS_SEND_MASK)) {
			mlx4_free_srq_wqe(srq, ntohs(cqe->wqe_index));
			++nfreed;
		} else if ((ntohl(cqe->vlan_my_qpn) & MLX4_CQE_QPN_MASK) == qpn) {
			if (srq && !(cqe->owner_sr_opcode & MLX4_CQE_IS_SEND_MASK))
				mlx4_free_srq_wqe(srq, ntohs(cqe->wqe_index));
			++nfreed;
		} else if (nfreed) {
			dest = get_cqe(cq, (prod_index + nfreed) & cq->ibv_cq.cqe);
			dest += cqe_inc;
			owner_bit = dest->owner_sr_opcode & MLX4_CQE_OWNER_MASK;
			memcpy(dest, cqe, sizeof *cqe);
			dest->owner_sr_opcode = owner_bit |
				(dest->owner_sr_opcode & ~MLX4_CQE_OWNER_MASK);
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

void mlx4_cq_clean(struct mlx4_cq *cq, uint32_t qpn, struct mlx4_srq *srq)
{
	mlx4_spin_lock(&cq->lock);
	__mlx4_cq_clean(cq, qpn, srq);
	mlx4_spin_unlock(&cq->lock);
}

int mlx4_get_outstanding_cqes(struct mlx4_cq *cq)
{
	uint32_t i;

	for (i = cq->cons_index; get_sw_cqe(cq, i); ++i)
		;

	return i - cq->cons_index;
}

void mlx4_cq_resize_copy_cqes(struct mlx4_cq *cq, void *buf, int old_cqe)
{
	struct mlx4_cqe *cqe;
	int i;
	int cqe_inc = cq->cqe_size == 64 ? 1 : 0;

	i = cq->cons_index;
	cqe = get_cqe(cq, (i & old_cqe));
	cqe += cqe_inc;

	while ((cqe->owner_sr_opcode & MLX4_CQE_OPCODE_MASK) != MLX4_CQE_OPCODE_RESIZE) {
		cqe->owner_sr_opcode = (cqe->owner_sr_opcode & ~MLX4_CQE_OWNER_MASK) |
			(((i + 1) & (cq->ibv_cq.cqe + 1)) ? MLX4_CQE_OWNER_MASK : 0);
		memcpy(buf + ((i + 1) & cq->ibv_cq.cqe) * cq->cqe_size,
		       cqe - cqe_inc, cq->cqe_size);
		++i;
		cqe = get_cqe(cq, (i & old_cqe));
		cqe += cqe_inc;
	}

	++cq->cons_index;
}

int mlx4_alloc_cq_buf(struct mlx4_context *mctx, struct mlx4_buf *buf, int nent,
			int entry_size)
{
	struct mlx4_device *dev = to_mdev(mctx->ibv_ctx.device);
	int ret;
	enum mlx4_alloc_type alloc_type;
	enum mlx4_alloc_type default_alloc_type = MLX4_ALLOC_TYPE_PREFER_CONTIG;

	if (mlx4_use_huge("HUGE_CQ"))
		default_alloc_type = MLX4_ALLOC_TYPE_HUGE;

	mlx4_get_alloc_type(MLX4_CQ_PREFIX, &alloc_type,
				default_alloc_type);

	ret = mlx4_alloc_prefered_buf(mctx, buf,
			align(nent * entry_size, dev->page_size),
			dev->page_size,
			alloc_type,
			MLX4_CQ_PREFIX);

	if (ret)
		return -1;

	memset(buf->buf, 0, nent * entry_size);

	return 0;
}
