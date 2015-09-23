/*
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
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
/* Added for reg_mr mmap munmap system calls */
#include <sys/mman.h>
#include "mlx4.h"
#include "mlx4-abi.h"
#include "mlx4_exp.h"
#include "wqe.h"

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

int mlx4_exp_modify_qp(struct ibv_qp *qp, struct ibv_exp_qp_attr *attr,
		       uint64_t attr_mask)
{
	struct ibv_exp_modify_qp cmd;
	struct ibv_port_attr port_attr;
	struct mlx4_qp *mqp = to_mqp(qp);
	int ret;

	memset(&cmd, 0, sizeof(cmd));

	if (attr_mask & IBV_EXP_QP_PORT) {
		ret = ibv_query_port(qp->context, attr->port_num,
				     &port_attr);
		if (ret)
			return ret;
		mqp->link_layer = port_attr.link_layer;
	}

	if (qp->state == IBV_QPS_RESET &&
	    (attr_mask & IBV_EXP_QP_STATE) &&
	    attr->qp_state == IBV_QPS_INIT) {
		mlx4_qp_init_sq_ownership(to_mqp(qp));
	}


	ret = ibv_exp_cmd_modify_qp(qp, attr, attr_mask, &cmd, sizeof(cmd));

	if (!ret		       &&
	    (attr_mask & IBV_EXP_QP_STATE) &&
	    attr->qp_state == IBV_QPS_RESET) {
		if (qp->recv_cq)
			mlx4_cq_clean(to_mcq(qp->recv_cq), qp->qp_num,
				      qp->srq ? to_msrq(qp->srq) : NULL);
		if (qp->send_cq && qp->send_cq != qp->recv_cq)
			mlx4_cq_clean(to_mcq(qp->send_cq), qp->qp_num, NULL);

		mlx4_init_qp_indices(to_mqp(qp));
		if (to_mqp(qp)->rq.wqe_cnt)
			*to_mqp(qp)->db = 0;
	}

	return ret;
}

static int verify_sizes(struct ibv_exp_qp_init_attr *attr, struct mlx4_context *context)
{
	int size;
	int nsegs;

	if (attr->cap.max_send_wr     > context->max_qp_wr ||
	    attr->cap.max_recv_wr     > context->max_qp_wr ||
	    attr->cap.max_send_sge    > context->max_sge   ||
	    attr->cap.max_recv_sge    > context->max_sge)
		return -1;

	if (attr->cap.max_inline_data) {
		nsegs = num_inline_segs(attr->cap.max_inline_data, attr->qp_type);
		size = MLX4_MAX_WQE_SIZE - nsegs * sizeof(struct mlx4_wqe_inline_seg);
		switch (attr->qp_type) {
		case IBV_QPT_UD:
			size -= (sizeof(struct mlx4_wqe_ctrl_seg) +
				 sizeof(struct mlx4_wqe_datagram_seg));
			break;

		case IBV_QPT_RC:
		case IBV_QPT_UC:
			size -= (sizeof(struct mlx4_wqe_ctrl_seg) +
				 sizeof(struct mlx4_wqe_raddr_seg));
			break;

		default:
			return 0;
		}

		if (attr->cap.max_inline_data > size)
			return -1;
	}

	return 0;
}

static int mlx4_exp_alloc_qp_buf(struct ibv_context *context,
				 struct ibv_exp_qp_init_attr *attr,
				 struct mlx4_qp *qp)
{
	int ret;
	enum mlx4_alloc_type alloc_type;
	enum mlx4_alloc_type default_alloc_type = MLX4_ALLOC_TYPE_PREFER_CONTIG;
	const char *qp_huge_key;
	int i, wqe_size;

	qp->rq.max_gs = attr->cap.max_recv_sge;
	wqe_size = qp->rq.max_gs * sizeof(struct mlx4_wqe_data_seg);
	if ((attr->comp_mask & IBV_EXP_QP_INIT_ATTR_INL_RECV) && (attr->max_inl_recv)) {
		qp->max_inlr_sg = qp->rq.max_gs;
		wqe_size = max(wqe_size, attr->max_inl_recv);
	}
	for (qp->rq.wqe_shift = 4; 1 << qp->rq.wqe_shift < wqe_size; qp->rq.wqe_shift++)
		; /* nothing */

