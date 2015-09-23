/*
 * Copyright (c) 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2005 Mellanox Technologies Ltd.  All rights reserved.
 * Copyright (c) 2007 Cisco, Inc.  All rights reserved.
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

#include <stdlib.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

#include "mlx4.h"
#include "doorbell.h"
#include "wqe.h"

#ifndef htobe64
#include <endian.h>
# if __BYTE_ORDER == __LITTLE_ENDIAN
# define htobe64(x) __bswap_64 (x)
# else
# define htobe64(x) (x)
# endif
#endif

enum {
	MLX4_OPCODE_BASIC	= 0x00010000,
	MLX4_OPCODE_MANAGED	= 0x00020000,

	MLX4_OPCODE_WITH_IMM	= 0x01000000
};

#define MLX4_IB_OPCODE(op, class, attr)     (((class) & 0x00FF0000) | ((attr) & 0xFF000000) | ((op) & 0x0000FFFF))
#define MLX4_IB_OPCODE_GET_CLASS(opcode)    ((opcode) & 0x00FF0000)
#define MLX4_IB_OPCODE_GET_OP(opcode)       ((opcode) & 0x0000FFFF)
#define MLX4_IB_OPCODE_GET_ATTR(opcode)     ((opcode) & 0xFF000000)

static const uint32_t mlx4_ib_opcode[] = {
	[IBV_WR_SEND]			= MLX4_OPCODE_SEND,
	[IBV_WR_SEND_WITH_IMM]		= MLX4_OPCODE_SEND_IMM,
	[IBV_WR_RDMA_WRITE]		= MLX4_OPCODE_RDMA_WRITE,
	[IBV_WR_RDMA_WRITE_WITH_IMM]	= MLX4_OPCODE_RDMA_WRITE_IMM,
	[IBV_WR_RDMA_READ]		= MLX4_OPCODE_RDMA_READ,
	[IBV_WR_ATOMIC_CMP_AND_SWP]	= MLX4_OPCODE_ATOMIC_CS,
	[IBV_WR_ATOMIC_FETCH_AND_ADD]	= MLX4_OPCODE_ATOMIC_FA,
};


static const uint32_t mlx4_ib_opcode_exp[] = {
	[IBV_EXP_WR_SEND]                   = MLX4_IB_OPCODE(MLX4_OPCODE_SEND,                MLX4_OPCODE_BASIC, 0),
	[IBV_EXP_WR_SEND_WITH_IMM]          = MLX4_IB_OPCODE(MLX4_OPCODE_SEND_IMM,            MLX4_OPCODE_BASIC, MLX4_OPCODE_WITH_IMM),
	[IBV_EXP_WR_RDMA_WRITE]             = MLX4_IB_OPCODE(MLX4_OPCODE_RDMA_WRITE,          MLX4_OPCODE_BASIC, 0),
	[IBV_EXP_WR_RDMA_WRITE_WITH_IMM]    = MLX4_IB_OPCODE(MLX4_OPCODE_RDMA_WRITE_IMM,      MLX4_OPCODE_BASIC, MLX4_OPCODE_WITH_IMM),
	[IBV_EXP_WR_RDMA_READ]              = MLX4_IB_OPCODE(MLX4_OPCODE_RDMA_READ,           MLX4_OPCODE_BASIC, 0),
	[IBV_EXP_WR_ATOMIC_CMP_AND_SWP]     = MLX4_IB_OPCODE(MLX4_OPCODE_ATOMIC_CS,           MLX4_OPCODE_BASIC, 0),
	[IBV_EXP_WR_ATOMIC_FETCH_AND_ADD]   = MLX4_IB_OPCODE(MLX4_OPCODE_ATOMIC_FA,           MLX4_OPCODE_BASIC, 0),
	[IBV_EXP_WR_EXT_MASKED_ATOMIC_CMP_AND_SWP]   = MLX4_IB_OPCODE(MLX4_OPCODE_ATOMIC_MASK_CS,  MLX4_OPCODE_BASIC, 0),
	[IBV_EXP_WR_EXT_MASKED_ATOMIC_FETCH_AND_ADD] = MLX4_IB_OPCODE(MLX4_OPCODE_ATOMIC_MASK_FA,  MLX4_OPCODE_BASIC, 0),
	[IBV_EXP_WR_LOCAL_INV]              = MLX4_IB_OPCODE(MLX4_OPCODE_LOCAL_INVAL,	      MLX4_OPCODE_BASIC, 0),
	[IBV_EXP_WR_SEND_WITH_INV]          = MLX4_IB_OPCODE(MLX4_OPCODE_SEND_INVAL,          MLX4_OPCODE_BASIC, MLX4_OPCODE_WITH_IMM),
	[IBV_EXP_WR_BIND_MW]                = MLX4_IB_OPCODE(MLX4_OPCODE_BIND_MW,             MLX4_OPCODE_BASIC, 0),
	[IBV_EXP_WR_SEND_ENABLE]            = MLX4_IB_OPCODE(MLX4_OPCODE_SEND_ENABLE,         MLX4_OPCODE_MANAGED, 0),
	[IBV_EXP_WR_RECV_ENABLE]            = MLX4_IB_OPCODE(MLX4_OPCODE_RECV_ENABLE,         MLX4_OPCODE_MANAGED, 0),
	[IBV_EXP_WR_CQE_WAIT]               = MLX4_IB_OPCODE(MLX4_OPCODE_CQE_WAIT,            MLX4_OPCODE_MANAGED, 0),
};

enum {
	MLX4_CALC_FLOAT64_ADD   = 0x00,
	MLX4_CALC_UINT64_ADD    = 0x01,
	MLX4_CALC_UINT64_MAXLOC = 0x02,
	MLX4_CALC_UINT64_AND    = 0x03,
	MLX4_CALC_UINT64_XOR    = 0x04,
	MLX4_CALC_UINT64_OR     = 0x05
};

enum {
	MLX4_WQE_CTRL_CALC_OP = 26
};

static const struct mlx4_calc_op {
	int valid;
	uint32_t opcode;
}  mlx4_calc_ops_table
	[IBV_EXP_CALC_DATA_SIZE_NUMBER]
		[IBV_EXP_CALC_OP_NUMBER]
			[IBV_EXP_CALC_DATA_TYPE_NUMBER] = {
	[IBV_EXP_CALC_DATA_SIZE_64_BIT] = {
		[IBV_EXP_CALC_OP_ADD] = {
			[IBV_EXP_CALC_DATA_TYPE_INT] = {
				.valid = 1,
				.opcode = MLX4_CALC_UINT64_ADD << MLX4_WQE_CTRL_CALC_OP },
			[IBV_EXP_CALC_DATA_TYPE_UINT] = {
				.valid = 1,
				.opcode = MLX4_CALC_UINT64_ADD << MLX4_WQE_CTRL_CALC_OP },
			[IBV_EXP_CALC_DATA_TYPE_FLOAT]  = {
				.valid = 1,
				.opcode = MLX4_CALC_FLOAT64_ADD << MLX4_WQE_CTRL_CALC_OP }
		},
		[IBV_EXP_CALC_OP_BXOR] = {
			[IBV_EXP_CALC_DATA_TYPE_INT] = {
				.valid = 1,
				.opcode = MLX4_CALC_UINT64_XOR << MLX4_WQE_CTRL_CALC_OP },
			[IBV_EXP_CALC_DATA_TYPE_UINT] = {
				.valid = 1,
				.opcode = MLX4_CALC_UINT64_XOR << MLX4_WQE_CTRL_CALC_OP },
			[IBV_EXP_CALC_DATA_TYPE_FLOAT]  = {
				.valid = 1,
				.opcode = MLX4_CALC_UINT64_XOR << MLX4_WQE_CTRL_CALC_OP }
		},
		[IBV_EXP_CALC_OP_BAND] = {
			[IBV_EXP_CALC_DATA_TYPE_INT] = {
				.valid = 1,
				.opcode = MLX4_CALC_UINT64_AND << MLX4_WQE_CTRL_CALC_OP },
			[IBV_EXP_CALC_DATA_TYPE_UINT] = {
				.valid = 1,
				.opcode = MLX4_CALC_UINT64_AND << MLX4_WQE_CTRL_CALC_OP },
			[IBV_EXP_CALC_DATA_TYPE_FLOAT]  = {
				.valid = 1,
				.opcode = MLX4_CALC_UINT64_AND << MLX4_WQE_CTRL_CALC_OP }
		},
		[IBV_EXP_CALC_OP_BOR] = {
			[IBV_EXP_CALC_DATA_TYPE_INT] = {
				.valid = 1,
				.opcode = MLX4_CALC_UINT64_OR << MLX4_WQE_CTRL_CALC_OP },
			[IBV_EXP_CALC_DATA_TYPE_UINT] = {
				.valid = 1,
				.opcode = MLX4_CALC_UINT64_OR << MLX4_WQE_CTRL_CALC_OP },
			[IBV_EXP_CALC_DATA_TYPE_FLOAT]  = {
				.valid = 1,
				.opcode = MLX4_CALC_UINT64_OR << MLX4_WQE_CTRL_CALC_OP }
		},
		[IBV_EXP_CALC_OP_MAXLOC] = {
			[IBV_EXP_CALC_DATA_TYPE_UINT] = {
				.valid = 1,
				.opcode = MLX4_CALC_UINT64_MAXLOC << MLX4_WQE_CTRL_CALC_OP }
		}
	}
};

#define MLX4_WAIT_EN_VALID (1<<30)

static inline void set_wait_en_seg(void *wqe_seg, uint32_t obj_num, uint32_t count)
{
	struct mlx4_wqe_wait_en_seg *seg = (struct mlx4_wqe_wait_en_seg *)wqe_seg;

	seg->valid   = htonl(MLX4_WAIT_EN_VALID);
	seg->pi      = htonl(count);
	seg->obj_num = htonl(obj_num);

	return;
}

void *mlx4_get_recv_wqe(struct mlx4_qp *qp, int n)
{
	return qp->buf.buf + qp->rq.offset + (n << qp->rq.wqe_shift);
}

static void *get_send_wqe(struct mlx4_qp *qp, unsigned int n)
{
	return qp->buf.buf + qp->sq.offset + (n << qp->sq.wqe_shift);
}

/*
 * Stamp a SQ WQE so that it is invalid if prefetched by marking the
 * first four bytes of every 64 byte chunk with 0xffffffff, except for
 * the very first chunk of the WQE.
 */
