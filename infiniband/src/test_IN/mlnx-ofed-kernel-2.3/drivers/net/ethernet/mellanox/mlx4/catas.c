/*
 * Copyright (c) 2007 Cisco Systems, Inc. All rights reserved.
 * Copyright (c) 2007, 2008 Mellanox Technologies. All rights reserved.
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

#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/mlx4/cmd.h>
#include <linux/printk.h>

#include "mlx4.h"

enum {
	MLX4_CATAS_POLL_INTERVAL	= 5 * HZ,
};


int internal_err_reset = 1;
module_param(internal_err_reset, int, 0644);
MODULE_PARM_DESC(internal_err_reset,
		 "Reset device on internal errors if non-zero (default 1)");

static int mlx4_reset_master(struct mlx4_dev *dev)
{
	int err = 0;

	if (mlx4_is_master(dev))
		report_internal_err_comm_event(dev);

	if (!pci_channel_offline(dev->pdev)) {
		err = mlx4_reset(dev);
		if (err)
			mlx4_err(dev, "Fail to reset HCA\n");
	}
	return err;
}

static int mlx4_reset_slave(struct mlx4_dev *dev)
{
#define COM_CHAN_RST_REQ_OFFSET 0x10
#define COM_CHAN_RST_ACK_OFFSET 0x08

	u32 comm_flags;
	u32 rst_req;
	u32 rst_ack;
	unsigned long end;
	struct mlx4_priv *priv = mlx4_priv(dev);

	if (pci_channel_offline(dev->pdev))
		return 0;

	if (!dev->caps.vf_reset) {
		mlx4_err(dev, "VF reset is not supported.\n");
		return -EOPNOTSUPP;
	}
	comm_flags = swab32(readl((void *)priv->mfunc.comm +
				  MLX4_COMM_CHAN_FLAGS));
	rst_req = (comm_flags & (u32)(1 << COM_CHAN_RST_REQ_OFFSET)) >>
		COM_CHAN_RST_REQ_OFFSET;
	rst_ack = (comm_flags & (u32)(1 << COM_CHAN_RST_ACK_OFFSET)) >>
		COM_CHAN_RST_ACK_OFFSET;
	if (rst_req != rst_ack) {
		mlx4_err(dev, "Communication channel isn't sync, fail to send reset.\n");
		return -EIO;
	}

	rst_req ^= 1;
	mlx4_warn(dev, "VF is sending reset request to Firmware.\n");
	comm_flags = rst_req << COM_CHAN_RST_REQ_OFFSET;
	__raw_writel((__force u32) cpu_to_be32(comm_flags),
		     (void *)priv->mfunc.comm + MLX4_COMM_CHAN_FLAGS);
	/* Make sure that our comm channel write doesn't
	 * get mixed in with writes from another CPU.
	 */
	mmiowb();

	end = msecs_to_jiffies(MLX4_COMM_TIME) + jiffies;
	while (time_before(jiffies, end)) {
		comm_flags = swab32(readl((void *)priv->mfunc.comm +
					  MLX4_COMM_CHAN_FLAGS));
		rst_ack = (comm_flags & (u32)(1 << COM_CHAN_RST_ACK_OFFSET)) >>
			COM_CHAN_RST_ACK_OFFSET;

		/* Reading rst_req again since the communication channel can
		 * be reset at any time by the PF and all its bits will be
		 * set to zero.
		 */
		rst_req = (comm_flags & (u32)(1 << COM_CHAN_RST_REQ_OFFSET)) >>
			COM_CHAN_RST_REQ_OFFSET;

		if (rst_ack == rst_req) {
			mlx4_warn(dev, "VF Reset succeed, unloading VF driver.\n");
			return 0;
		}
		cond_resched();
	}
	mlx4_err(dev, "Fail to send reset over the communication channel.\n");
	return -ETIMEDOUT;
}

static void mlx4_wake_completions(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	struct mlx4_cmd_context *context;
	int i;

	spin_lock(&priv->cmd.context_lock);
	if (priv->cmd.context) {
		for (i = 0; i < priv->cmd.max_cmds; ++i) {
			context = &priv->cmd.context[i];
			context->fw_status = CMD_STAT_INTERNAL_ERR;
			context->result    =
				mlx4_status_to_errno(CMD_STAT_INTERNAL_ERR);
			complete(&context->done);
		}
	}
	spin_unlock(&priv->cmd.context_lock);
}

static int mlx4_comm_internal_err(u32 slave_read)
{
	return (u32)COMM_CHAN_EVENT_INTERNAL_ERR ==
		(slave_read & (u32)COMM_CHAN_EVENT_INTERNAL_ERR) ? 1 : 0;
}

