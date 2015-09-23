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
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/in.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "mlx5.h"
#include "mlx5-abi.h"
#include "wqe.h"

int mlx5_single_threaded = 0;

static void __mlx5_query_device(uint64_t raw_fw_ver,
				struct ibv_device_attr *attr)
{
	unsigned major, minor, sub_minor;

	major     = (raw_fw_ver >> 32) & 0xffff;
	minor     = (raw_fw_ver >> 16) & 0xffff;
	sub_minor = raw_fw_ver & 0xffff;

	snprintf(attr->fw_ver, sizeof attr->fw_ver,
		 "%d.%d.%04d", major, minor, sub_minor);
}

int mlx5_query_device(struct ibv_context *context,
		      struct ibv_device_attr *attr)
{
	struct ibv_exp_device_attr attrx;
	struct ibv_exp_query_device cmd;
	uint64_t raw_fw_ver;
	int err;

	memset(&attrx, 0, sizeof(attrx));
	err = ibv_exp_cmd_query_device(context,
				       &attrx,
				       &raw_fw_ver, &cmd,
				       sizeof(cmd));
	if (err)
		return err;

	memcpy(attr, &attrx, sizeof(*attr));
	 __mlx5_query_device(raw_fw_ver, attr);

	 return err;
}

int mlx5_query_device_ex(struct ibv_context *context,
			 struct ibv_exp_device_attr *attr)
{
	struct ibv_exp_query_device cmd;
	uint64_t raw_fw_ver;
	int err;

	err = ibv_exp_cmd_query_device(context, attr, &raw_fw_ver,
				       &cmd, sizeof(cmd));
	if (err)
		return err;

	__mlx5_query_device(raw_fw_ver, (struct ibv_device_attr *)attr);

	attr->exp_device_cap_flags |= IBV_EXP_DEVICE_MR_ALLOCATE;
	if (attr->exp_device_cap_flags & IBV_EXP_DEVICE_CROSS_CHANNEL) {
		attr->comp_mask |= IBV_EXP_DEVICE_ATTR_CALC_CAP;
		attr->calc_cap.data_types =
				(1ULL << IBV_EXP_CALC_DATA_TYPE_INT) |
				(1ULL << IBV_EXP_CALC_DATA_TYPE_UINT) |
				(1ULL << IBV_EXP_CALC_DATA_TYPE_FLOAT);
		attr->calc_cap.data_sizes =
				(1ULL << IBV_EXP_CALC_DATA_SIZE_64_BIT);
		attr->calc_cap.int_ops = (1ULL << IBV_EXP_CALC_OP_ADD) |
				(1ULL << IBV_EXP_CALC_OP_BAND) |
				(1ULL << IBV_EXP_CALC_OP_BXOR) |
				(1ULL << IBV_EXP_CALC_OP_BOR);
		attr->calc_cap.uint_ops = attr->calc_cap.int_ops;
		attr->calc_cap.fp_ops = attr->calc_cap.int_ops;
	}

	return err;
}

int mlx5_query_port(struct ibv_context *context, uint8_t port,
		     struct ibv_port_attr *attr)
{
	struct ibv_query_port cmd;

	return ibv_cmd_query_port(context, port, attr, &cmd, sizeof cmd);
}

struct ibv_pd *mlx5_alloc_pd(struct ibv_context *context)
{
	struct ibv_alloc_pd       cmd;
	struct mlx5_alloc_pd_resp resp;
	struct mlx5_pd		 *pd;

	pd = calloc(1, sizeof *pd);
	if (!pd)
		return NULL;

	if (ibv_cmd_alloc_pd(context, &pd->ibv_pd, &cmd, sizeof cmd,
			     &resp.ibv_resp, sizeof(resp)))
		goto err;

	pd->pdn = resp.pdn;


	if (mlx5_init_implicit_lkey(&pd->r_ilkey, IBV_EXP_ACCESS_ON_DEMAND) ||
		mlx5_init_implicit_lkey(&pd->w_ilkey, IBV_EXP_ACCESS_ON_DEMAND |
					IBV_EXP_ACCESS_LOCAL_WRITE))
		goto err;

	return &pd->ibv_pd;

err:
	free(pd);
	return NULL;
}

int mlx5_free_pd(struct ibv_pd *pd)
{
	struct mlx5_pd *mpd = to_mpd(pd);
	int ret;

	/* TODO: Better handling of destruction failure due to resources
	* opened. At the moment, we might seg-fault here.*/
	mlx5_destroy_implicit_lkey(&mpd->r_ilkey);
	mlx5_destroy_implicit_lkey(&mpd->w_ilkey);

	ret = ibv_cmd_dealloc_pd(pd);
	if (ret)
		return ret;

	free(mpd);
	return 0;
}

static void *alloc_buf(struct mlx5_mr *mr,
		       struct ibv_pd *pd,
		       size_t length,
		       size_t *alloc_length,
		       void *contig_addr)
{
	int force_anon = 0;
	int force_contig = 0;
	enum mlx5_alloc_type alloc_type;
	int page_size = to_mdev(pd->context->device)->page_size;
	int err;

	mlx5_get_alloc_type(MLX5_MR_PREFIX, &alloc_type, MLX5_ALLOC_TYPE_ALL);

	if (alloc_type == MLX5_ALLOC_TYPE_CONTIG)
		force_contig = 1;
	else if (alloc_type == MLX5_ALLOC_TYPE_ANON)
		force_anon = 1;

	if (force_anon) {
		err = mlx5_alloc_buf(&mr->buf, align(length, page_size),
				     page_size);
		if (err)
			return NULL;

		*alloc_length = mr->buf.length;
		return mr->buf.buf;
	}

	*alloc_length = (contig_addr ? length : align(length, page_size));

	err = mlx5_alloc_buf_contig(to_mctx(pd->context), &mr->buf,
				    *alloc_length, page_size, MLX5_MR_PREFIX,
				    contig_addr);
	if (!err)
		return contig_addr ? contig_addr : mr->buf.buf;

	if (force_contig || contig_addr)
		return NULL;

	err = mlx5_alloc_buf(&mr->buf, align(length, page_size),
			     page_size);
	if (err)
		return NULL;

	*alloc_length = mr->buf.length;
	return mr->buf.buf;
}

struct ibv_mr *mlx5_exp_reg_mr(struct ibv_exp_reg_mr_in *in)
{
	struct mlx5_mr *mr;
	struct ibv_exp_reg_mr cmd;
	int ret;
	size_t alloc_length;
	int is_contig;

	if ((in->comp_mask > IBV_EXP_REG_MR_RESERVED - 1) ||
	    (in->exp_access > IBV_EXP_ACCESS_RESERVED - 1)) {
		errno = EINVAL;
		return NULL;
	}

	if (in->addr == 0 && in->length == MLX5_WHOLE_ADDR_SPACE &&
			(in->exp_access & IBV_EXP_ACCESS_ON_DEMAND))
		return mlx5_alloc_whole_addr_mr(in);

	if ((in->exp_access &
	    (IBV_EXP_ACCESS_ON_DEMAND | IBV_EXP_ACCESS_RELAXED)) ==
	    (IBV_EXP_ACCESS_ON_DEMAND | IBV_EXP_ACCESS_RELAXED)) {
		struct ibv_mr *ibv_mr = NULL;
		struct mlx5_pd *mpd = to_mpd(in->pd);
		struct mlx5_implicit_lkey *implicit_lkey =
			mlx5_get_implicit_lkey(mpd, in->exp_access);
		struct ibv_exp_prefetch_attr prefetch_attr = {
			.flags = in->exp_access &
				(IBV_ACCESS_LOCAL_WRITE |
				 IBV_ACCESS_REMOTE_WRITE |
				 IBV_ACCESS_REMOTE_READ) ?
				IBV_EXP_PREFETCH_WRITE_ACCESS : 0,
			.addr = in->addr,
			.length = in->length,
			.comp_mask = 0,
		};

		if (!implicit_lkey)
			return NULL;
		errno = mlx5_get_real_mr_from_implicit_lkey(mpd, implicit_lkey,
							    (uintptr_t)in->addr,
							    in->length,
							    &ibv_mr);
		if (errno)
			return NULL;

		/* Prefetch the requested range */
		ibv_exp_prefetch_mr(ibv_mr, &prefetch_attr);

		return ibv_mr;
	}

	mr = calloc(1, sizeof(*mr));
	if (!mr)
		return NULL;

	/*
	 * if addr is NULL and IBV_EXP_ACCESS_ALLOCATE_MR is set,
	 * the library allocates contiguous memory
	 */

	/* Need valgrind exception here due to compiler optimization problem */
	VALGRIND_MAKE_MEM_DEFINED(&in->create_flags, sizeof(in->create_flags));
	is_contig = (!in->addr && (in->exp_access & IBV_EXP_ACCESS_ALLOCATE_MR)) ||
		    ((in->comp_mask & IBV_EXP_REG_MR_CREATE_FLAGS) &&
		     (in->create_flags & IBV_EXP_REG_MR_CREATE_CONTIG));

	if (is_contig) {
		in->addr = alloc_buf(mr, in->pd, in->length, &alloc_length, in->addr);
		if (!in->addr) {
			free(mr);
			return NULL;
		}

		in->length = alloc_length;
		mr->alloc_flags |= IBV_EXP_ACCESS_ALLOCATE_MR;
		/*
		 * set the allocated address for the verbs consumer
		 */
		mr->ibv_mr.addr = in->addr;
	}

	/* We should store the ODP type of the MR to avoid
	 * calling "ibv_dofork_range" when invoking ibv_dereg_mr
	 */
	if (in->exp_access & IBV_EXP_ACCESS_ON_DEMAND)
		mr->type = MLX5_ODP_MR;