	if (qp->max_inlr_sg) {
		attr->max_inl_recv = 1 << qp->rq.wqe_shift;
		qp->max_inlr_sg = attr->max_inl_recv / sizeof(struct mlx4_wqe_data_seg);
	}

	if (qp->sq.wqe_cnt) {
		qp->sq.wrid = malloc(qp->sq.wqe_cnt * sizeof(uint64_t));
		if (!qp->sq.wrid)
			return -1;
	}

	if (qp->rq.wqe_cnt) {
		qp->rq.wrid = malloc(qp->rq.wqe_cnt * sizeof(uint64_t));
		if (!qp->rq.wrid) {
			free(qp->sq.wrid);
			return -1;
		}

		if (qp->max_inlr_sg) {
			qp->inlr_buff.buff = malloc(qp->rq.wqe_cnt * sizeof(*(qp->inlr_buff.buff)));
			if (!qp->inlr_buff.buff) {
				free(qp->sq.wrid);
				free(qp->rq.wrid);
				return -1;
			}
			qp->inlr_buff.len = qp->rq.wqe_cnt;
			qp->inlr_buff.buff[0].sg_list = malloc(qp->rq.wqe_cnt *
							       sizeof(*(qp->inlr_buff.buff->sg_list)) *
							       qp->max_inlr_sg);
			if (!qp->inlr_buff.buff->sg_list) {
				free(qp->sq.wrid);
				free(qp->rq.wrid);
				free(qp->inlr_buff.buff);
				return -1;
			}
			for (i = 1; i < qp->rq.wqe_cnt; i++)
				qp->inlr_buff.buff[i].sg_list = &qp->inlr_buff.buff[0].sg_list[i * qp->max_inlr_sg];
		}
	}

	qp->buf_size = (qp->rq.wqe_cnt << qp->rq.wqe_shift) +
		(qp->sq.wqe_cnt << qp->sq.wqe_shift);
	if (qp->rq.wqe_shift > qp->sq.wqe_shift) {
		qp->rq.offset = 0;
		qp->sq.offset = qp->rq.wqe_cnt << qp->rq.wqe_shift;
	} else {
		qp->rq.offset = qp->sq.wqe_cnt << qp->sq.wqe_shift;
		qp->sq.offset = 0;
	}

	if (qp->buf_size) {
		/* compatability support */
		qp_huge_key  = qptype2key(attr->qp_type);
		if (mlx4_use_huge(qp_huge_key))
			default_alloc_type = MLX4_ALLOC_TYPE_HUGE;


		mlx4_get_alloc_type(MLX4_QP_PREFIX, &alloc_type,
				default_alloc_type);

		ret = mlx4_alloc_prefered_buf(to_mctx(context), &qp->buf,
				align(qp->buf_size, to_mdev
				(context->device)->page_size),
				to_mdev(context->device)->page_size,
				alloc_type,
				MLX4_QP_PREFIX);

		if (ret) {
			free(qp->sq.wrid);
			free(qp->rq.wrid);
			if (qp->max_inlr_sg) {
				free(qp->inlr_buff.buff[0].sg_list);
				free(qp->inlr_buff.buff);
			}
			return -1;
		}

		memset(qp->buf.buf, 0, qp->buf_size);
	} else {
		qp->buf.buf = NULL;
	}

	return 0;
}

struct ibv_qp *mlx4_exp_create_qp(struct ibv_context *context,
				  struct ibv_exp_qp_init_attr *attr)
{
	struct mlx4_qp		 *qp;
	int			  ret;
	union {
		struct mlx4_create_qp		basic;
		struct mlx4_exp_create_qp	extended;
	} cmd_obj;
	union {
		struct ibv_create_qp_resp	basic;
		struct ibv_exp_create_qp_resp	extended;
	} resp_obj;
	struct mlx4_create_qp_base *cmd = NULL;
	int ext_kernel_cmd = 0;

	memset(&resp_obj, 0, sizeof(resp_obj));
	memset(&cmd_obj, 0, sizeof(cmd_obj));

	if (attr->comp_mask >= IBV_EXP_QP_INIT_ATTR_RESERVED) {
		errno = ENOSYS;
		return NULL;
	}

