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

#include <stdlib.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "mlx5.h"
#include "doorbell.h"
#include "wqe.h"

enum {
	MLX5_OPCODE_BASIC	= 0x00010000,
	MLX5_OPCODE_MANAGED	= 0x00020000,

	MLX5_OPCODE_WITH_IMM	= 0x01000000,
	MLX5_OPCODE_EXT_ATOMICS = 0x08
};

#define MLX5_IB_OPCODE(op, class, attr)     (((class) & 0x00FF0000) | ((attr) & 0xFF000000) | ((op) & 0x0000FFFF))
#define MLX5_IB_OPCODE_GET_CLASS(opcode)    ((opcode) & 0x00FF0000)
#define MLX5_IB_OPCODE_GET_OP(opcode)       ((opcode) & 0x0000FFFF)
#define MLX5_IB_OPCODE_GET_ATTR(opcode)     ((opcode) & 0xFF000000)


static const uint32_t mlx5_ib_opcode[] = {
	[IBV_EXP_WR_SEND]                       = MLX5_IB_OPCODE(MLX5_OPCODE_SEND,                MLX5_OPCODE_BASIC, 0),
	[IBV_EXP_WR_SEND_WITH_IMM]              = MLX5_IB_OPCODE(MLX5_OPCODE_SEND_IMM,            MLX5_OPCODE_BASIC, MLX5_OPCODE_WITH_IMM),
	[IBV_EXP_WR_RDMA_WRITE]                 = MLX5_IB_OPCODE(MLX5_OPCODE_RDMA_WRITE,          MLX5_OPCODE_BASIC, 0),
	[IBV_EXP_WR_RDMA_WRITE_WITH_IMM]        = MLX5_IB_OPCODE(MLX5_OPCODE_RDMA_WRITE_IMM,      MLX5_OPCODE_BASIC, MLX5_OPCODE_WITH_IMM),
	[IBV_EXP_WR_RDMA_READ]                  = MLX5_IB_OPCODE(MLX5_OPCODE_RDMA_READ,           MLX5_OPCODE_BASIC, 0),
	[IBV_EXP_WR_ATOMIC_CMP_AND_SWP]         = MLX5_IB_OPCODE(MLX5_OPCODE_ATOMIC_CS,           MLX5_OPCODE_BASIC, 0),
	[IBV_EXP_WR_ATOMIC_FETCH_AND_ADD]       = MLX5_IB_OPCODE(MLX5_OPCODE_ATOMIC_FA,           MLX5_OPCODE_BASIC, 0),
	[IBV_EXP_WR_EXT_MASKED_ATOMIC_CMP_AND_SWP]   = MLX5_IB_OPCODE(MLX5_OPCODE_ATOMIC_MASKED_CS,  MLX5_OPCODE_BASIC, 0),
	[IBV_EXP_WR_EXT_MASKED_ATOMIC_FETCH_AND_ADD] = MLX5_IB_OPCODE(MLX5_OPCODE_ATOMIC_MASKED_FA,  MLX5_OPCODE_BASIC, 0),
	[IBV_EXP_WR_SEND_ENABLE]                = MLX5_IB_OPCODE(MLX5_OPCODE_SEND_ENABLE,         MLX5_OPCODE_MANAGED, 0),
	[IBV_EXP_WR_RECV_ENABLE]                = MLX5_IB_OPCODE(MLX5_OPCODE_RECV_ENABLE,         MLX5_OPCODE_MANAGED, 0),
	[IBV_EXP_WR_CQE_WAIT]                   = MLX5_IB_OPCODE(MLX5_OPCODE_CQE_WAIT,            MLX5_OPCODE_MANAGED, 0),
	[IBV_EXP_WR_NOP]			= MLX5_IB_OPCODE(MLX5_OPCODE_NOP,		  MLX5_OPCODE_BASIC, 0),
	[IBV_EXP_WR_UMR_FILL]			= MLX5_IB_OPCODE(MLX5_OPCODE_UMR,		  MLX5_OPCODE_BASIC, 0),
	[IBV_EXP_WR_UMR_INVALIDATE]             = MLX5_IB_OPCODE(MLX5_OPCODE_UMR,                 MLX5_OPCODE_BASIC, 0),
};

enum {
	MLX5_CALC_UINT64_ADD    = 0x01,
	MLX5_CALC_FLOAT64_ADD   = 0x02,
	MLX5_CALC_UINT64_MAXLOC = 0x03,
	MLX5_CALC_UINT64_AND    = 0x04,
	MLX5_CALC_UINT64_OR     = 0x05,
	MLX5_CALC_UINT64_XOR    = 0x06
};

enum {
	MLX5_WQE_CTRL_CALC_OP = 24
};

static const struct mlx5_calc_op {
	int valid;
	uint32_t opcode;
}  mlx5_calc_ops_table
	[IBV_EXP_CALC_DATA_SIZE_NUMBER]
		[IBV_EXP_CALC_OP_NUMBER]
			[IBV_EXP_CALC_DATA_TYPE_NUMBER] = {
	[IBV_EXP_CALC_DATA_SIZE_64_BIT] = {
		[IBV_EXP_CALC_OP_ADD] = {
			[IBV_EXP_CALC_DATA_TYPE_INT] = {
				.valid = 1,
				.opcode = MLX5_CALC_UINT64_ADD << MLX5_WQE_CTRL_CALC_OP },
			[IBV_EXP_CALC_DATA_TYPE_UINT] = {
				.valid = 1,
				.opcode = MLX5_CALC_UINT64_ADD << MLX5_WQE_CTRL_CALC_OP },
			[IBV_EXP_CALC_DATA_TYPE_FLOAT]  = {
				.valid = 1,
				.opcode = MLX5_CALC_FLOAT64_ADD << MLX5_WQE_CTRL_CALC_OP }
		},
		[IBV_EXP_CALC_OP_BXOR] = {
			[IBV_EXP_CALC_DATA_TYPE_INT] = {
				.valid = 1,
				.opcode = MLX5_CALC_UINT64_XOR << MLX5_WQE_CTRL_CALC_OP },
			[IBV_EXP_CALC_DATA_TYPE_UINT] = {
				.valid = 1,
				.opcode = MLX5_CALC_UINT64_XOR << MLX5_WQE_CTRL_CALC_OP },
			[IBV_EXP_CALC_DATA_TYPE_FLOAT]  = {
				.valid = 1,
				.opcode = MLX5_CALC_UINT64_XOR << MLX5_WQE_CTRL_CALC_OP }
		},
		[IBV_EXP_CALC_OP_BAND] = {
			[IBV_EXP_CALC_DATA_TYPE_INT] = {
				.valid = 1,
				.opcode = MLX5_CALC_UINT64_AND << MLX5_WQE_CTRL_CALC_OP },
			[IBV_EXP_CALC_DATA_TYPE_UINT] = {
				.valid = 1,
				.opcode = MLX5_CALC_UINT64_AND << MLX5_WQE_CTRL_CALC_OP },
			[IBV_EXP_CALC_DATA_TYPE_FLOAT]  = {
				.valid = 1,
				.opcode = MLX5_CALC_UINT64_AND << MLX5_WQE_CTRL_CALC_OP }
		},
		[IBV_EXP_CALC_OP_BOR] = {
			[IBV_EXP_CALC_DATA_TYPE_INT] = {
				.valid = 1,
				.opcode = MLX5_CALC_UINT64_OR << MLX5_WQE_CTRL_CALC_OP },
			[IBV_EXP_CALC_DATA_TYPE_UINT] = {
				.valid = 1,
				.opcode = MLX5_CALC_UINT64_OR << MLX5_WQE_CTRL_CALC_OP },
			[IBV_EXP_CALC_DATA_TYPE_FLOAT]  = {
				.valid = 1,
				.opcode = MLX5_CALC_UINT64_OR << MLX5_WQE_CTRL_CALC_OP }
		},
		[IBV_EXP_CALC_OP_MAXLOC] = {
			[IBV_EXP_CALC_DATA_TYPE_UINT] = {
				.valid = 1,
				.opcode = MLX5_CALC_UINT64_MAXLOC << MLX5_WQE_CTRL_CALC_OP }
		}
	}
};