	{
		struct ibv_exp_reg_mr_resp resp;

		ret = ibv_cmd_exp_reg_mr(in,
				     (uintptr_t) in->addr,
				     &(mr->ibv_mr),
				     &cmd, sizeof(cmd),
				     &resp, sizeof(resp));
	}
	if (ret) {
		if ((mr->alloc_flags & IBV_EXP_ACCESS_ALLOCATE_MR)) {
			if (mr->buf.type == MLX5_ALLOC_TYPE_CONTIG)
				mlx5_free_buf_contig(to_mctx(in->pd->context),
						     &mr->buf);
			else
				mlx5_free_buf(&(mr->buf));
		}
		free(mr);
		return NULL;
	}

	return &mr->ibv_mr;
}
struct ibv_mr *mlx5_reg_mr(struct ibv_pd *pd, void *addr,
			   size_t length, int access)
{
	struct ibv_exp_reg_mr_in in;

	in.pd = pd;
	in.addr = addr;
	in.length = length;
	in.exp_access = access;
	in.comp_mask = 0;

	return mlx5_exp_reg_mr(&in);
}
int mlx5_dereg_mr(struct ibv_mr *ibmr)
{
	int ret;
	struct mlx5_mr *mr = to_mmr(ibmr);

	if (ibmr->lkey == ODP_GLOBAL_R_LKEY ||
	    ibmr->lkey == ODP_GLOBAL_W_LKEY) {
		mlx5_dealloc_whole_addr_mr(ibmr);
		return 0;
	}

	if (mr->alloc_flags & IBV_EXP_ACCESS_RELAXED)
		return 0;

	if (mr->alloc_flags & IBV_EXP_ACCESS_NO_RDMA)
		goto free_mr;

	ret = ibv_cmd_dereg_mr(ibmr);
	if (ret)
		return ret;

free_mr:
	if ((mr->alloc_flags & IBV_EXP_ACCESS_ALLOCATE_MR)) {
		if (mr->buf.type == MLX5_ALLOC_TYPE_CONTIG)
			mlx5_free_buf_contig(to_mctx(ibmr->context), &mr->buf);
		else
			mlx5_free_buf(&(mr->buf));
	}

	free(mr);
	return 0;
}

int mlx5_prefetch_mr(struct ibv_mr *mr, struct ibv_exp_prefetch_attr *attr)
{

	struct mlx5_pd *pd = to_mpd(mr->pd);

	if (attr->comp_mask >= IBV_EXP_PREFETCH_MR_RESERVED)
		return EINVAL;


	switch (mr->lkey) {
	case ODP_GLOBAL_R_LKEY:
		return mlx5_prefetch_implicit_lkey(pd, &pd->r_ilkey,
						   (unsigned long)attr->addr,
						   attr->length, attr->flags);
	case ODP_GLOBAL_W_LKEY:
		return mlx5_prefetch_implicit_lkey(pd, &pd->w_ilkey,
						   (unsigned long)attr->addr,
						   attr->length, attr->flags);
	default:
		break;
	}

	return ibv_cmd_exp_prefetch_mr(mr, attr);
}

int mlx5_round_up_power_of_two(long long sz)
{
	long long ret;

	for (ret = 1; ret < sz; ret <<= 1)
		; /* nothing */

	if (ret > INT_MAX) {
		fprintf(stderr, "%s: roundup overflow\n", __func__);
		return -ENOMEM;
	}

	return (int)ret;
}

static int align_queue_size(long long req)
{
	return mlx5_round_up_power_of_two(req);
}

static int get_cqe_size(void)
{
	char *env;
	int size = 64;

	env = getenv("MLX5_CQE_SIZE");
	if (env)
		size = atoi(env);

	switch (size) {
	case 64:
	case 128:
		return size;

	default:
		return -EINVAL;
	}
}

static int srq_sig_enabled(void)
{
	char *env;

	env = getenv("MLX5_SRQ_SIGNATURE");
	if (env)
		return 1;

	return 0;
}

static int qp_sig_enabled(void)
{
	char *env;

	env = getenv("MLX5_QP_SIGNATURE");
	if (env)
		return 1;

	return 0;
}

static struct ibv_cq *create_cq(struct ibv_context *context,
				int cqe,
				struct ibv_comp_channel *channel,
				int comp_vector,
				struct ibv_exp_cq_init_attr *attr)
{
	struct mlx5_create_cq		cmd;
	struct mlx5_exp_create_cq	cmd_e;
	struct mlx5_create_cq_resp	resp;
	struct mlx5_cq		       *cq;
	int				cqe_sz;
	int				ret;
	int				ncqe;
#ifdef MLX5_DEBUG
	FILE *fp = to_mctx(context)->dbg_fp;
#endif

	if (!cqe) {
		mlx5_dbg(fp, MLX5_DBG_CQ, "\n");
		errno = EINVAL;
		return NULL;
	}

	cq =  calloc(1, sizeof *cq);
	if (!cq) {
		mlx5_dbg(fp, MLX5_DBG_CQ, "\n");
		return NULL;
	}

	memset(&cmd, 0, sizeof(cmd));
	memset(&cmd_e, 0, sizeof(cmd_e));
	cq->cons_index = 0;
	/* wait_index should start at value before 0 */
	cq->wait_index = (uint32_t)(-1);
	cq->wait_count = 0;

	if (mlx5_spinlock_init(&cq->lock))
		goto err;

	/* The additional entry is required for resize CQ */
	if (cqe <= 0) {
		mlx5_dbg(fp, MLX5_DBG_CQ, "\n");
		errno = EINVAL;
		goto err_spl;
	}

	ncqe = align_queue_size(cqe + 1);
	if ((ncqe > (1 << 24)) || (ncqe < (cqe + 1))) {
		mlx5_dbg(fp, MLX5_DBG_CQ, "ncqe %d\n", ncqe);
		errno = EINVAL;
		goto err_spl;
	}

	cqe_sz = get_cqe_size();
	if (cqe_sz < 0) {
		mlx5_dbg(fp, MLX5_DBG_CQ, "\n");
		errno = -cqe_sz;
		goto err_spl;
	}

	if (mlx5_alloc_cq_buf(to_mctx(context), cq, &cq->buf_a, ncqe, cqe_sz)) {
		mlx5_dbg(fp, MLX5_DBG_CQ, "\n");
		goto err_spl;
	}

	cq->dbrec  = mlx5_alloc_dbrec(to_mctx(context));
	if (!cq->dbrec) {
		mlx5_dbg(fp, MLX5_DBG_CQ, "\n");
		goto err_buf;
	}

	cq->dbrec[MLX5_CQ_SET_CI]	= 0;
	cq->dbrec[MLX5_CQ_ARM_DB]	= 0;
	cq->arm_sn			= 0;
	cq->cqe_sz			= cqe_sz;

	if (attr->comp_mask) {
		cmd_e.buf_addr = (uintptr_t) cq->buf_a.buf;
		cmd_e.db_addr  = (uintptr_t) cq->dbrec;
		cmd_e.cqe_size = cqe_sz;
	} else {
		cmd.buf_addr = (uintptr_t) cq->buf_a.buf;
		cmd.db_addr  = (uintptr_t) cq->dbrec;
		cmd.cqe_size = cqe_sz;
	}

	if (attr->comp_mask)
		ret = ibv_exp_cmd_create_cq(context, ncqe - 1, channel,
					    comp_vector, &cq->ibv_cq,
					    &cmd_e.ibv_cmd,
					    sizeof(cmd_e.ibv_cmd),
					    sizeof(cmd_e) - sizeof(cmd_e.ibv_cmd),
					    &resp.ibv_resp, sizeof(resp.ibv_resp),
					    sizeof(resp) - sizeof(resp.ibv_resp), attr);
	else
		ret = ibv_cmd_create_cq(context, ncqe - 1, channel, comp_vector,
					&cq->ibv_cq, &cmd.ibv_cmd, sizeof cmd,
					&resp.ibv_resp, sizeof(resp));
	if (ret) {
		mlx5_dbg(fp, MLX5_DBG_CQ, "ret %d\n", ret);
		goto err_db;
	}

	cq->active_buf = &cq->buf_a;
	cq->resize_buf = NULL;
	cq->cqn = resp.cqn;
	cq->stall_enable = to_mctx(context)->stall_enable;
	cq->stall_adaptive_enable = to_mctx(context)->stall_adaptive_enable;
	cq->stall_cycles = to_mctx(context)->stall_cycles;

	return &cq->ibv_cq;

err_db:
	mlx5_free_db(to_mctx(context), cq->dbrec);

err_buf:
	mlx5_free_cq_buf(to_mctx(context), &cq->buf_a);

err_spl:
	mlx5_spinlock_destroy(&cq->lock);

err:
	free(cq);

	return NULL;
}

struct ibv_cq *mlx5_create_cq(struct ibv_context *context, int cqe,
			      struct ibv_comp_channel *channel,
			      int comp_vector)
{
	struct ibv_exp_cq_init_attr attr;

	attr.comp_mask = 0;
	return create_cq(context, cqe, channel, comp_vector, &attr);
}

struct ibv_cq *mlx5_create_cq_ex(struct ibv_context *context,
				 int cqe,
				 struct ibv_comp_channel *channel,
				 int comp_vector,
				 struct ibv_exp_cq_init_attr *attr)
{
	return create_cq(context, cqe, channel, comp_vector, attr);
}