static void stamp_send_wqe(struct mlx4_qp *qp, unsigned int n)
{
	uint32_t *wqe = get_send_wqe(qp, n);
	int i;
	int ds = (((struct mlx4_wqe_ctrl_seg *)wqe)->fence_size & 0x3f) << 2;

	for (i = 16; i < ds; i += 16)
		wqe[i] = 0xffffffff;
}

void mlx4_init_qp_indices(struct mlx4_qp *qp)
{
	qp->sq.head	 = 0;
	qp->sq.tail	 = 0;
	qp->rq.head	 = 0;
	qp->rq.tail	 = 0;
	qp->sq.head_en_index = 0;
	qp->sq.head_en_count = 0;
	qp->rq.head_en_index = 0;
	qp->rq.head_en_count = 0;
}

void mlx4_qp_init_sq_ownership(struct mlx4_qp *qp)
{
	struct mlx4_wqe_ctrl_seg *ctrl;
	int i;

	for (i = 0; i < qp->sq.wqe_cnt; ++i) {
		ctrl = get_send_wqe(qp, i);
		ctrl->owner_opcode = htonl(1 << 31);
		ctrl->fence_size = 1 << (qp->sq.wqe_shift - 4);

		stamp_send_wqe(qp, i);
	}
}

static int wq_overflow(struct mlx4_wq *wq, int nreq, struct mlx4_cq *cq)
{
	unsigned cur;

	cur = wq->head - wq->tail;
	if (cur + nreq < wq->max_post)
		return 0;

	mlx4_spin_lock(&cq->lock);
	cur = wq->head - wq->tail;
	mlx4_spin_unlock(&cq->lock);

	return cur + nreq >= wq->max_post;
}

static void set_bind_seg(struct mlx4_wqe_bind_seg *bseg, struct ibv_exp_send_wr *wr)
{
	uint64_t acc = wr->bind_mw.bind_info.exp_mw_access_flags;
	bseg->flags1 = 0;
	if (acc & IBV_EXP_ACCESS_REMOTE_ATOMIC)
		bseg->flags1 |= htonl(MLX4_WQE_MW_ATOMIC);
	if (acc & IBV_EXP_ACCESS_REMOTE_WRITE)
		bseg->flags1 |= htonl(MLX4_WQE_MW_REMOTE_WRITE);
	if (acc & IBV_EXP_ACCESS_REMOTE_READ)
		bseg->flags1 |= htonl(MLX4_WQE_MW_REMOTE_READ);

	bseg->flags2 = 0;
	if (((struct verbs_mw *)(wr->bind_mw.mw))->type == IBV_MW_TYPE_2)
		bseg->flags2 |= htonl(MLX4_WQE_BIND_TYPE_2);
	if (acc & IBV_EXP_ACCESS_MW_ZERO_BASED)
		bseg->flags2 |= htonl(MLX4_WQE_BIND_ZERO_BASED);

	bseg->new_rkey = htonl(wr->bind_mw.rkey);
	bseg->lkey = htonl(wr->bind_mw.bind_info.mr->lkey);
	bseg->addr = htobe64((uint64_t) wr->bind_mw.bind_info.addr);
	bseg->length = htobe64(wr->bind_mw.bind_info.length);
}

static inline void set_local_inv_seg(struct mlx4_wqe_local_inval_seg *iseg,
		uint32_t rkey)
{
	iseg->mem_key	= htonl(rkey);

	iseg->reserved1    = 0;
	iseg->reserved2    = 0;
	iseg->reserved3[0] = 0;
	iseg->reserved3[1] = 0;
}

static inline void set_raddr_seg(struct mlx4_wqe_raddr_seg *rseg,
				 uint64_t remote_addr, uint32_t rkey)
{
	rseg->raddr    = htonll(remote_addr);
	rseg->rkey     = htonl(rkey);
	rseg->reserved = 0;
}

