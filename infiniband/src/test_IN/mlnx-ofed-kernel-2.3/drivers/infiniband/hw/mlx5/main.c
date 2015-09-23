/*
 * Copyright (c) 2013, Mellanox Technologies inc.  All rights reserved.
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

#include <asm-generic/kmap_types.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/io-mapping.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <rdma/ib_user_verbs.h>
#include <rdma/ib_user_verbs_exp.h>
#include <rdma/ib_verbs_exp.h>
#include <rdma/ib_smi.h>
#include <rdma/ib_umem.h>
#include "user.h"
#include "mlx5_ib.h"

#define DRIVER_NAME "mlx5_ib"
#define DRIVER_VERSION "1.0"
#define DRIVER_RELDATE	"June 2013"

MODULE_AUTHOR("Eli Cohen <eli@mellanox.com>");
MODULE_DESCRIPTION("Mellanox Connect-IB HCA IB driver");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(DRIVER_VERSION);

static int deprecated_prof_sel = 2;
module_param_named(prof_sel, deprecated_prof_sel, int, 0444);
MODULE_PARM_DESC(prof_sel,
		 "Profile selector. Deprecated here. Moved to module mlx5_core");

static char mlx5_version[] =
	DRIVER_NAME ": Mellanox Connect-IB Infiniband driver v"
	DRIVER_VERSION " (" DRIVER_RELDATE ")\n";

int mlx5_vector2eqn(struct mlx5_ib_dev *dev, int vector, int *eqn, int *irqn,
		    struct mlx5_eq **peq)
{
	struct mlx5_eq_table *table = &dev->mdev->priv.eq_table;
	struct mlx5_eq *eq, *n, *chosen;
	int err = -ENOENT;

	spin_lock(&table->lock);
	chosen = list_entry(dev->eqs_list.next, struct mlx5_eq, list);
	list_for_each_entry_safe(eq, n, &dev->eqs_list, list) {
		if (vector || smp_processor_id() == (vector % num_online_cpus())) {
			if (eq->index == vector) {
				*eqn = eq->eqn;
				*irqn = eq->irqn;
				eq->cqs_counter++;
				*peq = eq;
				err = 0;
				mlx5_ib_dbg(dev, "requested %d, chosen %d\n",
					    vector, (*peq)->index);
				break;
			}
		}
		if (chosen->cqs_counter > eq->cqs_counter)
			chosen = eq;
	}
	if (err) {
		*eqn = chosen->eqn;
		*irqn = chosen->irqn;
		chosen->cqs_counter++;
		*peq = chosen;
		err = 0;
		mlx5_ib_dbg(dev, "requested %d, chosen %d\n",
			    vector, (*peq)->index);
	}
	spin_unlock(&table->lock);

	return err;
}

static int alloc_comp_eqs(struct mlx5_ib_dev *dev)
{
	struct mlx5_eq_table *table = &dev->mdev->priv.eq_table;
	char name[MLX5_MAX_EQ_NAME];
	struct mlx5_eq *eq, *n;
	int ncomp_vec;
	int nent;
	int err;
	int i;

	INIT_LIST_HEAD(&dev->eqs_list);
	ncomp_vec = table->num_comp_vectors;
	nent = MLX5_COMP_EQ_SIZE;
	for (i = 0; i < ncomp_vec; i++) {
		eq = kzalloc(sizeof(*eq), GFP_KERNEL);
		if (!eq) {
			err = -ENOMEM;
			goto clean;
		}

		snprintf(name, MLX5_MAX_EQ_NAME, "mlx5_comp%d", i);
		err = mlx5_create_map_eq(dev->mdev, eq,
					 i + MLX5_EQ_VEC_COMP_BASE, nent, 0,
					 name, &dev->mdev->priv.uuari.uars[0]);
		if (err) {
			kfree(eq);
			goto clean;
		}
		mlx5_ib_dbg(dev, "allocated completion EQN %d\n", eq->eqn);
		eq->index = i;
		spin_lock(&table->lock);
		list_add_tail(&eq->list, &dev->eqs_list);
		spin_unlock(&table->lock);
	}

	dev->num_comp_vectors = ncomp_vec;
	return 0;

clean:
	spin_lock(&table->lock);
	list_for_each_entry_safe(eq, n, &dev->eqs_list, list) {
		list_del(&eq->list);
		spin_unlock(&table->lock);
		if (mlx5_destroy_unmap_eq(dev->mdev, eq))
			mlx5_ib_warn(dev, "failed to destroy EQ 0x%x\n", eq->eqn);
		kfree(eq);
		spin_lock(&table->lock);
	}
	spin_unlock(&table->lock);
	return err;
}

static void free_comp_eqs(struct mlx5_ib_dev *dev)
{
	struct mlx5_eq_table *table = &dev->mdev->priv.eq_table;
	struct mlx5_eq *eq, *n;

	spin_lock(&table->lock);
	list_for_each_entry_safe(eq, n, &dev->eqs_list, list) {
		list_del(&eq->list);
		spin_unlock(&table->lock);
		if (mlx5_destroy_unmap_eq(dev->mdev, eq))
			mlx5_ib_warn(dev, "failed to destroy EQ 0x%x\n", eq->eqn);
		kfree(eq);
		spin_lock(&table->lock);
	}
	spin_unlock(&table->lock);
}

static void ext_atomic_caps(struct mlx5_ib_dev *dev,
			    struct ib_exp_device_attr *props)
{
	struct mlx5_caps *caps = &dev->mdev->caps;
	int tmp;
	unsigned long last;
	unsigned long arg;

	props->max_fa_bit_boudary = 0;
	props->log_max_atomic_inline_arg = 0;

	tmp = MLX5_ATOMIC_OPS_CMP_SWAP		|
	      MLX5_ATOMIC_OPS_FETCH_ADD		|
	      MLX5_ATOMIC_OPS_MASKED_CMP_SWAP	|
	      MLX5_ATOMIC_OPS_MASKED_FETCH_ADD;

	if (caps->atomic_ops != tmp)
		return;

	props->atomic_arg_sizes = caps->atomic_arg_sizes_qp & caps->atomic_arg_sizes_dc;
	props->max_fa_bit_boudary = 64;
	arg = (unsigned long)props->atomic_arg_sizes;
	last = find_last_bit(&arg, sizeof(arg));
	if (last < 6)
		props->log_max_atomic_inline_arg = last;
	else
		props->log_max_atomic_inline_arg = 6;

	props->device_cap_flags2 |= IB_EXP_DEVICE_EXT_ATOMICS;
}

static void get_atomic_caps(struct mlx5_caps *caps,
			    struct ib_device_attr *props,
			    int exp)
{
	int tmp;

	tmp = MLX5_ATOMIC_OPS_CMP_SWAP | MLX5_ATOMIC_OPS_FETCH_ADD;
	if (((caps->atomic_ops & tmp) == tmp) && (caps->atomic_arg_sizes_qp & 8)) {
		if (caps->atomic_req_host_endianess)
			props->atomic_cap = IB_ATOMIC_HCA;
		else {
			if (exp)
				props->atomic_cap = IB_ATOMIC_HCA_REPLY_BE;
			else
				props->atomic_cap = IB_ATOMIC_NONE;
		}
	} else {
		props->atomic_cap = IB_ATOMIC_NONE;
	}

	tmp = MLX5_ATOMIC_OPS_MASKED_CMP_SWAP | MLX5_ATOMIC_OPS_MASKED_FETCH_ADD;
	if (((caps->atomic_ops & tmp) == tmp) && (caps->atomic_arg_sizes_qp & 8)) {
		if (caps->atomic_req_host_endianess)
			props->masked_atomic_cap = IB_ATOMIC_HCA;
		else {
			if (exp)
				props->masked_atomic_cap = IB_ATOMIC_HCA_REPLY_BE;
			else
				props->masked_atomic_cap = IB_ATOMIC_NONE;
		}
	} else {
		props->masked_atomic_cap = IB_ATOMIC_NONE;
	}
}

static int query_device(struct ib_device *ibdev,
			struct ib_device_attr *props,
			int exp)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	struct ib_smp *in_mad  = NULL;
	struct ib_smp *out_mad = NULL;
	int err = -ENOMEM;
	int max_rq_sg;
	int max_sq_sg;
	u64 flags;
	int max_sq_desc;

	in_mad  = kzalloc(sizeof(*in_mad), GFP_KERNEL);
	out_mad = kmalloc(sizeof(*out_mad), GFP_KERNEL);
	if (!in_mad || !out_mad)
		goto out;

	init_query_mad(in_mad);
	in_mad->attr_id = IB_SMP_ATTR_NODE_INFO;

	err = mlx5_MAD_IFC(to_mdev(ibdev), 1, 1, 1, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	memset(props, 0, sizeof(*props));

	props->fw_ver = ((u64)fw_rev_maj(dev->mdev) << 32) |
		(fw_rev_min(dev->mdev) << 16) |
		fw_rev_sub(dev->mdev);
	props->device_cap_flags    = IB_DEVICE_CHANGE_PHY_PORT |
		IB_DEVICE_PORT_ACTIVE_EVENT		|
		IB_DEVICE_SYS_IMAGE_GUID		|
		IB_DEVICE_RC_RNR_NAK_GEN;
	flags = dev->mdev->caps.flags;
	if (flags & MLX5_DEV_CAP_FLAG_BAD_PKEY_CNTR)
		props->device_cap_flags |= IB_DEVICE_BAD_PKEY_CNTR;
	if (flags & MLX5_DEV_CAP_FLAG_BAD_QKEY_CNTR)
		props->device_cap_flags |= IB_DEVICE_BAD_QKEY_CNTR;
	if (flags & MLX5_DEV_CAP_FLAG_APM)
		props->device_cap_flags |= IB_DEVICE_AUTO_PATH_MIG;
	props->device_cap_flags |= IB_DEVICE_LOCAL_DMA_LKEY;
	if (flags & MLX5_DEV_CAP_FLAG_XRC)
		props->device_cap_flags |= IB_DEVICE_XRC;
	props->device_cap_flags |= IB_DEVICE_MEM_MGT_EXTENSIONS;
	if ((flags & MLX5_DEV_CAP_FLAG_CROSS_CHANNEL) &&
	    (flags & MLX5_DEV_CAP_FLAG_CQ_OI))
		props->device_cap_flags |= IB_DEVICE_CROSS_CHANNEL;
	if (flags & MLX5_DEV_CAP_FLAG_BLOCK_MCAST)
		props->device_cap_flags |= IB_DEVICE_BLOCK_MULTICAST_LOOPBACK;

	if (flags & MLX5_DEV_CAP_FLAG_SIG_HAND_OVER) {
		props->device_cap_flags |= IB_DEVICE_SIGNATURE_HANDOVER;
		props->sig_prot_cap = IB_PROT_T10DIF_TYPE_1 |
				      IB_PROT_T10DIF_TYPE_2 |
				      IB_PROT_T10DIF_TYPE_3;
		props->sig_guard_cap = IB_GUARD_T10DIF_CRC |
				       IB_GUARD_T10DIF_CSUM;
	}

	props->vendor_id	   = be32_to_cpup((__be32 *)(out_mad->data + 36)) &
		0xffffff;
	props->vendor_part_id	   = be16_to_cpup((__be16 *)(out_mad->data + 30));
	props->hw_ver		   = be32_to_cpup((__be32 *)(out_mad->data + 32));
	memcpy(&props->sys_image_guid, out_mad->data +	4, 8);

	props->max_mr_size	   = ~0ull;
	props->page_size_cap	   = dev->mdev->caps.min_page_sz;
	props->max_qp		   = 1 << dev->mdev->caps.log_max_qp;
	props->max_qp_wr	   = dev->mdev->caps.max_wqes;
	max_rq_sg = dev->mdev->caps.max_rq_desc_sz / sizeof(struct mlx5_wqe_data_seg);
	max_sq_desc = min(dev->mdev->caps.max_sq_desc_sz, 512);
	max_sq_sg = (max_sq_desc -
		     sizeof(struct mlx5_wqe_ctrl_seg) -
		     sizeof(struct mlx5_wqe_raddr_seg)) / sizeof(struct mlx5_wqe_data_seg);
	props->max_sge = min(max_rq_sg, max_sq_sg);
	props->max_cq		   = 1 << dev->mdev->caps.log_max_cq;
	props->max_cqe		   = dev->mdev->caps.max_cqes - 1;
	props->max_mr		   = 1 << dev->mdev->caps.log_max_mkey;
	props->max_pd		   = 1 << dev->mdev->caps.log_max_pd;
	props->max_qp_rd_atom	   = dev->mdev->caps.max_ra_req_qp;
	props->max_qp_init_rd_atom = dev->mdev->caps.max_ra_res_qp;
	props->max_res_rd_atom	   = props->max_qp_rd_atom * props->max_qp;
	props->max_srq		   = 1 << dev->mdev->caps.log_max_srq;
	props->max_srq_wr	   = dev->mdev->caps.max_srq_wqes - 1;
	props->max_srq_sge	   = max_rq_sg - 1;
	props->max_fast_reg_page_list_len = (unsigned int)-1;
	props->local_ca_ack_delay  = dev->mdev->caps.local_ca_ack_delay;
	get_atomic_caps(&dev->mdev->caps, props, exp);
	props->max_pkeys	   = be16_to_cpup((__be16 *)(out_mad->data + 28));
	props->max_mcast_grp	   = 1 << dev->mdev->caps.log_max_mcg;
	props->max_mcast_qp_attach = dev->mdev->caps.max_qp_mcg;
	props->max_total_mcast_qp_attach = props->max_mcast_qp_attach *
					   props->max_mcast_grp;
	props->max_map_per_fmr	= INT_MAX; /* no limit in ConnectIB */
	props->max_ah		= INT_MAX;