int mlx5_resize_cq(struct ibv_cq *ibcq, int cqe)
{
	struct mlx5_cq *cq = to_mcq(ibcq);
	struct mlx5_resize_cq_resp resp;
	struct mlx5_resize_cq cmd;
	struct mlx5_context *mctx = to_mctx(ibcq->context);
	int err;

	if (cqe < 0) {
		errno = EINVAL;
		return errno;
	}

	memset(&cmd, 0, sizeof(cmd));
	memset(&resp, 0, sizeof(resp));

	if (((long long)cqe * 64) > INT_MAX)
		return EINVAL;

	mlx5_spin_lock(&cq->lock);
	cq->active_cqes = cq->ibv_cq.cqe;
	if (cq->active_buf == &cq->buf_a)
		cq->resize_buf = &cq->buf_b;
	else
		cq->resize_buf = &cq->buf_a;

	cqe = align_queue_size(cqe + 1);
	if (cqe == ibcq->cqe + 1) {
		cq->resize_buf = NULL;
		err = 0;
		goto out;
	}

	/* currently we don't change cqe size */
	cq->resize_cqe_sz = cq->cqe_sz;
	cq->resize_cqes = cqe;
	err = mlx5_alloc_cq_buf(mctx, cq, cq->resize_buf, cq->resize_cqes, cq->resize_cqe_sz);
	if (err) {
		cq->resize_buf = NULL;
		errno = ENOMEM;
		goto out;
	}

	cmd.buf_addr = (uintptr_t)cq->resize_buf->buf;
	cmd.cqe_size = cq->resize_cqe_sz;

	err = ibv_cmd_resize_cq(ibcq, cqe - 1, &cmd.ibv_cmd, sizeof(cmd),
				&resp.ibv_resp, sizeof(resp));
	if (err)
		goto out_buf;

	mlx5_cq_resize_copy_cqes(cq);
	mlx5_free_cq_buf(mctx, cq->active_buf);
	cq->active_buf = cq->resize_buf;
	cq->ibv_cq.cqe = cqe - 1;
	mlx5_spin_unlock(&cq->lock);
	cq->resize_buf = NULL;
	return 0;

out_buf:
	mlx5_free_cq_buf(mctx, cq->resize_buf);
	cq->resize_buf = NULL;

out:
	mlx5_spin_unlock(&cq->lock);
	return err;
}

int mlx5_destroy_cq(struct ibv_cq *cq)
{
	int ret;

	ret = ibv_cmd_destroy_cq(cq);
	if (ret)
		return ret;

	mlx5_free_db(to_mctx(cq->context), to_mcq(cq)->dbrec);
	mlx5_free_cq_buf(to_mctx(cq->context), to_mcq(cq)->active_buf);
	free(to_mcq(cq));

	return 0;
}

struct ibv_srq *mlx5_create_srq(struct ibv_pd *pd,
				struct ibv_srq_init_attr *attr)
{
	struct mlx5_create_srq      cmd;
	struct mlx5_create_srq_resp resp;
	struct mlx5_srq		   *srq;
	int			    ret;
	struct mlx5_context	   *ctx;
	int			    max_sge;
	struct ibv_srq		   *ibsrq;

	ctx = to_mctx(pd->context);
	srq = calloc(1, sizeof *srq);
	if (!srq) {
		fprintf(stderr, "%s-%d:\n", __func__, __LINE__);
		return NULL;
	}
	ibsrq = (struct ibv_srq *)&srq->vsrq;
	srq->is_xsrq = 0;

	memset(&cmd, 0, sizeof cmd);
	if (mlx5_spinlock_init(&srq->lock)) {
		fprintf(stderr, "%s-%d:\n", __func__, __LINE__);
		goto err;
	}

	if (attr->attr.max_wr > ctx->max_srq_recv_wr) {
		fprintf(stderr, "%s-%d:max_wr %d, max_srq_recv_wr %d\n", __func__, __LINE__,
			attr->attr.max_wr, ctx->max_srq_recv_wr);
		errno = EINVAL;
		goto err;
	}

	/*
	 * this calculation does not consider required control segments. The
	 * final calculation is done again later. This is done so to avoid
	 * overflows of variables
	 */
	max_sge = ctx->max_rq_desc_sz / sizeof(struct mlx5_wqe_data_seg);
	if (attr->attr.max_sge > max_sge) {
		fprintf(stderr, "%s-%d:max_wr %d, max_srq_recv_wr %d\n", __func__, __LINE__,
			attr->attr.max_wr, ctx->max_srq_recv_wr);
		errno = EINVAL;
		goto err;
	}

	srq->max     = align_queue_size(attr->attr.max_wr + 1);
	srq->max_gs  = attr->attr.max_sge;
	srq->counter = 0;

	if (mlx5_alloc_srq_buf(pd->context, srq)) {
		fprintf(stderr, "%s-%d:\n", __func__, __LINE__);
		goto err;
	}

	srq->db = mlx5_alloc_dbrec(to_mctx(pd->context));
	if (!srq->db) {
		fprintf(stderr, "%s-%d:\n", __func__, __LINE__);
		goto err_free;
	}

	*srq->db = 0;

	cmd.buf_addr = (uintptr_t) srq->buf.buf;
	cmd.db_addr  = (uintptr_t) srq->db;
	srq->wq_sig = srq_sig_enabled();
	if (srq->wq_sig)
		cmd.flags = MLX5_SRQ_FLAG_SIGNATURE;

	attr->attr.max_sge = srq->max_gs;
	pthread_mutex_lock(&ctx->srq_table_mutex);
	ret = ibv_cmd_create_srq(pd, ibsrq, attr, &cmd.ibv_cmd, sizeof(cmd),
				 &resp.ibv_resp, sizeof(resp));
	if (ret)
		goto err_db;

	ret = mlx5_store_srq(ctx, resp.srqn, srq);
	if (ret)
		goto err_destroy;

	pthread_mutex_unlock(&ctx->srq_table_mutex);

	srq->srqn = resp.srqn;

	return ibsrq;

err_destroy:
	ibv_cmd_destroy_srq(ibsrq);

err_db:
	pthread_mutex_unlock(&ctx->srq_table_mutex);
	mlx5_free_db(to_mctx(pd->context), srq->db);

err_free:
	free(srq->wrid);
	mlx5_free_buf(&srq->buf);

err:
	free(srq);

	return NULL;
}

int mlx5_modify_srq(struct ibv_srq *srq,
		    struct ibv_srq_attr *attr,
		    int attr_mask)
{
	struct ibv_modify_srq cmd;

	if (srq->handle == LEGACY_XRC_SRQ_HANDLE)
		srq = (struct ibv_srq *)(((struct ibv_srq_legacy *) srq)->ibv_srq);

	return ibv_cmd_modify_srq(srq, attr, attr_mask, &cmd, sizeof cmd);
}

int mlx5_query_srq(struct ibv_srq *srq,
		    struct ibv_srq_attr *attr)
{
	struct ibv_query_srq cmd;
	if (srq->handle == LEGACY_XRC_SRQ_HANDLE)
		srq = (struct ibv_srq *)(((struct ibv_srq_legacy *) srq)->ibv_srq);

	return ibv_cmd_query_srq(srq, attr, &cmd, sizeof cmd);
}

int mlx5_destroy_srq(struct ibv_srq *srq)
{
	struct ibv_srq *legacy_srq = NULL;
	int ret;

	if (srq->handle == LEGACY_XRC_SRQ_HANDLE) {
		legacy_srq = srq;
		srq = (struct ibv_srq *)(((struct ibv_srq_legacy *) srq)->ibv_srq);
	}

	ret = ibv_cmd_destroy_srq(srq);
	if (ret)
		return ret;

	mlx5_clear_srq(to_mctx(srq->context), to_msrq(srq)->srqn);
	mlx5_free_db(to_mctx(srq->context), to_msrq(srq)->db);
	mlx5_free_buf(&to_msrq(srq)->buf);
	free(to_msrq(srq)->wrid);
	free(to_msrq(srq));

	if (legacy_srq)
		free(legacy_srq);

	return 0;
}

static int sq_overhead(struct ibv_exp_qp_init_attr *attr, struct mlx5_qp *qp,
		       int *inl_atom)
{
	int size1 = 0;
	int size2 = 0;
	int atom = 0;

	switch (attr->qp_type) {
	case IBV_QPT_RC:
		size1 = sizeof(struct mlx5_wqe_ctrl_seg) +
			sizeof(struct mlx5_wqe_umr_ctrl_seg) +
			sizeof(struct mlx5_mkey_seg) +
			sizeof(struct mlx5_seg_repeat_block);
		size2 = sizeof(struct mlx5_wqe_ctrl_seg) +
			sizeof(struct mlx5_wqe_raddr_seg);

		if (qp->enable_atomics) {
			if ((attr->comp_mask & IBV_EXP_QP_INIT_ATTR_ATOMICS_ARG) &&
			    (attr->max_atomic_arg > 4))
				atom = 4 * attr->max_atomic_arg;
			/* TBD: change when we support data pointer args */
			if (inl_atom)
				*inl_atom = max(sizeof(struct mlx5_wqe_atomic_seg), atom);
		}
		break;

	case IBV_QPT_UC:
		size2 = sizeof(struct mlx5_wqe_ctrl_seg) +
			sizeof(struct mlx5_wqe_raddr_seg);
		break;

	case IBV_QPT_UD:
		size1 = sizeof(struct mlx5_wqe_ctrl_seg) +
			sizeof(struct mlx5_wqe_umr_ctrl_seg) +
			sizeof(struct mlx5_mkey_seg) +
			sizeof(struct mlx5_seg_repeat_block);

		size2 = sizeof(struct mlx5_wqe_ctrl_seg) +
			sizeof(struct mlx5_wqe_datagram_seg);
		break;

	case IBV_QPT_XRC:
	case IBV_QPT_XRC_SEND:
	case IBV_QPT_XRC_RECV:
		size2 = sizeof(struct mlx5_wqe_ctrl_seg) +
			sizeof(struct mlx5_wqe_xrc_seg) +
			sizeof(struct mlx5_wqe_raddr_seg);
		if (qp->enable_atomics) {
			if ((attr->comp_mask & IBV_EXP_QP_INIT_ATTR_ATOMICS_ARG) &&
			    (attr->max_atomic_arg > 4))
				atom = 4 * attr->max_atomic_arg;
			/* TBD: change when we support data pointer args */
			if (inl_atom)
				*inl_atom = max(sizeof(struct mlx5_wqe_atomic_seg), atom);
		}
		break;

	case IBV_EXP_QPT_DC_INI:
		size1 = sizeof(struct mlx5_wqe_ctrl_seg) +
			sizeof(struct mlx5_wqe_umr_ctrl_seg) +
			sizeof(struct mlx5_mkey_seg) +
			sizeof(struct mlx5_seg_repeat_block);

		size2 = sizeof(struct mlx5_wqe_ctrl_seg) +
			sizeof(struct mlx5_wqe_datagram_seg) +
			sizeof(struct mlx5_wqe_raddr_seg);
		if (qp->enable_atomics) {
			if ((attr->comp_mask & IBV_EXP_QP_INIT_ATTR_ATOMICS_ARG) &&
			    (attr->max_atomic_arg > 4))
				atom = 4 * attr->max_atomic_arg;
			/* TBD: change when we support data pointer args */
			if (inl_atom)
				*inl_atom = max(sizeof(struct mlx5_wqe_atomic_seg), atom);
		}
		break;
	default:
		return -EINVAL;
	}

	if (qp->umr_en)
		return max(size1, size2);
	else
		return size2;
}