static void set_atomic_seg(struct mlx4_wqe_atomic_seg *aseg,
			   struct ibv_exp_send_wr *wr)
{
	struct ibv_exp_fetch_add *fa;

	if (wr->exp_opcode == IBV_EXP_WR_ATOMIC_CMP_AND_SWP) {
		aseg->swap_add = htonll(wr->wr.atomic.swap);
		aseg->compare  = htonll(wr->wr.atomic.compare_add);
	} else if (wr->exp_opcode == IBV_EXP_WR_EXT_MASKED_ATOMIC_FETCH_AND_ADD) {
		fa = &wr->ext_op.masked_atomics.wr_data.inline_data.op.fetch_add;
		aseg->swap_add = htonll(fa->add_val);
		aseg->compare = htonll(fa->field_boundary);
	} else {
		aseg->swap_add = htonll(wr->wr.atomic.compare_add);
		aseg->compare  = 0;
	}
}

static void set_masked_atomic_seg(struct mlx4_wqe_masked_atomic_seg *aseg,
				  struct ibv_exp_send_wr *wr)
{
	struct ibv_exp_cmp_swap *cs = &wr->ext_op.masked_atomics.wr_data.inline_data.op.cmp_swap;

	aseg->swap_data = htonll(cs->swap_val);
	aseg->cmp_data = htonll(cs->compare_val);
	aseg->swap_mask = htonll(cs->swap_mask);
	aseg->cmp_mask = htonll(cs->compare_mask);
}

static void set_datagram_seg(struct mlx4_wqe_datagram_seg *dseg,
			     struct ibv_send_wr *wr)
{
	memcpy(dseg->av, &to_mah(wr->wr.ud.ah)->av, sizeof (struct mlx4_av));
	dseg->dqpn = htonl(wr->wr.ud.remote_qpn);
	dseg->qkey = htonl(wr->wr.ud.remote_qkey);
	dseg->vlan = htons(to_mah(wr->wr.ud.ah)->vlan);
	memcpy(dseg->mac, to_mah(wr->wr.ud.ah)->mac, 6);
}

static void __set_data_seg(struct mlx4_wqe_data_seg *dseg, struct ibv_sge *sg)
{
	dseg->byte_count = htonl(sg->length);
	dseg->lkey       = htonl(sg->lkey);
	dseg->addr       = htonll(sg->addr);
}

static void set_data_seg(struct mlx4_wqe_data_seg *dseg, struct ibv_sge *sg)
{
	dseg->lkey       = htonl(sg->lkey);
	dseg->addr       = htonll(sg->addr);

	/*
	 * Need a barrier here before writing the byte_count field to
	 * make sure that all the data is visible before the
	 * byte_count field is set.  Otherwise, if the segment begins
	 * a new cacheline, the HCA prefetcher could grab the 64-byte
	 * chunk and get a valid (!= * 0xffffffff) byte count but
	 * stale data, and end up sending the wrong data.
	 */
	wmb();

	if (likely(sg->length))
		dseg->byte_count = htonl(sg->length);
	else
		dseg->byte_count = htonl(0x80000000);
}

/*
 * Avoid using memcpy() to copy to BlueFlame page, since memcpy()
 * implementations may use move-string-buffer assembler instructions,
 * which do not guarantee order of copying.
 */
static void mlx4_bf_copy(unsigned long *dst, unsigned long *src, unsigned bytecnt)
{
	while (bytecnt > 0) {
		*dst++ = *src++;
		*dst++ = *src++;
		bytecnt -= 2 * sizeof (long);
	}
}

int mlx4_post_send(struct ibv_qp *ibqp, struct ibv_send_wr *wr,
		     struct ibv_send_wr **bad_wr)
{
	struct mlx4_context *ctx;
	struct mlx4_qp *qp = to_mqp(ibqp);
	void *wqe;
	struct mlx4_wqe_ctrl_seg *ctrl = NULL;
	unsigned int ind;
	int nreq;
	int inl = 0;
	int ret = 0;
	int size = 0;
	int i;

	mlx4_spin_lock(&qp->sq.lock);

	/* XXX check that state is OK to post send */

	ind = qp->sq.head;

