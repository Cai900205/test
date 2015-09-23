/*
 * Copyright (c) 2012 Mellanox Technologies. All rights reserved.
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

#include "mlx4_ib.h"
#include <linux/debugfs.h>
#include <linux/export.h>
#include "linux/mlx4/device.h"
#include "ecn.h"

static struct dentry *mlx4_root;

#define ADD_RPRT_COUNTER_METADATA(_name, _offset) {.name = _name, \
						   .offset = _offset}

struct rprt_counters_entry {
	const char *name;
	u32 offset;
};

static const struct rprt_counters_entry
	rprt_cntrs_bsc[] = {
		ADD_RPRT_COUNTER_METADATA("IfRxFrames", 0x10),
		ADD_RPRT_COUNTER_METADATA("IfRxOctets", 0x18),
		ADD_RPRT_COUNTER_METADATA("IfTxFrames", 0x20),
		ADD_RPRT_COUNTER_METADATA("IfTxOctets", 0x28)
};
#define RPRT_COUNTERS_METADATA_BSC_SZ	\
		(sizeof(rprt_cntrs_bsc) / \
		 sizeof(rprt_cntrs_bsc[0]))

static const struct rprt_counters_entry
	rprt_cntrs_ext[] = {
		ADD_RPRT_COUNTER_METADATA("IfRxUnicastFrames", 0x10),
		ADD_RPRT_COUNTER_METADATA("IfRxUnicastOctets", 0x18),
		ADD_RPRT_COUNTER_METADATA("IfRxMulticastFrames", 0x20),
		ADD_RPRT_COUNTER_METADATA("IfRxMulticastOctets", 0x28),
		ADD_RPRT_COUNTER_METADATA("IfRxBroadcastFrames", 0x30),
		ADD_RPRT_COUNTER_METADATA("IfRxBroadcastOctets", 0x38),
		ADD_RPRT_COUNTER_METADATA("IfRxNoBufferFrames", 0x40),
		ADD_RPRT_COUNTER_METADATA("IfRxNoBufferOctets", 0x48),
		ADD_RPRT_COUNTER_METADATA("IfRxErrorFrames", 0x50),
		ADD_RPRT_COUNTER_METADATA("IfRxErrorOctets", 0x58),
		ADD_RPRT_COUNTER_METADATA("IfTxUnicastFrames", 0x100),
		ADD_RPRT_COUNTER_METADATA("IfTxUnicastOctets", 0x108),
		ADD_RPRT_COUNTER_METADATA("IfTxMulticastFrames", 0x110),
		ADD_RPRT_COUNTER_METADATA("IfTxMulticastOctets", 0x118),
		ADD_RPRT_COUNTER_METADATA("IfTxBroadcastFrames", 0x120),
		ADD_RPRT_COUNTER_METADATA("IfTxBroadcastOctets", 0x128),
		ADD_RPRT_COUNTER_METADATA("IfTxDroppedFrames", 0x130),
		ADD_RPRT_COUNTER_METADATA("IfTxDroppedOctets", 0x138),
		ADD_RPRT_COUNTER_METADATA("IfTxRequestedFramesSent", 0x140),
		ADD_RPRT_COUNTER_METADATA("IfTxGeneratedFramesSent", 0x148),
		ADD_RPRT_COUNTER_METADATA("IfTxTsoOctets", 0x150)
};

#define RPRT_COUNTERS_METADATA_EXT_SZ	\
		(sizeof(rprt_cntrs_ext) / \
		 sizeof(rprt_cntrs_ext[0]))
#define RPRT_COUNTERS_SZ (RPRT_COUNTERS_METADATA_BSC_SZ + \
			  RPRT_COUNTERS_METADATA_EXT_SZ)

struct query_if_stat_attr {
	struct mlx4_ib_dev	*dev;
	u32			offset;
	u8			counter_index;
};

struct query_if_stat {
	struct list_head	  list;
	/* +1 for the clear entry */
	struct query_if_stat_attr attrs[][RPRT_COUNTERS_SZ + 1];
};

LIST_HEAD(dbgfs_resources_list);