static int mlx5_max4(int t1, int t2, int t3, int t4)
{
	if (t1 < t2)
		t1 = t2;

	if (t1 < t3)
		t1 = t3;

	if (t1 < t4)
		return t4;

	return t1;
}

static int mlx5_calc_send_wqe(struct mlx5_context *ctx,
			      struct ibv_exp_qp_init_attr *attr,
			      struct mlx5_qp *qp)
{
	int inl_size = 0;
	int max_gather;
	int tot_size;
	int overhead;
	int inl_umr = 0;
	int inl_atom = 0;
	int t1 = 0;
	int t2 = 0;
	int t3 = 0;
	int t4 = 0;

	overhead = sq_overhead(attr, qp, &inl_atom);
	if (overhead < 0)
		return overhead;

	if ((attr->comp_mask & IBV_EXP_QP_INIT_ATTR_ATOMICS_ARG))
		qp->max_atomic_arg = attr->max_atomic_arg;
	if ((attr->comp_mask & IBV_EXP_QP_INIT_ATTR_MAX_INL_KLMS) &&
	    attr->max_inl_send_klms)
		inl_umr = attr->max_inl_send_klms * 16;

	if (attr->cap.max_inline_data) {
		inl_size = align(sizeof(struct mlx5_wqe_inl_data_seg) +
				 attr->cap.max_inline_data, 16);
	}

	max_gather = (ctx->max_sq_desc_sz -  overhead) /
		sizeof(struct mlx5_wqe_data_seg);
	if (attr->cap.max_send_sge > max_gather)
		return -EINVAL;

	if (inl_atom)
		t1 = overhead + sizeof(struct mlx5_wqe_data_seg) + inl_atom;

	t2 = overhead + attr->cap.max_send_sge * sizeof(struct mlx5_wqe_data_seg);

	t3 = overhead + inl_umr;
	t4 = overhead + inl_size;

	tot_size = mlx5_max4(t1, t2, t3, t4);

	if (tot_size > ctx->max_sq_desc_sz)
		return -EINVAL;

	return align(tot_size, MLX5_SEND_WQE_BB);
}

static int mlx5_calc_rcv_wqe(struct mlx5_context *ctx,
			     struct ibv_exp_qp_init_attr *attr,
			     struct mlx5_qp *qp)
{
	int size;
	int num_scatter;

	if (attr->srq)
		return 0;

	num_scatter = max(attr->cap.max_recv_sge, 1);
	size = sizeof(struct mlx5_wqe_data_seg) * num_scatter;
	if (qp->wq_sig)
		size += sizeof(struct mlx5_rwqe_sig);

	if (size < 0 || size > ctx->max_rq_desc_sz)
		return -EINVAL;

	size = mlx5_round_up_power_of_two(size);

	return size;
}

static int get_send_sge(struct ibv_exp_qp_init_attr *attr, int wqe_size, struct mlx5_qp *qp)
{
	int max_sge;
	int overhead = sq_overhead(attr, qp, NULL);

	if (attr->qp_type == IBV_QPT_RC)
		max_sge = (min(wqe_size, 512) -
			   sizeof(struct mlx5_wqe_ctrl_seg) -
			   sizeof(struct mlx5_wqe_raddr_seg)) /
		sizeof(struct mlx5_wqe_data_seg);
	else if (attr->qp_type == IBV_EXP_QPT_DC_INI)
		max_sge = (min(wqe_size, 512) -
			   sizeof(struct mlx5_wqe_ctrl_seg) -
			   sizeof(struct mlx5_wqe_datagram_seg) -
			   sizeof(struct mlx5_wqe_raddr_seg)) /
		sizeof(struct mlx5_wqe_data_seg);
	else if (attr->qp_type == IBV_QPT_XRC)
		max_sge = (min(wqe_size, 512) -
			   sizeof(struct mlx5_wqe_ctrl_seg) -
			   sizeof(struct mlx5_wqe_xrc_seg) -
			   sizeof(struct mlx5_wqe_raddr_seg)) /
		sizeof(struct mlx5_wqe_data_seg);
	else
		max_sge = (wqe_size - overhead) /
		sizeof(struct mlx5_wqe_data_seg);

	return min(max_sge, wqe_size - overhead /
		   sizeof(struct mlx5_wqe_data_seg));
}

static int mlx5_calc_sq_size(struct mlx5_context *ctx,
			     struct ibv_exp_qp_init_attr *attr,
			     struct mlx5_qp *qp)
{
	int wqe_size;
	int wq_size;
#ifdef MLX5_DEBUG
	FILE *fp = ctx->dbg_fp;
#endif

	if (!attr->cap.max_send_wr)
		return 0;

	wqe_size = mlx5_calc_send_wqe(ctx, attr, qp);
	if (wqe_size < 0) {
		mlx5_dbg(fp, MLX5_DBG_QP, "\n");
		return wqe_size;
	}

	if (attr->qp_type == IBV_EXP_QPT_DC_INI &&
	    wqe_size > ctx->max_desc_sz_sq_dc) {
		mlx5_dbg(fp, MLX5_DBG_QP, "\n");
		return -EINVAL;
	} else if (wqe_size > ctx->max_sq_desc_sz) {
		mlx5_dbg(fp, MLX5_DBG_QP, "\n");
		return -EINVAL;
	}

	qp->max_inline_data = wqe_size - sq_overhead(attr, qp, NULL) -
		sizeof(struct mlx5_wqe_inl_data_seg);
	attr->cap.max_inline_data = qp->max_inline_data;

	/*
	 * to avoid overflow, we limit max_send_wr so
	 * that the multiplication will fit in int
	 */
	if (attr->cap.max_send_wr > 0x7fffffff / ctx->max_sq_desc_sz) {
		mlx5_dbg(fp, MLX5_DBG_QP, "\n");
		return -ENOMEM;
	}

	wq_size = mlx5_round_up_power_of_two(attr->cap.max_send_wr * wqe_size);
	qp->sq.wqe_cnt = wq_size / MLX5_SEND_WQE_BB;
	if (qp->sq.wqe_cnt > ctx->max_send_wqebb) {
		mlx5_dbg(fp, MLX5_DBG_QP, "\n");
		return -ENOMEM;
	}

	qp->sq.wqe_shift = mlx5_ilog2(MLX5_SEND_WQE_BB);
	qp->sq.max_gs = get_send_sge(attr, wqe_size, qp);
	if (qp->sq.max_gs < attr->cap.max_send_sge)
		return -ENOMEM;

	attr->cap.max_send_sge = qp->sq.max_gs;
	if (qp->umr_en) {
		qp->sq.max_inl_send_klms = ((attr->qp_type == IBV_QPT_RC) ||
					    (attr->qp_type == IBV_EXP_QPT_DC_INI)) ?
					    attr->max_inl_send_klms : 0;
		attr->max_inl_send_klms = qp->sq.max_inl_send_klms;
	}
	qp->sq.max_post = wq_size / wqe_size;

	return wq_size;
}

static int qpt_has_rq(enum ibv_qp_type qpt)
{
	switch (qpt) {
	case IBV_QPT_RC:
	case IBV_QPT_UC:
	case IBV_QPT_UD:
	case IBV_QPT_RAW_ETH:
		return 1;

	case IBV_QPT_XRC:
	case IBV_QPT_XRC_SEND:
	case IBV_QPT_XRC_RECV:
	case IBV_EXP_QPT_DC_INI:
		return 0;
	}
	return 0;
}

static int mlx5_calc_rq_size(struct mlx5_context *ctx,
			     struct ibv_exp_qp_init_attr *attr,
			     struct mlx5_qp *qp)
{
	int wqe_size;
	int wq_size;
	int scat_spc;
#ifdef MLX5_DEBUG
	FILE *fp = ctx->dbg_fp;
#endif

	if (!attr->cap.max_recv_wr || !qpt_has_rq(attr->qp_type))
		return 0;

	if (attr->cap.max_recv_wr > ctx->max_recv_wr) {
		mlx5_dbg(fp, MLX5_DBG_QP, "\n");
		return -EINVAL;
	}