static inline void set_wait_en_seg(void *wqe_seg, uint32_t obj_num, uint32_t count)
{
	struct mlx5_wqe_wait_en_seg *seg = (struct mlx5_wqe_wait_en_seg *)wqe_seg;

	seg->pi      = htonl(count);
	seg->obj_num = htonl(obj_num);

	return;
}

static void *get_recv_wqe(struct mlx5_qp *qp, int n)
{
	return qp->buf.buf + qp->rq.offset + (n << qp->rq.wqe_shift);
}

static int copy_to_scat(struct mlx5_wqe_data_seg *scat, void *buf, int *size,
			 int max)
{
	int copy;
	int i;

	if (unlikely(!(*size)))
		return IBV_WC_SUCCESS;

	for (i = 0; i < max; ++i) {
		copy = min(*size, ntohl(scat->byte_count));
		memcpy((void *)(unsigned long)ntohll(scat->addr), buf, copy);
		*size -= copy;
		if (*size == 0)
			return IBV_WC_SUCCESS;

		buf += copy;
		++scat;
	}
	return IBV_WC_LOC_LEN_ERR;
}

int mlx5_copy_to_recv_wqe(struct mlx5_qp *qp, int idx, void *buf, int size)
{
	struct mlx5_wqe_data_seg *scat;
	int max = 1 << (qp->rq.wqe_shift - 4);

	scat = get_recv_wqe(qp, idx);
	if (unlikely(qp->wq_sig))
		++scat;

	return copy_to_scat(scat, buf, &size, max);
}

int mlx5_copy_to_send_wqe(struct mlx5_qp *qp, int idx, void *buf, int size)
{
	struct mlx5_wqe_ctrl_seg *ctrl;
	struct mlx5_wqe_data_seg *scat;
	void *p;
	int max;

	idx &= (qp->sq.wqe_cnt - 1);
	ctrl = mlx5_get_send_wqe(qp, idx);
	if (qp->verbs_qp.qp.qp_type != IBV_QPT_RC) {
		fprintf(stderr, "scatter to CQE is supported only for RC QPs\n");
		return IBV_WC_GENERAL_ERR;
	}
	p = ctrl + 1;

	switch (ntohl(ctrl->opmod_idx_opcode) & 0xff) {
	case MLX5_OPCODE_RDMA_READ:
		p = p + sizeof(struct mlx5_wqe_raddr_seg);
		break;

	case MLX5_OPCODE_ATOMIC_CS:
	case MLX5_OPCODE_ATOMIC_FA:
		p = p + sizeof(struct mlx5_wqe_raddr_seg) +
			sizeof(struct mlx5_wqe_atomic_seg);
		break;

	default:
		fprintf(stderr, "scatter to CQE for opcode %d\n",
			ntohl(ctrl->opmod_idx_opcode) & 0xff);
		return IBV_WC_REM_INV_REQ_ERR;
	}

	scat = p;
	max = (ntohl(ctrl->qpn_ds) & 0x3F) - (((void *)scat - (void *)ctrl) >> 4);
	if (unlikely((void *)(scat + max) > qp->sq.qend)) {
		int tmp = ((void *)qp->sq.qend - (void *)scat) >> 4;
		int orig_size = size;

		if (copy_to_scat(scat, buf, &size, tmp) == IBV_WC_SUCCESS)
			return IBV_WC_SUCCESS;
		max = max - tmp;
		buf += orig_size - size;
		scat = mlx5_get_send_wqe(qp, 0);
	}

	return copy_to_scat(scat, buf, &size, max);
}

void *mlx5_get_send_wqe(struct mlx5_qp *qp, int n)
{
	return qp->buf.buf + qp->sq.offset + (n << MLX5_SEND_WQE_SHIFT);
}

void mlx5_init_qp_indices(struct mlx5_qp *qp)
{
	qp->sq.head	 = 0;
	qp->sq.tail	 = 0;
	qp->rq.head	 = 0;
	qp->rq.tail	 = 0;
	qp->sq.cur_post  = 0;
	qp->sq.head_en_index = 0;
	qp->sq.head_en_count = 0;
	qp->rq.head_en_index = 0;
	qp->rq.head_en_count = 0;
}

static int mlx5_wq_overflow(struct mlx5_wq *wq, int nreq, struct mlx5_cq *cq)
{
	unsigned cur;

	cur = wq->head - wq->tail;
	if (cur + nreq < wq->max_post)
		return 0;

	mlx5_spin_lock(&cq->lock);
	cur = wq->head - wq->tail;
	mlx5_spin_unlock(&cq->lock);

	return cur + nreq >= wq->max_post;
}

static inline void set_raddr_seg(struct mlx5_wqe_raddr_seg *rseg,
				 uint64_t remote_addr, uint32_t rkey)
{
	rseg->raddr    = htonll(remote_addr);
	rseg->rkey     = htonl(rkey);
	rseg->reserved = 0;
}

static void set_atomic_seg(struct mlx5_wqe_atomic_seg *aseg,
			   enum ibv_wr_opcode   opcode,
			   uint64_t swap,
			   uint64_t compare_add)
{
	if (opcode == IBV_WR_ATOMIC_CMP_AND_SWP) {
		aseg->swap_add = htonll(swap);
		aseg->compare  = htonll(compare_add);
	} else {
		aseg->swap_add = htonll(compare_add);
		aseg->compare  = 0;
	}
}

static void set_datagram_seg(struct mlx5_wqe_datagram_seg *dseg,
			     struct ibv_exp_send_wr *wr)
{
	memcpy(&dseg->av, &to_mah(wr->wr.ud.ah)->av, sizeof dseg->av);
	dseg->av.dqp_dct = htonl(wr->wr.ud.remote_qpn | MLX5_EXTENED_UD_AV);
	dseg->av.key.qkey.qkey = htonl(wr->wr.ud.remote_qkey);
}

static void set_dci_seg(struct mlx5_wqe_datagram_seg *dseg,
			struct ibv_exp_send_wr *wr)
{
	memcpy(&dseg->av, &to_mah(wr->dc.ah)->av, sizeof(dseg->av));
	dseg->av.dqp_dct = htonl(wr->dc.dct_number | MLX5_EXTENED_UD_AV);
	dseg->av.key.dc_key = htonll(wr->dc.dct_access_key);
}

