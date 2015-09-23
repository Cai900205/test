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
#include <linux/mlx5/driver.h>
#include <linux/mlx5/cq.h>
#include <linux/mlx5/qp.h>
#include <linux/mlx5/srq.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include "mlx5_core.h"

#define DRIVER_NAME "mlx5_core"
#define DRIVER_VERSION "1.0"
#define DRIVER_RELDATE	"June 2013"

MODULE_AUTHOR("Eli Cohen <eli@mellanox.com>");
MODULE_DESCRIPTION("Mellanox ConnectX-IB HCA core library");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(DRIVER_VERSION);

int mlx5_core_debug_mask;
module_param_named(debug_mask, mlx5_core_debug_mask, int, 0644);
MODULE_PARM_DESC(debug_mask, "debug mask: 1 = dump cmd data, 2 = dump cmd exec time, 3 = both. Default=0");

#define MLX5_DEFAULT_PROF	2
static int prof_sel = MLX5_DEFAULT_PROF;
module_param_named(prof_sel, prof_sel, int, 0444);
MODULE_PARM_DESC(prof_sel, "profile selector. Valid range 0 - 2");

struct workqueue_struct *mlx5_core_wq;
static LIST_HEAD(intf_list);
static LIST_HEAD(dev_list);
static DEFINE_MUTEX(intf_mutex);

struct mlx5_device_context {
	struct list_head	list;
	struct mlx5_interface  *intf;
	void		       *context;
};

static struct mlx5_profile profile[] = {
	[0] = {
		.mask           = 0,
	},
	[1] = {
		.mask		= MLX5_PROF_MASK_QP_SIZE |
				  MLX5_PROF_MASK_DCT,
		.log_max_qp	= 12,
		.dct_enable	= 1,
	},
	[2] = {
		.mask		= MLX5_PROF_MASK_QP_SIZE  |
				  MLX5_PROF_MASK_MR_CACHE |
				  MLX5_PROF_MASK_DCT,
		.log_max_qp	= 17,
		.dct_enable	= 1,
		.mr_cache[0]	= {
			.size	= 500,
			.limit	= 250
		},
		.mr_cache[1]	= {
			.size	= 500,
			.limit	= 250
		},
		.mr_cache[2]	= {
			.size	= 500,
			.limit	= 250
		},
		.mr_cache[3]	= {
			.size	= 500,
			.limit	= 250
		},
		.mr_cache[4]	= {
			.size	= 500,
			.limit	= 250
		},
		.mr_cache[5]	= {
			.size	= 500,
			.limit	= 250
		},
		.mr_cache[6]	= {
			.size	= 500,
			.limit	= 250
		},
		.mr_cache[7]	= {
			.size	= 500,
			.limit	= 250
		},
		.mr_cache[8]	= {
			.size	= 500,
			.limit	= 250
		},
		.mr_cache[9]	= {
			.size	= 500,
			.limit	= 250
		},
		.mr_cache[10]	= {
			.size	= 500,
			.limit	= 250
		},
		.mr_cache[11]	= {
			.size	= 500,
			.limit	= 250
		},
		.mr_cache[12]	= {
			.size	= 64,
			.limit	= 32
		},
		.mr_cache[13]	= {
			.size	= 32,
			.limit	= 16
		},
		.mr_cache[14]	= {
			.size	= 16,
			.limit	= 8
		},
	},
};

#define FW_INIT_MAX_ITER	1000
#define FW_INIT_WAIT_MS		2

static int set_dma_caps(struct pci_dev *pdev)
{
	int err;

	err = pci_set_dma_mask(pdev, DMA_BIT_MASK(64));
	if (err) {
		dev_warn(&pdev->dev, "Warning: couldn't set 64-bit PCI DMA mask.\n");
		err = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
		if (err) {
			dev_err(&pdev->dev, "Can't set PCI DMA mask, aborting.\n");
			return err;
		}
	}

	err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64));
	if (err) {
		dev_warn(&pdev->dev,
			 "Warning: couldn't set 64-bit consistent PCI DMA mask.\n");
		err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
		if (err) {
			dev_err(&pdev->dev,
				"Can't set consistent PCI DMA mask, aborting.\n");
			return err;
		}
	}

	dma_set_max_seg_size(&pdev->dev, 2u * 1024 * 1024 * 1024);
	return err;
}