	wqe_size = mlx5_calc_rcv_wqe(ctx, attr, qp);
	if (wqe_size < 0 || wqe_size > ctx->max_rq_desc_sz) {
		mlx5_dbg(fp, MLX5_DBG_QP, "\n");
		return -EINVAL;
	}

	wq_size = mlx5_round_up_power_of_two(attr->cap.max_recv_wr) * wqe_size;
	if (wqe_size) {
		wq_size = max(wq_size, MLX5_SEND_WQE_BB);
		qp->rq.wqe_cnt = wq_size / wqe_size;
		qp->rq.wqe_shift = mlx5_ilog2(wqe_size);
		qp->rq.max_post = 1 << mlx5_ilog2(wq_size / wqe_size);
		scat_spc = wqe_size -
			(qp->wq_sig ? sizeof(struct mlx5_rwqe_sig) : 0);
		qp->rq.max_gs = scat_spc / sizeof(struct mlx5_wqe_data_seg);
	} else {
		qp->rq.wqe_cnt = 0;
		qp->rq.wqe_shift = 0;
		qp->rq.max_post = 0;
		qp->rq.max_gs = 0;
	}
	return wq_size;
}

static int mlx5_calc_wq_size(struct mlx5_context *ctx,
			     struct ibv_exp_qp_init_attr *attr,
			     struct mlx5_qp *qp)
{
	int ret;
	int result;

	ret = mlx5_calc_sq_size(ctx, attr, qp);
	if (ret < 0)
		return ret;

	result = ret;
	ret = mlx5_calc_rq_size(ctx, attr, qp);
	if (ret < 0)
		return ret;

	result += ret;

	qp->sq.offset = ret;
	qp->rq.offset = 0;

	return result;
}

static void map_uuar(struct ibv_context *context, struct mlx5_qp *qp,
		     int uuar_index)
{
	struct mlx5_context *ctx = to_mctx(context);

	qp->bf = &ctx->bfs[uuar_index];
}

static const char *qptype2key(enum ibv_qp_type type)
{
	switch (type) {
	case IBV_QPT_RC: return "HUGE_RC";
	case IBV_QPT_UC: return "HUGE_UC";
	case IBV_QPT_UD: return "HUGE_UD";
#ifdef _NOT_EXISTS_IN_OFED_2_0
	case IBV_QPT_RAW_PACKET: return "HUGE_RAW_ETH";
#endif
	default: return "HUGE_NA";
	}
}

static int mlx5_alloc_qp_buf(struct ibv_context *context,
			     struct ibv_qp_cap *cap, struct mlx5_qp *qp,
			     int size)
{
	int err;
	enum mlx5_alloc_type alloc_type;
	enum mlx5_alloc_type default_alloc_type = MLX5_ALLOC_TYPE_PREFER_CONTIG;
	const char *qp_huge_key;

	if (qp->sq.wqe_cnt) {
		qp->sq.wrid = malloc(qp->sq.wqe_cnt * sizeof(*qp->sq.wrid));
		if (!qp->sq.wrid) {
			errno = ENOMEM;
			err = -1;
		}
	}

	qp->sq.wqe_head = malloc(qp->sq.wqe_cnt * sizeof(*qp->sq.wqe_head));
	if (!qp->sq.wqe_head) {
		errno = ENOMEM;
		err = -1;
			goto ex_wrid;
	}

	if (qp->rq.wqe_cnt) {
		qp->rq.wrid = malloc(qp->rq.wqe_cnt * sizeof(uint64_t));
		if (!qp->rq.wrid) {
			errno = ENOMEM;
			err = -1;
			goto ex_wrid;
		}
	}

	/* compatability support */
	qp_huge_key  = qptype2key(qp->verbs_qp.qp.qp_type);
	if (mlx5_use_huge(qp_huge_key))
		default_alloc_type = MLX5_ALLOC_TYPE_HUGE;

	mlx5_get_alloc_type(MLX5_QP_PREFIX, &alloc_type,
			    default_alloc_type);

	err = mlx5_alloc_prefered_buf(to_mctx(context), &qp->buf,
				      align(qp->buf_size, to_mdev
				      (context->device)->page_size),
				      to_mdev(context->device)->page_size,
				      alloc_type,
				      MLX5_QP_PREFIX);

	if (err) {
		err = -ENOMEM;
		goto ex_wrid;
	}

	memset(qp->buf.buf, 0, qp->buf_size);

	return 0;

ex_wrid:
	if (qp->rq.wrid)
		free(qp->rq.wrid);

	if (qp->sq.wqe_head)
		free(qp->sq.wqe_head);

	if (qp->sq.wrid)
		free(qp->sq.wrid);

	return err;
}

static void mlx5_free_qp_buf(struct mlx5_qp *qp)
{
	struct mlx5_context *ctx = to_mctx(qp->verbs_qp.qp.context);

	mlx5_free_actual_buf(ctx, &qp->buf);
	if (qp->rq.wrid)
		free(qp->rq.wrid);

	if (qp->sq.wqe_head)
		free(qp->sq.wqe_head);

	if (qp->sq.wrid)
		free(qp->sq.wrid);
}

static void update_caps(struct ibv_context *context)
{
	struct mlx5_context *ctx;
	struct ibv_exp_device_attr attr;
	int err;

	ctx = to_mctx(context);
	if (ctx->info.valid)
		return;

	attr.comp_mask = IBV_EXP_DEVICE_ATTR_RESERVED - 1;
	err = ibv_exp_query_device(context, &attr);
	if (err)
		return;

	ctx->info.exp_atomic_cap = attr.exp_atomic_cap;
	ctx->info.valid = 1;
	ctx->max_sge = attr.max_sge;
	if (attr.comp_mask & IBV_EXP_DEVICE_ATTR_UMR)
		ctx->max_send_wqe_inline_klms =
			attr.umr_caps.max_send_wqe_inline_klms;
	return;
}

static struct ibv_qp *create_qp(struct ibv_context *context,
				struct ibv_exp_qp_init_attr *attrx,
				int is_exp)
{
	struct mlx5_create_qp		cmd;
	struct mlx5_create_qp_resp	resp;
	struct mlx5_exp_create_qp	cmdx;
	struct mlx5_exp_create_qp_resp	respx;
	struct mlx5_qp		       *qp;
	int				ret;
	struct mlx5_context	       *ctx = to_mctx(context);
	struct ibv_qp		       *ibqp;
	struct mlx5_drv_create_qp      *drv;
	int				lib_cmd_size;
	int				drv_cmd_size;
	int				lib_resp_size;
	int				drv_resp_size;
	void			      *_cmd;
	void			      *_resp;

#ifdef MLX5_DEBUG
	FILE *fp = ctx->dbg_fp;
#endif

	update_caps(context);
	qp = calloc(1, sizeof(*qp));
	if (!qp) {
		mlx5_dbg(fp, MLX5_DBG_QP, "\n");
		return NULL;
	}
	ibqp = (struct ibv_qp *)&qp->verbs_qp;

	if (is_exp) {
		memset(&cmdx, 0, sizeof(cmdx));
		drv = &cmdx.drv;
		_cmd = &cmdx.ibv_cmd;
		_resp = &respx.ibv_resp;
		lib_cmd_size = sizeof(cmdx.ibv_cmd);
		drv_cmd_size = sizeof(*drv);
		lib_resp_size = sizeof(respx.ibv_resp);
		drv_resp_size = sizeof(respx) - sizeof(respx.ibv_resp);
	} else {
		memset(&cmd, 0, sizeof(cmd));
		drv = &cmd.drv;
		_cmd = &cmd.ibv_cmd;
		_resp = &resp.ibv_resp;
		lib_cmd_size = sizeof(cmd.ibv_cmd);
		drv_cmd_size = sizeof(*drv);
		lib_resp_size = sizeof(resp.ibv_resp);
		drv_resp_size = sizeof(resp) - sizeof(resp.ibv_resp);
	}

	qp->wq_sig = qp_sig_enabled();
	if (qp->wq_sig)
		drv->flags |= MLX5_QP_FLAG_SIGNATURE;

	if ((ctx->info.exp_atomic_cap == IBV_EXP_ATOMIC_HCA_REPLY_BE) &&
	    (attrx->exp_create_flags & IBV_EXP_QP_CREATE_ATOMIC_BE_REPLY)) {
			qp->enable_atomics = 1;
	} else if ((ctx->info.exp_atomic_cap == IBV_EXP_ATOMIC_HCA) ||
		   (ctx->info.exp_atomic_cap == IBV_EXP_ATOMIC_GLOB)) {
		qp->enable_atomics = 1;
	}

	if ((attrx->comp_mask & IBV_EXP_QP_INIT_ATTR_MAX_INL_KLMS) &&
	    (!(attrx->comp_mask & IBV_EXP_QP_INIT_ATTR_CREATE_FLAGS) ||
	     !(attrx->exp_create_flags & IBV_EXP_QP_CREATE_UMR))) {
		errno = EINVAL;
		goto err;
	}

	if ((attrx->comp_mask & IBV_EXP_QP_INIT_ATTR_CREATE_FLAGS) &&
	    (attrx->exp_create_flags & IBV_EXP_QP_CREATE_UMR) &&
	    !(attrx->comp_mask & IBV_EXP_QP_INIT_ATTR_MAX_INL_KLMS)) {
		errno = EINVAL;
		goto err;
	}

	if ((attrx->comp_mask & IBV_EXP_QP_INIT_ATTR_CREATE_FLAGS) &&
	    (attrx->exp_create_flags & IBV_EXP_QP_CREATE_UMR))
		qp->umr_en = 1;

	if (attrx->cap.max_send_sge > ctx->max_sge) {
		errno = EINVAL;
		goto err;
	}

	if (qp->umr_en && (attrx->max_inl_send_klms >
			   ctx->max_send_wqe_inline_klms)) {
		errno = EINVAL;
		goto err;
	}

	ret = mlx5_calc_wq_size(ctx, attrx, qp);
	if (ret < 0) {
		errno = -ret;
		goto err;
	}
	qp->buf_size = ret;

	if (attrx->comp_mask & IBV_EXP_QP_INIT_ATTR_CREATE_FLAGS)
		qp->create_flags = attrx->exp_create_flags & IBV_EXP_QP_CREATE_MASK;

	if (mlx5_alloc_qp_buf(context, &attrx->cap, qp, ret)) {
		mlx5_dbg(fp, MLX5_DBG_QP, "\n");
		goto err;
	}

	qp->sq.qend = qp->buf.buf + qp->sq.offset +
		(qp->sq.wqe_cnt << qp->sq.wqe_shift);
	mlx5_init_qp_indices(qp);

	if (mlx5_spinlock_init(&qp->sq.lock) ||
	    mlx5_spinlock_init(&qp->rq.lock))
		goto err_free_qp_buf;

	qp->db = mlx5_alloc_dbrec(ctx);
	if (!qp->db) {
		mlx5_dbg(fp, MLX5_DBG_QP, "\n");
		goto err_free_qp_buf;
	}

	qp->db[MLX5_RCV_DBR] = 0;
	qp->db[MLX5_SND_DBR] = 0;

	drv->buf_addr = (uintptr_t) qp->buf.buf;
	drv->db_addr  = (uintptr_t) qp->db;
	drv->sq_wqe_count = qp->sq.wqe_cnt;
	drv->rq_wqe_count = qp->rq.wqe_cnt;
	drv->rq_wqe_shift = qp->rq.wqe_shift;

	pthread_mutex_lock(&ctx->rsc_table_mutex);

	ret = ibv_exp_cmd_create_qp(context, &qp->verbs_qp,
				    sizeof(qp->verbs_qp),
				    attrx,
				    _cmd,
				    lib_cmd_size,
				    drv_cmd_size,
				    _resp,
				    lib_resp_size,
				    drv_resp_size);
	if (ret) {
		mlx5_dbg(fp, MLX5_DBG_QP, "ret %d\n", ret);
		goto err_rq_db;
	}

	ret = mlx5_store_rsc(ctx, ibqp->qp_num, qp);
	if (ret) {
		mlx5_dbg(fp, MLX5_DBG_QP, "ret %d\n", ret);
		goto err_destroy;
	}

	pthread_mutex_unlock(&ctx->rsc_table_mutex);

	if (is_exp)
		map_uuar(context, qp, respx.uuar_index);
	else
		map_uuar(context, qp, resp.uuar_index);


	qp->rq.max_post = qp->rq.wqe_cnt;
	if (attrx->sq_sig_all)
		qp->sq_signal_bits = MLX5_WQE_CTRL_CQ_UPDATE;
	else
		qp->sq_signal_bits = 0;

	attrx->cap.max_send_wr = qp->sq.max_post;
	attrx->cap.max_recv_wr = qp->rq.max_post;
	attrx->cap.max_recv_sge = qp->rq.max_gs;
	qp->rsc.type = MLX5_RSC_TYPE_QP;
	qp->rsc.rsn = ibqp->qp_num;

	return ibqp;

err_destroy:
	ibv_cmd_destroy_qp(ibqp);

err_rq_db:
	pthread_mutex_unlock(&to_mctx(context)->rsc_table_mutex);
	mlx5_free_db(to_mctx(context), qp->db);

err_free_qp_buf:
	mlx5_free_qp_buf(qp);

err:
	free(qp);

	return NULL;
}