out:
	kfree(in_mad);
	kfree(out_mad);

	return err;
}

static int mlx5_ib_query_device(struct ib_device *ibdev,
				struct ib_device_attr *props)
{
	return query_device(ibdev, props, 0);
}

int mlx5_ib_query_port(struct ib_device *ibdev, u8 port,
		       struct ib_port_attr *props)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	struct ib_smp *in_mad  = NULL;
	struct ib_smp *out_mad = NULL;
	int ext_active_speed;
	int err = -ENOMEM;

	if (port < 1 || port > dev->mdev->caps.num_ports) {
		mlx5_ib_warn(dev, "invalid port number %d\n", port);
		return -EINVAL;
	}

	in_mad  = kzalloc(sizeof(*in_mad), GFP_KERNEL);
	out_mad = kmalloc(sizeof(*out_mad), GFP_KERNEL);
	if (!in_mad || !out_mad)
		goto out;

	memset(props, 0, sizeof(*props));

	init_query_mad(in_mad);
	in_mad->attr_id  = IB_SMP_ATTR_PORT_INFO;
	in_mad->attr_mod = cpu_to_be32(port);

	err = mlx5_MAD_IFC(dev, 1, 1, port, NULL, NULL, in_mad, out_mad);
	if (err) {
		mlx5_ib_warn(dev, "err %d\n", err);
		goto out;
	}


	props->lid		= be16_to_cpup((__be16 *)(out_mad->data + 16));
	props->lmc		= out_mad->data[34] & 0x7;
	props->sm_lid		= be16_to_cpup((__be16 *)(out_mad->data + 18));
	props->sm_sl		= out_mad->data[36] & 0xf;
	props->state		= out_mad->data[32] & 0xf;
	props->phys_state	= out_mad->data[33] >> 4;
	props->port_cap_flags	= be32_to_cpup((__be32 *)(out_mad->data + 20));
	props->gid_tbl_len	= out_mad->data[50];
	props->max_msg_sz	= 1 << to_mdev(ibdev)->mdev->caps.log_max_msg;
	props->pkey_tbl_len	= to_mdev(ibdev)->mdev->caps.port[port - 1].pkey_table_len;
	props->bad_pkey_cntr	= be16_to_cpup((__be16 *)(out_mad->data + 46));
	props->qkey_viol_cntr	= be16_to_cpup((__be16 *)(out_mad->data + 48));
	props->active_width	= out_mad->data[31] & 0xf;
	props->active_speed	= out_mad->data[35] >> 4;
	props->max_mtu		= out_mad->data[41] & 0xf;
	props->active_mtu	= out_mad->data[36] >> 4;
	props->subnet_timeout	= out_mad->data[51] & 0x1f;
	props->max_vl_num	= out_mad->data[37] >> 4;
	props->init_type_reply	= out_mad->data[41] >> 4;

	/* Check if extended speeds (EDR/FDR/...) are supported */
	if (props->port_cap_flags & IB_PORT_EXTENDED_SPEEDS_SUP) {
		ext_active_speed = out_mad->data[62] >> 4;

		switch (ext_active_speed) {
		case 1:
			props->active_speed = 16; /* FDR */
			break;
		case 2:
			props->active_speed = 32; /* EDR */
			break;
		}
	}

	/* If reported active speed is QDR, check if is FDR-10 */
	if (props->active_speed == 4) {
		if (dev->mdev->caps.ext_port_cap[port - 1] &
		    MLX_EXT_PORT_CAP_FLAG_EXTENDED_PORT_INFO) {
			init_query_mad(in_mad);
			in_mad->attr_id = MLX5_ATTR_EXTENDED_PORT_INFO;
			in_mad->attr_mod = cpu_to_be32(port);

			err = mlx5_MAD_IFC(dev, 1, 1, port,
					   NULL, NULL, in_mad, out_mad);
			if (err)
				goto out;

			/* Checking LinkSpeedActive for FDR-10 */
			if (out_mad->data[15] & 0x1)
				props->active_speed = 8;
		}
	}

out:
	kfree(in_mad);
	kfree(out_mad);

	return err;
}

static int mlx5_ib_query_gid(struct ib_device *ibdev, u8 port, int index,
			     union ib_gid *gid)
{
	struct ib_smp *in_mad  = NULL;
	struct ib_smp *out_mad = NULL;
	int err = -ENOMEM;

	in_mad  = kzalloc(sizeof(*in_mad), GFP_KERNEL);
	out_mad = kmalloc(sizeof(*out_mad), GFP_KERNEL);
	if (!in_mad || !out_mad)
		goto out;

	init_query_mad(in_mad);
	in_mad->attr_id  = IB_SMP_ATTR_PORT_INFO;
	in_mad->attr_mod = cpu_to_be32(port);

