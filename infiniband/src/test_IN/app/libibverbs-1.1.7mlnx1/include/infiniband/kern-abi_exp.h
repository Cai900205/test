/*
 * Copyright (c) 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2005, 2006 Cisco Systems.  All rights reserved.
 * Copyright (c) 2005 PathScale, Inc.  All rights reserved.
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

#ifndef KERN_ABI_EXP_H
#define KERN_ABI_EXP_H

#include <infiniband/kern-abi.h>

/*
 * This file must be kept in sync with the kernel's version of
 * drivers/infiniband/include/ib_user_verbs_exp.h
 */

enum {
	IB_USER_VERBS_EXP_CMD_FIRST = 64
};

enum {
	IB_USER_VERBS_EXP_CMD_CREATE_QP,
	IB_USER_VERBS_EXP_CMD_MODIFY_CQ,
	IB_USER_VERBS_EXP_CMD_MODIFY_QP,
	IB_USER_VERBS_EXP_CMD_CREATE_CQ,
	IB_USER_VERBS_EXP_CMD_QUERY_DEVICE,
	IB_USER_VERBS_EXP_CMD_CREATE_DCT,
	IB_USER_VERBS_EXP_CMD_DESTROY_DCT,
	IB_USER_VERBS_EXP_CMD_QUERY_DCT,
	IB_USER_VERBS_EXP_CMD_ARM_DCT,
	IB_USER_VERBS_EXP_CMD_CREATE_MR,
	IB_USER_VERBS_EXP_CMD_QUERY_MKEY,
	IB_USER_VERBS_EXP_CMD_REG_MR,
	IB_USER_VERBS_EXP_CMD_PREFETCH_MR,
	IB_USER_VERBS_EXP_CMD_REREG_MR,
};

enum ibv_exp_create_qp_comp_mask {
	IBV_EXP_CREATE_QP_CAP_FLAGS          = (1ULL << 0),
	IBV_EXP_CREATE_QP_INL_RECV           = (1ULL << 1),
	IBV_EXP_CREATE_QP_QPG                = (1ULL << 2),
	IBV_EXP_CREATE_QP_MAX_INL_KLMS	     = (1ULL << 3)
};

struct ibv_create_qpg_init_attrib {
	__u32 tss_child_count;
	__u32 rss_child_count;
};

struct ibv_create_qpg {
	__u32 qpg_type;
	union {
		struct {
			__u32 parent_handle;
			__u32 reserved;
		};
		struct ibv_create_qpg_init_attrib parent_attrib;
	};
	__u32 reserved2;
};

enum ibv_exp_create_qp_kernel_flags {
	IBV_EXP_CREATE_QP_KERNEL_FLAGS = IBV_EXP_QP_CREATE_CROSS_CHANNEL  |
					 IBV_EXP_QP_CREATE_MANAGED_SEND   |
					 IBV_EXP_QP_CREATE_MANAGED_RECV   |
					 IBV_EXP_QP_CREATE_ATOMIC_BE_REPLY
};

struct ibv_exp_create_qp {
	struct ex_hdr hdr;
	__u64 comp_mask;
	__u64 user_handle;
	__u32 pd_handle;
	__u32 send_cq_handle;
	__u32 recv_cq_handle;
	__u32 srq_handle;
	__u32 max_send_wr;
	__u32 max_recv_wr;
	__u32 max_send_sge;
	__u32 max_recv_sge;
	__u32 max_inline_data;
	__u8  sq_sig_all;
	__u8  qp_type;
	__u8  is_srq;
	__u8  reserved;
	__u64 qp_cap_flags;
	__u32 max_inl_recv;
	__u32 reserved1;
	struct ibv_create_qpg qpg;
	__u64 max_inl_send_klms;
	__u64 driver_data[0];
};

enum ibv_exp_create_qp_resp_comp_mask {
	IBV_EXP_CREATE_QP_RESP_INL_RECV       = (1ULL << 0),
};

struct ibv_exp_create_qp_resp {
	__u64 comp_mask;
	__u32 qp_handle;
	__u32 qpn;
	__u32 max_send_wr;
	__u32 max_recv_wr;
	__u32 max_send_sge;
	__u32 max_recv_sge;
	__u32 max_inline_data;
	__u32 max_inl_recv;
};

struct ibv_exp_umr_caps_resp {
	__u32 max_klm_list_size;
	__u32 max_send_wqe_inline_klms;
	__u32 max_umr_recursion_depth;
	__u32 max_umr_stride_dimension;
};