struct ibv_qp *mlx5_drv_create_qp(struct ibv_context *context,
				  struct ibv_qp_init_attr_ex *attrx)
{
	if (attrx->comp_mask >= IBV_QP_INIT_ATTR_RESERVED) {
		errno = EINVAL;
		return NULL;
	}

	return create_qp(context, (struct ibv_exp_qp_init_attr *)attrx, 1);
}

struct ibv_qp *mlx5_exp_create_qp(struct ibv_context *context,
				  struct ibv_exp_qp_init_attr *attrx)
{
	return create_qp(context, attrx, 1);
}

struct ibv_qp *mlx5_create_qp(struct ibv_pd *pd,
			      struct ibv_qp_init_attr *attr)
{
	struct ibv_exp_qp_init_attr attrx;
	struct ibv_qp *qp;
	int copy_sz = offsetof(struct ibv_qp_init_attr, xrc_domain);

	memset(&attrx, 0, sizeof(attrx));
	memcpy(&attrx, attr, copy_sz);
	attrx.comp_mask = IBV_QP_INIT_ATTR_PD;
	attrx.pd = pd;
	qp = create_qp(pd->context, &attrx, 0);
	if (qp)
		memcpy(attr, &attrx, copy_sz);

	return qp;
}

struct ibv_exp_dct *mlx5_create_dct(struct ibv_context *context,
				    struct ibv_exp_dct_init_attr *attr)
{
	struct mlx5_create_dct		cmd;
	struct mlx5_create_dct_resp	resp;
	struct mlx5_destroy_dct		cmdd;
	struct mlx5_destroy_dct_resp	respd;
	int				err;
	struct mlx5_dct			*dct;
	struct mlx5_context		*ctx  = to_mctx(context);

	memset(&cmd, 0, sizeof(cmd));
	memset(&cmdd, 0, sizeof(cmdd));
	memset(&resp, 0, sizeof(resp));
	dct = calloc(1, sizeof(*dct));
	if (!dct)
		return NULL;

	pthread_mutex_lock(&ctx->rsc_table_mutex);
	err = ibv_exp_cmd_create_dct(context, &dct->ibdct, attr, &cmd.ibv_cmd,
				     sizeof(cmd.ibv_cmd),
				     sizeof(cmd) - sizeof(cmd.ibv_cmd),
				     &resp.ibv_resp, sizeof(resp.ibv_resp),
				     sizeof(resp) - sizeof(resp.ibv_resp));
	if (err)
		goto ex_err;

	dct->ibdct.handle = resp.ibv_resp.dct_handle;
	dct->ibdct.dct_num = resp.ibv_resp.dct_num;
	dct->ibdct.pd = attr->pd;
	dct->ibdct.cq = attr->cq;
	dct->ibdct.srq = attr->srq;
	err = mlx5_store_rsc(ctx, dct->ibdct.dct_num, dct);
	if (err)
		goto err_destroy;

	dct->rsc.type = MLX5_RSC_TYPE_DCT;
	dct->rsc.rsn = resp.ibv_resp.dct_num;
	pthread_mutex_unlock(&ctx->rsc_table_mutex);

	return &dct->ibdct;

err_destroy:
	if (ibv_exp_cmd_destroy_dct(context, &dct->ibdct,
				    &cmdd.ibv_cmd,
				    sizeof(cmdd.ibv_cmd),
				    sizeof(cmdd) - sizeof(cmdd.ibv_cmd),
				    &respd.ibv_resp, sizeof(respd.ibv_resp),
				    sizeof(respd) - sizeof(respd.ibv_resp)))
		fprintf(stderr, "failed to destory DCT\n");

ex_err:
	pthread_mutex_unlock(&ctx->rsc_table_mutex);
	free(dct);
	return NULL;
}

int mlx5_destroy_dct(struct ibv_exp_dct *dct)
{
	struct mlx5_destroy_dct		cmd;
	struct mlx5_destroy_dct_resp	resp;
	int				err;
	struct mlx5_dct		       *mdct = to_mdct(dct);
	struct mlx5_context	       *ctx = to_mctx(dct->context);


	memset(&cmd, 0, sizeof(cmd));
	pthread_mutex_lock(&ctx->rsc_table_mutex);
	cmd.ibv_cmd.dct_handle = dct->handle;
	err = ibv_exp_cmd_destroy_dct(dct->context, dct,
				      &cmd.ibv_cmd,
				      sizeof(cmd.ibv_cmd),
				      sizeof(cmd) - sizeof(cmd.ibv_cmd),
				      &resp.ibv_resp, sizeof(resp.ibv_resp),
				      sizeof(resp) - sizeof(resp.ibv_resp));
	if (err)
		goto ex_err;

	mlx5_cq_clean(to_mcq(dct->cq), dct->dct_num, to_msrq(dct->srq));
	mlx5_clear_rsc(to_mctx(dct->context), dct->dct_num);
	pthread_mutex_unlock(&ctx->rsc_table_mutex);

	free(mdct);
	return 0;

ex_err:
	pthread_mutex_unlock(&ctx->rsc_table_mutex);
	return err;
}

int mlx5_query_dct(struct ibv_exp_dct *dct, struct ibv_exp_dct_attr *attr)
{
	struct mlx5_query_dct		cmd;
	struct mlx5_query_dct_resp	resp;
	int				err;

	cmd.ibv_cmd.dct_handle = dct->handle;
	err = ibv_exp_cmd_query_dct(dct->context, &cmd.ibv_cmd,
				    sizeof(cmd.ibv_cmd),
				    sizeof(cmd) - sizeof(cmd.ibv_cmd),
				    &resp.ibv_resp, sizeof(resp.ibv_resp),
				    sizeof(resp) - sizeof(resp.ibv_resp),
				    attr);
	if (err)
		goto out;

	attr->cq = dct->cq;
	attr->pd = dct->pd;
	attr->srq = dct->srq;

out:
	return err;
}