	err = mlx5_MAD_IFC(to_mdev(ibdev), 1, 1, port, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	memcpy(gid->raw, out_mad->data + 8, 8);

	init_query_mad(in_mad);
	in_mad->attr_id  = IB_SMP_ATTR_GUID_INFO;
	in_mad->attr_mod = cpu_to_be32(index / 8);

	err = mlx5_MAD_IFC(to_mdev(ibdev), 1, 1, port, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	memcpy(gid->raw + 8, out_mad->data + (index % 8) * 8, 8);

out:
	kfree(in_mad);
	kfree(out_mad);
	return err;
}

static int mlx5_ib_query_pkey(struct ib_device *ibdev, u8 port, u16 index,
			      u16 *pkey)
{
	struct ib_smp *in_mad  = NULL;
	struct ib_smp *out_mad = NULL;
	int err = -ENOMEM;

	in_mad  = kzalloc(sizeof(*in_mad), GFP_KERNEL);
	out_mad = kmalloc(sizeof(*out_mad), GFP_KERNEL);
	if (!in_mad || !out_mad)
		goto out;

	init_query_mad(in_mad);
	in_mad->attr_id  = IB_SMP_ATTR_PKEY_TABLE;
	in_mad->attr_mod = cpu_to_be32(index / 32);

	err = mlx5_MAD_IFC(to_mdev(ibdev), 1, 1, port, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	*pkey = be16_to_cpu(((__be16 *)out_mad->data)[index % 32]);

out:
	kfree(in_mad);
	kfree(out_mad);
	return err;
}

struct mlx5_reg_node_desc {
	u8	desc[64];
};

static int mlx5_ib_modify_device(struct ib_device *ibdev, int mask,
				 struct ib_device_modify *props)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	struct mlx5_reg_node_desc in;
	struct mlx5_reg_node_desc out;
	int err;

	if (mask & ~IB_DEVICE_MODIFY_NODE_DESC)
		return -EOPNOTSUPP;

	if (!(mask & IB_DEVICE_MODIFY_NODE_DESC))
		return 0;

	/*
	 * If possible, pass node desc to FW, so it can generate
	 * a 144 trap.  If cmd fails, just ignore.
	 */
	memcpy(&in, props->node_desc, 64);
	err = mlx5_core_access_reg(dev->mdev, &in, sizeof(in), &out,
				   sizeof(out), MLX5_REG_NODE_DESC, 0, 1);
	if (err)
		return err;

	memcpy(ibdev->node_desc, props->node_desc, 64);

	return err;
}

static int mlx5_ib_modify_port(struct ib_device *ibdev, u8 port, int mask,
			       struct ib_port_modify *props)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	struct ib_port_attr attr;
	u32 tmp;
	int err;

	mutex_lock(&dev->cap_mask_mutex);

	err = mlx5_ib_query_port(ibdev, port, &attr);
	if (err)
		goto out;

	tmp = (attr.port_cap_flags | props->set_port_cap_mask) &
		~props->clr_port_cap_mask;

	err = mlx5_set_port_caps(dev->mdev, port, tmp);

out:
	mutex_unlock(&dev->cap_mask_mutex);
	return err;
}

static struct ib_ucontext *mlx5_ib_alloc_ucontext(struct ib_device *ibdev,
						  struct ib_udata *udata)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	struct mlx5_ib_alloc_ucontext_req_v2 req;
	struct mlx5_ib_alloc_ucontext_resp resp;
	struct mlx5_ib_ucontext *context;
	struct mlx5_uuar_info *uuari;
	struct mlx5_uar *uars;
	int gross_uuars;
	int num_uars;
	int ver;
	int uuarn;
	int err;
	int i;
	int reqlen;

	if (!dev->ib_active)
		return ERR_PTR(-EAGAIN);

	memset(&req, 0, sizeof(req));
	reqlen = udata->inlen - sizeof(struct ib_uverbs_cmd_hdr);
	if (reqlen == sizeof(struct mlx5_ib_alloc_ucontext_req))
		ver = 0;
	else if (reqlen == sizeof(struct mlx5_ib_alloc_ucontext_req_v2))
		ver = 2;
	else
		return ERR_PTR(-EINVAL);

	err = ib_copy_from_udata(&req, udata, reqlen);
	if (err)
		return ERR_PTR(err);

	if (req.flags || req.reserved)
		return ERR_PTR(-EINVAL);

	if (req.total_num_uuars > MLX5_MAX_UUARS)
		return ERR_PTR(-ENOMEM);

	if (req.total_num_uuars == 0)
		return ERR_PTR(-EINVAL);

	req.total_num_uuars = ALIGN(req.total_num_uuars,
				    MLX5_NON_FP_BF_REGS_PER_PAGE);
	if (req.num_low_latency_uuars > req.total_num_uuars - 1)
		return ERR_PTR(-EINVAL);

	num_uars = req.total_num_uuars / MLX5_NON_FP_BF_REGS_PER_PAGE;
	gross_uuars = num_uars * MLX5_BF_REGS_PER_PAGE;
	resp.qp_tab_size      = 1 << dev->mdev->caps.log_max_qp;
	resp.bf_reg_size      = dev->mdev->caps.bf_reg_size;
	resp.cache_line_size  = L1_CACHE_BYTES;
	resp.max_sq_desc_sz = dev->mdev->caps.max_sq_desc_sz;
	resp.max_rq_desc_sz = dev->mdev->caps.max_rq_desc_sz;
	resp.max_send_wqebb = dev->mdev->caps.max_wqes;
	resp.max_recv_wr = dev->mdev->caps.max_wqes;
	resp.max_srq_recv_wr = dev->mdev->caps.max_srq_wqes;
	if (offsetof(struct mlx5_ib_alloc_ucontext_resp, max_desc_sz_sq_dc) < udata->outlen)
		resp.max_desc_sz_sq_dc = dev->mdev->caps.max_desc_sz_sq_dc;

	if (offsetof(struct mlx5_ib_alloc_ucontext_resp, atomic_arg_sizes_dc) < udata->outlen)
		resp.atomic_arg_sizes_dc = dev->mdev->caps.atomic_arg_sizes_dc;

	context = kzalloc(sizeof(*context), GFP_KERNEL);
	if (!context)
		return ERR_PTR(-ENOMEM);

	uuari = &context->uuari;
	mutex_init(&uuari->lock);
	uars = kcalloc(num_uars, sizeof(*uars), GFP_KERNEL);
	if (!uars) {
		err = -ENOMEM;
		goto out_ctx;
	}

	uuari->bitmap = kcalloc(BITS_TO_LONGS(gross_uuars),
				sizeof(*uuari->bitmap),
				GFP_KERNEL);
	if (!uuari->bitmap) {
		err = -ENOMEM;
		goto out_uar_ctx;
	}
	/*
	 * clear all fast path uuars
	 */
	for (i = 0; i < gross_uuars; i++) {
		uuarn = i & 3;
		if (uuarn == 2 || uuarn == 3)
			set_bit(i, uuari->bitmap);
	}

	uuari->count = kcalloc(gross_uuars, sizeof(*uuari->count), GFP_KERNEL);
	if (!uuari->count) {
		err = -ENOMEM;
		goto out_bitmap;
	}

	for (i = 0; i < num_uars; i++) {
		err = mlx5_cmd_alloc_uar(dev->mdev, &uars[i].index);
		if (err)
			goto out_uars;
	}

#ifdef CONFIG_INFINIBAND_ON_DEMAND_PAGING
	context->ibucontext.invalidate_range = &mlx5_ib_invalidate_range;
#endif

	INIT_LIST_HEAD(&context->vma_private_list);
	INIT_LIST_HEAD(&context->db_page_list);
	spin_lock_init(&context->vma_private_lock);
	mutex_init(&context->db_page_mutex);

	resp.tot_uuars = req.total_num_uuars;
	resp.num_ports = dev->mdev->caps.num_ports;
	err = ib_copy_to_udata(udata, &resp,
			       sizeof(resp) - sizeof(resp.reserved));
	if (err)
		goto out_uars;

	uuari->ver = ver;
	uuari->num_low_latency_uuars = req.num_low_latency_uuars;
	uuari->uars = uars;
	uuari->num_uars = num_uars;
	return &context->ibucontext;

out_uars:
	for (i--; i >= 0; i--)
		mlx5_cmd_free_uar(dev->mdev, uars[i].index);
	kfree(uuari->count);

out_bitmap:
	kfree(uuari->bitmap);

out_uar_ctx:
	kfree(uars);

out_ctx:
	kfree(context);
	return ERR_PTR(err);
}

static int mlx5_ib_dealloc_ucontext(struct ib_ucontext *ibcontext)
{
	struct mlx5_ib_ucontext *context = to_mucontext(ibcontext);
	struct mlx5_ib_dev *dev = to_mdev(ibcontext->device);
	struct mlx5_uuar_info *uuari = &context->uuari;
	int i;

	for (i = 0; i < uuari->num_uars; i++) {
		if (mlx5_cmd_free_uar(dev->mdev, uuari->uars[i].index))
			mlx5_ib_warn(dev, "failed to free UAR 0x%x\n", uuari->uars[i].index);
	}

	kfree(uuari->count);
	kfree(uuari->bitmap);
	kfree(uuari->uars);
	kfree(context);

	return 0;
}

static phys_addr_t uar_index2pfn(struct mlx5_ib_dev *dev, int index)
{
	return (pci_resource_start(dev->mdev->pdev, 0) >> PAGE_SHIFT) + index;
}

static int get_command(unsigned long offset)
{
	return (offset >> MLX5_IB_MMAP_CMD_SHIFT) & MLX5_IB_MMAP_CMD_MASK;
}

static int get_arg(unsigned long offset)
{
	return offset & ((1 << MLX5_IB_MMAP_CMD_SHIFT) - 1);
}

static int get_index(unsigned long offset)
{
	return get_arg(offset);
}

static int get_pg_order(unsigned long offset)
{
	return get_arg(offset);
}

#ifndef CONFIG_COMPAT_MISS_TASK_FUNCS
static void  mlx5_ib_vma_open(struct vm_area_struct *area)
{
	/* vma_open is called when a new VMA is created on top of our VMA.
	 * This is done through either mremap flow or split_vma (usually due to mlock,
	 * madvise, munmap, etc.)
	 * We do not support a clone of the vma, as this VMA is strongly hardware related.
	 * Therefore we set the vm_ops of the newly created/cloned VMA to NULL, to
	 * prevent it from calling us again and trying to do incorrect actions.
	 * We assume that the original vma size is exactly a single page, and therefore all
	 * "splitting" operation will not happen to it.
	 */
	area->vm_ops = NULL;
}