static int mlx4_query_if_stat_basic(void *data, u64 *val)
{
	char counter_buf[MLX4_IF_STAT_SZ(1)];
	union mlx4_counter *counter = (union mlx4_counter *)counter_buf;
	char counter_basic_buf[sizeof(struct mlx4_if_stat_basic) +
			       sizeof(((struct mlx4_if_stat_basic *)0)
			       ->counters[1])];
	struct mlx4_if_stat_basic *counter_basic = (struct mlx4_if_stat_basic *)
						    counter_basic_buf;
	struct query_if_stat_attr *attr = (struct query_if_stat_attr *)data;
	struct mlx4_if_stat_extended *cnt = &counter->ext;

	if (mlx4_ib_query_if_stat(attr->dev, attr->counter_index, counter, 0) ||
	    counter->control.cnt_mode == 0) {
		*val = be64_to_cpu(*(__be64 *)(((char *)&counter->ext) +
					       attr->offset));
		return 0;
	}
	counter_basic->counters[0].IfTxOctets =
		cpu_to_be64(be64_to_cpu(cnt->counters[0].IfTxUnicastOctets) +
			     be64_to_cpu(cnt->counters[0].IfTxMulticastOctets) +
			     be64_to_cpu(cnt->counters[0].IfTxDroppedOctets) +
			     be64_to_cpu(cnt->counters[0].
					 IfTxBroadcastOctets));
	counter_basic->counters[0].IfRxOctets =
		cpu_to_be64(be64_to_cpu(cnt->counters[0].IfRxUnicastOctets) +
			     be64_to_cpu(cnt->counters[0].IfRxMulticastOctets) +
			     be64_to_cpu(cnt->counters[0].IfRxNoBufferOctets) +
			     be64_to_cpu(cnt->counters[0].IfRxErrorOctets) +
			     be64_to_cpu(cnt->counters[0].
					 IfRxBroadcastOctets));
	counter_basic->counters[0].IfTxFrames =
		cpu_to_be64(be64_to_cpu(cnt->counters[0].IfTxUnicastFrames) +
			    be64_to_cpu(cnt->counters[0].IfTxMulticastFrames) +
			    be64_to_cpu(cnt->counters[0].IfTxDroppedFrames) +
			    be64_to_cpu(cnt->counters[0].IfTxBroadcastFrames));
	counter_basic->counters[0].IfRxFrames =
		cpu_to_be64(be64_to_cpu(cnt->counters[0].IfRxUnicastFrames) +
			    be64_to_cpu(cnt->counters[0].IfRxMulticastFrames) +
			    be64_to_cpu(cnt->counters[0].IfRxNoBufferFrames) +
			    be64_to_cpu(cnt->counters[0].IfRxErrorFrames) +
			    be64_to_cpu(cnt->counters[0].IfRxBroadcastFrames));
	*val = be64_to_cpu(*(__be64 *)(((char *)counter_basic) +
				       attr->offset));
	return 0;
}


static int mlx4_query_if_stat_ext(void *data, u64 *val)
{
	char				counter_buf[MLX4_IF_STAT_SZ(1)];
	union  mlx4_counter		*counter = (union mlx4_counter *)
						   counter_buf;
	struct query_if_stat_attr *attr = (struct query_if_stat_attr *)data;

	if (mlx4_ib_query_if_stat(attr->dev, attr->counter_index, counter, 0) ||
	    counter->control.cnt_mode != 1) {
		*val = -1;
		return 0;
	}
	*val = be64_to_cpu(*(__be64 *)(((char *)&counter->ext) + attr->offset));
	return 0;
}