static int set_data_ptr_seg(struct mlx5_wqe_data_seg *dseg, struct ibv_sge *sg,
			struct mlx5_pd *pd)
{
	uint32_t lkey;
	switch (sg->lkey) {
	case ODP_GLOBAL_R_LKEY:
		if (mlx5_get_real_lkey_from_implicit_lkey(pd, &pd->r_ilkey,
							  sg->addr, sg->length,
							  &lkey))
			return ENOMEM;
		break;
	case ODP_GLOBAL_W_LKEY:
		if (mlx5_get_real_lkey_from_implicit_lkey(pd, &pd->w_ilkey,
							  sg->addr, sg->length,
							  &lkey))
			return ENOMEM;
		break;
	default:
		lkey = sg->lkey;
	}

	dseg->byte_count = htonl(sg->length);
	dseg->lkey       = htonl(lkey);
	dseg->addr       = htonll(sg->addr);

	return 0;
}

/*
 * Avoid using memcpy() to copy to BlueFlame page, since memcpy()
 * implementations may use move-string-buffer assembler instructions,
 * which do not guarantee order of copying.
 */
static void mlx5_bf_copy(unsigned long long *dst, unsigned long long *src,
			 unsigned bytecnt, struct mlx5_qp *qp)
{
	while (bytecnt > 0) {
		*dst++ = *src++;
		*dst++ = *src++;
		*dst++ = *src++;
		*dst++ = *src++;
		*dst++ = *src++;
		*dst++ = *src++;
		*dst++ = *src++;
		*dst++ = *src++;
		bytecnt -= 8 * sizeof(unsigned long long);
		if (unlikely(src == qp->sq.qend))
			src = qp->buf.buf + qp->sq.offset;
	}
}

static uint32_t send_ieth(struct ibv_exp_send_wr *wr)
{
	return MLX5_IB_OPCODE_GET_ATTR(mlx5_ib_opcode[wr->exp_opcode]) &
			MLX5_OPCODE_WITH_IMM ?
				wr->ex.imm_data : 0;
}

static int set_data_inl_seg(struct mlx5_qp *qp, struct ibv_exp_send_wr *wr,
			    void *wqe, int *sz)
{
	struct mlx5_wqe_inline_seg *seg;
	void *addr;
	int len;
	int i;
	int inl = 0;
	void *qend = qp->sq.qend;
	int copy;

	seg = wqe;
	wqe += sizeof *seg;
	for (i = 0; i < wr->num_sge; ++i) {
		addr = (void *) (unsigned long)(wr->sg_list[i].addr);
		len  = wr->sg_list[i].length;
		inl += len;

		if (unlikely(inl > qp->max_inline_data))
			return ENOMEM;

		if (unlikely(wqe + len > qend)) {
			copy = qend - wqe;
			memcpy(wqe, addr, copy);
			addr += copy;
			len -= copy;
			wqe = mlx5_get_send_wqe(qp, 0);
		}
		memcpy(wqe, addr, len);
		wqe += len;
	}

	if (likely(inl)) {
		seg->byte_count = htonl(inl | MLX5_INLINE_SEG);
		*sz = align(inl + sizeof seg->byte_count, 16) / 16;
	} else
		*sz = 0;

	return 0;
}

static uint8_t wq_sig(struct mlx5_wqe_ctrl_seg *ctrl)
{
	return ~calc_xor(ctrl, (ntohl(ctrl->qpn_ds) & 0x3f) << 4);
}

#ifdef MLX5_DEBUG
void dump_wqe(FILE *fp, int idx, int size_16, struct mlx5_qp *qp)
{
	uint32_t *uninitialized_var(p);
	int i, j;
	int tidx = idx;

	fprintf(fp, "dump wqe at %p\n", mlx5_get_send_wqe(qp, tidx));
	for (i = 0, j = 0; i < size_16 * 4; i += 4, j += 4) {
		if ((i & 0xf) == 0) {
			void *buf = mlx5_get_send_wqe(qp, tidx);
			tidx = (tidx + 1) & (qp->sq.wqe_cnt - 1);
			p = buf;
			j = 0;
		}
		fprintf(fp, "%08x %08x %08x %08x\n", ntohl(p[j]), ntohl(p[j + 1]),
			ntohl(p[j + 2]), ntohl(p[j + 3]));
	}
}
#endif /* MLX5_DEBUG */


void *mlx5_get_atomic_laddr(struct mlx5_qp *qp, uint16_t idx, int *byte_count)
{
	struct mlx5_wqe_data_seg *dpseg;
	void *addr;

	dpseg = mlx5_get_send_wqe(qp, idx) + sizeof(struct mlx5_wqe_ctrl_seg) +
		sizeof(struct mlx5_wqe_raddr_seg) +
		sizeof(struct mlx5_wqe_atomic_seg);
	addr = (void *)(unsigned long)ntohll(dpseg->addr);

	/*
	 * Currently byte count is always 8 bytes. Fix this when
	 * we support variable size of atomics
	 */
	*byte_count = 8;
	return addr;
}

static int ext_cmp_swp(struct mlx5_qp *qp, void *seg,
		       struct ibv_exp_send_wr *wr)
{
	struct ibv_exp_cmp_swap *cs = &wr->ext_op.masked_atomics.wr_data.inline_data.op.cmp_swap;
	int arg_sz = 1 << wr->ext_op.masked_atomics.log_arg_sz;
	uint32_t *p32 = seg;
	uint64_t *p64 = seg;
	int i;

	if (arg_sz == 4) {
		*p32 = htonl((uint32_t)cs->swap_val);
		p32++;
		*p32 = htonl((uint32_t)cs->compare_val);
		p32++;
		*p32 = htonl((uint32_t)cs->swap_mask);
		p32++;
		*p32 = htonl((uint32_t)cs->compare_mask);
		return 16;
	} else if (arg_sz == 8) {
		*p64 = htonll(cs->swap_val);
		p64++;
		*p64 = htonll(cs->compare_val);
		p64++;
		if (unlikely(p64 == qp->sq.qend))
			p64 = mlx5_get_send_wqe(qp, 0);
		*p64 = htonll(cs->swap_mask);
		p64++;
		*p64 = htonll(cs->compare_mask);
		return 32;
	} else {
		for (i = 0; i < arg_sz; i += 8, p64++) {
			if (unlikely(p64 == qp->sq.qend))
				p64 = mlx5_get_send_wqe(qp, 0);
			*p64 = htonll(*(uint64_t *)(uintptr_t)(cs->swap_val + i));
		}

		for (i = 0; i < arg_sz; i += 8, p64++) {
			if (unlikely(p64 == qp->sq.qend))
				p64 = mlx5_get_send_wqe(qp, 0);
			*p64 = htonll(*(uint64_t *)(uintptr_t)(cs->compare_val + i));
		}

		for (i = 0; i < arg_sz; i += 8, p64++) {
			if (unlikely(p64 == qp->sq.qend))
				p64 = mlx5_get_send_wqe(qp, 0);
			*p64 = htonll(*(uint64_t *)(uintptr_t)(cs->swap_mask + i));
		}

		for (i = 0; i < arg_sz; i += 8, p64++) {
			if (unlikely(p64 == qp->sq.qend))
				p64 = mlx5_get_send_wqe(qp, 0);
			*p64 = htonll(*(uint64_t *)(uintptr_t)(cs->compare_mask + i));
		}
		return 4 * arg_sz;
	}
}