	for (nreq = 0; wr; ++nreq, wr = wr->next) {
		/* to be considered whether can throw first check, create_qp_exp with post_send */
		if (unlikely(!(qp->create_flags & IBV_EXP_QP_CREATE_IGNORE_SQ_OVERFLOW) &&
			wq_overflow(&qp->sq, nreq, to_mcq(ibqp->send_cq)))) {
			ret = ENOMEM;
			*bad_wr = wr;
			goto out;
		}

		if (unlikely(wr->num_sge > qp->sq.max_gs)) {
			ret = ENOMEM;
			*bad_wr = wr;
			goto out;
		}

		if (unlikely(wr->opcode >= sizeof(mlx4_ib_opcode) / sizeof(mlx4_ib_opcode[0]))) {
			ret = EINVAL;
			*bad_wr = wr;
			goto out;
		}

		ctrl = wqe = get_send_wqe(qp, ind & (qp->sq.wqe_cnt - 1));
		qp->sq.wrid[ind & (qp->sq.wqe_cnt - 1)] = wr->wr_id;

		ctrl->srcrb_flags =
			(wr->send_flags & IBV_SEND_SIGNALED ?
			 htonl(MLX4_WQE_CTRL_CQ_UPDATE) : 0) |
			(wr->send_flags & IBV_SEND_SOLICITED ?
			 htonl(MLX4_WQE_CTRL_SOLICIT) : 0)   |
			qp->sq_signal_bits;

		ctrl->imm = (wr->opcode == IBV_WR_SEND_WITH_IMM ||
				wr->opcode == IBV_WR_RDMA_WRITE_WITH_IMM)
			? wr->imm_data : 0;

		wqe += sizeof *ctrl;
		size = sizeof *ctrl / 16;

		switch (ibqp->qp_type) {
		case IBV_QPT_XRC_SEND:
		case IBV_QPT_XRC:
			ctrl->srcrb_flags |= MLX4_REMOTE_SRQN_FLAGS(wr);
			/* fall through */
		case IBV_QPT_RC:
		case IBV_QPT_UC:
			switch (wr->opcode) {
			case IBV_WR_ATOMIC_CMP_AND_SWP:
			case IBV_WR_ATOMIC_FETCH_AND_ADD:
				set_raddr_seg(wqe, wr->wr.atomic.remote_addr,
					      wr->wr.atomic.rkey);
				wqe  += sizeof (struct mlx4_wqe_raddr_seg);

				set_atomic_seg(wqe, (struct ibv_exp_send_wr *)wr);
				wqe  += sizeof (struct mlx4_wqe_atomic_seg);
				size += (sizeof (struct mlx4_wqe_raddr_seg) +
					 sizeof (struct mlx4_wqe_atomic_seg)) / 16;

				break;

			case IBV_WR_RDMA_READ:
				inl = 1;
				/* fall through */
			case IBV_WR_RDMA_WRITE_WITH_IMM:
				if (!wr->num_sge)
					inl = 1;
				/* fall through */
			case IBV_WR_RDMA_WRITE:
				set_raddr_seg(wqe, wr->wr.rdma.remote_addr,
							wr->wr.rdma.rkey);
				wqe  += sizeof (struct mlx4_wqe_raddr_seg);
				size += sizeof (struct mlx4_wqe_raddr_seg) / 16;

				break;

			case IBV_WR_SEND:
				break;

			default:
				/* No extra segments required for sends */
				break;
			}
			break;

		case IBV_QPT_UD:
			set_datagram_seg(wqe, wr);
			wqe  += sizeof (struct mlx4_wqe_datagram_seg);
			size += sizeof (struct mlx4_wqe_datagram_seg) / 16;
			break;

		case IBV_QPT_RAW_PACKET:
			/* Sanity check - prevent from posting empty SR */
			if (unlikely(!wr->num_sge)) {
				ret = -1;
				*bad_wr = wr;
				goto out;
			}
			if (qp->link_layer == IBV_LINK_LAYER_ETHERNET) {
				/* For raw eth, the MLX4_WQE_CTRL_SOLICIT flag is used
				* to indicate that no icrc should be calculated */
				ctrl->srcrb_flags |= htonl(MLX4_WQE_CTRL_SOLICIT);
				/* For raw eth, take the dmac from the payload */
				ctrl->srcrb_flags16[0] = *(uint16_t *)(uintptr_t)wr->sg_list[0].addr;
				ctrl->imm = *(uint32_t *)((uintptr_t)(wr->sg_list[0].addr)+2);
			}
			break;

		default:
			break;
		}

		if (wr->send_flags & IBV_SEND_INLINE && wr->num_sge) {
			struct mlx4_wqe_inline_seg *seg;
			void *addr;
			int len, seg_len;
			int num_seg;
			int off, to_copy;

			inl = 0;

			seg = wqe;
			wqe += sizeof *seg;
			off = ((uintptr_t) wqe) & (MLX4_INLINE_ALIGN - 1);
			num_seg = 0;
			seg_len = 0;

			for (i = 0; i < wr->num_sge; ++i) {
				addr = (void *) (uintptr_t) wr->sg_list[i].addr;
				len  = wr->sg_list[i].length;
				inl += len;

				if (inl > qp->max_inline_data) {
					inl = 0;
					ret = ENOMEM;
					*bad_wr = wr;
					goto out;
				}

				while (len >= MLX4_INLINE_ALIGN - off) {
					to_copy = MLX4_INLINE_ALIGN - off;
					memcpy(wqe, addr, to_copy);
					len -= to_copy;
					wqe += to_copy;
					addr += to_copy;
					seg_len += to_copy;
					wmb(); /* see comment below */
					seg->byte_count = htonl(MLX4_INLINE_SEG | seg_len);
					seg_len = 0;
					seg = wqe;
					wqe += sizeof *seg;
					off = sizeof *seg;
					++num_seg;
				}

				memcpy(wqe, addr, len);
				wqe += len;
				seg_len += len;
				off += len;
			}

			if (seg_len) {
				++num_seg;
				/*
				 * Need a barrier here to make sure
				 * all the data is visible before the
				 * byte_count field is set.  Otherwise
				 * the HCA prefetcher could grab the
				 * 64-byte chunk with this inline
				 * segment and get a valid (!=
				 * 0xffffffff) byte count but stale
				 * data, and end up sending the wrong
				 * data.
				 */
				wmb();
				seg->byte_count = htonl(MLX4_INLINE_SEG | seg_len);
			}

			size += (inl + num_seg * sizeof *seg + 15) / 16;
		} else if (likely(wr->num_sge == 1)) {
			struct mlx4_wqe_data_seg *seg = wqe;

			set_data_seg(seg, wr->sg_list);

			size += (sizeof *seg / 16);
		} else {
			struct mlx4_wqe_data_seg *seg = wqe;

			for (i = wr->num_sge - 1; i >= 0 ; --i)
				set_data_seg(seg + i, wr->sg_list + i);

			size += wr->num_sge * (sizeof *seg / 16);
		}

		ctrl->fence_size = (wr->send_flags & IBV_SEND_FENCE ?
				    MLX4_WQE_CTRL_FENCE : 0) | size;

		/*
		 * Make sure descriptor is fully written before
		 * setting ownership bit (because HW can start
		 * executing as soon as we do).
		 */
		wmb();

		ctrl->owner_opcode = htonl(mlx4_ib_opcode[wr->opcode]) |
			(ind & qp->sq.wqe_cnt ? htonl(1 << 31) : 0);

		/*
		 * We can improve latency by not stamping the last
		 * send queue WQE until after ringing the doorbell, so
		 * only stamp here if there are still more WQEs to post.
		 */
		if (likely(wr->next))
			stamp_send_wqe(qp, (ind + qp->sq_spare_wqes) &
				       (qp->sq.wqe_cnt - 1));

		++ind;
	}

out:
	ctx = to_mctx(ibqp->context);

	if (nreq == 1 && (inl || ctx->prefer_bf) && size > 1 && size <= ctx->bf_buf_size / 16) {
		ctrl->owner_opcode |= htonl((qp->sq.head & 0xffff) << 8);
		uint32_t *tmp = (uint32_t *)ctrl->reserved;

		*tmp |= qp->doorbell_qpn;

		/*
		 * Make sure that descriptor is written to memory
		 * before writing to BlueFlame page.
		 */
		wmb();

		++qp->sq.head;

		if (qp->create_flags & IBV_EXP_QP_CREATE_MANAGED_SEND) {
			wmb();
			goto post_send_no_db;
		}

		mlx4_spin_lock(&ctx->bf_lock);

		mlx4_bf_copy(ctx->bf_page + ctx->bf_offset, (unsigned long *) ctrl,
			     align(size * 16, 64));
		wc_wmb();

		ctx->bf_offset ^= ctx->bf_buf_size;

		mlx4_spin_unlock(&ctx->bf_lock);
	} else if (likely(nreq)) {
		qp->sq.head += nreq;

		if (qp->create_flags & IBV_EXP_QP_CREATE_MANAGED_SEND) {
			/* Controlled qp */
			wmb();
			goto post_send_no_db;
		}

		/*
		 * Make sure that descriptors are written before
		 * doorbell record.
		 */
		wmb();

		*(uint32_t *) (ctx->uar + MLX4_SEND_DOORBELL) = qp->doorbell_qpn;
	}

post_send_no_db:

	if (likely(nreq))
		stamp_send_wqe(qp, (ind + qp->sq_spare_wqes - 1) &
			       (qp->sq.wqe_cnt - 1));

	mlx4_spin_unlock(&qp->sq.lock);

	return ret;
}

int mlx4_exp_post_send(struct ibv_qp *ibqp, struct ibv_exp_send_wr *wr,
		     struct ibv_exp_send_wr **bad_wr)
{
	struct mlx4_context *ctx;
	struct mlx4_qp *qp = to_mqp(ibqp);
	void *wqe;
	struct mlx4_wqe_ctrl_seg *ctrl = NULL;
	unsigned int ind;
	int nreq;
	int inl = 0;
	int ret = 0;
	int size = 0;
	int i;
	uint32_t mlx4_wr_op;
	uint64_t exp_send_flags;