	if (attr->comp_mask & IBV_EXP_QP_INIT_ATTR_INL_RECV) {
		if (attr->srq)
			attr->max_inl_recv = 0;
		else
			attr->max_inl_recv = min(attr->max_inl_recv,
						 (to_mctx(context)->max_sge *
						 sizeof(struct mlx4_wqe_data_seg)));
	}

	/* Sanity check QP size before proceeding */
	if (verify_sizes(attr, to_mctx(context)))
		return NULL;

	if (attr->qp_type == IBV_QPT_XRC && attr->recv_cq &&
		attr->cap.max_recv_wr > 0 && mlx4_trace)
		fprintf(stderr, PFX "Warning: Legacy XRC sender should not use a recieve cq\n");

	qp = calloc(1, sizeof(*qp));
	if (!qp)
		return NULL;

	if (attr->comp_mask >= IBV_EXP_QP_INIT_ATTR_CREATE_FLAGS)
		ext_kernel_cmd = 1;
	if (attr->qp_type == IBV_QPT_XRC_RECV) {
		attr->cap.max_send_wr = qp->sq.wqe_cnt = 0;
	} else {
		if (attr->comp_mask & IBV_EXP_QP_INIT_ATTR_ATOMICS_ARG &&
		    attr->max_atomic_arg != 0) {
			if (attr->max_atomic_arg == 8) {
				qp->is_masked_atomic = 1;
			} else {
				fprintf(stderr, "%s: max_atomic_arg = %d is not valid for mlx4 (use 8 or 0)\n",
					__FUNCTION__, attr->max_atomic_arg);
				errno = EINVAL;
				goto err;
			}
		}

		mlx4_calc_sq_wqe_size(&attr->cap, attr->qp_type, qp);
		/*
		 * We need to leave 2 KB + 1 WQE of headroom in the SQ to
		 * allow HW to prefetch.
		 */
		qp->sq_spare_wqes = (2048 >> qp->sq.wqe_shift) + 1;
		qp->sq.wqe_cnt = align_queue_size(attr->cap.max_send_wr + qp->sq_spare_wqes);
	}

	if (attr->srq || attr->qp_type == IBV_QPT_XRC_SEND ||
	    attr->qp_type == IBV_QPT_XRC_RECV ||
	    attr->qp_type == IBV_QPT_XRC) {
		attr->cap.max_recv_wr = qp->rq.wqe_cnt = attr->cap.max_recv_sge = 0;
		if (attr->comp_mask & IBV_EXP_QP_INIT_ATTR_INL_RECV)
			attr->max_inl_recv = 0;
	} else {
		qp->rq.wqe_cnt = align_queue_size(attr->cap.max_recv_wr);
		if (attr->cap.max_recv_sge < 1)
			attr->cap.max_recv_sge = 1;
		if (attr->cap.max_recv_wr < 1)
			attr->cap.max_recv_wr = 1;
	}

	if (attr->comp_mask & IBV_EXP_QP_INIT_ATTR_CREATE_FLAGS)
		qp->create_flags = attr->exp_create_flags & IBV_EXP_QP_CREATE_MASK;

	if (mlx4_exp_alloc_qp_buf(context, attr, qp))
		goto err;

	mlx4_init_qp_indices(qp);

	if (mlx4_spinlock_init(&qp->sq.lock) ||
	    mlx4_spinlock_init(&qp->rq.lock))
		goto err_free;

	cmd = (ext_kernel_cmd ?
			&cmd_obj.extended.base : &cmd_obj.basic.base);

	if (attr->cap.max_recv_sge) {
		qp->db = mlx4_alloc_db(to_mctx(context), MLX4_DB_TYPE_RQ);
		if (!qp->db)
			goto err_free;

		*qp->db = 0;
		cmd->db_addr = (uintptr_t) qp->db;
	} else {
		cmd->db_addr = 0;
	}

	cmd->buf_addr	    = (uintptr_t) qp->buf.buf;
	cmd->log_sq_stride   = qp->sq.wqe_shift;
	for (cmd->log_sq_bb_count = 0;
	     qp->sq.wqe_cnt > 1 << cmd->log_sq_bb_count;
	     ++cmd->log_sq_bb_count)
		; /* nothing */
	cmd->sq_no_prefetch = 0;	/* OK for ABI 2: just a reserved field */
	memset(cmd->reserved, 0, sizeof(cmd->reserved));