static int request_bar(struct pci_dev *pdev)
{
	int err = 0;

	if (!(pci_resource_flags(pdev, 0) & IORESOURCE_MEM)) {
		dev_err(&pdev->dev, "Missing registers BAR, aborting.\n");
		return -ENODEV;
	}

	err = pci_request_regions(pdev, DRIVER_NAME);
	if (err)
		dev_err(&pdev->dev, "Couldn't get PCI resources, aborting\n");

	return err;
}

static void release_bar(struct pci_dev *pdev)
{
	pci_release_regions(pdev);
}

static int mlx5_enable_msix(struct mlx5_core_dev *dev)
{
	struct mlx5_eq_table *table = &dev->priv.eq_table;
	int num_eqs = 1 << dev->caps.log_max_eq;
	int nvec;
	int err;
	int i;

	nvec = dev->caps.num_ports * num_online_cpus() + MLX5_EQ_VEC_COMP_BASE;
	nvec = min_t(int, nvec, num_eqs);
	if (nvec <= MLX5_EQ_VEC_COMP_BASE)
		return -ENOMEM;

	table->msix_arr = kzalloc(nvec * sizeof(*table->msix_arr), GFP_KERNEL);
	if (!table->msix_arr)
		return -ENOMEM;

	for (i = 0; i < nvec; i++)
		table->msix_arr[i].entry = i;

retry:
	table->num_comp_vectors = nvec - MLX5_EQ_VEC_COMP_BASE;
	err = pci_enable_msix(dev->pdev, table->msix_arr, nvec);
	if (err <= 0) {
		return err;
	} else if (err > 2) {
		nvec = err;
		goto retry;
	}

	mlx5_core_dbg(dev, "received %d MSI vectors out of %d requested\n", err, nvec);

	return 0;
}

static void mlx5_disable_msix(struct mlx5_core_dev *dev)
{
	struct mlx5_eq_table *table = &dev->priv.eq_table;

	pci_disable_msix(dev->pdev);
	kfree(table->msix_arr);
}

struct mlx5_reg_host_endianess {
	u8	he;
	u8      rsvd[15];
};


#define CAP_MASK(pos, size) ((u64)((1 << (size)) - 1) << (pos))

enum {
	MLX5_CAP_BITS_RW_MASK	= CAP_MASK(MLX5_CAP_OFF_CMDIF_CSUM, 2) |
				  CAP_MASK(MLX5_CAP_OFF_DCT, 1),
};

/* selectively copy writable fields clearing any reserved area
 */
static void copy_rw_fields(struct mlx5_hca_cap *to, struct mlx5_hca_cap *from)
{
	u64 v64;

	to->log_max_qp = from->log_max_qp & 0x1f;
	to->log_max_ra_req_dc = from->log_max_ra_req_dc & 0x3f;
	to->log_max_ra_res_dc = from->log_max_ra_res_dc & 0x3f;
	to->log_max_ra_req_qp = from->log_max_ra_req_qp & 0x3f;
	to->log_max_ra_res_qp = from->log_max_ra_res_qp & 0x3f;
	to->pkey_table_size = from->pkey_table_size;
	v64 = be64_to_cpu(from->flags) & MLX5_CAP_BITS_RW_MASK;
	to->flags = cpu_to_be64(v64);
}

static int get_max_caps(struct mlx5_core_dev *dev, u64 *caps)
{
	struct mlx5_cmd_query_hca_cap_mbox_out *out;
	struct mlx5_cmd_query_hca_cap_mbox_in in;
	int err;

	memset(&in, 0, sizeof(in));
	out = kzalloc(sizeof(*out), GFP_KERNEL);
	if (!out)
		return -ENOMEM;

	in.hdr.opcode = cpu_to_be16(MLX5_CMD_OP_QUERY_HCA_CAP);
	in.hdr.opmod  = cpu_to_be16(HCA_CAP_OPMOD_GET_MAX);
	err = mlx5_cmd_exec(dev, &in, sizeof(in), out, sizeof(*out));
	if (err)
		goto query_ex;

	err = mlx5_cmd_status_to_err(&out->hdr);
	if (err) {
		mlx5_core_warn(dev, "query max hca cap failed, %d\n", err);
		goto query_ex;
	}
	*caps = be64_to_cpu(out->hca_cap.flags);

query_ex:
	kfree(out);
	return err;
}