	mlx4_spin_lock(&qp->sq.lock);

	/* XXX check that state is OK to post send */

	ind = qp->sq.head;

	for (nreq = 0; wr; ++nreq, wr = wr->next) {
		exp_send_flags = wr->exp_send_flags;

		if (unlikely(!(qp->create_flags & IBV_EXP_QP_CREATE_IGNORE_SQ_OVERFLOW) &&
			wq_overflow(&qp->sq, nreq, to_mcq(ibqp->send_cq)))) {
			ret = ENOMEM;
			*bad_wr = wr;
			goto out;
		}

		if (unlikely(wr->num_sge > qp->sq.max_gs)) {
			ret = ENOMEM;
			*bad_wr = wr;
			goto out;
		}

		if (unlikely(wr->exp_opcode >= sizeof(mlx4_ib_opcode_exp) / sizeof(mlx4_ib_opcode_exp[0]))) {
			ret = EINVAL;
			*bad_wr = wr;
			goto out;
		}

		if (((MLX4_IB_OPCODE_GET_CLASS(mlx4_ib_opcode_exp[wr->exp_opcode]) == MLX4_OPCODE_MANAGED) ||
		      (exp_send_flags & IBV_EXP_SEND_WITH_CALC)) &&
		     !(qp->create_flags & IBV_EXP_QP_CREATE_CROSS_CHANNEL)) {
			ret = EINVAL;
			*bad_wr = wr;
			goto out;
		}

		mlx4_wr_op = MLX4_IB_OPCODE_GET_OP(mlx4_ib_opcode_exp[wr->exp_opcode]);

		ctrl = wqe = get_send_wqe(qp, ind & (qp->sq.wqe_cnt - 1));
		qp->sq.wrid[ind & (qp->sq.wqe_cnt - 1)] = wr->wr_id;

		ctrl->srcrb_flags =
			(exp_send_flags & IBV_EXP_SEND_SIGNALED ?
			 htonl(MLX4_WQE_CTRL_CQ_UPDATE) : 0) |
			(exp_send_flags & IBV_EXP_SEND_SOLICITED ?
			 htonl(MLX4_WQE_CTRL_SOLICIT) : 0)   |
			(exp_send_flags & IBV_EXP_SEND_IP_CSUM ?
			 htonl(MLX4_WQE_CTRL_IP_CSUM) : 0)   |
			qp->sq_signal_bits;

		ctrl->imm = (MLX4_IB_OPCODE_GET_ATTR(mlx4_ib_opcode_exp[wr->exp_opcode]) & MLX4_OPCODE_WITH_IMM ?
				wr->ex.imm_data : 0);

		wqe += sizeof *ctrl;
		size = sizeof *ctrl / 16;

		switch (ibqp->qp_type) {
		case IBV_QPT_XRC_SEND:
		case IBV_QPT_XRC:
			ctrl->srcrb_flags |= MLX4_REMOTE_SRQN_FLAGS(wr);
			/* fall through */
		case IBV_QPT_RC:
		case IBV_QPT_UC:
			switch (wr->exp_opcode) {
			case IBV_EXP_WR_ATOMIC_CMP_AND_SWP:
			case IBV_EXP_WR_ATOMIC_FETCH_AND_ADD:
			case IBV_EXP_WR_EXT_MASKED_ATOMIC_FETCH_AND_ADD:
				if (wr->exp_opcode == IBV_EXP_WR_EXT_MASKED_ATOMIC_FETCH_AND_ADD) {
					if (!qp->is_masked_atomic) {
						ret = EINVAL;
						*bad_wr = wr;
						goto out;
					}
					set_raddr_seg(wqe,
						      wr->ext_op.masked_atomics.remote_addr,
						      wr->ext_op.masked_atomics.rkey);
				} else {
					set_raddr_seg(wqe, wr->wr.atomic.remote_addr,
						      wr->wr.atomic.rkey);
				}
				wqe  += sizeof (struct mlx4_wqe_raddr_seg);

				set_atomic_seg(wqe, wr);
				wqe  += sizeof (struct mlx4_wqe_atomic_seg);
				size += (sizeof (struct mlx4_wqe_raddr_seg) +
					 sizeof (struct mlx4_wqe_atomic_seg)) / 16;

				break;

			case IBV_EXP_WR_EXT_MASKED_ATOMIC_CMP_AND_SWP:
				if (!qp->is_masked_atomic) {
					ret = EINVAL;
					*bad_wr = wr;
					goto out;
				}
				set_raddr_seg(wqe,
					      wr->ext_op.masked_atomics.remote_addr,
					      wr->ext_op.masked_atomics.rkey);
				wqe += sizeof(struct mlx4_wqe_raddr_seg);

				set_masked_atomic_seg(wqe, wr);
				wqe  += sizeof(struct mlx4_wqe_masked_atomic_seg);
				size += (sizeof(struct mlx4_wqe_raddr_seg) +
					 sizeof(struct mlx4_wqe_masked_atomic_seg)) / 16;
				break;

			case IBV_EXP_WR_RDMA_READ:
				inl = 1;
				/* fall through */
			case IBV_EXP_WR_RDMA_WRITE_WITH_IMM:
				if (!wr->num_sge)
					inl = 1;
				/* fall through */
			case IBV_EXP_WR_RDMA_WRITE:
				if (exp_send_flags & IBV_EXP_SEND_WITH_CALC) {

					if ((uint32_t)wr->op.calc.data_size >= IBV_EXP_CALC_DATA_SIZE_NUMBER ||
					    (uint32_t)wr->op.calc.calc_op >= IBV_EXP_CALC_OP_NUMBER ||
					    (uint32_t)wr->op.calc.data_type >= IBV_EXP_CALC_DATA_TYPE_NUMBER ||
					    !mlx4_calc_ops_table
						[wr->op.calc.data_size]
							[wr->op.calc.calc_op]
								[wr->op.calc.data_type].valid) {
						ret = -1;
						*bad_wr = wr;
						goto out;
					}

					mlx4_wr_op = MLX4_OPCODE_CALC_RDMA_WRITE_IMM |
							mlx4_calc_ops_table
								[wr->op.calc.data_size]
									[wr->op.calc.calc_op]
										[wr->op.calc.data_type].opcode;
					set_raddr_seg(wqe, wr->wr.rdma.remote_addr,
								wr->wr.rdma.rkey);

				} else {
					set_raddr_seg(wqe, wr->wr.rdma.remote_addr,
							wr->wr.rdma.rkey);
				}
				wqe  += sizeof (struct mlx4_wqe_raddr_seg);
				size += sizeof (struct mlx4_wqe_raddr_seg) / 16;

				break;

			case IBV_EXP_WR_LOCAL_INV:
				ctrl->srcrb_flags |=
					htonl(MLX4_WQE_CTRL_STRONG_ORDER);
				set_local_inv_seg(wqe, wr->ex.invalidate_rkey);
				wqe  += sizeof
					(struct mlx4_wqe_local_inval_seg);
				size += sizeof
					(struct mlx4_wqe_local_inval_seg) / 16;
				break;

			case IBV_EXP_WR_BIND_MW:
				ctrl->srcrb_flags |=
					htonl(MLX4_WQE_CTRL_STRONG_ORDER);
				set_bind_seg(wqe, wr);
				wqe  += sizeof
					(struct mlx4_wqe_bind_seg);
				size += sizeof
					(struct mlx4_wqe_bind_seg) / 16;
				break;

			case IBV_EXP_WR_SEND:
				if (exp_send_flags & IBV_EXP_SEND_WITH_CALC) {

					if ((uint32_t)wr->op.calc.data_size >= IBV_EXP_CALC_DATA_SIZE_NUMBER ||
					    (uint32_t)wr->op.calc.calc_op >= IBV_EXP_CALC_OP_NUMBER ||
					    (uint32_t)wr->op.calc.data_type >= IBV_EXP_CALC_DATA_TYPE_NUMBER ||
					    !mlx4_calc_ops_table
						[wr->op.calc.data_size]
							[wr->op.calc.calc_op]
								[wr->op.calc.data_type].valid) {
						ret = -1;
						*bad_wr = wr;
						goto out;
					}

					mlx4_wr_op = MLX4_OPCODE_CALC_SEND |
							mlx4_calc_ops_table
								[wr->op.calc.data_size]
									[wr->op.calc.calc_op]
										[wr->op.calc.data_type].opcode;
				}

				break;

			case IBV_EXP_WR_CQE_WAIT:
				{
					struct mlx4_cq *wait_cq = to_mcq(wr->task.cqe_wait.cq);
					uint32_t wait_index = 0;

					wait_index = wait_cq->wait_index +
								wr->task.cqe_wait.cq_count;
					wait_cq->wait_count = max(wait_cq->wait_count,
								wr->task.cqe_wait.cq_count);

					if (exp_send_flags & IBV_EXP_SEND_WAIT_EN_LAST) {
						wait_cq->wait_index += wait_cq->wait_count;
						wait_cq->wait_count = 0;
					}

					set_wait_en_seg(wqe, wait_cq->cqn, wait_index);
					wqe   += sizeof(struct mlx4_wqe_wait_en_seg);
					size += sizeof(struct mlx4_wqe_wait_en_seg) / 16;
				}
				break;

			case IBV_EXP_WR_SEND_ENABLE:
			case IBV_EXP_WR_RECV_ENABLE:
				{
					unsigned head_en_index;
					struct mlx4_wq *wq;

					/*
					 * Posting work request for QP that does not support
					 * SEND/RECV ENABLE makes performance worse.
					 */
					if (((wr->exp_opcode == IBV_EXP_WR_SEND_ENABLE) &&
					     !(to_mqp(wr->task.wqe_enable.qp)->create_flags &
							     IBV_EXP_QP_CREATE_MANAGED_SEND)) ||
					     ((wr->exp_opcode == IBV_EXP_WR_RECV_ENABLE) &&
					     !(to_mqp(wr->task.wqe_enable.qp)->create_flags &
							     IBV_EXP_QP_CREATE_MANAGED_RECV))) {
						ret = -1;
						*bad_wr = wr;
						goto out;
					}

					wq = (wr->exp_opcode == IBV_EXP_WR_SEND_ENABLE) ?
						&to_mqp(wr->task.wqe_enable.qp)->sq :
						&to_mqp(wr->task.wqe_enable.qp)->rq;

					/* If wqe_count is 0 release all WRs from queue */
					if (wr->task.wqe_enable.wqe_count) {
						head_en_index = wq->head_en_index +
								wr->task.wqe_enable.wqe_count;
						wq->head_en_count = max(wq->head_en_count,
								wr->task.wqe_enable.wqe_count);

						if ((int)(wq->head - head_en_index) < 0) {
							ret = -1;
							*bad_wr = wr;
							goto out;
						}
					} else {
						head_en_index = wq->head;
						wq->head_en_count = wq->head - wq->head_en_index;
					}

					if (exp_send_flags & IBV_EXP_SEND_WAIT_EN_LAST) {
						wq->head_en_index += wq->head_en_count;
						wq->head_en_count = 0;
					}

					set_wait_en_seg(wqe,
							wr->task.wqe_enable.qp->qp_num,
							head_en_index);

					wqe += sizeof(struct mlx4_wqe_wait_en_seg);
					size += sizeof(struct mlx4_wqe_wait_en_seg) / 16;
				}
				break;

			case IBV_EXP_WR_SEND_WITH_INV:
				ctrl->imm = htonl(wr->ex.invalidate_rkey);
				break;

			default:
				/* No extra segments required for sends */
				break;
			}
			break;

		case IBV_QPT_UD:
			set_datagram_seg(wqe, (struct ibv_send_wr *)wr);
			wqe  += sizeof (struct mlx4_wqe_datagram_seg);
			size += sizeof (struct mlx4_wqe_datagram_seg) / 16;
			break;

		case IBV_QPT_RAW_PACKET:
			/* Sanity check - prevent from posting empty SR */
			if (unlikely(!wr->num_sge)) {
				ret = -1;
				*bad_wr = wr;
				goto out;
			}
			if (qp->link_layer == IBV_LINK_LAYER_ETHERNET) {
				/* For raw eth, the MLX4_WQE_CTRL_SOLICIT flag is used
				* to indicate that no icrc should be calculated */
				ctrl->srcrb_flags |= htonl(MLX4_WQE_CTRL_SOLICIT);
				/* For raw eth, take the dmac from the payload */
				ctrl->srcrb_flags16[0] = *(uint16_t *)(uintptr_t)wr->sg_list[0].addr;
				ctrl->imm = *(uint32_t *)((uintptr_t)(wr->sg_list[0].addr)+2);
			}
			break;

		default:
			break;
		}

		if ((exp_send_flags & IBV_EXP_SEND_INLINE) && wr->num_sge) {
			struct mlx4_wqe_inline_seg *seg;
			void *addr;
			int len, seg_len;
			int num_seg;
			int off, to_copy;

			inl = 0;

			seg = wqe;
			wqe += sizeof *seg;
			off = ((uintptr_t) wqe) & (MLX4_INLINE_ALIGN - 1);
			num_seg = 0;
			seg_len = 0;

			for (i = 0; i < wr->num_sge; ++i) {
				addr = (void *) (uintptr_t) wr->sg_list[i].addr;
				len  = wr->sg_list[i].length;
				inl += len;

				if (inl > qp->max_inline_data) {
					inl = 0;
					ret = ENOMEM;
					*bad_wr = wr;
					goto out;
				}

				while (len >= MLX4_INLINE_ALIGN - off) {
					to_copy = MLX4_INLINE_ALIGN - off;
					memcpy(wqe, addr, to_copy);
					len -= to_copy;
					wqe += to_copy;
					addr += to_copy;
					seg_len += to_copy;
					wmb(); /* see comment below */
					seg->byte_count = htonl(MLX4_INLINE_SEG | seg_len);
					seg_len = 0;
					seg = wqe;
					wqe += sizeof *seg;
					off = sizeof *seg;
					++num_seg;
				}

				memcpy(wqe, addr, len);
				wqe += len;
				seg_len += len;
				off += len;
			}

			if (seg_len) {
				++num_seg;
				/*
				 * Need a barrier here to make sure
				 * all the data is visible before the
				 * byte_count field is set.  Otherwise
				 * the HCA prefetcher could grab the
				 * 64-byte chunk with this inline
				 * segment and get a valid (!=
				 * 0xffffffff) byte count but stale
				 * data, and end up sending the wrong
				 * data.
				 */
				wmb();
				seg->byte_count = htonl(MLX4_INLINE_SEG | seg_len);
			}

			size += (inl + num_seg * sizeof * seg + 15) / 16;
		} else if (likely(wr->num_sge == 1)) {
			struct mlx4_wqe_data_seg *seg = wqe;

			set_data_seg(seg, wr->sg_list);

			size += (sizeof *seg / 16);
		} else {
			struct mlx4_wqe_data_seg *seg = wqe;

			for (i = wr->num_sge - 1; i >= 0 ; --i)
				set_data_seg(seg + i, wr->sg_list + i);

			size += wr->num_sge * (sizeof *seg / 16);
		}

		ctrl->fence_size = (exp_send_flags & IBV_EXP_SEND_FENCE ?
				    MLX4_WQE_CTRL_FENCE : 0) | size;

		/*
		 * Make sure descriptor is fully written before
		 * setting ownership bit (because HW can start
		 * executing as soon as we do).
		 */
		wmb();

		ctrl->owner_opcode = htonl(mlx4_wr_op) |
			(ind & qp->sq.wqe_cnt ? htonl(1 << 31) : 0);

		/*
		 * We can improve latency by not stamping the last
		 * send queue WQE until after ringing the doorbell, so
		 * only stamp here if there are still more WQEs to post.
		 */
		if (likely(wr->next))
			stamp_send_wqe(qp, (ind + qp->sq_spare_wqes) &
				       (qp->sq.wqe_cnt - 1));

		++ind;
	}

out:
	ctx = to_mctx(ibqp->context);