static void  mlx5_ib_vma_close(struct vm_area_struct *area)
{
	struct mlx5_ib_vma_private_data *mlx5_ib_vma_priv_data;

	/* It's guaranteed that all VMAs opened on a FD are closed before the file itself is closed, therefor no
	  * sync is needed with the regular closing flow. (e.g. mlx5 ib_dealloc_ucontext)
	  * However need a sync with accessing the vma as part of mlx5_ib_disassociate_ucontext.
	  * The close operation is usually called under mm->mmap_sem except when process is exiting.
	  * The exiting case is handled explicitly as part of mlx5_ib_disassociate_ucontext.
	*/
	mlx5_ib_vma_priv_data = (struct mlx5_ib_vma_private_data *)area->vm_private_data;

	/* setting the vma context pointer to null in the mlx5_ib driver's private data,
	 * to protect a race condition in mlx5_ib_dissassociate_ucontext().
	 */
	mlx5_ib_vma_priv_data->vma = NULL;
	list_del(&mlx5_ib_vma_priv_data->list);
	kfree(mlx5_ib_vma_priv_data);
}

static const struct vm_operations_struct mlx5_ib_vm_ops = {
	.open = mlx5_ib_vma_open,
	.close = mlx5_ib_vma_close
};

static int mlx5_ib_disassociate_ucontext(struct ib_ucontext *ibcontext)
{
	int ret;
	struct vm_area_struct *vma;
	struct mlx5_ib_vma_private_data *vma_private, *n;
	struct mlx5_ib_ucontext *context = to_mucontext(ibcontext);
	struct task_struct *owning_process  = NULL;
	struct mm_struct   *owning_mm       = NULL;

	owning_process = get_pid_task(ibcontext->tgid, PIDTYPE_PID);
	if (!owning_process)
		return 0;

	owning_mm = get_task_mm(owning_process);
	if (!owning_mm) {
		pr_info("no mm, disassociate ucontext is pending task termination\n");
		while (1) {
			put_task_struct(owning_process);
			msleep(1);
			owning_process = get_pid_task(ibcontext->tgid, PIDTYPE_PID);
			if (!owning_process || owning_process->state == TASK_DEAD) {
				pr_info("disassociate ucontext done, task was terminated\n");
				/* in case task was dead need to release the task struct */
				if (owning_process)
					put_task_struct(owning_process);
				return 0;
			}
		}
	}

	/* need to protect from a race on closing the vma as part of mlx5_ib_vma_close */
	down_read(&owning_mm->mmap_sem);
	list_for_each_entry_safe(vma_private, n, &context->vma_private_list, list) {
		vma = vma_private->vma;
		ret = zap_vma_ptes(vma, vma->vm_start,
				   PAGE_SIZE);

		BUG_ON(ret);
		/* need to turn off that flag to prevent double untracking VMA on RH 6.2 and some
		  * other kernels.
		*/
		vma->vm_flags &= ~VM_PFNMAP;

		/* context going to be destroyed, should not access ops any more */
		vma->vm_ops = NULL;
		list_del(&vma_private->list);
		kfree(vma_private);
	}
	up_read(&owning_mm->mmap_sem);
	mmput(owning_mm);
	put_task_struct(owning_process);
	return 0;
}

static int mlx5_ib_set_vma_data(struct vm_area_struct *vma,
				struct mlx5_ib_ucontext *ctx)
{
	struct mlx5_ib_vma_private_data *vma_prv;
	struct list_head *vma_head = &ctx->vma_private_list;

	vma_prv = kzalloc(sizeof(*vma_prv), GFP_KERNEL);
	if (!vma_prv)
		return -ENOMEM;

	vma_prv->vma = vma;
	vma->vm_private_data = vma_prv;
	vma->vm_ops =  &mlx5_ib_vm_ops;

	list_add(&vma_prv->list, vma_head);

	return 0;
}
#endif

static int mlx5_ib_mmap(struct ib_ucontext *ibcontext, struct vm_area_struct *vma)
{
	struct mlx5_ib_ucontext *context = to_mucontext(ibcontext);
	struct mlx5_ib_dev *dev = to_mdev(ibcontext->device);
	struct mlx5_uuar_info *uuari = &context->uuari;
	unsigned long command;
	unsigned long idx;
	phys_addr_t pfn;
	int err;
	unsigned long total_size;
	unsigned long order;
	struct ib_cmem *ib_cmem;

	command = get_command(vma->vm_pgoff);
	switch (command) {
	case MLX5_IB_MMAP_REGULAR_PAGE:
		if (vma->vm_end - vma->vm_start != PAGE_SIZE)
			return -EINVAL;

		idx = get_index(vma->vm_pgoff);
		pfn = uar_index2pfn(dev, uuari->uars[idx].index);
		mlx5_ib_dbg(dev, "uar idx 0x%lx, pfn 0x%llx\n", idx,
			    (unsigned long long)pfn);

		if (idx >= uuari->num_uars)
			return -EINVAL;

		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
		if (io_remap_pfn_range(vma, vma->vm_start, pfn,
				       PAGE_SIZE, vma->vm_page_prot))
			return -EAGAIN;

		mlx5_ib_dbg(dev, "mapped WC at 0x%lx, PA 0x%llx\n",
			    vma->vm_start,
			    (unsigned long long)pfn << PAGE_SHIFT);
#ifndef CONFIG_COMPAT_MISS_TASK_FUNCS
		err = mlx5_ib_set_vma_data(vma, context);
		if (err)
			return err;
#endif
		break;

	case MLX5_IB_MMAP_GET_CONTIGUOUS_PAGES:
		total_size = vma->vm_end - vma->vm_start;
		order = get_pg_order(vma->vm_pgoff);

		ib_cmem = ib_cmem_alloc_contiguous_pages(ibcontext, total_size,
							 order);
		if (IS_ERR(ib_cmem))
			return PTR_ERR(ib_cmem);

		err = ib_cmem_map_contiguous_pages_to_vma(ib_cmem, vma);
		if (err) {
			ib_cmem_release_contiguous_pages(ib_cmem);
			return err;
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static unsigned long mlx5_ib_get_unmapped_area(struct file *file,
					       unsigned long addr,
					       unsigned long len,
					       unsigned long pgoff,
					       unsigned long flags)
{
	struct mm_struct *mm;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 11, 0))
	struct vm_area_struct *vma;
	unsigned long start_addr;
#endif
	unsigned long order;
	unsigned long command;

	mm = current->mm;
	if (addr)
		return current->mm->get_unmapped_area(file, addr, len,
						      pgoff, flags);
	command = get_command(pgoff);
	if (command == MLX5_IB_MMAP_REGULAR_PAGE)
		return current->mm->get_unmapped_area(file, addr, len,
						      pgoff, flags);

	if (command != MLX5_IB_MMAP_GET_CONTIGUOUS_PAGES)
		return -EINVAL;

	order = get_pg_order(pgoff);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 11, 0))
	/*
	 * code is based on the huge-pages get_unmapped_area code
	 */
	start_addr = mm->free_area_cache;
	if (len <= mm->cached_hole_size)
		start_addr = TASK_UNMAPPED_BASE;
full_search:
	addr = ALIGN(start_addr, 1 << order);

	for (vma = find_vma(mm, addr); ; vma = vma->vm_next) {
		if (addr > TASK_SIZE - len) {
			if (start_addr != TASK_UNMAPPED_BASE) {
				start_addr = TASK_UNMAPPED_BASE;
				goto full_search;
			}
			return -ENOMEM;
		}

		if (!vma || addr + len <= vma->vm_start)
			return addr;
		addr = ALIGN(vma->vm_end, 1 << order);
	}
#else
	return current->mm->get_unmapped_area(file, addr, len,
					      pgoff, flags);
#endif
}


static int alloc_pa_mkey(struct mlx5_ib_dev *dev, u32 *key, u32 pdn)
{
	struct mlx5_create_mkey_mbox_in *in;
	struct mlx5_mkey_seg *seg;
	struct mlx5_core_mr mr;
	int err;

	in = kzalloc(sizeof(*in), GFP_KERNEL);
	if (!in)
		return -ENOMEM;

	seg = &in->seg;
	seg->flags = MLX5_PERM_LOCAL_READ | MLX5_ACCESS_MODE_PA;
	seg->flags_pd = cpu_to_be32(pdn | MLX5_MKEY_LEN64);
	seg->qpn_mkey7_0 = cpu_to_be32(0xffffff << 8);
	seg->start_addr = 0;

	err = mlx5_core_create_mkey(dev->mdev, &mr, in, sizeof(*in),
				    NULL, NULL, NULL);
	if (err) {
		mlx5_ib_warn(dev, "failed to create mkey, %d\n", err);
		goto err_in;
	}

	kfree(in);
	*key = mr.key;

	return 0;

err_in:
	kfree(in);

	return err;
}

static void free_pa_mkey(struct mlx5_ib_dev *dev, u32 key)
{
	struct mlx5_core_mr mr;
	int err;

	memset(&mr, 0, sizeof(mr));
	mr.key = key;
	err = mlx5_core_destroy_mkey(dev->mdev, &mr);
	if (err)
		mlx5_ib_warn(dev, "failed to destroy mkey 0x%x\n", key);
}

static struct ib_pd *mlx5_ib_alloc_pd(struct ib_device *ibdev,
				      struct ib_ucontext *context,
				      struct ib_udata *udata)
{
	struct mlx5_ib_alloc_pd_resp resp;
	struct mlx5_ib_pd *pd;
	int err;