static int ext_fetch_add(struct mlx5_qp *qp, void *seg,
			 struct ibv_exp_send_wr *wr)
{
	struct ibv_exp_fetch_add *fa = &wr->ext_op.masked_atomics.wr_data.inline_data.op.fetch_add;
	int arg_sz = 1 << wr->ext_op.masked_atomics.log_arg_sz;
	uint32_t *p32 = seg;
	uint64_t *p64 = seg;
	int i;

	if (arg_sz == 4) {
		*p32 = htonl((uint32_t)fa->add_val);
		p32++;
		*p32 = htonl((uint32_t)fa->field_boundary);
		p32++;
		*p32 = htonl(0);
		p32++;
		*p32 = htonl(0);
		return 16;
	} else if (arg_sz == 8) {
		*p64 = htonll(fa->add_val);
		p64++;
		*p64 = htonll(fa->field_boundary);
		return 16;
	} else {
		for (i = 0; i < arg_sz; i += 8, p64++) {
			if (unlikely(p64 == qp->sq.qend))
				p64 = mlx5_get_send_wqe(qp, 0);
			*p64 = htonll(*(uint64_t *)(uintptr_t)(fa->add_val + i));
		}

		for (i = 0; i < arg_sz; i += 8, p64++) {
			if (unlikely(p64 == qp->sq.qend))
				p64 = mlx5_get_send_wqe(qp, 0);
			*p64 = htonll(*(uint64_t *)(uintptr_t)(fa->field_boundary + i));
		}

		return 2 * arg_sz;
	}
}

static int set_ext_atomic_seg(struct mlx5_qp *qp, void *seg,
			      struct ibv_exp_send_wr *wr)
{
	/* currently only inline is supported */
	if (unlikely(!(wr->exp_send_flags & IBV_EXP_SEND_EXT_ATOMIC_INLINE)))
		return -1;

	if (unlikely((1 << wr->ext_op.masked_atomics.log_arg_sz) > qp->max_atomic_arg))
		return -1;

	if (wr->exp_opcode == IBV_EXP_WR_EXT_MASKED_ATOMIC_CMP_AND_SWP)
		return ext_cmp_swp(qp, seg, wr);
	else if (wr->exp_opcode == IBV_EXP_WR_EXT_MASKED_ATOMIC_FETCH_AND_ADD)
		return ext_fetch_add(qp, seg, wr);
	else
		return -1;
}

enum {
	MLX5_UMR_CTRL_INLINE	= 1 << 7,
};

static uint64_t umr_mask(int fill)
{
	uint64_t mask;

	if (fill)
		mask =  MLX5_MKEY_MASK_LEN		|
			MLX5_MKEY_MASK_START_ADDR	|
			MLX5_MKEY_MASK_LR		|
			MLX5_MKEY_MASK_LW		|
			MLX5_MKEY_MASK_RR		|
			MLX5_MKEY_MASK_RW		|
			MLX5_MKEY_MASK_FREE		|
			MLX5_MKEY_MASK_A;
	else
		mask = MLX5_MKEY_MASK_FREE;

	return mask;
}

static void set_umr_ctrl_seg(struct ibv_exp_send_wr *wr,
			     struct mlx5_wqe_umr_ctrl_seg *seg)
{
	int fill = wr->exp_opcode == IBV_EXP_WR_UMR_FILL ? 1 : 0;

	memset(seg, 0, sizeof(*seg));

	if (wr->exp_send_flags & IBV_EXP_SEND_INLINE || !fill)
		seg->flags = MLX5_UMR_CTRL_INLINE;

	seg->mkey_mask = htonll(umr_mask(fill));
}

static int lay_umr(struct mlx5_qp *qp, struct ibv_exp_send_wr *wr,
		   void *seg, int *wqe_size, int *xlat_size,
		   uint64_t *reglen)
{
	enum ibv_exp_umr_wr_type type = wr->ext_op.umr.umr_type;
	struct ibv_exp_mem_region *mlist;
	struct ibv_exp_mem_repeat_block *rep;
	struct mlx5_wqe_data_seg *dseg;
	struct mlx5_seg_repeat_block *rb;
	struct mlx5_seg_repeat_ent *re;
	struct mlx5_klm_buf *klm = NULL;
	void *qend = qp->sq.qend;
	int i;
	int j;
	int n;
	int byte_count = 0;
	int inl = wr->exp_send_flags & IBV_EXP_SEND_INLINE;
	void *buf;
	int tmp;

	if (inl) {
		if (unlikely(qp->sq.max_inl_send_klms <
			     wr->ext_op.umr.num_mrs))
			return EINVAL;
		buf = seg;
	} else {
		klm = to_klm(wr->ext_op.umr.memory_objects);
		buf = klm->align_buf;
	}

	*reglen = 0;
	n = wr->ext_op.umr.num_mrs;
	if (type == IBV_EXP_UMR_MR_LIST) {
		mlist = wr->ext_op.umr.mem_list.mem_reg_list;
		dseg = buf;

		for (i = 0, j = 0; i < n; i++, j++) {
			if (inl && unlikely((&dseg[j] == qend))) {
				dseg = mlx5_get_send_wqe(qp, 0);
				j = 0;
			}

			dseg[j].addr =  htonll((uint64_t)(uintptr_t)mlist[i].base_addr);
			dseg[j].lkey = htonl(mlist[i].mr->lkey);
			dseg[j].byte_count = htonl(mlist[i].length);
			byte_count += mlist[i].length;
		}
		if (inl)
			*wqe_size = align(n * sizeof(*dseg), 64);
		else
			*wqe_size = 0;

		*reglen = byte_count;
		*xlat_size = n * sizeof(*dseg);
	} else {
		rep = wr->ext_op.umr.mem_list.rb.mem_repeat_block_list;
		rb = buf;
		rb->const_0x400 = htonl(0x400);
		rb->reserved = 0;
		rb->num_ent = htons(n);
		re = rb->entries;
		rb->repeat_count = htonl(wr->ext_op.umr.mem_list.rb.repeat_count[0]);

		if (unlikely(wr->ext_op.umr.mem_list.rb.stride_dim != 1)) {
			fprintf(stderr, "dimention must be 1\n");
			return -ENOMEM;
		}


		for (i = 0, j = 0; i < n; i++, j++, rep++, re++) {
			if (inl && unlikely((re == qend)))
				re = mlx5_get_send_wqe(qp, 0);

			byte_count += rep->byte_count[0];
			re->va = htonll(rep->base_addr);
			re->byte_count = htons(rep->byte_count[0]);
			re->stride = htons(rep->stride[0]);
			re->memkey = htonl(rep->mr->lkey);
		}
		rb->byte_count = htonl(byte_count);
		*reglen = byte_count * ntohl(rb->repeat_count);
		tmp = align((n + 1), 4) - n - 1;
		memset(re, 0, tmp * sizeof(*re));
		if (inl) {
			*wqe_size = align(sizeof(*rb) + sizeof(*re) * n, 64);
			*xlat_size = (n + 1) * sizeof(*re);
		} else {
			*wqe_size = 0;
			*xlat_size = (n + 1) * sizeof(*re);
		}
	}
	return 0;
}