static int handle_hca_cap(struct mlx5_core_dev *dev)
{
	struct mlx5_cmd_query_hca_cap_mbox_out *query_out = NULL;
	struct mlx5_cmd_set_hca_cap_mbox_in *set_ctx = NULL;
	struct mlx5_cmd_query_hca_cap_mbox_in query_ctx;
	struct mlx5_cmd_set_hca_cap_mbox_out set_out;
	struct mlx5_profile *prof = dev->profile;
	u64 set_flags;
	u64 cap_flags;
	int err;

	err = get_max_caps(dev, &cap_flags);
	if (err)
		return err;

	memset(&query_ctx, 0, sizeof(query_ctx));
	query_out = kzalloc(sizeof(*query_out), GFP_KERNEL);
	if (!query_out)
		return -ENOMEM;

	set_ctx = kzalloc(sizeof(*set_ctx), GFP_KERNEL);
	if (!set_ctx) {
		err = -ENOMEM;
		goto query_ex;
	}

	query_ctx.hdr.opcode = cpu_to_be16(MLX5_CMD_OP_QUERY_HCA_CAP);
	query_ctx.hdr.opmod  = cpu_to_be16(HCA_CAP_OPMOD_GET_CUR);
	err = mlx5_cmd_exec(dev, &query_ctx, sizeof(query_ctx),
				 query_out, sizeof(*query_out));
	if (err)
		goto query_ex;

	err = mlx5_cmd_status_to_err(&query_out->hdr);
	if (err) {
		mlx5_core_warn(dev, "query hca cap failed, %d\n", err);
		goto query_ex;
	}

	copy_rw_fields(&set_ctx->hca_cap, &query_out->hca_cap);

	set_flags = be64_to_cpu(set_ctx->hca_cap.flags);

	if (prof->mask & MLX5_PROF_MASK_DCT) {
		if (prof->dct_enable) {
			if (cap_flags & MLX5_DEV_CAP_FLAG_DCT) {
				set_flags |= MLX5_DEV_CAP_FLAG_DCT;
				dev->aysnc_events_mask |= (1ull << MLX5_EVENT_TYPE_DCT_DRAINED) |
					(1ull << MLX5_EVENT_TYPE_DCT_KEY_VIOLATION);
			}
		} else {
			set_flags &= ~MLX5_DEV_CAP_FLAG_DCT;
		}
	}

	if (prof->mask & MLX5_PROF_MASK_QP_SIZE)
		set_ctx->hca_cap.log_max_qp = prof->log_max_qp;

	/* disable checksum */
	set_flags &= ~MLX5_DEV_CAP_FLAG_CMDIF_CSUM;

	/* we limit the size of the pkey table to 128 entries for now */
	set_ctx->hca_cap.pkey_table_size = cpu_to_be16(0);
	set_ctx->hca_cap.flags = cpu_to_be64(set_flags);
	memset(&set_out, 0, sizeof(set_out));
	set_ctx->hca_cap.log_uar_page_sz = cpu_to_be16(PAGE_SHIFT - 12);
	set_ctx->hdr.opcode = cpu_to_be16(MLX5_CMD_OP_SET_HCA_CAP);
	err = mlx5_cmd_exec(dev, set_ctx, sizeof(*set_ctx),
				 &set_out, sizeof(set_out));
	if (err) {
		mlx5_core_warn(dev, "set hca cap failed, %d\n", err);
		goto query_ex;
	}

	err = mlx5_cmd_status_to_err(&set_out.hdr);
	if (err)
		goto query_ex;

query_ex:
	kfree(query_out);
	kfree(set_ctx);

	return err;
}

static int set_hca_ctrl(struct mlx5_core_dev *dev)
{
	struct mlx5_reg_host_endianess he_in;
	struct mlx5_reg_host_endianess he_out;
	int err;

	memset(&he_in, 0, sizeof(he_in));
	he_in.he = MLX5_SET_HOST_ENDIANNESS;
	err = mlx5_core_access_reg(dev, &he_in,  sizeof(he_in),
					&he_out, sizeof(he_out),
					MLX5_REG_HOST_ENDIANNESS, 0, 1);
	return err;
}

static int mlx5_core_enable_hca(struct mlx5_core_dev *dev)
{
	int err;
	struct mlx5_enable_hca_mbox_in in;
	struct mlx5_enable_hca_mbox_out out;

	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));
	in.hdr.opcode = cpu_to_be16(MLX5_CMD_OP_ENABLE_HCA);
	err = mlx5_cmd_exec(dev, &in, sizeof(in), &out, sizeof(out));
	if (err)
		return err;

	if (out.hdr.status)
		return mlx5_cmd_status_to_err(&out.hdr);

	return 0;
}