	if (nreq == 1 && (inl || ctx->prefer_bf) && size > 1 && size <= ctx->bf_buf_size / 16) {
		ctrl->owner_opcode |= htonl((qp->sq.head & 0xffff) << 8);
		uint32_t *tmp = (uint32_t *)ctrl->reserved;

		*tmp |= qp->doorbell_qpn;

		/*
		 * Make sure that descriptor is written to memory
		 * before writing to BlueFlame page.
		 */
		wmb();

		++qp->sq.head;

		if (qp->create_flags & IBV_EXP_QP_CREATE_MANAGED_SEND) {
			wmb();
			goto post_send_no_db;
		}

		mlx4_spin_lock(&ctx->bf_lock);

		mlx4_bf_copy(ctx->bf_page + ctx->bf_offset, (unsigned long *) ctrl,
			     align(size * 16, 64));
		wc_wmb();

		ctx->bf_offset ^= ctx->bf_buf_size;

		mlx4_spin_unlock(&ctx->bf_lock);
	} else if (likely(nreq)) {
		qp->sq.head += nreq;

		if (qp->create_flags & IBV_EXP_QP_CREATE_MANAGED_SEND) {
			/* Controlled qp */
			wmb();
			goto post_send_no_db;
		}

		/*
		 * Make sure that descriptors are written before
		 * doorbell record.
		 */
		wmb();

		*(uint32_t *) (ctx->uar + MLX4_SEND_DOORBELL) = qp->doorbell_qpn;
	}

post_send_no_db:

	if (likely(nreq))
		stamp_send_wqe(qp, (ind + qp->sq_spare_wqes - 1) &
			       (qp->sq.wqe_cnt - 1));

	mlx4_spin_unlock(&qp->sq.lock);

	return ret;
}



int mlx4_post_recv(struct ibv_qp *ibqp, struct ibv_recv_wr *wr,
		   struct ibv_recv_wr **bad_wr)
{
	struct mlx4_qp *qp = to_mqp(ibqp);
	struct mlx4_wqe_data_seg *scat;
	int ret = 0;
	int nreq;
	unsigned int ind;
	int i;
	struct mlx4_inlr_rbuff *rbuffs;

	mlx4_spin_lock(&qp->rq.lock);

	/* XXX check that state is OK to post receive */
	ind = qp->rq.head & (qp->rq.wqe_cnt - 1);

	for (nreq = 0; wr; ++nreq, wr = wr->next) {
		if (unlikely(!(qp->create_flags & IBV_EXP_QP_CREATE_IGNORE_RQ_OVERFLOW) &&
			wq_overflow(&qp->rq, nreq, to_mcq(ibqp->recv_cq)))) {
			ret = ENOMEM;
			*bad_wr = wr;
			goto out;
		}

		if (unlikely(wr->num_sge > qp->rq.max_gs)) {
			ret = ENOMEM;
			*bad_wr = wr;
			goto out;
		}

		scat = mlx4_get_recv_wqe(qp, ind);

		for (i = 0; i < wr->num_sge; ++i)
			__set_data_seg(scat + i, wr->sg_list + i);

		if (likely(i < qp->rq.max_gs)) {
			scat[i].byte_count = 0;
			scat[i].lkey       = htonl(MLX4_INVALID_LKEY);
			scat[i].addr       = 0;
		}
		if (qp->max_inlr_sg) {
			rbuffs = qp->inlr_buff.buff[ind].sg_list;
			qp->inlr_buff.buff[ind].list_len = wr->num_sge;
			for (i = 0; i < wr->num_sge; ++i) {
				rbuffs->rbuff = (void *)(unsigned long)(wr->sg_list[i].addr);
				rbuffs->rlen = wr->sg_list[i].length;
				rbuffs++;
			}
		}

		qp->rq.wrid[ind] = wr->wr_id;

		ind = (ind + 1) & (qp->rq.wqe_cnt - 1);
	}

out:
	if (likely(nreq)) {
		qp->rq.head += nreq;

		/*
		 * Make sure that descriptors are written before
		 * doorbell record.
		 */
		wmb();

		*qp->db = htonl(qp->rq.head & 0xffff);
	}

	mlx4_spin_unlock(&qp->rq.lock);

	return ret;
}

int num_inline_segs(int data, enum ibv_qp_type type)
{
	/*
	 * Inline data segments are not allowed to cross 64 byte
	 * boundaries.  For UD QPs, the data segments always start
	 * aligned to 64 bytes (16 byte control segment + 48 byte
	 * datagram segment); for other QPs, there will be a 16 byte
	 * control segment and possibly a 16 byte remote address
	 * segment, so in the worst case there will be only 32 bytes
	 * available for the first data segment.
	 */
	if (type == IBV_QPT_UD)
		data += (sizeof (struct mlx4_wqe_ctrl_seg) +
			 sizeof (struct mlx4_wqe_datagram_seg)) %
			MLX4_INLINE_ALIGN;
	else
		data += (sizeof (struct mlx4_wqe_ctrl_seg) +
			 sizeof (struct mlx4_wqe_raddr_seg)) %
			MLX4_INLINE_ALIGN;

	return (data + MLX4_INLINE_ALIGN - sizeof (struct mlx4_wqe_inline_seg) - 1) /
		(MLX4_INLINE_ALIGN - sizeof (struct mlx4_wqe_inline_seg));
}