static void *adjust_seg(struct mlx5_qp *qp, void *seg)
{
	return mlx5_get_send_wqe(qp, 0) + (seg - qp->sq.qend);
}

static uint8_t get_umr_flags(int acc)
{
	return (acc & IBV_ACCESS_REMOTE_ATOMIC ? MLX5_PERM_ATOMIC       : 0) |
	       (acc & IBV_ACCESS_REMOTE_WRITE  ? MLX5_PERM_REMOTE_WRITE : 0) |
	       (acc & IBV_ACCESS_REMOTE_READ   ? MLX5_PERM_REMOTE_READ  : 0) |
	       (acc & IBV_ACCESS_LOCAL_WRITE   ? MLX5_PERM_LOCAL_WRITE  : 0) |
		MLX5_PERM_LOCAL_READ | MLX5_PERM_UMR_EN;
}

static void set_mkey_seg(struct ibv_exp_send_wr *wr, struct mlx5_mkey_seg *seg)
{
	memset(seg, 0, sizeof(*seg));
	if (wr->exp_opcode != IBV_EXP_WR_UMR_FILL) {
		seg->status = 1 << 6;
		return;
	}

	seg->flags = get_umr_flags(wr->ext_op.umr.exp_access);
	seg->start_addr = htonll(wr->ext_op.umr.base_addr);
	seg->qpn_mkey7_0 = htonl(0xffffff00 | (wr->ext_op.umr.modified_mr->lkey & 0xff));
}

static uint8_t get_fence(uint8_t fence, struct ibv_exp_send_wr *wr)
{
	if (unlikely(wr->exp_opcode == IBV_EXP_WR_LOCAL_INV &&
		     wr->exp_send_flags & IBV_EXP_SEND_FENCE))
		return MLX5_FENCE_MODE_STRONG_ORDERING;

	if (unlikely(fence)) {
		if (wr->exp_send_flags & IBV_EXP_SEND_FENCE)
			return MLX5_FENCE_MODE_SMALL_AND_FENCE;
		else
			return fence;

	} else {
		return 0;
	}
}

static int is_atomic_op(struct ibv_exp_send_wr *wr, int *arg_sz)
{
	switch (wr->exp_opcode) {
	case IBV_EXP_WR_ATOMIC_CMP_AND_SWP:
	case IBV_EXP_WR_ATOMIC_FETCH_AND_ADD:
		*arg_sz = 8;
		break;
	case IBV_EXP_WR_EXT_MASKED_ATOMIC_CMP_AND_SWP:
	case IBV_EXP_WR_EXT_MASKED_ATOMIC_FETCH_AND_ADD:
		*arg_sz = 1 << wr->ext_op.masked_atomics.log_arg_sz;
		break;
	default:
		*arg_sz = 0;
		return 0;
	}
	return 1;
}

int __mlx5_post_send(struct ibv_qp *ibqp, struct ibv_exp_send_wr *wr,
			  struct ibv_exp_send_wr **bad_wr, int is_exp_wr)
{
	struct mlx5_context *ctx;
	struct mlx5_qp *qp = to_mqp(ibqp);
	struct mlx5_pd *pd = to_mpd(ibqp->pd);
	struct mlx5_klm_buf *klm;
	void *seg;
	struct mlx5_wqe_ctrl_seg *ctrl = NULL;
	struct mlx5_wqe_data_seg *dpseg;
	int nreq;
	int inl = 0;
	int err = 0;
	int size = 0;
	int i;
	unsigned idx;
	uint8_t opmod = 0;
	struct mlx5_bf *bf = qp->bf;
	void *qend = qp->sq.qend;
	uint32_t mlx5_opcode;
	struct mlx5_wqe_xrc_seg *xrc;
	uint64_t exp_send_flags;
	int tmp;
	int num_sge;
	uint8_t next_fence = 0;
	uint8_t fence;
	struct mlx5_wqe_umr_ctrl_seg *umr_ctrl;
	int xlat_size;
	struct mlx5_mkey_seg *mk;
	int wqe_sz;
	uint64_t reglen;
	struct ibv_sge sge;
	struct ibv_sge *psge;
	int atom_arg;
#ifdef MLX5_DEBUG
	FILE *fp = to_mctx(ibqp->context)->dbg_fp;
#endif

	if (unlikely(ibqp->state < IBV_QPS_RTS)) {
		mlx5_dbg(fp, MLX5_DBG_QP_SEND, "bad QP state\n");
		errno = EINVAL;
		err = EINVAL;
		*bad_wr = wr;
		return err;
	}

	mlx5_spin_lock(&qp->sq.lock);