static int mlx5_core_disable_hca(struct mlx5_core_dev *dev)
{
	int err;
	struct mlx5_disable_hca_mbox_in in;
	struct mlx5_disable_hca_mbox_out out;

	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));
	in.hdr.opcode = cpu_to_be16(MLX5_CMD_OP_DISABLE_HCA);
	err = mlx5_cmd_exec(dev, &in, sizeof(in), &out, sizeof(out));
	if (err)
		return err;

	if (out.hdr.status)
		return mlx5_cmd_status_to_err(&out.hdr);

	return 0;
}

static void mlx5_add_device(struct mlx5_interface *intf, struct mlx5_priv *priv)
{
	struct mlx5_device_context *dev_ctx;
	struct mlx5_core_dev *dev = container_of(priv, struct mlx5_core_dev, priv);

	dev_ctx = kmalloc(sizeof(*dev_ctx), GFP_KERNEL);
	if (!dev_ctx) {
		pr_warn("mlx5_add_device: alloc context failed\n");
		return;
	}

	dev_ctx->intf    = intf;
	dev_ctx->context = intf->add(dev);

	if (dev_ctx->context) {
		spin_lock_irq(&priv->ctx_lock);
		list_add_tail(&dev_ctx->list, &priv->ctx_list);
		spin_unlock_irq(&priv->ctx_lock);
	} else {
		kfree(dev_ctx);
	}
}

static void mlx5_remove_device(struct mlx5_interface *intf, struct mlx5_priv *priv)
{
	struct mlx5_device_context *dev_ctx;
	struct mlx5_core_dev *dev = container_of(priv, struct mlx5_core_dev, priv);

	list_for_each_entry(dev_ctx, &priv->ctx_list, list)
		if (dev_ctx->intf == intf) {
			spin_lock_irq(&priv->ctx_lock);
			list_del(&dev_ctx->list);
			spin_unlock_irq(&priv->ctx_lock);

			intf->remove(dev, dev_ctx->context);
			kfree(dev_ctx);
			return;
		}
}

static int mlx5_register_device(struct mlx5_core_dev *dev)
{
	struct mlx5_priv *priv = &dev->priv;
	struct mlx5_interface *intf;

	mutex_lock(&intf_mutex);
	list_add_tail(&priv->dev_list, &dev_list);
	list_for_each_entry(intf, &intf_list, list)
		mlx5_add_device(intf, priv);
	mutex_unlock(&intf_mutex);

	return 0;
}

static void mlx5_unregister_device(struct mlx5_core_dev *dev)
{
	struct mlx5_priv *priv = &dev->priv;
	struct mlx5_interface *intf;

	mutex_lock(&intf_mutex);
	list_for_each_entry(intf, &intf_list, list)
		mlx5_remove_device(intf, priv);
	list_del(&priv->dev_list);
	mutex_unlock(&intf_mutex);
}

static int mlx5_pci_init(struct mlx5_core_dev *dev, struct mlx5_priv *priv)
{
	int err = 0;
	struct pci_dev *pdev = dev->pdev;

	pci_set_drvdata(dev->pdev, dev);
	strncpy(priv->name, dev_name(&pdev->dev), MLX5_MAX_NAME_LEN);
	priv->name[MLX5_MAX_NAME_LEN - 1] = 0;

	mutex_init(&priv->pgdir_mutex);
	INIT_LIST_HEAD(&priv->pgdir_list);
	spin_lock_init(&priv->mkey_lock);

	priv->dbg_root = debugfs_create_dir(dev_name(&pdev->dev), mlx5_debugfs_root);
	if (!priv->dbg_root)
		return -ENOMEM;

	err = pci_enable_device(pdev);
	if (err) {
		dev_err(&pdev->dev, "Cannot enable PCI device, aborting.\n");
		goto err_dbg;
	}

	err = request_bar(pdev);
	if (err) {
		dev_err(&pdev->dev, "error requesting BARs, aborting.\n");
		goto err_disable;
	}

	pci_set_master(pdev);

	err = set_dma_caps(pdev);
	if (err) {
		dev_err(&pdev->dev, "Failed setting DMA capabilities mask, aborting\n");
		goto err_clr_master;
	}

	dev->iseg_base = pci_resource_start(dev->pdev, 0);
	dev->iseg = ioremap(dev->iseg_base, sizeof(*dev->iseg));
	if (!dev->iseg) {
		dev_err(&dev->pdev->dev, "Failed mapping initialization segment, aborting\n");
		err = -ENOMEM;
		goto err_clr_master;
	}

	return 0;

err_clr_master:
	pci_clear_master(dev->pdev);
	release_bar(dev->pdev);

err_disable:
	pci_disable_device(dev->pdev);

err_dbg:
	debugfs_remove(priv->dbg_root);
	return err;

}