	pthread_mutex_lock(&to_mctx(context)->qp_table_mutex);
	ret = ibv_exp_cmd_create_qp(context, &qp->verbs_qp,
				    sizeof(qp->verbs_qp), attr,
				    ext_kernel_cmd ?
				    (void *)&cmd_obj.extended.ibv_cmd :
				    (void *)&cmd_obj.basic.ibv_cmd,
				    ext_kernel_cmd ?
				    sizeof(cmd_obj.extended.ibv_cmd) :
				    sizeof(cmd_obj.basic.ibv_cmd),
				    ext_kernel_cmd ?
				    sizeof(cmd_obj.extended.base) :
				    sizeof(cmd_obj.basic.base),
				    ext_kernel_cmd ?
				    (void *)&resp_obj.extended : (void *)&resp_obj.basic,
				    ext_kernel_cmd ?
				    sizeof(resp_obj.extended) :
				    sizeof(resp_obj.basic),
				    0);
	if (ret) {
		errno = ret;
		goto err_rq_db;
	}

	if (qp->max_inlr_sg && (attr->max_inl_recv != (1 << qp->rq.wqe_shift)))
		goto err_destroy;

	if (qp->sq.wqe_cnt || qp->rq.wqe_cnt) {
		ret = mlx4_store_qp(to_mctx(context), qp->verbs_qp.qp.qp_num, qp);
		if (ret)
			goto err_destroy;
	}
	pthread_mutex_unlock(&to_mctx(context)->qp_table_mutex);

	qp->rq.wqe_cnt = attr->cap.max_recv_wr;
	qp->rq.max_gs  = attr->cap.max_recv_sge;

	/* adjust rq maxima to not exceed reported device maxima */
	attr->cap.max_recv_wr = min(to_mctx(context)->max_qp_wr,
					attr->cap.max_recv_wr);
	attr->cap.max_recv_sge = min(to_mctx(context)->max_sge,
					attr->cap.max_recv_sge);

	qp->rq.max_post = attr->cap.max_recv_wr;
	if (attr->qp_type != IBV_QPT_XRC_RECV)
		mlx4_set_sq_sizes(qp, &attr->cap, attr->qp_type);

	qp->doorbell_qpn    = htonl(qp->verbs_qp.qp.qp_num << 8);
	if (attr->sq_sig_all)
		qp->sq_signal_bits = htonl(MLX4_WQE_CTRL_CQ_UPDATE);
	else
		qp->sq_signal_bits = 0;

	return &qp->verbs_qp.qp;

err_destroy:
	ibv_cmd_destroy_qp(&qp->verbs_qp.qp);

err_rq_db:
	pthread_mutex_unlock(&to_mctx(context)->qp_table_mutex);
	if (attr->cap.max_recv_sge)
		mlx4_free_db(to_mctx(context), MLX4_DB_TYPE_RQ, qp->db);

err_free:
	mlx4_dealloc_qp_buf(context, qp);

err:
	free(qp);

	return NULL;
}

int mlx4_exp_query_device(struct ibv_context *context,
			  struct ibv_exp_device_attr *device_attr)
{
	struct ibv_exp_query_device cmd;
	uint64_t raw_fw_ver;
	int ret;

	ret = ibv_exp_cmd_query_device(context, device_attr, &raw_fw_ver,
				       &cmd, sizeof(cmd));
	if (ret)
		return ret;


	if (device_attr->exp_device_cap_flags & IBV_EXP_DEVICE_CROSS_CHANNEL) {
		device_attr->comp_mask |= IBV_EXP_DEVICE_ATTR_CALC_CAP;
		device_attr->calc_cap.data_types = (1ULL << IBV_EXP_CALC_DATA_TYPE_INT) |
						   (1ULL << IBV_EXP_CALC_DATA_TYPE_UINT) |
						   (1ULL << IBV_EXP_CALC_DATA_TYPE_FLOAT);
		device_attr->calc_cap.data_sizes = (1ULL << IBV_EXP_CALC_DATA_SIZE_64_BIT);
		device_attr->calc_cap.int_ops = (1ULL << IBV_EXP_CALC_OP_ADD) |
						(1ULL << IBV_EXP_CALC_OP_BAND) |
						(1ULL << IBV_EXP_CALC_OP_BXOR) |
						(1ULL << IBV_EXP_CALC_OP_BOR);
		device_attr->calc_cap.uint_ops = device_attr->calc_cap.int_ops;
		device_attr->calc_cap.fp_ops = device_attr->calc_cap.int_ops;
	}
	device_attr->exp_device_cap_flags |= IBV_EXP_DEVICE_MR_ALLOCATE;