	for (nreq = 0; wr; ++nreq, wr = wr->next) {
		exp_send_flags = is_exp_wr ? wr->exp_send_flags : ((struct ibv_send_wr *)wr)->send_flags;
		if (unlikely(wr->exp_opcode < 0 ||
		    wr->exp_opcode >= sizeof(mlx5_ib_opcode) / sizeof(mlx5_ib_opcode[0]))) {
			mlx5_dbg(fp, MLX5_DBG_QP_SEND, "bad opcode %d\n", wr->exp_opcode);
			errno = EINVAL;
			err = EINVAL;
			*bad_wr = wr;
			goto out;
		}

		if (unlikely(!(qp->create_flags & IBV_EXP_QP_CREATE_IGNORE_SQ_OVERFLOW) &&
			mlx5_wq_overflow(&qp->sq, nreq,
			      to_mcq(qp->verbs_qp.qp.send_cq)))) {
			mlx5_dbg(fp, MLX5_DBG_QP_SEND, "work queue overflow\n");
			errno = ENOMEM;
			err = errno;
			*bad_wr = wr;
			goto out;
		}

		if (unlikely(wr->num_sge > qp->sq.max_gs)) {
			mlx5_dbg(fp, MLX5_DBG_QP_SEND, "max gs exceeded %d (max = %d)\n",
				 wr->num_sge, qp->sq.max_gs);
			errno = ENOMEM;
			err = errno;
			*bad_wr = wr;
			goto out;
		}

		if (unlikely(((MLX5_IB_OPCODE_GET_CLASS(mlx5_ib_opcode[wr->exp_opcode]) == MLX5_OPCODE_MANAGED) ||
			      (exp_send_flags & IBV_EXP_SEND_WITH_CALC)) &&
			     !(qp->create_flags & IBV_EXP_QP_CREATE_CROSS_CHANNEL))) {
			mlx5_dbg(fp, MLX5_DBG_QP_SEND, "unsupported cross-channel functionality\n");
			errno = EINVAL;
			err = errno;
			*bad_wr = wr;
			goto out;
		}

		mlx5_opcode = MLX5_IB_OPCODE_GET_OP(mlx5_ib_opcode[wr->exp_opcode]);
		num_sge = wr->num_sge;
		fence = qp->fm_cache;

		idx = qp->sq.cur_post & (qp->sq.wqe_cnt - 1);
		ctrl = seg = mlx5_get_send_wqe(qp, idx);
		*(uint32_t *)(seg + 8) = 0;
		ctrl->imm = send_ieth(wr);
		ctrl->fm_ce_se = qp->sq_signal_bits |
			(exp_send_flags & IBV_EXP_SEND_SIGNALED ?
			 MLX5_WQE_CTRL_CQ_UPDATE : 0) |
			(exp_send_flags & IBV_EXP_SEND_SOLICITED ?
			 MLX5_WQE_CTRL_SOLICITED : 0) |
			(exp_send_flags & IBV_EXP_SEND_FENCE ?
			 MLX5_WQE_CTRL_FENCE : 0);

		seg += sizeof *ctrl;
		size = sizeof *ctrl / 16;

		switch (ibqp->qp_type) {
		case IBV_QPT_XRC_SEND:
		case IBV_QPT_XRC:
		case IBV_EXP_QPT_DC_INI:
			if (ibqp->qp_type == IBV_EXP_QPT_DC_INI) {
				if (likely(wr->exp_opcode != IBV_EXP_WR_NOP))
					set_dci_seg(seg, wr);
				seg  += sizeof(struct mlx5_wqe_datagram_seg);
				size += sizeof(struct mlx5_wqe_datagram_seg) / 16;
				if (unlikely((seg == qend)))
					seg = mlx5_get_send_wqe(qp, 0);

			} else {
				xrc = seg;
				xrc->xrc_srqn = htonl(wr->qp_type.xrc.remote_srqn);
				seg += sizeof(*xrc);
				size += sizeof(*xrc) / 16;
			}
			/* fall through */
		case IBV_QPT_RC:
			switch (wr->exp_opcode) {
			case IBV_EXP_WR_RDMA_READ:
			case IBV_EXP_WR_RDMA_WRITE:
			case IBV_EXP_WR_RDMA_WRITE_WITH_IMM:
				if (unlikely(exp_send_flags & IBV_EXP_SEND_WITH_CALC)) {
					if ((uint32_t)wr->op.calc.data_size >= IBV_EXP_CALC_DATA_SIZE_NUMBER ||
						(uint32_t)wr->op.calc.calc_op >= IBV_EXP_CALC_OP_NUMBER ||
						(uint32_t)wr->op.calc.data_type >= IBV_EXP_CALC_DATA_TYPE_NUMBER ||
						!mlx5_calc_ops_table
						[wr->op.calc.data_size]
							[wr->op.calc.calc_op]
								[wr->op.calc.data_type].valid) {
						errno = EINVAL;
						err = errno;
						*bad_wr = wr;
						goto out;
					}

					mlx5_opcode |= mlx5_calc_ops_table
								[wr->op.calc.data_size]
									[wr->op.calc.calc_op]
										[wr->op.calc.data_type].opcode;
					set_raddr_seg(seg, wr->wr.rdma.remote_addr,
							wr->wr.rdma.rkey);
				} else {
					set_raddr_seg(seg, wr->wr.rdma.remote_addr,
							wr->wr.rdma.rkey);
				}
				seg  += sizeof(struct mlx5_wqe_raddr_seg);
				size += sizeof(struct mlx5_wqe_raddr_seg) / 16;
				break;

			case IBV_EXP_WR_ATOMIC_CMP_AND_SWP:
			case IBV_EXP_WR_ATOMIC_FETCH_AND_ADD:
				if (unlikely(!qp->enable_atomics)) {
					mlx5_dbg(fp, MLX5_DBG_QP_SEND, "atomics not allowed\n");
					errno = EINVAL;
					err = errno;
					*bad_wr = wr;
					goto out;
				}
				set_raddr_seg(seg, wr->wr.atomic.remote_addr,
					      wr->wr.atomic.rkey);
				seg  += sizeof(struct mlx5_wqe_raddr_seg);

				set_atomic_seg(seg, wr->exp_opcode, wr->wr.atomic.swap,
					       wr->wr.atomic.compare_add);
				seg  += sizeof(struct mlx5_wqe_atomic_seg);

				size += (sizeof(struct mlx5_wqe_raddr_seg) +
				sizeof(struct mlx5_wqe_atomic_seg)) / 16;
				break;

			case IBV_EXP_WR_EXT_MASKED_ATOMIC_CMP_AND_SWP:
			case IBV_EXP_WR_EXT_MASKED_ATOMIC_FETCH_AND_ADD:
				if (unlikely(!qp->enable_atomics)) {
					mlx5_dbg(fp, MLX5_DBG_QP_SEND, "atomics not allowed\n");
					errno = EINVAL;
					err = errno;
					*bad_wr = wr;
					goto out;
				}
				if (unlikely(wr->ext_op.masked_atomics.log_arg_sz < 2)) {
					mlx5_dbg(fp, MLX5_DBG_QP_SEND, "to small atomics arg\n");
					errno = EINVAL;
					err = errno;
					*bad_wr = wr;
					goto out;
				}

				if (unlikely(wr->ext_op.masked_atomics.log_arg_sz > 6)) {
					mlx5_dbg(fp, MLX5_DBG_QP_SEND, "data pointer for atomics not supported yet\n");
					errno = EINVAL;
					err = errno;
					*bad_wr = wr;
					goto out;
				}

				set_raddr_seg(seg, wr->ext_op.masked_atomics.remote_addr,
					      wr->ext_op.masked_atomics.rkey);
				seg  += sizeof(struct mlx5_wqe_raddr_seg);
				size += sizeof(struct mlx5_wqe_raddr_seg) / 16;
				tmp = set_ext_atomic_seg(qp, seg, wr);
				if (unlikely(tmp < 0)) {
					mlx5_dbg(fp, MLX5_DBG_QP_SEND, "invalid atomic arguments\n");
					errno = EINVAL;
					err = errno;
					*bad_wr = wr;
					goto out;
				}
				size += (tmp >> 4);
				seg += tmp;
				if (unlikely((seg >= qend)))
					seg = seg - qend + mlx5_get_send_wqe(qp, 0);
				opmod = MLX5_OPCODE_EXT_ATOMICS | (wr->ext_op.masked_atomics.log_arg_sz - 2);
				break;

			case IBV_EXP_WR_SEND:
				if (exp_send_flags & IBV_EXP_SEND_WITH_CALC) {

					if ((uint32_t)wr->op.calc.data_size >= IBV_EXP_CALC_DATA_SIZE_NUMBER ||
						(uint32_t)wr->op.calc.calc_op >= IBV_EXP_CALC_OP_NUMBER ||
						(uint32_t)wr->op.calc.data_type >= IBV_EXP_CALC_DATA_TYPE_NUMBER ||
						!mlx5_calc_ops_table
						[wr->op.calc.data_size]
							[wr->op.calc.calc_op]
								[wr->op.calc.data_type].valid) {
						errno = EINVAL;
						err = errno;
						*bad_wr = wr;
						goto out;
					}

					mlx5_opcode |= mlx5_calc_ops_table
								[wr->op.calc.data_size]
									[wr->op.calc.calc_op]
										[wr->op.calc.data_type].opcode;
				}
				break;

			case IBV_EXP_WR_CQE_WAIT:
				{
					struct mlx5_cq *wait_cq = to_mcq(wr->task.cqe_wait.cq);
					uint32_t wait_index = 0;

					wait_index = wait_cq->wait_index +
							wr->task.cqe_wait.cq_count;
					wait_cq->wait_count = max(wait_cq->wait_count,
							wr->task.cqe_wait.cq_count);

					if (exp_send_flags & IBV_EXP_SEND_WAIT_EN_LAST) {
						wait_cq->wait_index += wait_cq->wait_count;
						wait_cq->wait_count = 0;
					}

					set_wait_en_seg(seg, wait_cq->cqn, wait_index);
					seg   += sizeof(struct mlx5_wqe_wait_en_seg);
					size += sizeof(struct mlx5_wqe_wait_en_seg) / 16;
				}
				break;

			case IBV_EXP_WR_SEND_ENABLE:
			case IBV_EXP_WR_RECV_ENABLE:
				{
					unsigned head_en_index;
					struct mlx5_wq *wq;

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
						errno = EINVAL;
						err = errno;
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
							errno = EINVAL;
							err = errno;
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

					set_wait_en_seg(seg,
							wr->task.wqe_enable.qp->qp_num,
							head_en_index);

					seg += sizeof(struct mlx5_wqe_wait_en_seg);
					size += sizeof(struct mlx5_wqe_wait_en_seg) / 16;
				}
				break;
			case IBV_EXP_WR_UMR_FILL:
			case IBV_EXP_WR_UMR_INVALIDATE:
				if (unlikely(!qp->umr_en)) {
					mlx5_dbg(fp, MLX5_DBG_QP_SEND, "UMR not supported\n");
					err = EINVAL;
					errno = err;
					*bad_wr = wr;
					goto out;
				}
				next_fence = MLX5_FENCE_MODE_INITIATOR_SMALL;
				ctrl->imm = htonl(wr->ext_op.umr.modified_mr->lkey);
				num_sge = 0;
				umr_ctrl = seg;
				set_umr_ctrl_seg(wr, seg);
				seg += sizeof(struct mlx5_wqe_umr_ctrl_seg);
				size += sizeof(struct mlx5_wqe_umr_ctrl_seg) / 16;

				if (unlikely((seg == qend)))
					seg = mlx5_get_send_wqe(qp, 0);
				mk = seg;
				set_mkey_seg(wr, seg);
				seg += sizeof(*mk);
				size += (sizeof(*mk) / 16);
				if (wr->exp_opcode == IBV_EXP_WR_UMR_INVALIDATE)
					break;

				if (unlikely((seg == qend)))
					seg = mlx5_get_send_wqe(qp, 0);
				err = lay_umr(qp, wr, seg, &wqe_sz, &xlat_size, &reglen);
				if (err) {
					mlx5_dbg(fp, MLX5_DBG_QP_SEND, "lay_umr failure\n");
					errno = err;
					*bad_wr = wr;
					goto out;
				}
				mk->len = htonll(reglen);
				size += wqe_sz / 16;
				seg += wqe_sz;
				umr_ctrl->klm_octowords = htons(align(xlat_size, 64) / 16);
				if (unlikely((seg >= qend)))
					seg = adjust_seg(qp, seg);
				if (!(wr->exp_send_flags & IBV_EXP_SEND_INLINE)) {
					klm = to_klm(wr->ext_op.umr.memory_objects);
					sge.addr = (uint64_t)(uintptr_t)klm->mr->addr;
					sge.lkey = klm->mr->lkey;
					sge.length = 0;
					set_data_ptr_seg(seg, &sge, pd);
					size += sizeof(struct mlx5_wqe_data_seg) / 16;
					seg += sizeof(struct mlx5_wqe_data_seg);
				}
				break;

			case IBV_EXP_WR_NOP:
				break;

			default:
				break;
			}
			break;

		case IBV_QPT_UC:
			switch (wr->exp_opcode) {
			case IBV_WR_RDMA_WRITE:
			case IBV_WR_RDMA_WRITE_WITH_IMM:
				set_raddr_seg(seg, wr->wr.rdma.remote_addr,
					      wr->wr.rdma.rkey);
				seg  += sizeof(struct mlx5_wqe_raddr_seg);
				size += sizeof(struct mlx5_wqe_raddr_seg) / 16;
				break;

			default:
				break;
			}
			break;

		case IBV_QPT_UD:
			set_datagram_seg(seg, wr);
			seg  += sizeof(struct mlx5_wqe_datagram_seg);
			size += sizeof(struct mlx5_wqe_datagram_seg) / 16;
			if (unlikely((seg == qend)))
				seg = mlx5_get_send_wqe(qp, 0);
			break;

		default:
			break;
		}

		if ((exp_send_flags & IBV_EXP_SEND_INLINE) && num_sge) {
			int uninitialized_var(sz);

			err = set_data_inl_seg(qp, wr, seg, &sz);
			if (unlikely(err)) {
				*bad_wr = wr;
				mlx5_dbg(fp, MLX5_DBG_QP_SEND,
					 "inline layout failed, err %d\n", err);
				errno = err;
				goto out;
			}
			inl = 1;
			size += sz;
		} else {
			dpseg = seg;
			for (i = 0; i < num_sge; ++i) {
				if (unlikely(dpseg == qend)) {
					seg = mlx5_get_send_wqe(qp, 0);
					dpseg = seg;
				}
				if (likely(wr->sg_list[i].length)) {
					if (unlikely(is_atomic_op(wr, &atom_arg))) {
						sge = wr->sg_list[i];
						sge.length = atom_arg;
						psge = &sge;
					} else {
						psge = wr->sg_list + i;
					}
					if (unlikely(set_data_ptr_seg(dpseg, psge, pd))) {
						mlx5_dbg(fp, MLX5_DBG_QP_SEND, "failed allocating memory for implicit lkey structure\n");
						errno = ENOMEM;
						err = -1;
						*bad_wr = wr;
						goto out;
					}
					++dpseg;
					size += sizeof(struct mlx5_wqe_data_seg) / 16;
				}
			}
		}
		ctrl->fm_ce_se |= get_fence(fence, wr);

		ctrl->opmod_idx_opcode = htonl(((qp->sq.cur_post & 0xffff) << 8) |
					       mlx5_opcode			 |
					       (opmod << 24));
		ctrl->qpn_ds = htonl(size | (ibqp->qp_num << 8));

		if (unlikely(qp->wq_sig))
			ctrl->signature = wq_sig(ctrl);

		qp->sq.wrid[idx] = wr->wr_id;
		qp->sq.wqe_head[idx] = qp->sq.head + nreq;
		qp->sq.cur_post += DIV_ROUND_UP(size * 16, MLX5_SEND_WQE_BB);
		qp->fm_cache = next_fence;

#ifdef MLX5_DEBUG
		if (mlx5_debug_mask & MLX5_DBG_QP_SEND)
			dump_wqe(to_mctx(ibqp->context)->dbg_fp, idx, size, qp);
#endif
	}

out:
	if (likely(nreq)) {
		qp->sq.head += nreq;

		if (qp->create_flags & IBV_EXP_QP_CREATE_MANAGED_SEND) {
			/* Controlled qp */
			wmb();
			goto post_send_no_db;
		}

		/*
		 * Make sure that descriptors are written before
		 * updating doorbell record and ringing the doorbell
		 */
		wmb();
		qp->db[MLX5_SND_DBR] = htonl(qp->sq.cur_post & 0xffff);

		wc_wmb();
		ctx = to_mctx(ibqp->context);
		if (bf->need_lock)
			mlx5_spin_lock(&bf->lock);

		if (!ctx->shut_up_bf && nreq == 1 && bf->uuarn &&
		    (inl || ctx->prefer_bf) && size > 1 &&
		    size <= bf->buf_size / 16)
			mlx5_bf_copy(bf->reg + bf->offset, (unsigned long long *)ctrl,
				     align(size * 16, 64), qp);
		else
			mlx5_write64((__be32 *)ctrl, bf->reg + bf->offset,
				     &ctx->lock32);

		/*
		 * use wc_wmb() to ensure write combining buffers are flushed out
		 * of the running CPU. This must be carried inside the spinlock.
		 * Otherwise, there is a potential race. In the race, CPU A
		 * writes doorbell 1, which is waiting in the WC buffer. CPU B
		 * writes doorbell 2, and it's write is flushed earlier. Since
		 * the wc_wmb is CPU local, this will result in the HCA seeing
		 * doorbell 2, followed by doorbell 1.
		 */
		wc_wmb();
		bf->offset ^= bf->buf_size;
		if (bf->need_lock)
			mlx5_spin_unlock(&bf->lock);
	}

post_send_no_db:

	mlx5_spin_unlock(&qp->sq.lock);

	return err;
}

int mlx5_post_send(struct ibv_qp *ibqp, struct ibv_send_wr *wr,
		   struct ibv_send_wr **bad_wr)
{
	return __mlx5_post_send(ibqp, (struct ibv_exp_send_wr *)wr,
				(struct ibv_exp_send_wr **)bad_wr, 0);
}

int mlx5_exp_post_send(struct ibv_qp *ibqp, struct ibv_exp_send_wr *wr,
		       struct ibv_exp_send_wr **bad_wr)
{
	return __mlx5_post_send(ibqp, wr, bad_wr, 1);
}

static void set_sig_seg(struct mlx5_qp *qp, struct mlx5_rwqe_sig *sig,
			int size, uint16_t idx)
{
	uint8_t  sign;
	uint32_t qpn = qp->verbs_qp.qp.qp_num;

	sign = calc_xor(sig + 1, size);
	sign ^= calc_xor(&qpn, 4);
	sign ^= calc_xor(&idx, 2);
	sig->signature = ~sign;
}

int mlx5_post_recv(struct ibv_qp *ibqp, struct ibv_recv_wr *wr,
		   struct ibv_recv_wr **bad_wr)
{
	struct mlx5_qp *qp = to_mqp(ibqp);
	struct mlx5_pd *pd = to_mpd(ibqp->pd);
	struct mlx5_wqe_data_seg *scat;
	int err = 0;
	int nreq;
	int ind;
	int i, j;
	struct mlx5_rwqe_sig *sig;
	int sigsz;
#ifdef MLX5_DEBUG
	FILE *fp = to_mctx(ibqp->context)->dbg_fp;
#endif