static int mlx5_load_one(struct mlx5_core_dev *dev, struct mlx5_priv *priv)
{
	int err;

	mutex_lock(&dev->intf_state_mutex);
	if (dev->interface_state == MLX5_INTERFACE_STATE_UP) {
		dev_warn(&dev->pdev->dev, "%s: interface is up, NOP\n",
			 __func__);
		goto out;
	}
	dev_info(&dev->pdev->dev, "firmware version: %d.%d.%d\n", fw_rev_maj(dev),
		 fw_rev_min(dev), fw_rev_sub(dev));

	if (fw_initializing(dev)) {
		int i;
		int done = 0;

		for (i = 0; i < FW_INIT_MAX_ITER; i++) {
			msleep(FW_INIT_WAIT_MS);
			if (!fw_initializing(dev)) {
				dev_info(&dev->pdev->dev, "Driver waited %d MS for FW initialization\n",
					 (i + 1) * FW_INIT_WAIT_MS);
				done = 1;
				break;
			}
		}
		if (!done) {
			dev_err(&dev->pdev->dev, "Firmware over %d MS in initializing state, aborting\n",
				FW_INIT_MAX_ITER * FW_INIT_WAIT_MS);
			dev->priv.health.health = &dev->iseg->health;
			mlx5_print_health_info(dev);
			err = -EBUSY;
			return err;
		}
	}
	/* on load removing any previous indication of internal error, device is
	 * up
	 */
	dev->state = MLX5_DEVICE_STATE_UP;

	err = mlx5_cmd_init(dev);
	if (err) {
		dev_err(&dev->pdev->dev, "Failed initializing command interface, aborting\n");
		return err;
	}

	mlx5_pagealloc_init(dev);

	err = mlx5_core_enable_hca(dev);
	if (err) {
		dev_err(&dev->pdev->dev, "enable hca failed\n");
		goto err_pagealloc_cleanup;
	}

	err = mlx5_satisfy_startup_pages(dev, 1);
	if (err) {
		dev_err(&dev->pdev->dev, "failed to allocate boot pages\n");
		goto err_disable_hca;
	}

	err = set_hca_ctrl(dev);
	if (err) {
		dev_err(&dev->pdev->dev, "set_hca_ctrl failed\n");
		goto reclaim_boot_pages;
	}

	err = handle_hca_cap(dev);
	if (err) {
		dev_err(&dev->pdev->dev, "handle_hca_cap failed\n");
		goto reclaim_boot_pages;
	}

	err = mlx5_satisfy_startup_pages(dev, 0);
	if (err) {
		dev_err(&dev->pdev->dev, "failed to allocate init pages\n");
		goto reclaim_boot_pages;
	}

	err = mlx5_pagealloc_start(dev);
	if (err) {
		dev_err(&dev->pdev->dev, "mlx5_pagealloc_start failed\n");
		goto reclaim_boot_pages;
	}

	err = mlx5_cmd_init_hca(dev);
	if (err) {
		dev_err(&dev->pdev->dev, "init hca failed\n");
		goto err_pagealloc_stop;
	}

	mlx5_start_health_poll(dev);

	err = mlx5_cmd_query_hca_cap(dev, &dev->caps);
	if (err) {
		dev_err(&dev->pdev->dev, "query hca failed\n");
		goto err_stop_poll;
	}

	err = mlx5_cmd_query_adapter(dev);
	if (err) {
		dev_err(&dev->pdev->dev, "query adapter failed\n");
		goto err_stop_poll;
	}

	err = mlx5_enable_msix(dev);
	if (err) {
		dev_err(&dev->pdev->dev, "enable msix failed\n");
		goto err_stop_poll;
	}

	err = mlx5_eq_init(dev);
	if (err) {
		dev_err(&dev->pdev->dev, "failed to initialize eq\n");
		goto disable_msix;
	}

	err = mlx5_alloc_uuars(dev, &priv->uuari);
	if (err) {
		dev_err(&dev->pdev->dev, "Failed allocating uar, aborting\n");
		goto err_eq_cleanup;
	}

	INIT_LIST_HEAD(&priv->ctx_list);
	spin_lock_init(&priv->ctx_lock);
	err = mlx5_start_eqs(dev);
	if (err) {
		dev_err(&dev->pdev->dev, "Failed to start pages and async EQs\n");
		goto err_free_uar;
	}

	MLX5_INIT_DOORBELL_LOCK(&priv->cq_uar_lock);

	mlx5_init_cq_table(dev);
	mlx5_init_qp_table(dev);
	mlx5_init_srq_table(dev);
	mlx5_init_mr_table(dev);

	err = mlx5_register_device(dev);
	if (err) {
		dev_err(&dev->pdev->dev, "mlx5_register_device failed %d\n", err);
		goto err_stop_eqs;
	}
	dev->interface_state = MLX5_INTERFACE_STATE_UP;
out:
	mutex_unlock(&dev->intf_state_mutex);

	return 0;

err_stop_eqs:
	mlx5_stop_eqs(dev);

err_free_uar:
	mlx5_free_uuars(dev, &priv->uuari);

err_eq_cleanup:
	mlx5_eq_cleanup(dev);

disable_msix:
	mlx5_disable_msix(dev);

err_stop_poll:
	mlx5_stop_health_poll(dev);
	if (mlx5_cmd_teardown_hca(dev)) {
		dev_err(&dev->pdev->dev, "tear_down_hca failed, skip cleanup\n");
		return err;
	}

err_pagealloc_stop:
	mlx5_pagealloc_stop(dev);

reclaim_boot_pages:
	mlx5_reclaim_startup_pages(dev);

err_disable_hca:
	mlx5_core_disable_hca(dev);

err_pagealloc_cleanup:
	mlx5_pagealloc_cleanup(dev);
	mlx5_cmd_cleanup(dev);

	return err;
}