	return __mlx4_query_device(
			raw_fw_ver,
			(struct ibv_device_attr *)device_attr);
}

int mlx4_exp_query_port(struct ibv_context *context, uint8_t port_num,
			struct ibv_exp_port_attr *port_attr)
{
	/* Check that only valid flags were given */
	if (!(port_attr->comp_mask & IBV_EXP_QUERY_PORT_ATTR_MASK1) ||
	    (port_attr->comp_mask & ~IBV_EXP_QUERY_PORT_ATTR_MASKS) ||
	    (port_attr->mask1 & ~IBV_EXP_QUERY_PORT_MASK)) {
		return EINVAL;
	}

	/* Optimize the link type query */
	if (port_attr->comp_mask == IBV_EXP_QUERY_PORT_ATTR_MASK1) {
		if (!(port_attr->mask1 & ~(IBV_EXP_QUERY_PORT_LINK_LAYER |
					   IBV_EXP_QUERY_PORT_CAP_FLAGS))) {
			struct mlx4_context *mctx = to_mctx(context);
			if (port_num <= 0 || port_num > MLX4_PORTS_NUM)
				return EINVAL;
			if (mctx->port_query_cache[port_num - 1].valid) {
				if (port_attr->mask1 &
				    IBV_EXP_QUERY_PORT_LINK_LAYER)
					port_attr->link_layer =
						mctx->
						port_query_cache[port_num - 1].
						link_layer;
				if (port_attr->mask1 &
				    IBV_EXP_QUERY_PORT_CAP_FLAGS)
					port_attr->port_cap_flags =
						mctx->
						port_query_cache[port_num - 1].
						caps;
				return 0;
			}
		}
		if (port_attr->mask1 & IBV_EXP_QUERY_PORT_STD_MASK) {
			return mlx4_query_port(context, port_num,
					       &port_attr->port_attr);
		}
	}

	return EOPNOTSUPP;
}

struct ibv_ah *mlx4_exp_create_ah(struct ibv_pd *pd,
				  struct ibv_exp_ah_attr *attr_ex)
{
	struct ibv_exp_port_attr port_attr;
	struct ibv_ah *ah;
	struct mlx4_ah *mah;

	port_attr.comp_mask = IBV_EXP_QUERY_PORT_ATTR_MASK1;
	port_attr.mask1 = IBV_EXP_QUERY_PORT_LINK_LAYER;

	if (ibv_exp_query_port(pd->context, attr_ex->port_num, &port_attr))
		return NULL;

	ah = mlx4_create_ah_common(pd, (struct ibv_ah_attr *)attr_ex,
				   port_attr.link_layer);

	if (NULL == ah)
		return NULL;

	mah = to_mah(ah);

	/* If vlan was given, check that we could use it */
	if (attr_ex->comp_mask & IBV_EXP_AH_ATTR_VID &&
	    attr_ex->vid <= 0xfff &&
	    (0 == attr_ex->ll_address.len ||
	     !(attr_ex->comp_mask & IBV_EXP_AH_ATTR_LL)))
		goto err;

	/* ll_address.len == 0 means no ll address given */
	if (attr_ex->comp_mask & IBV_EXP_AH_ATTR_LL &&
	    0 != attr_ex->ll_address.len) {
		if (LL_ADDRESS_ETH != attr_ex->ll_address.type ||
		    port_attr.link_layer != IBV_LINK_LAYER_ETHERNET)
			/* mlx4 provider currently only support ethernet
			 * extensions */
			goto err;

		/* link layer is ethernet */
		if (6 != attr_ex->ll_address.len ||
		    NULL == attr_ex->ll_address.address)
			goto err;

		memcpy(mah->mac, attr_ex->ll_address.address,
		       attr_ex->ll_address.len);

		if (attr_ex->comp_mask & IBV_EXP_AH_ATTR_VID &&
		    attr_ex->vid <= 0xfff) {
				mah->av.port_pd |= htonl(1 << 29);
				mah->vlan = attr_ex->vid |
					((attr_ex->sl & 7) << 13);
		}
	}

	return ah;

err:
	free(ah);
	return NULL;
}