struct ibv_exp_odp_caps_resp {
	__u64	general_odp_caps;
	struct {
		__u32	rc_odp_caps;
		__u32	uc_odp_caps;
		__u32	ud_odp_caps;
		__u32	dc_odp_caps;
		__u32	xrc_odp_caps;
		__u32	raw_eth_odp_caps;
	} per_transport_caps;
};

struct ibv_exp_query_device {
	struct ex_hdr hdr;
	__u64 comp_mask;
	__u64 driver_data[0];
};

struct ibv_exp_query_device_resp {
	__u64 comp_mask;
	__u64 fw_ver;
	__u64 node_guid;
	__u64 sys_image_guid;
	__u64 max_mr_size;
	__u64 page_size_cap;
	__u32 vendor_id;
	__u32 vendor_part_id;
	__u32 hw_ver;
	__u32 max_qp;
	__u32 max_qp_wr;
	__u32 device_cap_flags;
	__u32 max_sge;
	__u32 max_sge_rd;
	__u32 max_cq;
	__u32 max_cqe;
	__u32 max_mr;
	__u32 max_pd;
	__u32 max_qp_rd_atom;
	__u32 max_ee_rd_atom;
	__u32 max_res_rd_atom;
	__u32 max_qp_init_rd_atom;
	__u32 max_ee_init_rd_atom;
	__u32 exp_atomic_cap;
	__u32 max_ee;
	__u32 max_rdd;
	__u32 max_mw;
	__u32 max_raw_ipv6_qp;
	__u32 max_raw_ethy_qp;
	__u32 max_mcast_grp;
	__u32 max_mcast_qp_attach;
	__u32 max_total_mcast_qp_attach;
	__u32 max_ah;
	__u32 max_fmr;
	__u32 max_map_per_fmr;
	__u32 max_srq;
	__u32 max_srq_wr;
	__u32 max_srq_sge;
	__u16 max_pkeys;
	__u8  local_ca_ack_delay;
	__u8  phys_port_cnt;
	__u8  reserved[4];
	__u64 timestamp_mask;
	__u64 hca_core_clock;
	__u64 device_cap_flags2;
	__u32 dc_rd_req;
	__u32 dc_rd_res;
	__u32 inline_recv_sz;
	__u32 max_rss_tbl_sz;
	__u64 log_atomic_arg_sizes;
	__u32 max_fa_bit_boundary;
	__u32 log_max_atomic_inline;
	struct ibv_exp_umr_caps_resp umr_caps;
	struct ibv_exp_odp_caps_resp odp_caps;
	__u32 max_dct;
	__u32 reserved1;
};

struct ibv_exp_create_dct {
	struct ex_hdr hdr;
	__u64 comp_mask;
	__u64 user_handle;
	__u32 pd_handle;
	__u32 cq_handle;
	__u32 srq_handle;
	__u32 access_flags;
	__u64 dc_key;
	__u32 flow_label;
	__u8  min_rnr_timer;
	__u8  tclass;
	__u8  port;
	__u8  pkey_index;
	__u8  gid_index;
	__u8  hop_limit;
	__u8  mtu;
	__u8  rsvd0;
	__u32 create_flags;
	__u32 inline_size;
	__u32 rsvd1;
	__u64 driver_data[0];
};

struct ibv_exp_create_dct_resp {
	__u32 dct_handle;
	__u32 dct_num;
	__u32 inline_size;
	__u32 rsvd;
};

struct ibv_exp_destroy_dct {
	struct ex_hdr hdr;
	__u64 comp_mask;
	__u32 dct_handle;
	__u32 rsvd;
	__u64 driver_data[0];
};

struct ibv_exp_destroy_dct_resp {
	__u32	events_reported;
	__u32	reserved;
};

struct ibv_exp_query_dct {
	struct ex_hdr hdr;
	__u64 comp_mask;
	__u32 dct_handle;
	__u32 reserved;
	__u64 driver_data[0];
};

struct ibv_exp_query_dct_resp {
	__u64	dc_key;
	__u32	access_flags;
	__u32	flow_label;
	__u32	key_violations;
	__u8	port;
	__u8	min_rnr_timer;
	__u8	tclass;
	__u8	mtu;
	__u8	pkey_index;
	__u8	gid_index;
	__u8	hop_limit;
	__u8	state;
	__u32	rsvd;
	__u64	driver_data[0];
};