static void mlx5_pci_close(struct mlx5_core_dev *dev, struct mlx5_priv *priv)
{
	iounmap(dev->iseg);
	pci_clear_master(dev->pdev);
	release_bar(dev->pdev);
	pci_disable_device(dev->pdev);
	debugfs_remove(priv->dbg_root);
}

static int mlx5_dev_init(struct mlx5_core_dev *dev, struct pci_dev *pdev)
{
	struct mlx5_priv *priv = &dev->priv;
	int err;

	dev->pdev = pdev;
	err = mlx5_pci_init(dev, priv);
	if (err) {
		dev_err(&pdev->dev, "%s failed with error code %d\n", __func__, err);
		return err;
	}

	err = mlx5_load_one(dev, priv);
	if (err) {
		dev_err(&pdev->dev, "%s failed with error code %d\n", __func__, err);
		goto clean_pci;
	}

	return 0;

clean_pci:
	mlx5_pci_close(dev, priv);
	return err;
}

static int mlx5_unload_one(struct mlx5_core_dev *dev, struct mlx5_priv *priv)
{
	int err = 0;

	mutex_lock(&dev->intf_state_mutex);
	if (dev->interface_state == MLX5_INTERFACE_STATE_DOWN) {
		dev_warn(&dev->pdev->dev, "%s: interface is down, NOP\n",
			 __func__);
		goto out;
	}
	mlx5_unregister_device(dev);
	mlx5_cleanup_srq_table(dev);
	mlx5_cleanup_qp_table(dev);
	mlx5_cleanup_cq_table(dev);
	mlx5_stop_eqs(dev);
	mlx5_free_uuars(dev, &priv->uuari);
	mlx5_eq_cleanup(dev);
	mlx5_disable_msix(dev);
	mlx5_stop_health_poll(dev);
	err = mlx5_cmd_teardown_hca(dev);
	if (err) {
		dev_err(&dev->pdev->dev, "tear_down_hca failed, skip cleanup\n");
		goto out;
	}
	mlx5_pagealloc_stop(dev);
	mlx5_reclaim_startup_pages(dev);
	mlx5_core_disable_hca(dev);
	mlx5_pagealloc_cleanup(dev);
	mlx5_cmd_cleanup(dev);

out:
	dev->interface_state = MLX5_INTERFACE_STATE_DOWN;
	mutex_unlock(&dev->intf_state_mutex);
	return err;
}

int mlx5_register_interface(struct mlx5_interface *intf)
{
	struct mlx5_priv *priv;

	if (!intf->add || !intf->remove)
		return -EINVAL;

	mutex_lock(&intf_mutex);
	list_add_tail(&intf->list, &intf_list);
	list_for_each_entry(priv, &dev_list, dev_list)
		mlx5_add_device(intf, priv);
	mutex_unlock(&intf_mutex);

	return 0;
}
EXPORT_SYMBOL(mlx5_register_interface);