void mlx4_enter_error_state(struct mlx4_dev *dev)
{
	int err;

	if (!internal_err_reset)
		return;

	mutex_lock(&dev->device_state_mutex);
	if (dev->state & MLX4_DEVICE_STATE_INTERNAL_ERROR)
		goto out;

	mlx4_err(dev, "mlx4_enter_error_state: device is going to be reset\n");
	if (mlx4_is_slave(dev))
		err = mlx4_reset_slave(dev);
	else
		err = mlx4_reset_master(dev);
	BUG_ON(err != 0);

	dev->state |= MLX4_DEVICE_STATE_INTERNAL_ERROR;
	mlx4_err(dev, "mlx4_enter_error_state: device was reset successfully\n");
	mutex_unlock(&dev->device_state_mutex);

	/* At that step HW was already reset, now notify clients and restart driver */
	mlx4_dispatch_event(dev, MLX4_DEV_EVENT_CATASTROPHIC_ERROR, 0);
	mlx4_wake_completions(dev);
	mlx4_err(dev, "mlx4_enter_error_state: end\n");
	return;

out:
	mutex_unlock(&dev->device_state_mutex);
}

void mlx4_handle_error_state(struct mlx4_dev *dev)
{
	int err = 0;

	mlx4_err(dev, "mlx4_handle_error_state was started\n");
	mlx4_enter_error_state(dev);

	mutex_lock(&dev->interface_state_mutex);
	if (dev->interface_state & MLX4_INTERFACE_STATE_UP &&
	    !(dev->interface_state & MLX4_INTERFACE_STATE_DELETION)) {
		pr_warn("mlx4_handle_error_state: calling mlx4_restart_one\n");
		err = mlx4_restart_one(dev->pdev);
		pr_warn("mlx4_handle_error_state: mlx4_restart_one was ended, ret=%d\n", err);
	} else {
		mlx4_err(dev, "mlx4_handle_error_state: interface is not up, NOP\n");
	}
	mutex_unlock(&dev->interface_state_mutex);

	pr_warn("mlx4_handle_error_state end\n");
}

static void dump_err_buf(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);

	int i;

	mlx4_err(dev, "Internal error detected:\n");
	for (i = 0; i < priv->fw.catas_size; ++i)
		mlx4_err(dev, "  buf[%02x]: %08x\n",
			 i, swab32(readl(priv->catas_err.map + i)));
}

static void poll_catas(unsigned long dev_ptr)
{
	struct mlx4_dev *dev = (struct mlx4_dev *) dev_ptr;
	struct mlx4_priv *priv = mlx4_priv(dev);
	u32 slave_read;

	if (mlx4_is_slave(dev)) {
		slave_read = swab32(readl(&priv->mfunc.comm->slave_read));
		if (mlx4_comm_internal_err(slave_read)) {
			mlx4_warn(dev, "Internal error detected on the communication channel.\n");
			goto internal_err;
		}
	} else if (readl(priv->catas_err.map)) {
		dump_err_buf(dev);
		goto internal_err;
	}

	/* Device is moved once to catas list regardless number of markings as timer
	  * on that device is not scheduled once has an internal error.
	*/
	if (dev->state & MLX4_DEVICE_STATE_INTERNAL_ERROR) {
		mlx4_warn(dev, "Internal error mark was detected on device %p\n", dev);
		goto internal_err;
	}

	mod_timer(&priv->catas_err.timer,
		  round_jiffies(jiffies + MLX4_CATAS_POLL_INTERVAL));
	return;

internal_err:
	if (internal_err_reset)
		queue_work(dev->catas_wq, &dev->catas_work);
}

static void catas_reset(struct work_struct *work)
{
	struct mlx4_dev *dev = container_of(work, struct mlx4_dev, catas_work);

	mlx4_handle_error_state(dev);
}

void mlx4_start_catas_poll(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);
	phys_addr_t addr;

	INIT_LIST_HEAD(&priv->catas_err.list);
	init_timer(&priv->catas_err.timer);
	priv->catas_err.map = NULL;

	if (!mlx4_is_slave(dev)) {
		addr = pci_resource_start(dev->pdev, priv->fw.catas_bar) +
			priv->fw.catas_offset;

		priv->catas_err.map = ioremap(addr, priv->fw.catas_size * 4);
		if (!priv->catas_err.map) {
			mlx4_warn(dev, "Failed to map internal error buffer at 0x%llx\n",
				  (unsigned long long) addr);
			return;
		}
	}

	priv->catas_err.timer.data     = (unsigned long) dev;
	priv->catas_err.timer.function = poll_catas;
	priv->catas_err.timer.expires  =
		round_jiffies(jiffies + MLX4_CATAS_POLL_INTERVAL);
	add_timer(&priv->catas_err.timer);
}

void mlx4_stop_catas_poll(struct mlx4_dev *dev)
{
	struct mlx4_priv *priv = mlx4_priv(dev);

	del_timer_sync(&priv->catas_err.timer);

	if (priv->catas_err.map) {
		iounmap(priv->catas_err.map);
		priv->catas_err.map = NULL;
	}

	if (dev->interface_state & MLX4_INTERFACE_STATE_DELETION)
		flush_workqueue(dev->catas_wq);
}

int  mlx4_catas_init(struct mlx4_dev *dev)
{
	INIT_WORK(&dev->catas_work, catas_reset);
	dev->catas_wq = create_singlethread_workqueue("mlx4_health");
	if (!dev->catas_wq)
		return -ENOMEM;

	return 0;
}

void mlx4_catas_end(struct mlx4_dev *dev)
{
	if (dev->catas_wq) {
		destroy_workqueue(dev->catas_wq);
		dev->catas_wq = NULL;
	}
}