	pd = kmalloc(sizeof(*pd), GFP_KERNEL);
	if (!pd)
		return ERR_PTR(-ENOMEM);

	err = mlx5_core_alloc_pd(to_mdev(ibdev)->mdev, &pd->pdn);
	if (err) {
		kfree(pd);
		return ERR_PTR(err);
	}

	if (context) {
		resp.pdn = pd->pdn;
		if (ib_copy_to_udata(udata, &resp, sizeof(resp))) {
			mlx5_core_dealloc_pd(to_mdev(ibdev)->mdev, pd->pdn);
			kfree(pd);
			return ERR_PTR(-EFAULT);
		}
	} else {
		err = alloc_pa_mkey(to_mdev(ibdev), &pd->pa_lkey, pd->pdn);
		if (err) {
			mlx5_core_dealloc_pd(to_mdev(ibdev)->mdev, pd->pdn);
			kfree(pd);
			return ERR_PTR(err);
		}
	}

	return &pd->ibpd;
}

static int mlx5_ib_dealloc_pd(struct ib_pd *pd)
{
	struct mlx5_ib_dev *mdev = to_mdev(pd->device);
	struct mlx5_ib_pd *mpd = to_mpd(pd);

	if (!pd->uobject)
		free_pa_mkey(mdev, mpd->pa_lkey);

	mlx5_core_dealloc_pd(mdev->mdev, mpd->pdn);
	kfree(mpd);

	return 0;
}

static int mlx5_ib_mcg_attach(struct ib_qp *ibqp, union ib_gid *gid, u16 lid)
{
	struct mlx5_ib_dev *dev = to_mdev(ibqp->device);
	int err;

	err = mlx5_core_attach_mcg(dev->mdev, gid, ibqp->qp_num);
	if (err)
		mlx5_ib_warn(dev, "failed attaching QPN 0x%x, MGID %pI6\n",
			     ibqp->qp_num, gid->raw);

	return err;
}

static int mlx5_ib_mcg_detach(struct ib_qp *ibqp, union ib_gid *gid, u16 lid)
{
	struct mlx5_ib_dev *dev = to_mdev(ibqp->device);
	int err;

	err = mlx5_core_detach_mcg(dev->mdev, gid, ibqp->qp_num);
	if (err)
		mlx5_ib_warn(dev, "failed detaching QPN 0x%x, MGID %pI6\n",
			     ibqp->qp_num, gid->raw);

	return err;
}