void mlx5_unregister_interface(struct mlx5_interface *intf)
{
	struct mlx5_priv *priv;

	mutex_lock(&intf_mutex);
	list_for_each_entry(priv, &dev_list, dev_list)
	       mlx5_remove_device(intf, priv);
	list_del(&intf->list);
	mutex_unlock(&intf_mutex);
}
EXPORT_SYMBOL(mlx5_unregister_interface);

void mlx5_core_event(struct mlx5_core_dev *dev, enum mlx5_dev_event event,
		     void *data)
{
	struct mlx5_priv *priv = &dev->priv;
	struct mlx5_device_context *dev_ctx;
	unsigned long flags;

	spin_lock_irqsave(&priv->ctx_lock, flags);

	list_for_each_entry(dev_ctx, &priv->ctx_list, list)
		if (dev_ctx->intf->event)
			dev_ctx->intf->event(dev, dev_ctx->context, event, data);

	spin_unlock_irqrestore(&priv->ctx_lock, flags);
}

struct mlx5_core_event_handler {
	void (*event)(struct mlx5_core_dev *dev,
		      enum mlx5_dev_event event,
		      void *data);
};

static int init_one(struct pci_dev *pdev,
		    const struct pci_device_id *id)
{
	struct mlx5_core_dev *dev;
	struct mlx5_priv *priv;
	int err;

	mlx5_register_debugfs();
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&pdev->dev, "kzalloc failed\n");
		err = -ENOMEM;
		goto remove_debugfs;
	}
	priv = &dev->priv;

	pci_set_drvdata(pdev, dev);

	if (prof_sel < 0 || prof_sel >= ARRAY_SIZE(profile)) {
		pr_warn("selected profile out of range, selecting default (%d)\n",
			MLX5_DEFAULT_PROF);
		prof_sel = MLX5_DEFAULT_PROF;
	}
	dev->profile = &profile[prof_sel];
	dev->event = mlx5_core_event;
	mutex_init(&dev->intf_state_mutex);

	err = mlx5_dev_init(dev, pdev);
	if (err) {
		dev_err(&pdev->dev, "mlx5_dev_init failed %d\n", err);
		goto out;
	}


	pci_save_state(pdev);

	return 0;

out:
	pci_set_drvdata(pdev, NULL);
	kfree(dev);
remove_debugfs:
	mlx5_unregister_debugfs();

	return err;
}

static void remove_one(struct pci_dev *pdev)
{
	struct mlx5_core_dev *dev  = pci_get_drvdata(pdev);
	struct mlx5_priv *priv = &dev->priv;

	if (mlx5_unload_one(dev, priv)) {
		dev_err(&dev->pdev->dev, "mlx5_unload_one failed\n");
		return;
	}
	mlx5_pci_close(dev, priv);
	pci_set_drvdata(pdev, NULL);
	kfree(dev);
	mlx5_unregister_debugfs();
}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 27))
#ifdef CONFIG_PM
static int suspend(struct device *device)
{
	struct pci_dev *pdev = to_pci_dev(device);
	struct mlx5_core_dev *dev = pci_get_drvdata(pdev);
	struct mlx5_priv *priv = &dev->priv;
	int ret;

	dev_info(&pdev->dev, "suspend was called\n");

	ret = mlx5_unload_one(dev, priv);
	if (ret) {
		dev_err(&pdev->dev, "mlx5_unload_one failed with error code: %d\n", ret);
		return ret;
	}

	ret = pci_save_state(pdev);
	if (ret) {
		dev_err(&pdev->dev, "pci_save_state failed with error code: %d\n", ret);
		return ret;
	}

	ret = pci_enable_wake(pdev, PCI_D3hot, 0);
	if (ret) {
		dev_err(&pdev->dev, "pci_enable_wake failed with error code: %d\n", ret);
		return ret;
	}

	pci_disable_device(pdev);
	ret = pci_set_power_state(pdev, PCI_D3hot);
	if (ret) {
		dev_warn(&pdev->dev, "pci_set_power_state failed with error code: %d\n", ret);
		return ret;
	}

	return 0;
}