struct ibv_exp_arm_dct {
	struct ex_hdr hdr;
	__u64 comp_mask;
	__u32 dct_handle;
	__u32 reserved;
	__u64 driver_data[0];
};

struct ibv_exp_arm_dct_resp {
	__u64	reserved;
};

struct ibv_exp_modify_cq {
	struct ex_hdr hdr;
	__u32 cq_handle;
	__u32 attr_mask;
	__u16 cq_count;
	__u16 cq_period;
	__u32 cq_cap_flags;
	__u32 comp_mask;
	__u32 rsvd;
};

struct ibv_exp_modify_qp {
	struct ex_hdr hdr;
	__u32 comp_mask;
	struct ibv_qp_dest dest;
	struct ibv_qp_dest alt_dest;
	__u32 qp_handle;
	__u32 attr_mask;
	__u32 qkey;
	__u32 rq_psn;
	__u32 sq_psn;
	__u32 dest_qp_num;
	__u32 qp_access_flags;
	__u16 pkey_index;
	__u16 alt_pkey_index;
	__u8  qp_state;
	__u8  cur_qp_state;
	__u8  path_mtu;
	__u8  path_mig_state;
	__u8  en_sqd_async_notify;
	__u8  max_rd_atomic;
	__u8  max_dest_rd_atomic;
	__u8  min_rnr_timer;
	__u8  port_num;
	__u8  timeout;
	__u8  retry_cnt;
	__u8  rnr_retry;
	__u8  alt_port_num;
	__u8  alt_timeout;
	__u8  reserved[6];
	__u64 dct_key;
	__u32 exp_attr_mask;
	__u32 rsvd;
	__u64 driver_data[0];
};

enum ibv_exp_create_cq_comp_mask {
	IBV_EXP_CREATE_CQ_CAP_FLAGS	= (uint64_t)1 << 0,
};

struct ibv_exp_create_cq {
	struct ex_hdr hdr;
	__u64 comp_mask;
	__u64 user_handle;
	__u32 cqe;
	__u32 comp_vector;
	__s32 comp_channel;
	__u32 reserved;
	__u64 create_flags;
	__u64 driver_data[0];
};

struct ibv_exp_create_mr {
	struct ex_hdr hdr;
	__u64 comp_mask;
	__u32 pd_handle;
	__u32 max_klm_list_size;
	__u64 exp_access_flags;
	__u32 create_flags;
	__u32 reserved;
	__u64 driver_data[0];
};

struct ibv_exp_create_mr_resp {
	__u64 comp_mask;
	__u32 handle;
	__u32 lkey;
	__u32 rkey;
	__u32 reserved;
	__u64 driver_data[0];
};

struct ibv_exp_query_mkey {
	struct ex_hdr hdr;
	__u64 comp_mask;
	__u32 handle;
	__u32 lkey;
	__u32 rkey;
	__u32 reserved;
	__u64 driver_data[0];
};

struct ibv_exp_query_mkey_resp {
	__u64 comp_mask;
	__u32 max_klm_list_size;
	__u32 reserved;
	__u64 driver_data[0];
};

enum ibv_exp_reg_mr_comp_mask {
	IBV_EXP_REG_MR_EXP_ACCESS_FLAGS = 1ULL << 0,
};

struct ibv_exp_reg_mr {
	struct ex_hdr hdr;
	__u64 start;
	__u64 length;
	__u64 hca_va;
	__u32 pd_handle;
	__u32 reserved;
	__u64 exp_access_flags;
	__u64 comp_mask;
};

struct ibv_exp_prefetch_mr {
	struct ex_hdr hdr;
	__u64 comp_mask;
	__u32 mr_handle;
	__u32 flags;
	__u64 start;
	__u64 length;
};

struct ibv_exp_reg_mr_resp {
	__u32 mr_handle;
	__u32 lkey;
	__u32 rkey;
	__u32 reserved;
	__u64 comp_mask;
};

struct ibv_exp_rereg_mr {
	struct ex_hdr hdr;
	__u32 comp_mask;
	__u32 mr_handle;
	__u32 flags;
	__u32 reserved;
	__u64 start;
	__u64 length;
	__u64 hca_va;
	__u32 pd_handle;
	__u32 access_flags;
};

struct ibv_exp_rereg_mr_resp {
	__u32 comp_mask;
	__u32 lkey;
	__u32 rkey;
	__u32 reserved;
};

#endif /* KERN_ABI_EXP_H */