	mlx5_spin_lock(&qp->rq.lock);

	ind = qp->rq.head & (qp->rq.wqe_cnt - 1);

	for (nreq = 0; wr; ++nreq, wr = wr->next) {
		if (unlikely(!(qp->create_flags & IBV_EXP_QP_CREATE_IGNORE_RQ_OVERFLOW) &&
		    mlx5_wq_overflow(&qp->rq, nreq,
				      to_mcq(qp->verbs_qp.qp.recv_cq)))) {
			errno = ENOMEM;
			err = errno;
			*bad_wr = wr;
			goto out;
		}

		if (unlikely(wr->num_sge > qp->rq.max_gs)) {
			errno = EINVAL;
			err = errno;
			*bad_wr = wr;
			goto out;
		}

		scat = get_recv_wqe(qp, ind);
		sig = (struct mlx5_rwqe_sig *)scat;
		if (unlikely(qp->wq_sig))
			++scat;

		for (i = 0, j = 0; i < wr->num_sge; ++i) {
			if (unlikely(!wr->sg_list[i].length))
				continue;
			if (unlikely(set_data_ptr_seg(scat + j++,
				     wr->sg_list + i, pd))) {
				mlx5_dbg(fp, MLX5_DBG_QP_SEND, "failed allocating memory for global lkey structure\n");
				errno = ENOMEM;
				err = -1;
				*bad_wr = wr;
				goto out;
			}
		}

		if (j < qp->rq.max_gs) {
			scat[j].byte_count = 0;
			scat[j].lkey       = htonl(MLX5_INVALID_LKEY);
			scat[j].addr       = 0;
		}

		if (unlikely(qp->wq_sig)) {
			sigsz = min(wr->num_sge, (1 << (qp->rq.wqe_shift - 4)) - 1);

			set_sig_seg(qp, sig, sigsz << 4, qp->rq.head +  nreq);
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

		qp->db[MLX5_RCV_DBR] = htonl(qp->rq.head & 0xffff);
	}

	mlx5_spin_unlock(&qp->rq.lock);

	return err;
}

int mlx5_use_huge(const char *key)
{
	char *e;
	e = getenv(key);
	if (e && !strcmp(e, "y"))
		return 1;

	return 0;
}

void *mlx5_find_rsc(struct mlx5_context *ctx, uint32_t rsn)
{
	int tind = rsn >> MLX5_QP_TABLE_SHIFT;

	if (ctx->rsc_table[tind].refcnt)
		return ctx->rsc_table[tind].table[rsn & MLX5_QP_TABLE_MASK];
	else
		return NULL;
}

int mlx5_store_rsc(struct mlx5_context *ctx, uint32_t rsn, void *rsc)
{
	int tind = rsn >> MLX5_QP_TABLE_SHIFT;

	if (!ctx->rsc_table[tind].refcnt) {
		ctx->rsc_table[tind].table = calloc(MLX5_QP_TABLE_MASK + 1,
						    sizeof(void *));
		if (!ctx->rsc_table[tind].table)
			return -1;
	}

	++ctx->rsc_table[tind].refcnt;
	ctx->rsc_table[tind].table[rsn & MLX5_QP_TABLE_MASK] = rsc;
	return 0;
}

void mlx5_clear_rsc(struct mlx5_context *ctx, uint32_t rsn)
{
	int tind = rsn >> MLX5_QP_TABLE_SHIFT;

	if (!--ctx->rsc_table[tind].refcnt)
		free(ctx->rsc_table[tind].table);
	else
		ctx->rsc_table[tind].table[rsn & MLX5_QP_TABLE_MASK] = NULL;
}

int mlx5_post_task(struct ibv_context *context,
		   struct ibv_exp_task *task_list,
		   struct ibv_exp_task **bad_task)
{
	int rc = 0;
	struct ibv_exp_task *cur_task = NULL;
	struct ibv_exp_send_wr *bad_wr;
	struct mlx5_context *mlx5_ctx = to_mctx(context);

	if (!task_list)
		return rc;

	pthread_mutex_lock(&mlx5_ctx->task_mutex);

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

	pthread_mutex_unlock(&mlx5_ctx->task_mutex);

	return rc;
}