static int resume(struct device *device)
{
	struct pci_dev *pdev = to_pci_dev(device);
	struct mlx5_core_dev *dev = pci_get_drvdata(pdev);
	struct mlx5_priv *priv = &dev->priv;
	int ret;

	dev_info(&pdev->dev, "resume was called\n");

	ret = pci_set_power_state(pdev, PCI_D0);
	if (ret) {
		dev_warn(&pdev->dev, "pci_set_power_state failed with error code: %d\n", ret);
		return ret;
	}

	pci_restore_state(pdev);
	ret = pci_save_state(pdev);
	if (ret) {
		dev_err(&pdev->dev, "pci_save_state failed with error code: %d\n", ret);
		return ret;
	}
	ret = pci_enable_device(pdev);
	if (ret) {
		dev_err(&pdev->dev, "pci_enabel_device failed with error code: %d\n", ret);
		return ret;
	}
	pci_set_master(pdev);

	ret = mlx5_load_one(dev, priv);
	if (ret) {
		dev_err(&pdev->dev, "mlx5_load_one failed with error code: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct dev_pm_ops mlnx_pm = {
	.suspend = suspend,
	.resume = resume,
};
#endif	/* CONFIG_PM */
#endif

static pci_ers_result_t mlx5_pci_err_detected(struct pci_dev *pdev,
					      pci_channel_state_t state)
{
	struct mlx5_core_dev *dev = pci_get_drvdata(pdev);
	struct mlx5_priv *priv = &dev->priv;

	dev_info(&pdev->dev, "%s was called\n", __func__);
	if (state ==  pci_channel_io_normal)
		return PCI_ERS_RESULT_NONE;
	mlx5_enter_error_state(dev);
	mlx5_unload_one(dev, priv);
	pci_disable_device(pdev);
	return state == pci_channel_io_perm_failure ?
		PCI_ERS_RESULT_DISCONNECT : PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t mlx5_pci_slot_reset(struct pci_dev *pdev)
{
	struct mlx5_core_dev *dev = pci_get_drvdata(pdev);
	struct mlx5_priv *priv = &dev->priv;
	int ret = 0;

	dev_info(&pdev->dev, "%s was called\n", __func__);

	ret = pci_enable_device(pdev);
	if (ret) {
		dev_err(&pdev->dev, "%s: pci_enable_device failed with error code: %d\n"
			, __func__, ret);
		return PCI_ERS_RESULT_DISCONNECT;
	}
	pci_set_master(pdev);
	pci_restore_state(pdev);
	pci_save_state(pdev);
	ret = mlx5_load_one(dev, priv);
	if (ret)
		dev_err(&pdev->dev, "%s: mlx5_load_one failed with error code: %d\n"
			, __func__, ret);

	return ret ? PCI_ERS_RESULT_DISCONNECT : PCI_ERS_RESULT_RECOVERED;
}

static const struct pci_error_handlers mlx5_err_handler = {
	.error_detected = mlx5_pci_err_detected,
	.slot_reset	= mlx5_pci_slot_reset
};

static void shutdown(struct pci_dev *pdev)
{
	struct mlx5_core_dev  *dev  = pci_get_drvdata(pdev);
	struct mlx5_priv *priv = &dev->priv;

	dev_info(&pdev->dev, "shutdown was called\n");
	mlx5_unload_one(dev, priv);
}

static const struct pci_device_id mlx5_core_pci_table[] = {
	{ PCI_VDEVICE(MELLANOX, 4113) }, /* MT4113 Connect-IB */
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, mlx5_core_pci_table);

static struct pci_driver mlx5_core_driver = {
	.name           = DRIVER_NAME,
	.id_table       = mlx5_core_pci_table,
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 27))
#ifdef CONFIG_PM
	.driver = {
		.pm	= &mlnx_pm,
	},
#endif /* CONFIG_PM */
#endif
	.probe          = init_one,
	.remove         = remove_one,
	.shutdown	= shutdown,
	.err_handler	= &mlx5_err_handler
};

static int __init init(void)
{
	int err;

	mlx5_core_wq = create_singlethread_workqueue("mlx5_core_wq");
	if (!mlx5_core_wq) {
		return -ENOMEM;
	}
	mlx5_health_init();

	err = pci_register_driver(&mlx5_core_driver);
	if (err)
		goto err_health;

	return 0;

err_health:
	mlx5_health_cleanup();
	destroy_workqueue(mlx5_core_wq);

	return err;
}

static void __exit cleanup(void)
{
	pci_unregister_driver(&mlx5_core_driver);
	mlx5_health_cleanup();
	destroy_workqueue(mlx5_core_wq);
}

module_init(init);
module_exit(cleanup);