int mlx5_arm_dct(struct ibv_exp_dct *dct, struct ibv_exp_arm_attr *attr)
{
	struct mlx5_arm_dct		cmd;
	struct mlx5_arm_dct_resp	resp;
	int				err;

	memset(&cmd, 0, sizeof(cmd));
	memset(&resp, 0, sizeof(resp));
	cmd.ibv_cmd.dct_handle = dct->handle;
	err = ibv_exp_cmd_arm_dct(dct->context, attr, &cmd.ibv_cmd,
				  sizeof(cmd.ibv_cmd),
				  sizeof(cmd) - sizeof(cmd.ibv_cmd),
				  &resp.ibv_resp, sizeof(resp.ibv_resp),
				  sizeof(resp) - sizeof(resp.ibv_resp));
	return err;
}

static void mlx5_lock_cqs(struct ibv_qp *qp)
{
	struct mlx5_cq *send_cq = to_mcq(qp->send_cq);
	struct mlx5_cq *recv_cq = to_mcq(qp->recv_cq);

	if (send_cq && recv_cq) {
		if (send_cq == recv_cq) {
			mlx5_spin_lock(&send_cq->lock);
		} else if (send_cq->cqn < recv_cq->cqn) {
			mlx5_spin_lock(&send_cq->lock);
			mlx5_spin_lock(&recv_cq->lock);
		} else {
			mlx5_spin_lock(&recv_cq->lock);
			mlx5_spin_lock(&send_cq->lock);
		}
	} else if (send_cq) {
		mlx5_spin_lock(&send_cq->lock);
	} else if (recv_cq) {
		mlx5_spin_lock(&recv_cq->lock);
	}
}

static void mlx5_unlock_cqs(struct ibv_qp *qp)
{
	struct mlx5_cq *send_cq = to_mcq(qp->send_cq);
	struct mlx5_cq *recv_cq = to_mcq(qp->recv_cq);

	if (send_cq && recv_cq) {
		if (send_cq == recv_cq) {
			mlx5_spin_unlock(&send_cq->lock);
		} else if (send_cq->cqn < recv_cq->cqn) {
			mlx5_spin_unlock(&recv_cq->lock);
			mlx5_spin_unlock(&send_cq->lock);
		} else {
			mlx5_spin_unlock(&send_cq->lock);
			mlx5_spin_unlock(&recv_cq->lock);
		}
	} else if (send_cq) {
		mlx5_spin_unlock(&send_cq->lock);
	} else if (recv_cq) {
		mlx5_spin_unlock(&recv_cq->lock);
	}
}

int mlx5_destroy_qp(struct ibv_qp *ibqp)
{
	struct mlx5_qp *qp = to_mqp(ibqp);
	int ret;

	pthread_mutex_lock(&to_mctx(ibqp->context)->rsc_table_mutex);
	ret = ibv_cmd_destroy_qp(ibqp);
	if (ret) {
		pthread_mutex_unlock(&to_mctx(ibqp->context)->rsc_table_mutex);
		return ret;
	}

	mlx5_lock_cqs(ibqp);

	__mlx5_cq_clean(to_mcq(ibqp->recv_cq), ibqp->qp_num,
			ibqp->srq ? to_msrq(ibqp->srq) : NULL);
	if (ibqp->send_cq != ibqp->recv_cq)
		__mlx5_cq_clean(to_mcq(ibqp->send_cq), ibqp->qp_num, NULL);

	mlx5_clear_rsc(to_mctx(ibqp->context), ibqp->qp_num);

	mlx5_unlock_cqs(ibqp);
	pthread_mutex_unlock(&to_mctx(ibqp->context)->rsc_table_mutex);

	mlx5_free_db(to_mctx(ibqp->context), qp->db);
	mlx5_free_qp_buf(qp);
	free(qp);

	return 0;
}

int mlx5_query_qp(struct ibv_qp *ibqp, struct ibv_qp_attr *attr,
		  int attr_mask, struct ibv_qp_init_attr *init_attr)
{
	struct ibv_query_qp cmd;
	struct mlx5_qp *qp = to_mqp(ibqp);
	int ret;

	ret = ibv_cmd_query_qp(ibqp, attr, attr_mask, init_attr, &cmd, sizeof(cmd));
	if (ret)
		return ret;

	init_attr->cap.max_send_wr     = qp->sq.max_post;
	init_attr->cap.max_send_sge    = qp->sq.max_gs;
	init_attr->cap.max_inline_data = qp->max_inline_data;

	attr->cap = init_attr->cap;

	return 0;
}

int mlx5_modify_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr,
		   int attr_mask)
{
	struct ibv_modify_qp cmd;
	int ret;
	uint32_t *db;

	ret = ibv_cmd_modify_qp(qp, attr, attr_mask, &cmd, sizeof(cmd));

	if (!ret		       &&
	    (attr_mask & IBV_QP_STATE) &&
	    attr->qp_state == IBV_QPS_RESET) {
		if (qp->recv_cq) {
			mlx5_cq_clean(to_mcq(qp->recv_cq), qp->qp_num,
				      qp->srq ? to_msrq(qp->srq) : NULL);
		}
		if (qp->send_cq != qp->recv_cq && qp->send_cq)
			mlx5_cq_clean(to_mcq(qp->send_cq), qp->qp_num, NULL);

		mlx5_init_qp_indices(to_mqp(qp));
		db = to_mqp(qp)->db;
		db[MLX5_RCV_DBR] = 0;
		db[MLX5_SND_DBR] = 0;
	}

	return ret;
}

struct ibv_ah *mlx5_create_ah(struct ibv_pd *pd, struct ibv_ah_attr *attr)
{
	struct mlx5_ah *ah;
	uint32_t tmp;
	struct mlx5_context *ctx = to_mctx(pd->context);

	if (unlikely(attr->port_num < 1 || attr->port_num > ctx->num_ports)) {
		errno = EINVAL;
		return NULL;
	}

	if (unlikely(!attr->dlid)) {
		errno = EINVAL;
		return NULL;
	}

	ah = calloc(1, sizeof *ah);
	if (unlikely(!ah)) {
		errno = ENOMEM;
		return NULL;
	}

	ah->av.stat_rate_sl = (attr->static_rate << 4) | attr->sl;
	ah->av.fl_mlid = attr->src_path_bits & 0x7f;
	ah->av.rlid = htons(attr->dlid);
	if (attr->is_global) {
		ah->av.tclass = attr->grh.traffic_class;
		ah->av.hop_limit = attr->grh.hop_limit;
		tmp = htonl((1 << 30) |
			    ((attr->grh.sgid_index & 0xff) << 20) |
			    (attr->grh.flow_label & 0xfffff));
		ah->av.grh_gid_fl = tmp;
		memcpy(ah->av.rgid, attr->grh.dgid.raw, 16);
	}

	return &ah->ibv_ah;
}

int mlx5_destroy_ah(struct ibv_ah *ah)
{
	free(to_mah(ah));

	return 0;
}

int mlx5_attach_mcast(struct ibv_qp *qp, const union ibv_gid *gid, uint16_t lid)
{
	return ibv_cmd_attach_mcast(qp, gid, lid);
}

int mlx5_detach_mcast(struct ibv_qp *qp, const union ibv_gid *gid, uint16_t lid)
{
	return ibv_cmd_detach_mcast(qp, gid, lid);
}

struct ibv_xrcd	*mlx5_open_xrcd(struct ibv_context *context,
				struct ibv_xrcd_init_attr *xrcd_init_attr)
{
	int err;
	struct verbs_xrcd *xrcd;
	struct ibv_open_xrcd cmd = {0};
	struct ibv_open_xrcd_resp resp = {0};

	xrcd = calloc(1, sizeof(*xrcd));
	if (!xrcd)
		return NULL;

	err = ibv_cmd_open_xrcd(context, xrcd, sizeof(*xrcd), xrcd_init_attr,
				&cmd, sizeof(cmd), &resp, sizeof(resp));
	if (err) {
		free(xrcd);
		return NULL;
	}

	return &xrcd->xrcd;
}

struct ibv_srq *mlx5_create_xrc_srq(struct ibv_context *context,
				    struct ibv_srq_init_attr_ex *attr)
{
	int err;
	struct mlx5_create_srq_ex cmd;
	struct mlx5_create_srq_resp resp;
	struct mlx5_srq	*msrq;
	struct mlx5_context *ctx;
	int max_sge;
	struct ibv_srq *ibsrq;

	msrq = calloc(1, sizeof(*msrq));
	if (!msrq)
		return NULL;

	msrq->is_xsrq = 1;
	ibsrq = (struct ibv_srq *)&msrq->vsrq;

	memset(&cmd, 0, sizeof(cmd));
	memset(&resp, 0, sizeof(resp));

	ctx = to_mctx(context);

	if (mlx5_spinlock_init(&msrq->lock)) {
		fprintf(stderr, "%s-%d:\n", __func__, __LINE__);
		goto err;
	}

	if (attr->attr.max_wr > ctx->max_srq_recv_wr) {
		fprintf(stderr, "%s-%d:max_wr %d, max_srq_recv_wr %d\n",
			__func__, __LINE__, attr->attr.max_wr,
			ctx->max_srq_recv_wr);
		errno = EINVAL;
		goto err;
	}

	/*
	 * this calculation does not consider required control segments. The
	 * final calculation is done again later. This is done so to avoid
	 * overflows of variables
	 */
	max_sge = ctx->max_recv_wr / sizeof(struct mlx5_wqe_data_seg);
	if (attr->attr.max_sge > max_sge) {
		fprintf(stderr, "%s-%d:max_wr %d, max_srq_recv_wr %d\n",
			__func__, __LINE__, attr->attr.max_wr,
			ctx->max_srq_recv_wr);
		errno = EINVAL;
		goto err;
	}

	msrq->max     = align_queue_size(attr->attr.max_wr + 1);
	msrq->max_gs  = attr->attr.max_sge;
	msrq->counter = 0;

	if (mlx5_alloc_srq_buf(context, msrq)) {
		fprintf(stderr, "%s-%d:\n", __func__, __LINE__);
		goto err;
	}