static int init_node_data(struct mlx5_ib_dev *dev)
{
	struct ib_smp *in_mad  = NULL;
	struct ib_smp *out_mad = NULL;
	int err = -ENOMEM;

	in_mad  = kzalloc(sizeof(*in_mad), GFP_KERNEL);
	out_mad = kmalloc(sizeof(*out_mad), GFP_KERNEL);
	if (!in_mad || !out_mad)
		goto out;

	init_query_mad(in_mad);
	in_mad->attr_id = IB_SMP_ATTR_NODE_DESC;

	err = mlx5_MAD_IFC(dev, 1, 1, 1, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	memcpy(dev->ib_dev.node_desc, out_mad->data, 64);

	in_mad->attr_id = IB_SMP_ATTR_NODE_INFO;

	err = mlx5_MAD_IFC(dev, 1, 1, 1, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	dev->mdev->rev_id = be32_to_cpup((__be32 *)(out_mad->data + 32));
	memcpy(&dev->ib_dev.node_guid, out_mad->data + 12, 8);

out:
	kfree(in_mad);
	kfree(out_mad);
	return err;
}

static ssize_t show_fw_pages(struct device *device, struct device_attribute *attr,
			     char *buf)
{
	struct mlx5_ib_dev *dev =
		container_of(device, struct mlx5_ib_dev, ib_dev.dev);

	return sprintf(buf, "%d\n", dev->mdev->priv.fw_pages);
}

static ssize_t show_reg_pages(struct device *device,
			      struct device_attribute *attr, char *buf)
{
	struct mlx5_ib_dev *dev =
		container_of(device, struct mlx5_ib_dev, ib_dev.dev);

	return sprintf(buf, "%d\n", atomic_read(&dev->mdev->priv.reg_pages));
}

static ssize_t show_hca(struct device *device, struct device_attribute *attr,
			char *buf)
{
	struct mlx5_ib_dev *dev =
		container_of(device, struct mlx5_ib_dev, ib_dev.dev);
	return sprintf(buf, "MT%d\n", dev->mdev->pdev->device);
}

static ssize_t show_fw_ver(struct device *device, struct device_attribute *attr,
			   char *buf)
{
	struct mlx5_ib_dev *dev =
		container_of(device, struct mlx5_ib_dev, ib_dev.dev);
	return sprintf(buf, "%d.%d.%d\n", fw_rev_maj(dev->mdev),
		       fw_rev_min(dev->mdev), fw_rev_sub(dev->mdev));
}

static ssize_t show_rev(struct device *device, struct device_attribute *attr,
			char *buf)
{
	struct mlx5_ib_dev *dev =
		container_of(device, struct mlx5_ib_dev, ib_dev.dev);
	return sprintf(buf, "%x\n", dev->mdev->rev_id);
}

static ssize_t show_board(struct device *device, struct device_attribute *attr,
			  char *buf)
{
	struct mlx5_ib_dev *dev =
		container_of(device, struct mlx5_ib_dev, ib_dev.dev);
	return sprintf(buf, "%.*s\n", MLX5_BOARD_ID_LEN,
		       dev->mdev->board_id);
}

static DEVICE_ATTR(hw_rev,   S_IRUGO, show_rev,    NULL);
static DEVICE_ATTR(fw_ver,   S_IRUGO, show_fw_ver, NULL);
static DEVICE_ATTR(hca_type, S_IRUGO, show_hca,    NULL);
static DEVICE_ATTR(board_id, S_IRUGO, show_board,  NULL);
static DEVICE_ATTR(fw_pages, S_IRUGO, show_fw_pages, NULL);
static DEVICE_ATTR(reg_pages, S_IRUGO, show_reg_pages, NULL);

static struct device_attribute *mlx5_class_attributes[] = {
	&dev_attr_hw_rev,
	&dev_attr_fw_ver,
	&dev_attr_hca_type,
	&dev_attr_board_id,
	&dev_attr_fw_pages,
	&dev_attr_reg_pages,
};

static void mlx5_ib_handle_internal_error(struct mlx5_ib_dev *ibdev)
{
	struct mlx5_ib_qp *mqp;
	struct mlx5_ib_cq *send_mcq, *recv_mcq;
	struct mlx5_core_cq *mcq;
	struct list_head cq_armed_list;
	unsigned long flags_qp;
	unsigned long flags_cq;
	unsigned long flags;

	pr_warn("%s was started\n", __func__);
	INIT_LIST_HEAD(&cq_armed_list);

	/* Go over qp list reside on that ibdev, sync with create/destroy qp.*/
	spin_lock_irqsave(&ibdev->reset_flow_resource_lock, flags);
	list_for_each_entry(mqp, &ibdev->qp_list, qps_list) {
		spin_lock_irqsave(&mqp->sq.lock, flags_qp);
		if (mqp->sq.tail != mqp->sq.head) {
			send_mcq = to_mcq(mqp->ibqp.send_cq);
			spin_lock_irqsave(&send_mcq->lock, flags_cq);
			if (send_mcq->mcq.comp &&
			    mqp->ibqp.send_cq->comp_handler) {
				if (!send_mcq->mcq.reset_notify_added) {
					send_mcq->mcq.reset_notify_added = 1;
					list_add_tail(&send_mcq->mcq.reset_notify,
						      &cq_armed_list);
				}
			}
			spin_unlock_irqrestore(&send_mcq->lock, flags_cq);
		}
		spin_unlock_irqrestore(&mqp->sq.lock, flags_qp);
		spin_lock_irqsave(&mqp->rq.lock, flags_qp);
		/* no handling is needed for SRQ */
		if (!mqp->ibqp.srq) {
			if (mqp->rq.tail != mqp->rq.head) {
				recv_mcq = to_mcq(mqp->ibqp.recv_cq);
				spin_lock_irqsave(&recv_mcq->lock, flags_cq);
				if (recv_mcq->mcq.comp &&
				    mqp->ibqp.recv_cq->comp_handler) {
					if (!recv_mcq->mcq.reset_notify_added) {
						recv_mcq->mcq.reset_notify_added = 1;
						list_add_tail(&recv_mcq->mcq.reset_notify,
							      &cq_armed_list);
					}
				}
				spin_unlock_irqrestore(&recv_mcq->lock,
						       flags_cq);
			}
		}
		spin_unlock_irqrestore(&mqp->rq.lock, flags_qp);
	}
	/*At that point all inflight post send were put to be executed as of we
	 * lock/unlock above locks Now need to arm all involved CQs.
	 */
	list_for_each_entry(mcq, &cq_armed_list, reset_notify) {
		mcq->comp(mcq);
	}
	spin_unlock_irqrestore(&ibdev->reset_flow_resource_lock, flags);
	pr_warn("%s ended\n", __func__);
	return;
}

static void mlx5_ib_event(struct mlx5_core_dev *dev, void *context,
			  enum mlx5_dev_event event, void *data)
{
	struct mlx5_ib_dev *ibdev = context;
	struct ib_event ibev;

	u8 port = 0;

	switch (event) {
	case MLX5_DEV_EVENT_SYS_ERROR:
		ibdev->ib_active = false;
		ibev.event = IB_EVENT_DEVICE_FATAL;
		mlx5_ib_handle_internal_error(ibdev);
		break;

	case MLX5_DEV_EVENT_PORT_UP:
		ibev.event = IB_EVENT_PORT_ACTIVE;
		port = *(u8 *)data;
		break;

	case MLX5_DEV_EVENT_PORT_DOWN:
		ibev.event = IB_EVENT_PORT_ERR;
		port = *(u8 *)data;
		break;

	case MLX5_DEV_EVENT_PORT_INITIALIZED:
		/* not used by ULPs */
		return;

	case MLX5_DEV_EVENT_LID_CHANGE:
		ibev.event = IB_EVENT_LID_CHANGE;
		port = *(u8 *)data;
		break;

	case MLX5_DEV_EVENT_PKEY_CHANGE:
		ibev.event = IB_EVENT_PKEY_CHANGE;
		port = *(u8 *)data;
		break;

	case MLX5_DEV_EVENT_GUID_CHANGE:
		ibev.event = IB_EVENT_GID_CHANGE;
		port = *(u8 *)data;
		break;

	case MLX5_DEV_EVENT_CLIENT_REREG:
		ibev.event = IB_EVENT_CLIENT_REREGISTER;
		port = *(u8 *)data;
		break;
	}

	ibev.device	      = &ibdev->ib_dev;
	ibev.element.port_num = port;

	if (port < 1 || port > ibdev->num_ports) {
		mlx5_ib_warn(ibdev, "warning: event on port %d\n", port);
		return;
	}

	if (ibdev->ib_active)
		ib_dispatch_event(&ibev);
}

static void get_ext_port_caps(struct mlx5_ib_dev *dev)
{
	int port;

	for (port = 1; port <= dev->mdev->caps.num_ports; port++)
		mlx5_query_ext_port_caps(dev, port);
}

static void config_atomic_responder(struct mlx5_ib_dev *dev,
				    struct ib_exp_device_attr *props)
{
	enum ib_atomic_cap cap = props->base.atomic_cap;

	if (cap == IB_ATOMIC_HCA ||
	    cap == IB_ATOMIC_GLOB ||
	    cap == IB_ATOMIC_HCA_REPLY_BE)
		dev->enable_atomic_resp = 1;
}

int mlx5_ib_exp_query_device(struct ib_device *ibdev,
			     struct ib_exp_device_attr *props)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	u64 flags;
	int err;

	err = query_device(ibdev, &props->base, 1);
	if (err)
		return err;

	props->exp_comp_mask = IB_EXP_DEVICE_ATTR_CAP_FLAGS2;
	props->device_cap_flags2 = 0;
	flags = dev->mdev->caps.flags;
	props->exp_comp_mask |= IB_EXP_DEVICE_ATTR_DC_REQ_RD;
	props->exp_comp_mask |= IB_EXP_DEVICE_ATTR_DC_RES_RD;
	props->exp_comp_mask |= IB_EXP_DEVICE_ATTR_MAX_DCT;
	if (flags & MLX5_DEV_CAP_FLAG_DCT) {
		props->device_cap_flags2 |= IB_EXP_DEVICE_DC_TRANSPORT;
		props->dc_rd_req = dev->mdev->caps.max_ra_req_dc;
		props->dc_rd_res = dev->mdev->caps.max_ra_res_dc;
		props->max_dct = props->base.max_qp;
	} else {
		props->dc_rd_req = 0;
		props->dc_rd_res = 0;
		props->max_dct = 0;
	}

	props->exp_comp_mask |= IB_EXP_DEVICE_ATTR_INLINE_RECV_SZ;
	if (flags & MLX5_DEV_CAP_FLAG_SCATER_CQE)
		props->inline_recv_sz = MLX5_MAX_INLINE_RECEIVE_SIZE;
	else
		props->inline_recv_sz = 0;

	ext_atomic_caps(dev, props);
	props->exp_comp_mask |= IB_EXP_DEVICE_ATTR_EXT_ATOMIC_ARGS;

	props->device_cap_flags2 |= IB_EXP_DEVICE_NOP;

	props->device_cap_flags2 |= IB_EXP_DEVICE_UMR;
	props->umr_caps.max_reg_descriptors = dev->mdev->caps.max_reg_descriptors;
	props->umr_caps.max_send_wqe_inline_klms = 20;
	props->umr_caps.max_umr_recursion_depth = dev->mdev->caps.max_umr_indirection;
	props->umr_caps.max_umr_stride_dimenson = 1;
	props->exp_comp_mask |= IB_EXP_DEVICE_ATTR_UMR;
#ifdef CONFIG_INFINIBAND_ON_DEMAND_PAGING
	if (dev->mdev->caps.flags & MLX5_DEV_CAP_FLAG_ON_DMND_PG) {
		props->exp_comp_mask |= IB_EXP_DEVICE_ATTR_ODP;
		props->device_cap_flags2 |= IB_EXP_DEVICE_ODP;
		props->odp_caps.general_odp_caps = IB_ODP_SUPPORT;
		MLX5_IB_COPY_ODP_CAPS(dev->mdev->caps.odp_caps, props->odp_caps,
				      per_transport_caps.ud_odp_caps, SEND);
		MLX5_IB_COPY_ODP_CAPS(dev->mdev->caps.odp_caps, props->odp_caps,
				      per_transport_caps.rc_odp_caps, SEND);
		MLX5_IB_COPY_ODP_CAPS(dev->mdev->caps.odp_caps, props->odp_caps,
				      per_transport_caps.rc_odp_caps, RECV);
		MLX5_IB_COPY_ODP_CAPS(dev->mdev->caps.odp_caps, props->odp_caps,
				      per_transport_caps.rc_odp_caps, WRITE);
		MLX5_IB_COPY_ODP_CAPS(dev->mdev->caps.odp_caps, props->odp_caps,
				      per_transport_caps.rc_odp_caps, READ);
	}
#endif
	return err;
}

static int get_port_caps(struct mlx5_ib_dev *dev)
{
	struct ib_exp_device_attr *dprops = NULL;
	struct ib_port_attr *pprops = NULL;
	int err = 0;
	int port;

	pprops = kmalloc(sizeof(*pprops), GFP_KERNEL);
	if (!pprops)
		goto out;

	dprops = kmalloc(sizeof(*dprops), GFP_KERNEL);
	if (!dprops)
		goto out;

	err = mlx5_ib_exp_query_device(&dev->ib_dev, dprops);
	if (err) {
		mlx5_ib_warn(dev, "query_device failed %d\n", err);
		goto out;
	}
	config_atomic_responder(dev, dprops);

	for (port = 1; port <= dev->mdev->caps.num_ports; port++) {
		err = mlx5_ib_query_port(&dev->ib_dev, port, pprops);
		if (err) {
			mlx5_ib_warn(dev, "query_port %d failed %d\n", port, err);
			break;
		}
		dev->mdev->caps.port[port - 1].pkey_table_len = dprops->base.max_pkeys;
		dev->mdev->caps.port[port - 1].gid_table_len = pprops->gid_tbl_len;
		mlx5_ib_dbg(dev, "pkey_table_len %d, gid_table_len %d\n",
			    dprops->base.max_pkeys, pprops->gid_tbl_len);
	}

out:
	kfree(pprops);
	kfree(dprops);

	return err;
}

static void destroy_umrc_res(struct mlx5_ib_dev *dev)
{
	int err;

	err = mlx5_mr_cache_cleanup(dev);
	if (err)
		mlx5_ib_warn(dev, "mr cache cleanup failed\n");

	mlx5_ib_destroy_qp(dev->umrc.qp);
	ib_destroy_cq(dev->umrc.cq);
	ib_dereg_mr(dev->umrc.mr);
	ib_dealloc_pd(dev->umrc.pd);
}

enum {
	MAX_UMR_WR = 128,
};

static int create_umr_res(struct mlx5_ib_dev *dev)
{
	struct ib_qp_init_attr *init_attr = NULL;
	struct ib_qp_attr *attr = NULL;
	struct ib_pd *pd;
	struct ib_cq *cq;
	struct ib_qp *qp;
	struct ib_mr *mr;
	int ret;

	attr = kzalloc(sizeof(*attr), GFP_KERNEL);
	init_attr = kzalloc(sizeof(*init_attr), GFP_KERNEL);
	if (!attr || !init_attr) {
		ret = -ENOMEM;
		goto error_0;
	}

	pd = ib_alloc_pd(&dev->ib_dev);
	if (IS_ERR(pd)) {
		mlx5_ib_dbg(dev, "Couldn't create PD for sync UMR QP\n");
		ret = PTR_ERR(pd);
		goto error_0;
	}

	mr = ib_get_dma_mr(pd,  IB_ACCESS_LOCAL_WRITE);
	if (IS_ERR(mr)) {
		mlx5_ib_dbg(dev, "Couldn't create DMA MR for sync UMR QP\n");
		ret = PTR_ERR(mr);
		goto error_1;
	}

	cq = ib_create_cq(&dev->ib_dev, mlx5_umr_cq_handler, NULL, NULL, 128,
			  0);
	if (IS_ERR(cq)) {
		mlx5_ib_dbg(dev, "Couldn't create CQ for sync UMR QP\n");
		ret = PTR_ERR(cq);
		goto error_2;
	}
	ib_req_notify_cq(cq, IB_CQ_NEXT_COMP);

	init_attr->send_cq = cq;
	init_attr->recv_cq = cq;
	init_attr->sq_sig_type = IB_SIGNAL_ALL_WR;
	init_attr->cap.max_send_wr = MAX_UMR_WR;
	init_attr->cap.max_send_sge = 1;
	init_attr->qp_type = MLX5_IB_QPT_REG_UMR;
	init_attr->port_num = 1;
	qp = mlx5_ib_create_qp(pd, init_attr, NULL);
	if (IS_ERR(qp)) {
		mlx5_ib_dbg(dev, "Couldn't create sync UMR QP\n");
		ret = PTR_ERR(qp);
		goto error_3;
	}
	qp->device     = &dev->ib_dev;
	qp->real_qp    = qp;
	qp->uobject    = NULL;
	qp->qp_type    = MLX5_IB_QPT_REG_UMR;

	attr->qp_state = IB_QPS_INIT;
	attr->port_num = 1;
	ret = mlx5_ib_modify_qp(qp, attr, IB_QP_STATE | IB_QP_PKEY_INDEX |
				IB_QP_PORT, NULL);
	if (ret) {
		mlx5_ib_dbg(dev, "Couldn't modify UMR QP\n");
		goto error_4;
	}

	memset(attr, 0, sizeof(*attr));
	attr->qp_state = IB_QPS_RTR;
	attr->path_mtu = IB_MTU_256;

	ret = mlx5_ib_modify_qp(qp, attr, IB_QP_STATE, NULL);
	if (ret) {
		mlx5_ib_dbg(dev, "Couldn't modify umr QP to rtr\n");
		goto error_4;
	}

	memset(attr, 0, sizeof(*attr));
	attr->qp_state = IB_QPS_RTS;
	ret = mlx5_ib_modify_qp(qp, attr, IB_QP_STATE, NULL);
	if (ret) {
		mlx5_ib_dbg(dev, "Couldn't modify umr QP to rts\n");
		goto error_4;
	}

	dev->umrc.qp = qp;
	dev->umrc.cq = cq;
	dev->umrc.mr = mr;
	dev->umrc.pd = pd;

	sema_init(&dev->umrc.sem, MAX_UMR_WR);
	ret = mlx5_mr_cache_init(dev);
	if (ret) {
		mlx5_ib_warn(dev, "mr cache init failed %d\n", ret);
		goto error_4;
	}

	kfree(attr);
	kfree(init_attr);

	return 0;

error_4:
	mlx5_ib_destroy_qp(qp);

error_3:
	ib_destroy_cq(cq);

error_2:
	ib_dereg_mr(mr);

error_1:
	ib_dealloc_pd(pd);

error_0:
	kfree(attr);
	kfree(init_attr);
	return ret;
}

static int create_dev_resources(struct mlx5_ib_resources *devr)
{
	struct ib_srq_init_attr attr;
	struct mlx5_ib_dev *dev;
	struct ib_cq_init_attr cq_attr;
	int ret = 0;

	dev = container_of(devr, struct mlx5_ib_dev, devr);

	devr->p0 = mlx5_ib_alloc_pd(&dev->ib_dev, NULL, NULL);
	if (IS_ERR(devr->p0)) {
		ret = PTR_ERR(devr->p0);
		goto error0;
	}
	devr->p0->device  = &dev->ib_dev;
	devr->p0->uobject = NULL;
	atomic_set(&devr->p0->usecnt, 0);

	memset(&cq_attr, 0, sizeof(cq_attr));
	cq_attr.cqe = 1;
	devr->c0 = mlx5_ib_create_cq(&dev->ib_dev, &cq_attr, NULL, NULL);
	if (IS_ERR(devr->c0)) {
		ret = PTR_ERR(devr->c0);
		goto error1;
	}
	devr->c0->device        = &dev->ib_dev;
	devr->c0->uobject       = NULL;
	devr->c0->comp_handler  = NULL;
	devr->c0->event_handler = NULL;
	devr->c0->cq_context    = NULL;
	atomic_set(&devr->c0->usecnt, 0);

	devr->x0 = mlx5_ib_alloc_xrcd(&dev->ib_dev, NULL, NULL);
	if (IS_ERR(devr->x0)) {
		ret = PTR_ERR(devr->x0);
		goto error2;
	}
	devr->x0->device = &dev->ib_dev;
	devr->x0->inode = NULL;
	atomic_set(&devr->x0->usecnt, 0);
	mutex_init(&devr->x0->tgt_qp_mutex);
	INIT_LIST_HEAD(&devr->x0->tgt_qp_list);

	devr->x1 = mlx5_ib_alloc_xrcd(&dev->ib_dev, NULL, NULL);
	if (IS_ERR(devr->x1)) {
		ret = PTR_ERR(devr->x1);
		goto error3;
	}
	devr->x1->device = &dev->ib_dev;
	devr->x1->inode = NULL;
	atomic_set(&devr->x1->usecnt, 0);
	mutex_init(&devr->x1->tgt_qp_mutex);
	INIT_LIST_HEAD(&devr->x1->tgt_qp_list);

	memset(&attr, 0, sizeof(attr));
	attr.attr.max_sge = 1;
	attr.attr.max_wr = 1;
	attr.srq_type = IB_SRQT_XRC;
	attr.ext.xrc.cq = devr->c0;
	attr.ext.xrc.xrcd = devr->x0;

	devr->s0 = mlx5_ib_create_srq(devr->p0, &attr, NULL);
	if (IS_ERR(devr->s0)) {
		ret = PTR_ERR(devr->s0);
		goto error4;
	}
	devr->s0->device	= &dev->ib_dev;
	devr->s0->pd		= devr->p0;
	devr->s0->uobject       = NULL;
	devr->s0->event_handler = NULL;
	devr->s0->srq_context   = NULL;
	devr->s0->srq_type      = IB_SRQT_XRC;
	devr->s0->ext.xrc.xrcd	= devr->x0;
	devr->s0->ext.xrc.cq	= devr->c0;
	atomic_inc(&devr->s0->ext.xrc.xrcd->usecnt);
	atomic_inc(&devr->s0->ext.xrc.cq->usecnt);
	atomic_inc(&devr->p0->usecnt);
	atomic_set(&devr->s0->usecnt, 0);

	return 0;

error4:
	mlx5_ib_dealloc_xrcd(devr->x1);
error3:
	mlx5_ib_dealloc_xrcd(devr->x0);
error2:
	mlx5_ib_destroy_cq(devr->c0);
error1:
	mlx5_ib_dealloc_pd(devr->p0);
error0:
	return ret;
}

static void destroy_dev_resources(struct mlx5_ib_resources *devr)
{
	mlx5_ib_destroy_srq(devr->s0);
	mlx5_ib_dealloc_xrcd(devr->x0);
	mlx5_ib_dealloc_xrcd(devr->x1);
	mlx5_ib_destroy_cq(devr->c0);
	mlx5_ib_dealloc_pd(devr->p0);
}


static void *mlx5_ib_add(struct mlx5_core_dev *mdev)
{
	struct mlx5_ib_dev *dev;
	int err;
	int i;

	printk_once(KERN_INFO "%s", mlx5_version);

	dev = (struct mlx5_ib_dev *)ib_alloc_device(sizeof(*dev));
	if (!dev)
		return NULL;
	dev->mdev = mdev;

	err = get_port_caps(dev);
	if (err)
		goto err_dealloc;

	get_ext_port_caps(dev);

	err = alloc_comp_eqs(dev);
	if (err)
		goto err_dealloc;

	MLX5_INIT_DOORBELL_LOCK(&dev->uar_lock);

	strlcpy(dev->ib_dev.name, "mlx5_%d", IB_DEVICE_NAME_MAX);
	dev->ib_dev.owner		= THIS_MODULE;
	dev->ib_dev.node_type		= RDMA_NODE_IB_CA;
	dev->ib_dev.local_dma_lkey	= mdev->caps.reserved_lkey;
	dev->num_ports		= mdev->caps.num_ports;
	dev->ib_dev.phys_port_cnt     = dev->num_ports;
	dev->ib_dev.num_comp_vectors	= dev->num_comp_vectors;
	dev->ib_dev.dma_device	= &mdev->pdev->dev;

	dev->ib_dev.uverbs_abi_ver	= MLX5_IB_UVERBS_ABI_VERSION;
	dev->ib_dev.uverbs_cmd_mask	=
		(1ull << IB_USER_VERBS_CMD_GET_CONTEXT)		|
		(1ull << IB_USER_VERBS_CMD_QUERY_DEVICE)	|
		(1ull << IB_USER_VERBS_CMD_QUERY_PORT)		|
		(1ull << IB_USER_VERBS_CMD_ALLOC_PD)		|
		(1ull << IB_USER_VERBS_CMD_DEALLOC_PD)		|
		(1ull << IB_USER_VERBS_CMD_REG_MR)		|
		(1ull << IB_USER_VERBS_CMD_DEREG_MR)		|
		(1ull << IB_USER_VERBS_CMD_CREATE_COMP_CHANNEL)	|
		(1ull << IB_USER_VERBS_CMD_CREATE_CQ)		|
		(1ull << IB_USER_VERBS_CMD_RESIZE_CQ)		|
		(1ull << IB_USER_VERBS_CMD_DESTROY_CQ)		|
		(1ull << IB_USER_VERBS_CMD_CREATE_QP)		|
		(1ull << IB_USER_VERBS_CMD_MODIFY_QP)		|
		(1ull << IB_USER_VERBS_CMD_QUERY_QP)		|
		(1ull << IB_USER_VERBS_CMD_DESTROY_QP)		|
		(1ull << IB_USER_VERBS_CMD_ATTACH_MCAST)	|
		(1ull << IB_USER_VERBS_CMD_DETACH_MCAST)	|
		(1ull << IB_USER_VERBS_CMD_CREATE_SRQ)		|
		(1ull << IB_USER_VERBS_CMD_MODIFY_SRQ)		|
		(1ull << IB_USER_VERBS_CMD_QUERY_SRQ)		|
		(1ull << IB_USER_VERBS_CMD_DESTROY_SRQ)		|
		(1ull << IB_USER_VERBS_CMD_CREATE_XSRQ)		|
		(1ull << IB_USER_VERBS_CMD_OPEN_QP);
	dev->ib_dev.uverbs_exp_cmd_mask	=
		(1ull << IB_USER_VERBS_EXP_CMD_REG_MR_EX)       |
#ifdef CONFIG_INFINIBAND_ON_DEMAND_PAGING
		(1ull << IB_USER_VERBS_EXP_CMD_PREFETCH_MR)	|
#endif
		(1ull << IB_USER_VERBS_EXP_CMD_MODIFY_QP)	|
		(1ull << IB_USER_VERBS_EXP_CMD_CREATE_CQ)	|
		(1ull << IB_USER_VERBS_EXP_CMD_MODIFY_CQ);

	dev->ib_dev.query_device	= mlx5_ib_query_device;
	dev->ib_dev.query_port		= mlx5_ib_query_port;
	dev->ib_dev.query_gid		= mlx5_ib_query_gid;
	dev->ib_dev.query_pkey		= mlx5_ib_query_pkey;
	dev->ib_dev.modify_device	= mlx5_ib_modify_device;
	dev->ib_dev.modify_port		= mlx5_ib_modify_port;
	dev->ib_dev.alloc_ucontext	= mlx5_ib_alloc_ucontext;
	dev->ib_dev.dealloc_ucontext	= mlx5_ib_dealloc_ucontext;
	dev->ib_dev.mmap		= mlx5_ib_mmap;
	dev->ib_dev.get_unmapped_area	= mlx5_ib_get_unmapped_area;
	dev->ib_dev.alloc_pd		= mlx5_ib_alloc_pd;
	dev->ib_dev.dealloc_pd		= mlx5_ib_dealloc_pd;
	dev->ib_dev.create_ah		= mlx5_ib_create_ah;
	dev->ib_dev.query_ah		= mlx5_ib_query_ah;
	dev->ib_dev.destroy_ah		= mlx5_ib_destroy_ah;
	dev->ib_dev.create_srq		= mlx5_ib_create_srq;
	dev->ib_dev.modify_srq		= mlx5_ib_modify_srq;
	dev->ib_dev.query_srq		= mlx5_ib_query_srq;
	dev->ib_dev.destroy_srq		= mlx5_ib_destroy_srq;
	dev->ib_dev.post_srq_recv	= mlx5_ib_post_srq_recv;
	dev->ib_dev.create_qp		= mlx5_ib_create_qp;
	dev->ib_dev.modify_qp		= mlx5_ib_modify_qp;
	dev->ib_dev.query_qp		= mlx5_ib_query_qp;
	dev->ib_dev.destroy_qp		= mlx5_ib_destroy_qp;
	dev->ib_dev.post_send		= mlx5_ib_post_send;
	dev->ib_dev.post_recv		= mlx5_ib_post_recv;
	dev->ib_dev.create_cq		= mlx5_ib_create_cq;
	dev->ib_dev.modify_cq		= mlx5_ib_modify_cq;
	dev->ib_dev.resize_cq		= mlx5_ib_resize_cq;
	dev->ib_dev.destroy_cq		= mlx5_ib_destroy_cq;
	dev->ib_dev.poll_cq		= mlx5_ib_poll_cq;
	dev->ib_dev.req_notify_cq	= mlx5_ib_arm_cq;
	dev->ib_dev.get_dma_mr		= mlx5_ib_get_dma_mr;
	dev->ib_dev.reg_user_mr		= mlx5_ib_reg_user_mr;
	dev->ib_dev.dereg_mr		= mlx5_ib_dereg_mr;
	dev->ib_dev.destroy_mr		= mlx5_ib_destroy_mr;
	dev->ib_dev.attach_mcast	= mlx5_ib_mcg_attach;
	dev->ib_dev.detach_mcast	= mlx5_ib_mcg_detach;
	dev->ib_dev.process_mad		= mlx5_ib_process_mad;
	dev->ib_dev.create_mr		= mlx5_ib_create_mr;
	dev->ib_dev.alloc_fast_reg_mr	= mlx5_ib_alloc_fast_reg_mr;
	dev->ib_dev.alloc_fast_reg_page_list = mlx5_ib_alloc_fast_reg_page_list;
	dev->ib_dev.free_fast_reg_page_list  = mlx5_ib_free_fast_reg_page_list;
	dev->ib_dev.check_mr_status	= mlx5_ib_check_mr_status;
#ifndef CONFIG_COMPAT_MISS_TASK_FUNCS
	dev->ib_dev.disassociate_ucontext = mlx5_ib_disassociate_ucontext;
#endif
#ifdef CONFIG_INFINIBAND_ON_DEMAND_PAGING
	dev->ib_dev.exp_prefetch_mr	= mlx5_ib_prefetch_mr;
#endif
	if (mdev->caps.flags & MLX5_DEV_CAP_FLAG_XRC) {
		dev->ib_dev.alloc_xrcd = mlx5_ib_alloc_xrcd;
		dev->ib_dev.dealloc_xrcd = mlx5_ib_dealloc_xrcd;
		dev->ib_dev.uverbs_cmd_mask |=
			(1ull << IB_USER_VERBS_CMD_OPEN_XRCD) |
			(1ull << IB_USER_VERBS_CMD_CLOSE_XRCD);
	}

	if (mdev->caps.flags & MLX5_DEV_CAP_FLAG_DCT) {
		dev->ib_dev.exp_create_dct = mlx5_ib_create_dct;
		dev->ib_dev.exp_destroy_dct = mlx5_ib_destroy_dct;
		dev->ib_dev.exp_query_dct = mlx5_ib_query_dct;
		dev->ib_dev.exp_arm_dct = mlx5_ib_arm_dct;
		dev->ib_dev.uverbs_exp_cmd_mask |=
			(1ull << IB_USER_VERBS_EXP_CMD_CREATE_DCT)	|
			(1ull << IB_USER_VERBS_EXP_CMD_DESTROY_DCT)	|
			(1ull << IB_USER_VERBS_EXP_CMD_QUERY_DCT)	|
			(1ull << IB_USER_VERBS_EXP_CMD_ARM_DCT);
	}
	dev->ib_dev.uverbs_exp_cmd_mask |= (1ull << IB_USER_VERBS_EXP_CMD_CREATE_MR);

	dev->ib_dev.exp_create_qp = mlx5_ib_exp_create_qp;
	dev->ib_dev.uverbs_exp_cmd_mask |= (1ull << IB_USER_VERBS_EXP_CMD_CREATE_QP);

	dev->ib_dev.exp_query_device = mlx5_ib_exp_query_device;
	dev->ib_dev.uverbs_exp_cmd_mask	|= (1 << IB_USER_VERBS_EXP_CMD_QUERY_DEVICE);
	dev->ib_dev.exp_query_mkey	= mlx5_ib_exp_query_mkey;
	dev->ib_dev.uverbs_exp_cmd_mask	|= (1 << IB_USER_VERBS_EXP_CMD_QUERY_MKEY);

	err = init_node_data(dev);
	if (err)
		goto err_eqs;

	mutex_init(&dev->cap_mask_mutex);
	INIT_LIST_HEAD(&dev->qp_list);
	spin_lock_init(&dev->reset_flow_resource_lock);

	err = create_dev_resources(&dev->devr);
	if (err)
		goto err_eqs;

	err = mlx5_ib_odp_init_one(dev);
	if (err)
		goto err_rsrc;

	err = ib_register_device(&dev->ib_dev, NULL);
	if (err)
		goto err_odp;
	err = create_umr_res(dev);
	if (err)
		goto err_dev;

	for (i = 0; i < ARRAY_SIZE(mlx5_class_attributes); i++) {
		err = device_create_file(&dev->ib_dev.dev,
					 mlx5_class_attributes[i]);
		if (err)
			goto err_umrc;
	}

	dev->ib_active = true;

	return dev;

err_umrc:
	destroy_umrc_res(dev);

err_dev:
	ib_unregister_device(&dev->ib_dev);

err_odp:
	mlx5_ib_odp_remove_one(dev);

err_rsrc:
	destroy_dev_resources(&dev->devr);

err_eqs:
	free_comp_eqs(dev);

err_dealloc:
	ib_dealloc_device((struct ib_device *)dev);

	return NULL;
}

static void mlx5_ib_remove(struct mlx5_core_dev *mdev, void *context)
{
	struct mlx5_ib_dev *dev = context;

	ib_unregister_device(&dev->ib_dev);
	destroy_umrc_res(dev);
	mlx5_ib_odp_remove_one(dev);
	destroy_dev_resources(&dev->devr);
	free_comp_eqs(dev);
	ib_dealloc_device(&dev->ib_dev);
}

static struct mlx5_interface mlx5_ib_interface = {
	.add            = mlx5_ib_add,
	.remove         = mlx5_ib_remove,
	.event          = mlx5_ib_event,
};

static int __init mlx5_ib_init(void)
{
	int err;

	err = mlx5_ib_odp_init();
	if (err)
		return err;

	if (deprecated_prof_sel != 2)
		pr_warn("prof_sel is deprecated for mlx5_ib, set it for mlx5_core\n");

	err = mlx5_register_interface(&mlx5_ib_interface);
	if (err)
		goto clean_odp;

	return err;

clean_odp:
	mlx5_ib_odp_cleanup();
	return err;
}

static void __exit mlx5_ib_cleanup(void)
{
	mlx5_unregister_interface(&mlx5_ib_interface);
	mlx5_ib_odp_cleanup();
}

module_init(mlx5_ib_init);
module_exit(mlx5_ib_cleanup);