void mlx4_calc_sq_wqe_size(struct ibv_qp_cap *cap, enum ibv_qp_type type,
			   struct mlx4_qp *qp)
{
	int size;
	int atomic_size;
	int max_sq_sge;

	max_sq_sge	 = align(cap->max_inline_data +
				 num_inline_segs(cap->max_inline_data, type) *
				 sizeof (struct mlx4_wqe_inline_seg),
				 sizeof (struct mlx4_wqe_data_seg)) /
		sizeof (struct mlx4_wqe_data_seg);
	if (max_sq_sge < cap->max_send_sge)
		max_sq_sge = cap->max_send_sge;

	size = max_sq_sge * sizeof (struct mlx4_wqe_data_seg);
	switch (type) {
	case IBV_QPT_UD:
		size += sizeof (struct mlx4_wqe_datagram_seg);
		break;

	case IBV_QPT_UC:
		size += sizeof (struct mlx4_wqe_raddr_seg);
		break;

	case IBV_QPT_XRC_SEND:
	case IBV_QPT_XRC:
	case IBV_QPT_RC:
		size += sizeof (struct mlx4_wqe_raddr_seg);
		/*
		 * An atomic op will require an atomic segment, a
		 * remote address segment and one scatter entry.
		 */
		atomic_size = (qp->is_masked_atomic ?
			       sizeof(struct mlx4_wqe_masked_atomic_seg) :
			       sizeof(struct mlx4_wqe_atomic_seg)) +
			      sizeof(struct mlx4_wqe_raddr_seg) +
			      sizeof(struct mlx4_wqe_data_seg);

		if (size < atomic_size)
			size = atomic_size;
		break;

	default:
		break;
	}

	/* Make sure that we have enough space for a bind request */
	if (size < sizeof (struct mlx4_wqe_bind_seg))
		size = sizeof (struct mlx4_wqe_bind_seg);

	size += sizeof (struct mlx4_wqe_ctrl_seg);

	for (qp->sq.wqe_shift = 6; 1 << qp->sq.wqe_shift < size;
	     qp->sq.wqe_shift++)
		; /* nothing */
}

int mlx4_use_huge(const char *key)
{
	char *e;

	e = getenv(key);
	if (e && !strcmp(e, "y"))
		return 1;

	return 0;
}

void mlx4_dealloc_qp_buf(struct ibv_context *context, struct mlx4_qp *qp)
{
	if (qp->rq.wqe_cnt) {
		free(qp->rq.wrid);
		if (qp->max_inlr_sg) {
			free(qp->inlr_buff.buff[0].sg_list);
			free(qp->inlr_buff.buff);
		}
	}
	if (qp->sq.wqe_cnt)
		free(qp->sq.wrid);

	if (qp->buf.hmem != NULL)
		mlx4_free_buf_huge(to_mctx(context), &qp->buf);
	else
		mlx4_free_buf(&qp->buf);
}

void mlx4_set_sq_sizes(struct mlx4_qp *qp, struct ibv_qp_cap *cap,
		       enum ibv_qp_type type)
{
	int wqe_size;
	struct mlx4_context *ctx = to_mctx(qp->verbs_qp.qp.context);

	wqe_size = min((1 << qp->sq.wqe_shift), MLX4_MAX_WQE_SIZE) -
		sizeof (struct mlx4_wqe_ctrl_seg);
	switch (type) {
	case IBV_QPT_UD:
		wqe_size -= sizeof (struct mlx4_wqe_datagram_seg);
		break;

	case IBV_QPT_XRC_SEND:
	case IBV_QPT_XRC:
	case IBV_QPT_UC:
	case IBV_QPT_RC:
		wqe_size -= sizeof (struct mlx4_wqe_raddr_seg);
		break;

	default:
		break;
	}

	qp->sq.max_gs	     = wqe_size / sizeof (struct mlx4_wqe_data_seg);
	cap->max_send_sge    = min(ctx->max_sge, qp->sq.max_gs);
	qp->sq.max_post	     = min(ctx->max_qp_wr,
				   qp->sq.wqe_cnt - qp->sq_spare_wqes);
	cap->max_send_wr     = qp->sq.max_post;

	/*
	 * Inline data segments can't cross a 64 byte boundary.  So
	 * subtract off one segment header for each 64-byte chunk,
	 * taking into account the fact that wqe_size will be 32 mod
	 * 64 for non-UD QPs.
	 */
	qp->max_inline_data  = wqe_size -
		sizeof (struct mlx4_wqe_inline_seg) *
		(align(wqe_size, MLX4_INLINE_ALIGN) / MLX4_INLINE_ALIGN);
	cap->max_inline_data = qp->max_inline_data;
}

struct mlx4_qp *mlx4_find_qp(struct mlx4_context *ctx, uint32_t qpn)
{
	int tind = (qpn & (ctx->num_qps - 1)) >> ctx->qp_table_shift;

	if (ctx->qp_table[tind].refcnt)
		return ctx->qp_table[tind].table[qpn & ctx->qp_table_mask];
	else
		return NULL;
}

int mlx4_store_qp(struct mlx4_context *ctx, uint32_t qpn, struct mlx4_qp *qp)
{
	int tind = (qpn & (ctx->num_qps - 1)) >> ctx->qp_table_shift;

	if (!ctx->qp_table[tind].refcnt) {
		ctx->qp_table[tind].table = calloc(ctx->qp_table_mask + 1,
						   sizeof (struct mlx4_qp *));
		if (!ctx->qp_table[tind].table)
			return -1;
	}

	++ctx->qp_table[tind].refcnt;
	ctx->qp_table[tind].table[qpn & ctx->qp_table_mask] = qp;
	return 0;
}

void mlx4_clear_qp(struct mlx4_context *ctx, uint32_t qpn)
{
	int tind = (qpn & (ctx->num_qps - 1)) >> ctx->qp_table_shift;

	if (!--ctx->qp_table[tind].refcnt)
		free(ctx->qp_table[tind].table);
	else
		ctx->qp_table[tind].table[qpn & ctx->qp_table_mask] = NULL;
}

int mlx4_post_task(struct ibv_context *context,
		   struct ibv_exp_task *task_list,
		   struct ibv_exp_task **bad_task)
{
	int rc = 0;
	struct ibv_exp_task *cur_task = NULL;
	struct ibv_exp_send_wr  *bad_wr;
	struct mlx4_context *mlx4_ctx = to_mctx(context);

	if (!task_list)
		return rc;

	pthread_mutex_lock(&mlx4_ctx->task_mutex);

	cur_task = task_list;
	while (!rc && cur_task) {

		switch (cur_task->task_type) {
		case IBV_EXP_TASK_SEND:
			rc = ibv_exp_post_send(cur_task->item.qp,
					       cur_task->item.send_wr,
					       &bad_wr);
			break;

		case IBV_EXP_TASK_RECV:
			rc = ibv_post_recv(cur_task->item.qp,
					cur_task->item.recv_wr,
					NULL);
			break;

		default:
			rc = -1;
		}

		if (rc && bad_task) {
			*bad_task = cur_task;
			break;
		}

		cur_task = cur_task->next;
	}

	pthread_mutex_unlock(&mlx4_ctx->task_mutex);

	return rc;
}