static int mlx4_clear_if_stat(void *data, u64 val)
{
	char				counter_buf[MLX4_IF_STAT_SZ(1)];
	union  mlx4_counter		*counter = (union mlx4_counter *)
						   counter_buf;
	struct query_if_stat_attr *attr = (struct query_if_stat_attr *)data;

	mlx4_ib_query_if_stat(attr->dev, attr->counter_index, counter, val);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(mlx4_fops_query_if_stat_basic,
			mlx4_query_if_stat_basic, NULL,  "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(mlx4_fops_query_if_stat_ext,
			mlx4_query_if_stat_ext, NULL,  "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(mlx4_fops_query_if_stat_clear, NULL,
			mlx4_clear_if_stat,  "%llu\n");


static struct dentry *mlx4_debugfs_create_query_stat(
				const char *name,
				umode_t mode, struct dentry *parent,
				struct query_if_stat_attr *attr,
				struct mlx4_ib_dev *dev, u8 counter_index,
				u32 offset,
				const struct file_operations *fops)
{
	attr->dev = dev;
	attr->offset = offset;
	attr->counter_index = counter_index;

	return debugfs_create_file(name, mode, parent, attr, fops);
}

static struct dentry *mlx4_debugfs_create_query_stat_basic(
				const char *name,
				umode_t mode, struct dentry *parent,
				struct query_if_stat_attr *attr,
				struct mlx4_ib_dev *dev, u8 counter_index,
				u32 offset) {
	return mlx4_debugfs_create_query_stat(name, mode, parent,
			attr, dev, counter_index, offset,
			&mlx4_fops_query_if_stat_basic);
}

static struct dentry *mlx4_debugfs_create_query_stat_ext(
				const char *name,
				umode_t mode, struct dentry *parent,
				struct query_if_stat_attr *attr,
				struct mlx4_ib_dev *dev, u8 counter_index,
				u32 offset)
{
	return mlx4_debugfs_create_query_stat(name, mode, parent,
			attr, dev, counter_index, offset,
			&mlx4_fops_query_if_stat_ext);
}

void mlx4_ib_create_debug_files(struct mlx4_ib_dev *dev)
{
	int i;
	int j;
	int k;
	char element_name[5];
	struct dentry *ports;
	struct dentry *ecn;
	struct query_if_stat *dev_if_stat_attrs;

	dev->dev_root = NULL;
	if (!mlx4_root)
		return;
	dev->dev_root = debugfs_create_dir(dev->ib_dev.name, mlx4_root);
	if (!dev->dev_root)
		return;

	ports = debugfs_create_dir("ports", dev->dev_root);
	if (!ports)
		return;

	dev_if_stat_attrs = kmalloc(sizeof(struct query_if_stat) +
				    sizeof(((struct query_if_stat *)0)
				    ->attrs[0]) * dev->num_ports,
				    GFP_KERNEL);
	if (!dev_if_stat_attrs)
		return;

	list_add(&dev_if_stat_attrs->list, &dbgfs_resources_list);

	for (i = 0; i < dev->num_ports; i++) {
		struct dentry *port;
		int counter_index = dev->counters[i].counter_index;

		if (counter_index == -1)
			continue;
		snprintf(element_name, sizeof(element_name), "%d", i + 1);
		port = debugfs_create_dir(element_name, ports);
		for (j = 0; j < RPRT_COUNTERS_METADATA_BSC_SZ; j++) {
			mlx4_debugfs_create_query_stat_basic(rprt_cntrs_bsc[j]
							     .name, 0444, port,
							     &dev_if_stat_attrs
							     ->attrs[i][j], dev,
							     counter_index,
							     rprt_cntrs_bsc[j]
							     .offset);
		}
		for (k = 0; k < RPRT_COUNTERS_METADATA_EXT_SZ; k++) {
			mlx4_debugfs_create_query_stat_ext(rprt_cntrs_ext[k]
							   .name, 0444, port,
							   &dev_if_stat_attrs
							   ->attrs[i][j + k]
							   , dev, counter_index,
							   rprt_cntrs_ext[k]
							   .offset);
		}
		mlx4_debugfs_create_query_stat("clear", 0222,
					       port, &dev_if_stat_attrs->
					       attrs[i][j+k], dev,
					       counter_index, 0,
					       &mlx4_fops_query_if_stat_clear);
	}

	if (en_ecn && dev->dev->caps.ecn_qcn_ver > 0) {
		ecn = debugfs_create_dir("ecn", dev->dev_root);
		if (!con_ctrl_dbgfs_init(ecn, dev)) {
			for (i = CTRL_ALGO_R_ROCE_ECN_1_REACTION_POINT;
			     i < CTRL_ALGO_SZ; i++) {
				void *algo_alloced =
					con_ctrl_dbgfs_add_algo(ecn, dev, i);
				if (algo_alloced != NULL) {
					INIT_LIST_HEAD((struct list_head *)
						 algo_alloced);
					list_add((struct list_head *)
						 algo_alloced,
						 &dbgfs_resources_list);
				}
			}
		} else {
			pr_warn("ecn: failed to initialize qcn/ecn");
		}
	}
}

void mlx4_ib_delete_debug_files(struct mlx4_ib_dev *dev)
{
	if (dev->dev_root) {
		struct list_head *dev_res, *temp;

		debugfs_remove_recursive(dev->dev_root);
		list_for_each_safe(dev_res, temp, &dbgfs_resources_list) {
			struct query_if_stat *dev_if_stat_attrs =
			       list_entry(dev_res, struct query_if_stat, list);
			list_del(dev_res);
			kfree(dev_if_stat_attrs);
		}
		con_ctrl_dbgfs_free(dev);
	}
}

int mlx4_ib_register_debugfs(void)
{
	mlx4_root = debugfs_create_dir("mlx4_ib", NULL);
	return mlx4_root ? 0 : -ENOMEM;
}

void mlx4_ib_unregister_debugfs(void)
{
	debugfs_remove_recursive(mlx4_root);
}