	msrq->db = mlx5_alloc_dbrec(ctx);
	if (!msrq->db) {
		fprintf(stderr, "%s-%d:\n", __func__, __LINE__);
		goto err_free;
	}

	*msrq->db = 0;

	cmd.buf_addr = (uintptr_t) msrq->buf.buf;
	cmd.db_addr  = (uintptr_t) msrq->db;
	msrq->wq_sig = srq_sig_enabled();
	if (msrq->wq_sig)
		cmd.flags = MLX5_SRQ_FLAG_SIGNATURE;

	attr->attr.max_sge = msrq->max_gs;
	pthread_mutex_lock(&ctx->srq_table_mutex);
	err = ibv_cmd_create_srq_ex(context, &msrq->vsrq, sizeof(msrq->vsrq),
				    attr, &cmd.ibv_cmd, sizeof(cmd),
				    &resp.ibv_resp, sizeof(resp));
	if (err)
		goto err_free_db;

	err = mlx5_store_srq(to_mctx(context), resp.srqn, msrq);
	if (err)
		goto err_destroy;

	pthread_mutex_unlock(&ctx->srq_table_mutex);

	msrq->srqn = resp.srqn;

	return ibsrq;

err_destroy:
	ibv_cmd_destroy_srq(ibsrq);

err_free_db:
	pthread_mutex_unlock(&ctx->srq_table_mutex);
	mlx5_free_db(ctx, msrq->db);

err_free:
	free(msrq->wrid);
	mlx5_free_buf(&msrq->buf);

err:
	free(msrq);

	return NULL;
}
struct ibv_srq *mlx5_create_srq_ex(struct ibv_context *context,
				   struct ibv_srq_init_attr_ex *attr)
{
	if (!(attr->comp_mask & IBV_SRQ_INIT_ATTR_TYPE) ||
	    (attr->srq_type == IBV_SRQT_BASIC))
		return mlx5_create_srq(attr->pd,
				       (struct ibv_srq_init_attr *)attr);
	else if (attr->srq_type == IBV_SRQT_XRC)
		return mlx5_create_xrc_srq(context, attr);

	return NULL;
}

int mlx5_get_srq_num(struct ibv_srq *srq, uint32_t *srq_num)
{
	struct mlx5_srq	*msrq = to_msrq(srq);

	*srq_num = msrq->srqn;

	return 0;
}

struct ibv_qp *mlx5_open_qp(struct ibv_context *context,
			    struct ibv_qp_open_attr *attr)
{
	struct ibv_open_qp cmd;
	struct ibv_create_qp_resp resp;
	struct mlx5_qp *qp;
	int ret;
	struct mlx5_context *ctx = to_mctx(context);

	qp = calloc(1, sizeof(*qp));
	if (!qp)
		return NULL;

	ret = ibv_cmd_open_qp(context, &qp->verbs_qp, sizeof(qp->verbs_qp),
			      attr, &cmd, sizeof(cmd), &resp, sizeof(resp));
	if (ret)
		goto err;

	pthread_mutex_lock(&ctx->rsc_table_mutex);
	if (mlx5_store_rsc(to_mctx(context), qp->verbs_qp.qp.qp_num, qp)) {
		pthread_mutex_unlock(&ctx->rsc_table_mutex);
		goto destroy;
	}
	pthread_mutex_unlock(&ctx->rsc_table_mutex);

	return (struct ibv_qp *)&qp->verbs_qp;

destroy:
	ibv_cmd_destroy_qp(&qp->verbs_qp.qp);
err:
	free(qp);
	return NULL;
}

int mlx5_close_xrcd(struct ibv_xrcd *ib_xrcd)
{
	struct verbs_xrcd *xrcd = container_of(ib_xrcd, struct verbs_xrcd, xrcd);
	int ret;

	ret = ibv_cmd_close_xrcd(xrcd);
	if (!ret)
		free(xrcd);

	return ret;
}

int mlx5_modify_qp_ex(struct ibv_qp *qp, struct ibv_exp_qp_attr *attr,
		      uint64_t attr_mask)
{
	struct ibv_exp_modify_qp cmd;
	int ret;
	uint32_t *db;

	memset(&cmd, 0, sizeof(cmd));
	ret = ibv_exp_cmd_modify_qp(qp, attr, attr_mask, &cmd, sizeof(cmd));

	if (!ret		       &&
	    (attr_mask & IBV_QP_STATE) &&
	    attr->qp_state == IBV_QPS_RESET) {
		if (qp->qp_type != IBV_EXP_QPT_DC_INI)
			mlx5_cq_clean(to_mcq(qp->recv_cq), qp->qp_num,
				      qp->srq ? to_msrq(qp->srq) : NULL);

		if (qp->send_cq != qp->recv_cq)
			mlx5_cq_clean(to_mcq(qp->send_cq), qp->qp_num, NULL);

		mlx5_init_qp_indices(to_mqp(qp));
		db = to_mqp(qp)->db;
		db[MLX5_RCV_DBR] = 0;
		db[MLX5_SND_DBR] = 0;
	}

	return ret;
}

void *mlx5_get_legacy_xrc(struct ibv_srq *srq)
{
	struct mlx5_srq	*msrq = to_msrq(srq);

	return msrq->ibv_srq_legacy;
}

void mlx5_set_legacy_xrc(struct ibv_srq *srq, void *legacy_xrc_srq)
{
	struct mlx5_srq	*msrq = to_msrq(srq);

	msrq->ibv_srq_legacy = legacy_xrc_srq;
	return;
}

int mlx5_modify_cq(struct ibv_cq *cq, struct ibv_exp_cq_attr *attr, int attr_mask)
{
	struct ibv_exp_modify_cq cmd;

	return ibv_exp_cmd_modify_cq(cq, attr, attr_mask, &cmd, sizeof(cmd));
}

struct ibv_exp_mkey_list_container *mlx5_alloc_mkey_mem(struct ibv_exp_mkey_list_container_attr *attr)
{
	struct mlx5_klm_buf *klm;
	int size;

	if (attr->mkey_list_type !=
			IBV_EXP_MKEY_LIST_TYPE_INDIRECT_MR) {
		errno = ENOMEM;
		return NULL;
	}

	klm = calloc(1, sizeof(*klm));
	if (!klm) {
		errno = ENOMEM;
		return NULL;
	}

	size = align(attr->max_klm_list_size * sizeof(struct mlx5_wqe_data_seg), 64);

	klm->alloc_buf = malloc(size + MLX5_UMR_PTR_ALIGN - 1);
	if (!klm->alloc_buf) {
		errno = ENOMEM;
		goto ex_klm;
	}

	klm->align_buf = align_ptr(klm->alloc_buf, MLX5_UMR_PTR_ALIGN);

	memset(klm->align_buf, 0, size);
	klm->mr = ibv_reg_mr(attr->pd, klm->align_buf, size, 0);
	if (!klm->mr)
		goto ex_list;

	klm->ibv_klm_list.max_klm_list_size = attr->max_klm_list_size;
	klm->ibv_klm_list.context = klm->mr->context;

	return &klm->ibv_klm_list;

ex_list:
	free(klm->alloc_buf);
ex_klm:
	free(klm);
	return NULL;
}

int mlx5_free_mkey_mem(struct ibv_exp_mkey_list_container *mem)
{
	struct mlx5_klm_buf *klm;
	int err;

	klm = to_klm(mem);
	err = ibv_dereg_mr(klm->mr);
	if (err) {
		fprintf(stderr, "unreg klm failed\n");
		return err;
	}
	free(klm->alloc_buf);
	free(klm);
	return 0;
}

int mlx5_query_mkey(struct ibv_mr *mr, struct ibv_exp_mkey_attr *mkey_attr)
{
	struct mlx5_query_mkey		cmd;
	struct mlx5_query_mkey_resp	resp;
	int				err;

	memset(&cmd, 0, sizeof(cmd));
	err = ibv_exp_cmd_query_mkey(mr->context, mr, mkey_attr, &cmd.ibv_cmd,
				     sizeof(cmd.ibv_cmd), sizeof(cmd),
				     &resp.ibv_resp, sizeof(resp.ibv_resp),
				     sizeof(resp));

	return err;
};

struct ibv_mr *mlx5_create_mr(struct ibv_exp_create_mr_in *in)
{
	struct mlx5_create_mr		cmd;
	struct mlx5_create_mr_resp	resp;
	struct mlx5_mr		       *mr;
	int				err;

	memset(&cmd, 0, sizeof(cmd));
	memset(&resp, 0, sizeof(resp));

	mr = calloc(1, sizeof(*mr));
	if (!mr)
		return NULL;

	err = ibv_exp_cmd_create_mr(in, &mr->ibv_mr, &cmd.ibv_cmd,
				    sizeof(cmd.ibv_cmd),
				    sizeof(cmd) - sizeof(cmd.ibv_cmd),
				    &resp.ibv_resp,
				    sizeof(resp.ibv_resp), sizeof(resp) - sizeof(resp.ibv_resp));
	if (err)
		goto out;

	return &mr->ibv_mr;

out:
	free(mr);
	return NULL;
};

int mlx5_exp_dereg_mr(struct ibv_mr *ibmr, struct ibv_exp_dereg_out *out)
{
	struct mlx5_mr *mr;

	if (ibmr->lkey == ODP_GLOBAL_R_LKEY || ibmr->lkey == ODP_GLOBAL_W_LKEY) {
		out->need_dofork = 0;
	} else {
		mr = to_mmr(ibmr);
		out->need_dofork = (mr->buf.type == MLX5_ALLOC_TYPE_CONTIG ||
				    mr->type == MLX5_ODP_MR) ? 0 : 1;
	}

	return mlx5_dereg_mr(ibmr);
}
