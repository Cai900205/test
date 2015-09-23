/*
 * Copyright (c) 2007 Mellanox Technologies. All rights reserved.
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
 *
 */

#include <linux/etherdevice.h>
#include <linux/tcp.h>
#include <linux/if_vlan.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/hash.h>
#include <net/ip.h>
#ifdef CONFIG_NET_RX_BUSY_POLL
#include <net/busy_poll.h>
#endif
#ifdef CONFIG_COMPAT_VXLAN_ENABLED
#ifdef CONFIG_COMPAT_VXLAN_DYNAMIC_PORT
#include <net/vxlan.h>
#endif
#endif

#include <linux/mlx4/driver.h>
#include <linux/mlx4/device.h>
#include <linux/mlx4/cmd.h>
#include <linux/mlx4/cq.h>

#include "mlx4_en.h"
#include "en_port.h"

static int mlx4_en_uc_steer_add(struct mlx4_en_priv *priv,
				unsigned char *mac, int *qpn,
				u64 *reg_id, u16 vlan);
static void mlx4_en_uc_steer_release(struct mlx4_en_priv *priv,
				     unsigned char *mac, int qpn, u64 reg_id);
static int mlx4_en_add_to_mac_list(struct mlx4_en_priv *priv, u64 reg_id);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)) || defined (CONFIG_COMPAT_NEW_TX_RING_SCHEME)
int mlx4_en_setup_tc(struct net_device *dev, u8 up)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	int i;
	unsigned int offset = 0;

	if (up && up != MLX4_EN_NUM_UP)
		return -EINVAL;

	netdev_set_num_tc(dev, up);

	/* Partition Tx queues evenly amongst UP's */
	for (i = 0; i < up; i++) {
		netdev_set_tc_queue(dev, i, priv->num_tx_rings_p_up, offset);
		offset += priv->num_tx_rings_p_up;
	}
#ifndef CONFIG_COMPAT_DISABLE_DCB
#ifdef CONFIG_MLX4_EN_DCB
	if (!mlx4_is_slave(priv->mdev->dev)) {
		if (up) {
			priv->flags |= MLX4_EN_FLAG_DCB_ENABLED;
		} else {
			priv->flags &= ~MLX4_EN_FLAG_DCB_ENABLED;
			priv->temp_dcb_cfg.pfc_mode_enable = false;
			priv->dcb_cfg.pfc_mode_enable = false;
		}
	}
#endif /* CONFIG_MLX4_EN_DCB */
#endif /*CONFIG_COMPAT_DISABLE_DCB */

	return 0;
}
#endif

#ifdef CONFIG_NET_RX_BUSY_POLL
/* must be called with local_bh_disable()d */
static int mlx4_en_low_latency_recv(struct napi_struct *napi)
{
	struct mlx4_en_cq *cq = container_of(napi, struct mlx4_en_cq, napi);
	struct net_device *dev = cq->dev;
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_rx_ring *rx_ring = priv->rx_ring[cq->ring];
	int done;

	if (!priv->port_up)
		return LL_FLUSH_FAILED;

	if (!mlx4_en_cq_lock_poll(cq))
		return LL_FLUSH_BUSY;

	done = mlx4_en_process_rx_cq(dev, cq, 4);
#ifdef LL_EXTENDED_STATS
	if (done)
		rx_ring->cleaned += done;
	else
		rx_ring->misses++;
#endif

	mlx4_en_cq_unlock_poll(cq);

	return done;
}
#endif	/* CONFIG_NET_RX_BUSY_POLL */

#ifdef CONFIG_RFS_ACCEL

struct mlx4_en_filter {
	struct list_head next;
	struct work_struct work;

	u8     ip_proto;
	__be32 src_ip;
	__be32 dst_ip;
	__be16 src_port;
	__be16 dst_port;

	int rxq_index;
	struct mlx4_en_priv *priv;
	u32 flow_id;			/* RFS infrastructure id */
	int id;				/* mlx4_en driver id */
	u64 reg_id;			/* Flow steering API id */
	u8 activated;			/* Used to prevent expiry before filter
					 * is attached
					 */
	struct hlist_node filter_chain;
};

static void mlx4_en_filter_rfs_expire(struct mlx4_en_priv *priv);

static enum mlx4_net_trans_rule_id mlx4_ip_proto_to_trans_rule_id(u8 ip_proto)
{
	switch (ip_proto) {
	case IPPROTO_UDP:
		return MLX4_NET_TRANS_RULE_ID_UDP;
	case IPPROTO_TCP:
		return MLX4_NET_TRANS_RULE_ID_TCP;
	default:
		return -EPROTONOSUPPORT;
	}
};

static void mlx4_en_filter_work(struct work_struct *work)
{
	struct mlx4_en_filter *filter = container_of(work,
						     struct mlx4_en_filter,
						     work);
	struct mlx4_en_priv *priv = filter->priv;
	struct mlx4_spec_list spec_tcp_udp = {
		.id = mlx4_ip_proto_to_trans_rule_id(filter->ip_proto),
		{
			.tcp_udp = {
				.dst_port = filter->dst_port,
				.dst_port_msk = (__force __be16)-1,
				.src_port = filter->src_port,
				.src_port_msk = (__force __be16)-1,
			},
		},
	};
	struct mlx4_spec_list spec_ip = {
		.id = MLX4_NET_TRANS_RULE_ID_IPV4,
		{
			.ipv4 = {
				.dst_ip = filter->dst_ip,
				.dst_ip_msk = (__force __be32)-1,
				.src_ip = filter->src_ip,
				.src_ip_msk = (__force __be32)-1,
			},
		},
	};
	struct mlx4_spec_list spec_eth = {
		.id = MLX4_NET_TRANS_RULE_ID_ETH,
	};
	struct mlx4_net_trans_rule rule = {
		.list = LIST_HEAD_INIT(rule.list),
		.queue_mode = MLX4_NET_TRANS_Q_LIFO,
		.exclusive = 1,
		.allow_loopback = 1,
		.promisc_mode = MLX4_FS_REGULAR,
		.port = priv->port,
		.priority = MLX4_DOMAIN_RFS,
	};
	int rc;
	__be64 mac_mask = cpu_to_be64(MLX4_MAC_MASK << 16);

	if (spec_tcp_udp.id < 0) {
		en_warn(priv, "RFS: ignoring unsupported ip protocol (%d)\n",
			filter->ip_proto);
		goto ignore;
	}
	list_add_tail(&spec_eth.list, &rule.list);
	list_add_tail(&spec_ip.list, &rule.list);
	list_add_tail(&spec_tcp_udp.list, &rule.list);

	rule.qpn = priv->rss_map.qps[filter->rxq_index].qpn;
	memcpy(spec_eth.eth.dst_mac, priv->dev->dev_addr, ETH_ALEN);
	memcpy(spec_eth.eth.dst_mac_msk, &mac_mask, ETH_ALEN);

	filter->activated = 0;

	if (filter->reg_id) {
		rc = mlx4_flow_detach(priv->mdev->dev, filter->reg_id);
		if (rc && rc != -ENOENT)
			en_err(priv, "Error detaching flow. rc = %d\n", rc);
	}

	rc = mlx4_flow_attach(priv->mdev->dev, &rule, &filter->reg_id);
	if (rc)
		en_err(priv, "Error attaching flow. err = %d\n", rc);

ignore:
	mlx4_en_filter_rfs_expire(priv);

	filter->activated = 1;
}

static inline struct hlist_head *
filter_hash_bucket(struct mlx4_en_priv *priv, __be32 src_ip, __be32 dst_ip,
		   __be16 src_port, __be16 dst_port)
{
	unsigned long l;
	int bucket_idx;

	l = (__force unsigned long)src_port |
	    ((__force unsigned long)dst_port << 2);
	l ^= (__force unsigned long)(src_ip ^ dst_ip);

	bucket_idx = hash_long(l, MLX4_EN_FILTER_HASH_SHIFT);

	return &priv->filter_hash[bucket_idx];
}

static struct mlx4_en_filter *
mlx4_en_filter_alloc(struct mlx4_en_priv *priv, int rxq_index, __be32 src_ip,
		     __be32 dst_ip, u8 ip_proto, __be16 src_port,
		     __be16 dst_port, u32 flow_id)
{
	struct mlx4_en_filter *filter = NULL;

	filter = kzalloc(sizeof(struct mlx4_en_filter), GFP_ATOMIC);
	if (!filter)
		return NULL;

	filter->priv = priv;
	filter->rxq_index = rxq_index;
	INIT_WORK(&filter->work, mlx4_en_filter_work);

	filter->src_ip = src_ip;
	filter->dst_ip = dst_ip;
	filter->ip_proto = ip_proto;
	filter->src_port = src_port;
	filter->dst_port = dst_port;

	filter->flow_id = flow_id;

	filter->id = priv->last_filter_id++ % RPS_NO_FILTER;

	list_add_tail(&filter->next, &priv->filters);
	hlist_add_head(&filter->filter_chain,
		       filter_hash_bucket(priv, src_ip, dst_ip, src_port,
					  dst_port));

	return filter;
}

static void mlx4_en_filter_free(struct mlx4_en_filter *filter)
{
	struct mlx4_en_priv *priv = filter->priv;
	int rc;

	list_del(&filter->next);

	rc = mlx4_flow_detach(priv->mdev->dev, filter->reg_id);
	if (rc && rc != -ENOENT)
		en_err(priv, "Error detaching flow. rc = %d\n", rc);

	kfree(filter);
}

static inline struct mlx4_en_filter *
mlx4_en_filter_find(struct mlx4_en_priv *priv, __be32 src_ip, __be32 dst_ip,
		    u8 ip_proto, __be16 src_port, __be16 dst_port)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0))
	struct hlist_node *elem;
#endif
	struct mlx4_en_filter *filter;
	struct mlx4_en_filter *ret = NULL;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0))
	hlist_for_each_entry(filter, elem,
#else
	hlist_for_each_entry(filter,
#endif
			     filter_hash_bucket(priv, src_ip, dst_ip,
						src_port, dst_port),
			     filter_chain) {
		if (filter->src_ip == src_ip &&
		    filter->dst_ip == dst_ip &&
		    filter->ip_proto == ip_proto &&
		    filter->src_port == src_port &&
		    filter->dst_port == dst_port) {
			ret = filter;
			break;
		}
	}

	return ret;
}

static int
mlx4_en_filter_rfs(struct net_device *net_dev, const struct sk_buff *skb,
		   u16 rxq_index, u32 flow_id)
{
	struct mlx4_en_priv *priv = netdev_priv(net_dev);
	struct mlx4_en_filter *filter;
	const struct iphdr *ip;
	const __be16 *ports;
	u8 ip_proto;
	__be32 src_ip;
	__be32 dst_ip;
	__be16 src_port;
	__be16 dst_port;
	int nhoff = skb_network_offset(skb);
	int ret = 0;

	if (skb->protocol != htons(ETH_P_IP))
		return -EPROTONOSUPPORT;

	ip = (const struct iphdr *)(skb->data + nhoff);
	if (ip_is_fragment(ip))
		return -EPROTONOSUPPORT;

	if ((ip->protocol != IPPROTO_TCP) && (ip->protocol != IPPROTO_UDP))
		return -EPROTONOSUPPORT;
	ports = (const __be16 *)(skb->data + nhoff + 4 * ip->ihl);

	ip_proto = ip->protocol;
	src_ip = ip->saddr;
	dst_ip = ip->daddr;
	src_port = ports[0];
	dst_port = ports[1];

	spin_lock_bh(&priv->filters_lock);
	filter = mlx4_en_filter_find(priv, src_ip, dst_ip, ip_proto,
				     src_port, dst_port);
	if (filter) {
		if (filter->rxq_index == rxq_index)
			goto out;

		filter->rxq_index = rxq_index;
	} else {
		filter = mlx4_en_filter_alloc(priv, rxq_index,
					      src_ip, dst_ip, ip_proto,
					      src_port, dst_port, flow_id);
		if (!filter) {
			ret = -ENOMEM;
			goto err;
		}
	}

	queue_work(priv->mdev->workqueue, &filter->work);

out:
	ret = filter->id;
err:
	spin_unlock_bh(&priv->filters_lock);

	return ret;
}

void mlx4_en_cleanup_filters(struct mlx4_en_priv *priv)
{
	struct mlx4_en_filter *filter, *tmp;
	LIST_HEAD(del_list);

	spin_lock_bh(&priv->filters_lock);
	list_for_each_entry_safe(filter, tmp, &priv->filters, next) {
		list_move(&filter->next, &del_list);
		hlist_del(&filter->filter_chain);
	}
	spin_unlock_bh(&priv->filters_lock);

	list_for_each_entry_safe(filter, tmp, &del_list, next) {
		cancel_work_sync(&filter->work);
		mlx4_en_filter_free(filter);
	}
}

static void mlx4_en_filter_rfs_expire(struct mlx4_en_priv *priv)
{
	struct mlx4_en_filter *filter = NULL, *tmp, *last_filter = NULL;
	LIST_HEAD(del_list);
	int i = 0;

	spin_lock_bh(&priv->filters_lock);
	list_for_each_entry_safe(filter, tmp, &priv->filters, next) {
		if (i > MLX4_EN_FILTER_EXPIRY_QUOTA)
			break;

		if (filter->activated &&
		    !work_pending(&filter->work) &&
		    rps_may_expire_flow(priv->dev,
					filter->rxq_index, filter->flow_id,
					filter->id)) {
			list_move(&filter->next, &del_list);
			hlist_del(&filter->filter_chain);
		} else
			last_filter = filter;

		i++;
	}

	if (last_filter && (&last_filter->next != priv->filters.next))
		list_move(&priv->filters, &last_filter->next);

	spin_unlock_bh(&priv->filters_lock);

	list_for_each_entry_safe(filter, tmp, &del_list, next)
		mlx4_en_filter_free(filter);
}
#endif

static void mlx4_en_update_vlan_start_index(struct mlx4_en_priv *priv,
					    u16 vid, int ring_num)
{
	int i;

	for (i = 0; i < MAX_VLANS; i++)
		if (priv->vsi[i].vlan_id == 0) {
			priv->vsi[i].vlan_id = vid;
			priv->vsi[i].start_tx_index = ring_num;
			return;
		}
}

static int mlx4_en_vlan_new(struct mlx4_en_priv *priv, u16 vid)
{
	int i;

	for (i = 0; i < MAX_VLANS; i++)
		if (priv->vsi[i].vlan_id == vid)
			return 0;
		return 1;
}

static void mlx4_en_remove_tx_rings_per_vlan(struct mlx4_en_priv *priv)
{
	struct net_device *dev = priv->dev;
	int i, j;

	for (i = 0; i < MLX4_EN_NUM_TX_RING_PER_VLAN; i++) {
		j = priv->tx_ring_num - (i + 1);
		mlx4_en_deactivate_tx_ring(priv,
					   priv->tx_ring[j]);
		mlx4_en_deactivate_cq(priv, priv->tx_cq[j]);
		if (priv->tx_ring[j])
			mlx4_en_destroy_tx_ring(priv, &priv->tx_ring[j]);
		if (priv->tx_cq[j])
			mlx4_en_destroy_cq(priv, &priv->tx_cq[j]);
	}
	priv->tx_ring_num -= MLX4_EN_NUM_TX_RING_PER_VLAN;
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)) || defined (CONFIG_COMPAT_IS_NUM_TX_QUEUES) || defined (CONFIG_X86_XEN)) && \
	!defined (CONFIG_COMPAT_DISABLE_REAL_NUM_TXQ)
	netif_set_real_num_tx_queues(dev, priv->tx_ring_num);
#else
	dev->real_num_tx_queues = priv->tx_ring_num;
#endif
}

static int mlx4_en_add_tx_rings_per_vlan(struct mlx4_en_priv *priv,
					 u16 vid, int idx)
{
	struct net_device *dev = priv->dev;
	struct mlx4_en_port_profile *prof = priv->prof;
	struct mlx4_en_cq *cq;
	struct mlx4_en_tx_ring *tx_ring;
	int err = 0;
	int i;
	int j;
	int node;

	for (i = priv->tx_ring_num;
	     i < priv->tx_ring_num + MLX4_EN_NUM_TX_RING_PER_VLAN;
	     i++) {
		node = cpu_to_node(priv->current_cpu);
		if (mlx4_en_create_cq(priv, &priv->tx_cq[i],
				      prof->tx_ring_size, i, TX, node)) {
			en_err(priv, "Failed to create Tx CQ\n");
			goto err;
		}

		mlx4_en_set_cq_affinity(priv, priv->tx_cq[i]);

		if (mlx4_en_create_tx_ring(priv, &priv->tx_ring[i],
					   prof->tx_ring_size, TXBB_SIZE,
					   node)) {
			en_err(priv, "Failed to create Tx Ring\n");
			goto ring_err;
		}

		/* Configure cq */
		cq = priv->tx_cq[i];
		err = mlx4_en_activate_cq(priv, cq, i);
		if (err) {
			en_err(priv, "Failed to activate Tx CQ\n");
			goto tx_err;
		}
		err = mlx4_en_set_cq_moder(priv, cq);
		if (err) {
			en_err(priv, "Failed setting cq moderation parameters");
			goto cq_err;
		}
		en_dbg(DRV, priv,
		       "Resetting index of collapsed CQ:%d to -1\n", i);
		cq->buf->wqe_index = cpu_to_be16(0xffff);

		/* Configure ring */
		tx_ring = priv->tx_ring[i];

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)) || defined (CONFIG_COMPAT_NEW_TX_RING_SCHEME)
		err = mlx4_en_activate_tx_ring(priv, tx_ring,
					       cq->mcq.cqn, 0, idx);
#else
		err = mlx4_en_activate_tx_ring(priv, tx_ring,
					       cq->mcq.cqn, idx);
#endif
		if (err) {
			en_err(priv, "Failed allocating Tx ring\n");
			goto cq_err;
		}
		tx_ring->tx_queue = netdev_get_tx_queue(dev, i);

		/* Arm CQ for TX completions */
		mlx4_en_arm_cq(priv, cq);

		/* Set initial ownership of all Tx TXBBs to SW (1) */
		for (j = 0; j < tx_ring->buf_size; j += STAMP_STRIDE)
			*((u32 *)(tx_ring->buf + j)) = 0xffffffff;
	}
	mlx4_en_update_vlan_start_index(priv, vid, priv->tx_ring_num);
	priv->tx_ring_num += MLX4_EN_NUM_TX_RING_PER_VLAN;
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)) || defined (CONFIG_COMPAT_IS_NUM_TX_QUEUES) || defined (CONFIG_X86_XEN)) && \
	!defined (CONFIG_COMPAT_DISABLE_REAL_NUM_TXQ)
	netif_set_real_num_tx_queues(dev, priv->tx_ring_num);
#else
	dev->real_num_tx_queues = priv->tx_ring_num;
#endif

	return 0;

cq_err:
	mlx4_en_deactivate_cq(priv, priv->tx_cq[i]);

tx_err:
	mlx4_en_destroy_tx_ring(priv, &priv->tx_ring[i]);

ring_err:
	mlx4_en_destroy_cq(priv, &priv->tx_cq[i]);

err:
	en_err(priv, "Failed to allocate NIC resources per VLAN\n");
	for (j = priv->tx_ring_num; j < priv->tx_ring_num + i; j++) {
		mlx4_en_deactivate_tx_ring(priv, priv->tx_ring[j]);
		mlx4_en_deactivate_cq(priv, priv->tx_cq[j]);
		if (priv->tx_ring[j])
			mlx4_en_destroy_tx_ring(priv, &priv->tx_ring[j]);
		if (priv->tx_cq[j])
			mlx4_en_destroy_cq(priv, &priv->tx_cq[j]);
	}
	return -ENOMEM;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0))
static void mlx4_en_vlan_rx_register(struct net_device *dev, struct vlan_group *grp)
{
        struct mlx4_en_priv *priv = netdev_priv(dev);

        en_dbg(HW, priv, "Registering VLAN group:%p\n", grp);

        priv->vlgrp = grp;
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
static int mlx4_en_vlan_rx_add_vid(struct net_device *dev, __be16 proto,
				    unsigned short vid)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
static int mlx4_en_vlan_rx_add_vid(struct net_device *dev, unsigned short vid)
#else
static void mlx4_en_vlan_rx_add_vid(struct net_device *dev, unsigned short vid)
#endif
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int *qpn = &priv->base_qpn;
	u64 reg_id;
	int err;
	int idx;

	en_dbg(HW, priv, "adding VLAN:%d\n", vid);

	set_bit(vid, priv->active_vlans);

	/* Add VID to port VLAN filter */
	mutex_lock(&mdev->state_lock);
	if (mdev->device_up && priv->port_up) {
		err = mlx4_SET_VLAN_FLTR(mdev->dev, priv);
		if (err)
			en_err(priv, "Failed configuring VLAN filter\n");
	}
	err = mlx4_register_vlan(mdev->dev, priv->port, vid, &idx);
	if (err) {
		if (mdev->dev->caps.force_vlan[priv->port - 1]) {
			en_err(priv,
			       "Failed to add VLAN %d due to VLAN policy\n",
			       vid);
			mutex_unlock(&mdev->state_lock);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
			return -EPERM;
#else
			return;
#endif
		}
		en_dbg(HW, priv, "failed adding vlan %d\n", vid);
		goto out;
	}

	if (mdev->dev->caps.force_vlan[priv->port - 1] &&
	    mlx4_en_vlan_new(priv, vid)) {
		u64 mac = mlx4_mac_to_u64(priv->dev->dev_addr);
		if (!vid)
			goto out;

		err = mlx4_en_uc_steer_add(priv, priv->dev->dev_addr,
					   qpn, &reg_id, vid);
		if (err) {
			en_err(priv, "Failed to open VLAN %hu\n", vid);
			goto out;
		}
		err = mlx4_register_mac(priv->mdev->dev, priv->port, mac);
		if (err < 0) {
			en_err(priv, "Failed to register MAC per VLAN\n");
			goto steer_err;
		}

		err = mlx4_en_add_tx_rings_per_vlan(priv, vid, idx);
		if (err) {
			en_err(priv, "Failed to create rings per VLAN\n");
			goto mac_err;
		}

		err = mlx4_en_add_to_mac_list(priv, reg_id);
		if (err) {
			en_err(priv, "Failed to create rings per VLAN\n");
			goto rings_err;
		}
		goto out;

rings_err:
		mlx4_en_remove_tx_rings_per_vlan(priv);
mac_err:
		mlx4_unregister_mac(priv->mdev->dev, priv->port, mac);
steer_err:
		mlx4_en_uc_steer_release(priv, priv->dev->dev_addr, *qpn, reg_id);
	}

out:
	mutex_unlock(&mdev->state_lock);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
	return 0;
#endif
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
static int mlx4_en_vlan_rx_kill_vid(struct net_device *dev, __be16 proto,
				     unsigned short vid)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
static int mlx4_en_vlan_rx_kill_vid(struct net_device *dev, unsigned short vid)
#else
static void mlx4_en_vlan_rx_kill_vid(struct net_device *dev, unsigned short vid)
#endif
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int err;

	en_dbg(HW, priv, "Killing VID:%d\n", vid);

	clear_bit(vid, priv->active_vlans);

	/* Remove VID from port VLAN filter */
	mutex_lock(&mdev->state_lock);
	mlx4_unregister_vlan(mdev->dev, priv->port, vid);

	if (mdev->device_up && priv->port_up) {
		err = mlx4_SET_VLAN_FLTR(mdev->dev, priv);
		if (err)
			en_err(priv, "Failed configuring VLAN filter\n");
	}
	mutex_unlock(&mdev->state_lock);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
	return 0;
#endif
}

static void mlx4_en_u64_to_mac(unsigned char dst_mac[ETH_ALEN + 2], u64 src_mac)
{
	int i;

	for (i = ETH_ALEN; i; i--) {
		dst_mac[i - 1] = src_mac & 0xff;
		src_mac >>= 8;
	}
	memset(&dst_mac[ETH_ALEN], 0, 2);
}

#ifdef CONFIG_COMPAT_VXLAN_ENABLED
static int mlx4_en_tunnel_steer_add(struct mlx4_en_priv *priv, unsigned char *addr,
				    int qpn, u64 *reg_id)
{
	int err;

	if (priv->mdev->dev->caps.tunnel_offload_mode != MLX4_TUNNEL_OFFLOAD_MODE_VXLAN)
		return 0; /* do nothing */

	err = mlx4_tunnel_steer_add(priv->mdev->dev, addr, priv->port, qpn,
				    MLX4_DOMAIN_NIC, reg_id);
	if (err) {
		en_err(priv, "failed to add vxlan steering rule, err %d\n", err);
		return err;
	}
	en_dbg(DRV, priv, "added vxlan steering rule, mac %pM reg_id %llx\n", addr, *reg_id);
	return 0;
}
#endif

static int mlx4_en_add_to_mac_list(struct mlx4_en_priv *priv, u64 reg_id)
{
	struct mlx4_mac_entry *entry;

	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return -ENOMEM;

	memcpy(entry->mac, priv->dev->dev_addr, sizeof(entry->mac));
	entry->reg_id = reg_id;

	hlist_add_head_rcu(&entry->hlist,
			   &priv->mac_hash[entry->mac[MLX4_EN_MAC_HASH_IDX]]);

	return 0;
}

static int mlx4_en_uc_steer_add(struct mlx4_en_priv *priv,
				unsigned char *mac, int *qpn,
				u64 *reg_id, u16 vlan)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_dev *dev = mdev->dev;
	int err;

	switch (dev->caps.steering_mode) {
	case MLX4_STEERING_MODE_B0: {
		struct mlx4_qp qp;
		u8 gid[16] = {0};

		if (vlan != MLX4_EN_NO_VLAN) {
			en_err(priv, "Invalid parameter for current steering mode\n");
			err = -EINVAL;
			break;
		}
		qp.qpn = *qpn;
		memcpy(&gid[10], mac, ETH_ALEN);
		gid[5] = priv->port;

		err = mlx4_unicast_attach(dev, &qp, gid, 0, MLX4_PROT_ETH);
		break;
	}
	case MLX4_STEERING_MODE_DEVICE_MANAGED: {
		struct mlx4_spec_list spec_eth = { {NULL} };
		__be64 mac_mask = cpu_to_be64(MLX4_MAC_MASK << 16);

		struct mlx4_net_trans_rule rule = {
			.queue_mode = MLX4_NET_TRANS_Q_FIFO,
			.exclusive = 0,
			.allow_loopback = 1,
			.promisc_mode = MLX4_FS_REGULAR,
			.priority = MLX4_DOMAIN_NIC,
		};

		rule.port = priv->port;
		rule.qpn = *qpn;
		INIT_LIST_HEAD(&rule.list);

		spec_eth.id = MLX4_NET_TRANS_RULE_ID_ETH;
		memcpy(spec_eth.eth.dst_mac, mac, ETH_ALEN);
		memcpy(spec_eth.eth.dst_mac_msk, &mac_mask, ETH_ALEN);
		if (vlan != MLX4_EN_NO_VLAN) {
			spec_eth.eth.vlan_id = cpu_to_be16(vlan);
			spec_eth.eth.vlan_id_msk = cpu_to_be16(0xfff);
		}
		list_add_tail(&spec_eth.list, &rule.list);

		err = mlx4_flow_attach(dev, &rule, reg_id);
		break;
	}
	default:
		return -EINVAL;
	}
	if (err)
		en_warn(priv, "Failed Attaching Unicast\n");

	return err;
}

static void mlx4_en_uc_steer_release(struct mlx4_en_priv *priv,
				     unsigned char *mac, int qpn, u64 reg_id)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_dev *dev = mdev->dev;

	switch (dev->caps.steering_mode) {
	case MLX4_STEERING_MODE_B0: {
		struct mlx4_qp qp;
		u8 gid[16] = {0};

		qp.qpn = qpn;
		memcpy(&gid[10], mac, ETH_ALEN);
		gid[5] = priv->port;

		mlx4_unicast_detach(dev, &qp, gid, MLX4_PROT_ETH);
		break;
	}
	case MLX4_STEERING_MODE_DEVICE_MANAGED: {
		mlx4_flow_detach(dev, reg_id);
		break;
	}
	default:
		en_err(priv, "Invalid steering mode.\n");
	}
}

static int mlx4_en_set_rss_steer_rules(struct mlx4_en_priv *priv)
{
	int *qpn = &priv->base_qpn;
	int err = 0;
	u64 reg_id;

	err = mlx4_en_uc_steer_add(priv, priv->dev->dev_addr,
				   qpn, &reg_id, MLX4_EN_NO_VLAN);
	if (err)
		return err;

#ifdef CONFIG_COMPAT_VXLAN_ENABLED
	err = mlx4_en_tunnel_steer_add(priv, priv->dev->dev_addr, *qpn,
				       &priv->tunnel_reg_id);
	if (err)
		goto tunnel_err;
#endif

	err = mlx4_en_add_to_mac_list(priv, reg_id);
	if (err) {
		err = -ENOMEM;
		goto alloc_err;
	}

	return 0;

alloc_err:
#ifdef CONFIG_COMPAT_VXLAN_ENABLED
	if (priv->tunnel_reg_id)
		mlx4_flow_detach(priv->mdev->dev, priv->tunnel_reg_id);

tunnel_err:
#endif
	mlx4_en_uc_steer_release(priv, priv->dev->dev_addr, *qpn, reg_id);
	return err;
}

static void mlx4_en_delete_rss_steer_rules(struct mlx4_en_priv *priv)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_dev *dev = mdev->dev;
	struct mlx4_mac_entry *entry;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0))
	struct hlist_node *n, *tmp;
#else
	struct hlist_node *tmp;
#endif
	struct hlist_head *bucket;
	int qpn = priv->base_qpn;
	u64 mac;
	unsigned int i;

	for (i = 0; i < MLX4_EN_MAC_HASH_SIZE; ++i) {
		bucket = &priv->mac_hash[i];
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0))
		hlist_for_each_entry_safe(entry, n, tmp, bucket, hlist) {
#else
		hlist_for_each_entry_safe(entry, tmp, bucket, hlist) {
#endif
			mac = mlx4_mac_to_u64(entry->mac);
			en_dbg(DRV, priv, "Registering MAC: %pM for deleting\n",
			       entry->mac);
			mlx4_en_uc_steer_release(priv, entry->mac,
						 qpn, entry->reg_id);

			mlx4_unregister_mac(dev, priv->port, mac);
			hlist_del_rcu(&entry->hlist);
			kfree_rcu(entry, rcu);
		}
	}

#ifdef CONFIG_COMPAT_VXLAN_ENABLED
	if (priv->tunnel_reg_id) {
		mlx4_flow_detach(priv->mdev->dev, priv->tunnel_reg_id);
		priv->tunnel_reg_id = 0;
	}
#endif
}

static int mlx4_en_get_qp(struct mlx4_en_priv *priv)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_dev *dev = mdev->dev;
	int index = 0;
	int err = 0;
	int *qpn = &priv->base_qpn;
	u64 mac = mlx4_mac_to_u64(priv->dev->dev_addr);

	en_dbg(DRV, priv, "Registering MAC: %pM for adding\n",
	       priv->dev->dev_addr);
	index = mlx4_register_mac(dev, priv->port, mac);
	if (index < 0) {
		err = index;
		en_err(priv, "Failed adding MAC: %pM\n",
		       priv->dev->dev_addr);
		return err;
	}

	if (dev->caps.steering_mode == MLX4_STEERING_MODE_A0) {
		int base_qpn = mlx4_get_base_qpn(dev, priv->port);
		*qpn = base_qpn + index;
		return 0;
	}

	err = mlx4_qp_reserve_range(dev, 1, 1, qpn, MLX4_RESERVE_A0_RSS);
	en_dbg(DRV, priv, "Reserved qp %d\n", *qpn);
	if (err) {
		en_err(priv, "Failed to reserve qp for mac registration\n");
		mlx4_unregister_mac(dev, priv->port, mac);
		return err;
	}

	return 0;
}

static void mlx4_en_put_qp(struct mlx4_en_priv *priv)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_dev *dev = mdev->dev;
	int qpn = priv->base_qpn;

	if (dev->caps.steering_mode == MLX4_STEERING_MODE_A0) {
		u64 mac = mlx4_mac_to_u64(priv->dev->dev_addr);
		en_dbg(DRV, priv, "Registering MAC: %pM for deleting\n",
		       priv->dev->dev_addr);
		mlx4_unregister_mac(dev, priv->port, mac);
	} else {
		en_dbg(DRV, priv, "Releasing qp: port %d, qpn %d\n",
		       priv->port, qpn);
		mlx4_qp_release_range(dev, qpn, 1);
		priv->flags &= ~MLX4_EN_FLAG_FORCE_PROMISC;
	}
}

static int mlx4_en_replace_mac(struct mlx4_en_priv *priv, int qpn,
			       unsigned char *new_mac, unsigned char *prev_mac)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_dev *dev = mdev->dev;
	int err = 0;
	u64 new_mac_u64 = mlx4_mac_to_u64(new_mac);

	if (dev->caps.steering_mode != MLX4_STEERING_MODE_A0) {
		struct hlist_head *bucket;
		unsigned int mac_hash;
		struct mlx4_mac_entry *entry;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0))
		struct hlist_node *n, *tmp;
#else
		struct hlist_node *tmp;
#endif
		u64 prev_mac_u64 = mlx4_mac_to_u64(prev_mac);

		bucket = &priv->mac_hash[prev_mac[MLX4_EN_MAC_HASH_IDX]];
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0))
		hlist_for_each_entry_safe(entry, n, tmp, bucket, hlist) {
#else
		hlist_for_each_entry_safe(entry, tmp, bucket, hlist) {
#endif
			if (ether_addr_equal_64bits(entry->mac, prev_mac)) {
				mlx4_en_uc_steer_release(priv, entry->mac,
							 qpn, entry->reg_id);
				mlx4_unregister_mac(dev, priv->port,
						    prev_mac_u64);
				hlist_del_rcu(&entry->hlist);
				synchronize_rcu();
				memcpy(entry->mac, new_mac, ETH_ALEN);
				entry->reg_id = 0;
				mac_hash = new_mac[MLX4_EN_MAC_HASH_IDX];
				hlist_add_head_rcu(&entry->hlist,
						   &priv->mac_hash[mac_hash]);
				synchronize_rcu();
				err = mlx4_register_mac(dev, priv->port,
							new_mac_u64);
				if (err < 0)
					return err;
				err = mlx4_en_uc_steer_add(priv, new_mac,
							   &qpn,
							   &entry->reg_id,
							   MLX4_EN_NO_VLAN);
#ifdef CONFIG_COMPAT_VXLAN_ENABLED
				if (err)
					return err;
				if (priv->tunnel_reg_id) {
					mlx4_flow_detach(priv->mdev->dev,
							 priv->tunnel_reg_id);
					priv->tunnel_reg_id = 0;
				}
				err = mlx4_en_tunnel_steer_add(priv, new_mac, qpn,
							       &priv->tunnel_reg_id);
#endif
				return err;
			}
		}
		return -EINVAL;
	}

	return __mlx4_replace_mac(dev, priv->port, qpn, new_mac_u64);
}

static int mlx4_en_do_set_mac(struct mlx4_en_priv *priv,
				unsigned char new_mac[ETH_ALEN + 2])
{
	int err = 0;

	if (priv->port_up) {
		/* Remove old MAC and insert the new one */
		err = mlx4_en_replace_mac(priv, priv->base_qpn,
					  new_mac, priv->current_mac);
		if (err)
			en_err(priv, "Failed changing HW MAC address\n");
	} else
		en_dbg(HW, priv, "Port is down while registering mac, exiting...\n");

	if (!err)
		memcpy(priv->current_mac, new_mac, sizeof(priv->current_mac));

	return err;
}

static int mlx4_en_set_mac(struct net_device *dev, void *addr)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	struct sockaddr *saddr = addr;
	unsigned char new_mac[ETH_ALEN + 2];
	int err;

	if (!is_valid_ether_addr(saddr->sa_data))
		return -EADDRNOTAVAIL;

	mutex_lock(&mdev->state_lock);
	memcpy(new_mac, saddr->sa_data, ETH_ALEN);
	err = mlx4_en_do_set_mac(priv, new_mac);
	if (!err)
		memcpy(dev->dev_addr, saddr->sa_data, ETH_ALEN);
	mutex_unlock(&mdev->state_lock);

	return err;
}

static void mlx4_en_clear_list(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_mc_list *tmp, *mc_to_del;

	list_for_each_entry_safe(mc_to_del, tmp, &priv->mc_list, list) {
		list_del(&mc_to_del->list);
		kfree(mc_to_del);
	}
}

static void mlx4_en_cache_mclist(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))
	struct netdev_hw_addr *ha;
#else
	struct dev_mc_list *mclist;
#endif
	struct mlx4_en_mc_list *tmp;

	mlx4_en_clear_list(dev);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))
	netdev_for_each_mc_addr(ha, dev) {
#else
	for (mclist = dev->mc_list; mclist; mclist = mclist->next) {
#endif
		tmp = kzalloc(sizeof(struct mlx4_en_mc_list), GFP_ATOMIC);
		if (!tmp) {
			en_err(priv, "failed to allocate multicast list\n");
			mlx4_en_clear_list(dev);
			return;
		}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))
		memcpy(tmp->addr, ha->addr, ETH_ALEN);
#else
		memcpy(tmp->addr, mclist->dmi_addr, ETH_ALEN);
#endif
		list_add_tail(&tmp->list, &priv->mc_list);
	}
}

static void update_mclist_flags(struct mlx4_en_priv *priv,
				struct list_head *dst,
				struct list_head *src)
{
	struct mlx4_en_mc_list *dst_tmp, *src_tmp, *new_mc;
	bool found;

	/* Find all the entries that should be removed from dst,
	 * These are the entries that are not found in src
	 */
	list_for_each_entry(dst_tmp, dst, list) {
		found = false;
		list_for_each_entry(src_tmp, src, list) {
			if (!memcmp(dst_tmp->addr, src_tmp->addr, ETH_ALEN)) {
				found = true;
				break;
			}
		}
		if (!found)
			dst_tmp->action = MCLIST_REM;
	}

	/* Add entries that exist in src but not in dst
	 * mark them as need to add
	 */
	list_for_each_entry(src_tmp, src, list) {
		found = false;
		list_for_each_entry(dst_tmp, dst, list) {
			if (!memcmp(dst_tmp->addr, src_tmp->addr, ETH_ALEN)) {
				dst_tmp->action = MCLIST_NONE;
				found = true;
				break;
			}
		}
		if (!found) {
			new_mc = kmalloc(sizeof(struct mlx4_en_mc_list),
					 GFP_KERNEL);
			if (!new_mc) {
				en_err(priv, "Failed to allocate current multicast list\n");
				return;
			}
			memcpy(new_mc, src_tmp,
			       sizeof(struct mlx4_en_mc_list));
			new_mc->action = MCLIST_ADD;
			list_add_tail(&new_mc->list, dst);
		}
	}
}

static void mlx4_en_set_rx_mode(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);

	if (!priv->port_up)
		return;

	queue_work(priv->mdev->workqueue, &priv->rx_mode_task);
}

static void mlx4_en_set_promisc_mode(struct mlx4_en_priv *priv,
				     struct mlx4_en_dev *mdev)
{
	int err = 0;

	if (!(priv->flags & MLX4_EN_FLAG_PROMISC)) {
		if (netif_msg_rx_status(priv))
			en_warn(priv, "Entering promiscuous mode\n");
		priv->flags |= MLX4_EN_FLAG_PROMISC;

		/* Enable promiscouos mode */
		switch (mdev->dev->caps.steering_mode) {
		case MLX4_STEERING_MODE_DEVICE_MANAGED:
			err = mlx4_flow_steer_promisc_add(mdev->dev,
							  priv->port,
							  priv->base_qpn,
							  MLX4_FS_ALL_DEFAULT);
			if (err)
				en_err(priv, "Failed enabling promiscuous mode\n");
			priv->flags |= MLX4_EN_FLAG_MC_PROMISC;
			break;

		case MLX4_STEERING_MODE_B0:
			err = mlx4_unicast_promisc_add(mdev->dev,
						       priv->base_qpn,
						       priv->port);
			if (err)
				en_err(priv, "Failed enabling unicast promiscuous mode\n");

			/* Add the default qp number as multicast
			 * promisc
			 */
			if (!(priv->flags & MLX4_EN_FLAG_MC_PROMISC)) {
				err = mlx4_multicast_promisc_add(mdev->dev,
								 priv->base_qpn,
								 priv->port);
				if (err)
					en_err(priv, "Failed enabling multicast promiscuous mode\n");
				priv->flags |= MLX4_EN_FLAG_MC_PROMISC;
			}
			break;

		case MLX4_STEERING_MODE_A0:
			err = mlx4_SET_PORT_qpn_calc(mdev->dev,
						     priv->port,
						     priv->base_qpn,
						     1);
			if (err)
				en_err(priv, "Failed enabling promiscuous mode\n");
			break;
		}

		/* Disable port multicast filter (unconditionally) */
		err = mlx4_SET_MCAST_FLTR(mdev->dev, priv->port, 0,
					  0, MLX4_MCAST_DISABLE);
		if (err)
			en_err(priv, "Failed disabling multicast filter\n");
	}
}

static void mlx4_en_clear_promisc_mode(struct mlx4_en_priv *priv,
				       struct mlx4_en_dev *mdev)
{
	int err = 0;

	if (netif_msg_rx_status(priv))
		en_warn(priv, "Leaving promiscuous mode\n");
	priv->flags &= ~MLX4_EN_FLAG_PROMISC;

	/* Disable promiscouos mode */
	switch (mdev->dev->caps.steering_mode) {
	case MLX4_STEERING_MODE_DEVICE_MANAGED:
		err = mlx4_flow_steer_promisc_remove(mdev->dev,
						     priv->port,
						     MLX4_FS_ALL_DEFAULT);
		if (err)
			en_err(priv, "Failed disabling promiscuous mode\n");
		priv->flags &= ~MLX4_EN_FLAG_MC_PROMISC;
		break;

	case MLX4_STEERING_MODE_B0:
		err = mlx4_unicast_promisc_remove(mdev->dev,
						  priv->base_qpn,
						  priv->port);
		if (err)
			en_err(priv, "Failed disabling unicast promiscuous mode\n");
		/* Disable Multicast promisc */
		if (priv->flags & MLX4_EN_FLAG_MC_PROMISC) {
			err = mlx4_multicast_promisc_remove(mdev->dev,
							    priv->base_qpn,
							    priv->port);
			if (err)
				en_err(priv, "Failed disabling multicast promiscuous mode\n");
			priv->flags &= ~MLX4_EN_FLAG_MC_PROMISC;
		}
		break;

	case MLX4_STEERING_MODE_A0:
		err = mlx4_SET_PORT_qpn_calc(mdev->dev,
					     priv->port,
					     priv->base_qpn, 0);
		if (err)
			en_err(priv, "Failed disabling promiscuous mode\n");
		break;
	}
}

static void mlx4_en_do_multicast(struct mlx4_en_priv *priv,
				 struct net_device *dev,
				 struct mlx4_en_dev *mdev)
{
	struct mlx4_en_mc_list *mclist, *tmp;
	u64 mcast_addr = 0;
	u8 mc_list[16] = {0};
	int err = 0;

	/* Enable/disable the multicast filter according to IFF_ALLMULTI */
	if (dev->flags & IFF_ALLMULTI) {
		err = mlx4_SET_MCAST_FLTR(mdev->dev, priv->port, 0,
					  0, MLX4_MCAST_DISABLE);
		if (err)
			en_err(priv, "Failed disabling multicast filter\n");

		/* Add the default qp number as multicast promisc */
		if (!(priv->flags & MLX4_EN_FLAG_MC_PROMISC)) {
			switch (mdev->dev->caps.steering_mode) {
			case MLX4_STEERING_MODE_DEVICE_MANAGED:
				err = mlx4_flow_steer_promisc_add(mdev->dev,
								  priv->port,
								  priv->base_qpn,
								  MLX4_FS_MC_DEFAULT);
				break;

			case MLX4_STEERING_MODE_B0:
				err = mlx4_multicast_promisc_add(mdev->dev,
								 priv->base_qpn,
								 priv->port);
				break;

			case MLX4_STEERING_MODE_A0:
				break;
			}
			if (err)
				en_err(priv, "Failed entering multicast promisc mode\n");
			priv->flags |= MLX4_EN_FLAG_MC_PROMISC;
		}
	} else {
		/* Disable Multicast promisc */
		if (priv->flags & MLX4_EN_FLAG_MC_PROMISC) {
			switch (mdev->dev->caps.steering_mode) {
			case MLX4_STEERING_MODE_DEVICE_MANAGED:
				err = mlx4_flow_steer_promisc_remove(mdev->dev,
								     priv->port,
								     MLX4_FS_MC_DEFAULT);
				break;

			case MLX4_STEERING_MODE_B0:
				err = mlx4_multicast_promisc_remove(mdev->dev,
								    priv->base_qpn,
								    priv->port);
				break;

			case MLX4_STEERING_MODE_A0:
				break;
			}
			if (err)
				en_err(priv, "Failed disabling multicast promiscuous mode\n");
			priv->flags &= ~MLX4_EN_FLAG_MC_PROMISC;
		}

		err = mlx4_SET_MCAST_FLTR(mdev->dev, priv->port, 0,
					  0, MLX4_MCAST_DISABLE);
		if (err)
			en_err(priv, "Failed disabling multicast filter\n");

		/* Flush mcast filter and init it with broadcast address */
		mlx4_SET_MCAST_FLTR(mdev->dev, priv->port, ETH_BCAST,
				    1, MLX4_MCAST_CONFIG);

		/* Update multicast list - we cache all addresses so they won't
		 * change while HW is updated holding the command semaphor */
		netif_addr_lock_bh(dev);
		mlx4_en_cache_mclist(dev);
		netif_addr_unlock_bh(dev);
		list_for_each_entry(mclist, &priv->mc_list, list) {
			mcast_addr = mlx4_mac_to_u64(mclist->addr);
			mlx4_SET_MCAST_FLTR(mdev->dev, priv->port,
					    mcast_addr, 0, MLX4_MCAST_CONFIG);
		}
		err = mlx4_SET_MCAST_FLTR(mdev->dev, priv->port, 0,
					  0, MLX4_MCAST_ENABLE);
		if (err)
			en_err(priv, "Failed enabling multicast filter\n");

		update_mclist_flags(priv, &priv->curr_list, &priv->mc_list);
		list_for_each_entry_safe(mclist, tmp, &priv->curr_list, list) {
			if (mclist->action == MCLIST_REM) {
				/* detach this address and delete from list */
				memcpy(&mc_list[10], mclist->addr, ETH_ALEN);
				mc_list[5] = priv->port;
				err = mlx4_multicast_detach(mdev->dev,
							    &priv->rss_map.indir_qp,
							    mc_list,
							    MLX4_PROT_ETH,
							    mclist->reg_id);
				if (err)
					en_err(priv, "Fail to detach multicast address\n");

#ifdef CONFIG_COMPAT_VXLAN_ENABLED
				if (mclist->tunnel_reg_id) {
					err = mlx4_flow_detach(priv->mdev->dev, mclist->tunnel_reg_id);
					if (err)
						en_err(priv, "Failed to detach multicast address\n");
				}
#endif

				/* remove from list */
				list_del(&mclist->list);
				kfree(mclist);
			} else if (mclist->action == MCLIST_ADD) {
				/* attach the address */
				memcpy(&mc_list[10], mclist->addr, ETH_ALEN);
				/* needed for B0 steering support */
				mc_list[5] = priv->port;
				err = mlx4_multicast_attach(mdev->dev,
							    &priv->rss_map.indir_qp,
							    mc_list,
							    priv->port, 0,
							    MLX4_PROT_ETH,
							    &mclist->reg_id);
				if (err)
					en_err(priv, "Fail to attach multicast address\n");

#ifdef CONFIG_COMPAT_VXLAN_ENABLED
				err = mlx4_en_tunnel_steer_add(priv, &mc_list[10], priv->base_qpn,
							       &mclist->tunnel_reg_id);
				if (err)
					en_err(priv, "Failed to attach multicast address\n");
#endif
			}
		}
	}
}

static void mlx4_en_do_uc_filter(struct mlx4_en_priv *priv,
				 struct net_device *dev,
				 struct mlx4_en_dev *mdev)
{
	struct netdev_hw_addr *ha;
	struct mlx4_mac_entry *entry;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0))
	struct hlist_node *n, *tmp;
#else
	struct hlist_node *tmp;
#endif
	bool found;
	u64 mac;
	int err = 0;
	struct hlist_head *bucket;
	unsigned int i;
	int removed = 0;
	u32 prev_flags;

	/* Note that we do not need to protect our mac_hash traversal with rcu,
	 * since all modification code is protected by mdev->state_lock
	 */

	/* find what to remove */
	for (i = 0; i < MLX4_EN_MAC_HASH_SIZE; ++i) {
		bucket = &priv->mac_hash[i];
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0))
		hlist_for_each_entry_safe(entry, n, tmp, bucket, hlist) {
#else
		hlist_for_each_entry_safe(entry, tmp, bucket, hlist) {
#endif
			found = false;
			netdev_for_each_uc_addr(ha, dev) {
				if (ether_addr_equal_64bits(entry->mac,
							    ha->addr)) {
					found = true;
					break;
				}
			}

			/* MAC address of the port is not in uc list */
			if (ether_addr_equal_64bits(entry->mac,
						    priv->current_mac))
				found = true;

			if (!found) {
				mac = mlx4_mac_to_u64(entry->mac);
				mlx4_en_uc_steer_release(priv, entry->mac,
							 priv->base_qpn,
							 entry->reg_id);
				mlx4_unregister_mac(mdev->dev, priv->port, mac);

				hlist_del_rcu(&entry->hlist);
				kfree_rcu(entry, rcu);
				en_dbg(DRV, priv, "Removed MAC %pM on port:%d\n",
				       entry->mac, priv->port);
				++removed;
			}
		}
	}

	/* if we didn't remove anything, there is no use in trying to add
	 * again once we are in a forced promisc mode state
	 */
	if ((priv->flags & MLX4_EN_FLAG_FORCE_PROMISC) && 0 == removed)
		return;

	prev_flags = priv->flags;
	priv->flags &= ~MLX4_EN_FLAG_FORCE_PROMISC;

	/* find what to add */
	netdev_for_each_uc_addr(ha, dev) {
		found = false;
		bucket = &priv->mac_hash[ha->addr[MLX4_EN_MAC_HASH_IDX]];
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0))
		hlist_for_each_entry(entry, n, bucket, hlist) {
#else
		hlist_for_each_entry(entry, bucket, hlist) {
#endif
			if (ether_addr_equal_64bits(entry->mac, ha->addr)) {
				found = true;
				break;
			}
		}

		if (!found) {
			entry = kmalloc(sizeof(*entry), GFP_KERNEL);
			if (!entry) {
				en_err(priv, "Failed adding MAC %pM on port:%d (out of memory)\n",
				       ha->addr, priv->port);
				priv->flags |= MLX4_EN_FLAG_FORCE_PROMISC;
				break;
			}
			mac = mlx4_mac_to_u64(ha->addr);
			memcpy(entry->mac, ha->addr, ETH_ALEN);
			err = mlx4_register_mac(mdev->dev, priv->port, mac);
			if (err < 0) {
				en_err(priv, "Failed registering MAC %pM on port %d: %d\n",
				       ha->addr, priv->port, err);
				kfree(entry);
				priv->flags |= MLX4_EN_FLAG_FORCE_PROMISC;
				break;
			}
			err = mlx4_en_uc_steer_add(priv, ha->addr,
						   &priv->base_qpn,
						   &entry->reg_id,
						   MLX4_EN_NO_VLAN);
			if (err) {
				en_err(priv, "Failed adding MAC %pM on port %d: %d\n",
				       ha->addr, priv->port, err);
				mlx4_unregister_mac(mdev->dev, priv->port, mac);
				kfree(entry);
				priv->flags |= MLX4_EN_FLAG_FORCE_PROMISC;
				break;
			} else {
				unsigned int mac_hash;
				en_dbg(DRV, priv, "Added MAC %pM on port:%d\n",
				       ha->addr, priv->port);
				mac_hash = ha->addr[MLX4_EN_MAC_HASH_IDX];
				bucket = &priv->mac_hash[mac_hash];
				hlist_add_head_rcu(&entry->hlist, bucket);
			}
		}
	}

	if (priv->flags & MLX4_EN_FLAG_FORCE_PROMISC) {
		en_warn(priv, "Forcing promiscuous mode on port:%d\n",
			priv->port);
	} else if (prev_flags & MLX4_EN_FLAG_FORCE_PROMISC) {
		en_warn(priv, "Stop forcing promiscuous mode on port:%d\n",
			priv->port);
	}
}

static void mlx4_en_do_set_rx_mode(struct work_struct *work)
{
	struct mlx4_en_priv *priv = container_of(work, struct mlx4_en_priv,
						 rx_mode_task);
	struct mlx4_en_dev *mdev = priv->mdev;
	struct net_device *dev = priv->dev;

	mutex_lock(&mdev->state_lock);
	if (!mdev->device_up) {
		en_dbg(HW, priv, "Card is not up, ignoring rx mode change.\n");
		goto out;
	}
	if (!priv->port_up) {
		en_dbg(HW, priv, "Port is down, ignoring rx mode change.\n");
		goto out;
	}

	if (!netif_carrier_ok(dev)) {
		if (!mlx4_en_QUERY_PORT(mdev, priv->port)) {
			if (priv->port_state.link_state) {
				priv->last_link_state = MLX4_DEV_EVENT_PORT_UP;
				netif_carrier_on(dev);
				en_dbg(LINK, priv, "Link Up\n");
			}
		}
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0))
	if (dev->priv_flags & IFF_UNICAST_FLT)
#else
	if (mdev->dev->caps.steering_mode != MLX4_STEERING_MODE_A0)
#endif
		mlx4_en_do_uc_filter(priv, dev, mdev);

	/* Promsicuous mode: disable all filters */
	if ((dev->flags & IFF_PROMISC) ||
	    (priv->flags & MLX4_EN_FLAG_FORCE_PROMISC)) {
		mlx4_en_set_promisc_mode(priv, mdev);
		goto out;
	}

	/* Not in promiscuous mode */
	if (priv->flags & MLX4_EN_FLAG_PROMISC)
		mlx4_en_clear_promisc_mode(priv, mdev);

	mlx4_en_do_multicast(priv, dev, mdev);
out:
	mutex_unlock(&mdev->state_lock);
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static void mlx4_en_netpoll(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_cq *cq;
	unsigned long flags;
	int i;

	for (i = 0; i < priv->rx_ring_num; i++) {
		cq = priv->rx_cq[i];
		spin_lock_irqsave(&cq->lock, flags);
		napi_synchronize(&cq->napi);
		mlx4_en_process_rx_cq(dev, cq, 0);
		spin_unlock_irqrestore(&cq->lock, flags);
	}
}
#endif

static void mlx4_en_tx_timeout(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int i;

	if (netif_msg_timer(priv))
		en_warn(priv, "Tx timeout called on port:%d\n", priv->port);

	for (i = 0; i < priv->tx_ring_num; i++) {
		if (!netif_tx_queue_stopped(netdev_get_tx_queue(dev, i)))
			continue;
		en_info(priv, "TX timeout detected on queue: %d,\n"
			"QP: 0x%x, CQ: 0x%x,\n"
			"Cons index: 0x%x, Prod index: 0x%x\n", i,
			priv->tx_ring[i]->qpn, priv->tx_ring[i]->cqn,
			priv->tx_ring[i]->cons, priv->tx_ring[i]->prod);
	}

	priv->port_stats.tx_timeout++;
	en_dbg(DRV, priv, "Scheduling watchdog\n");
	queue_work(mdev->workqueue, &priv->watchdog_task);
}


static struct net_device_stats *mlx4_en_get_stats(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);

	spin_lock_bh(&priv->stats_lock);
	memcpy(&priv->ret_stats, &priv->stats, sizeof(priv->stats));
	spin_unlock_bh(&priv->stats_lock);

	return &priv->ret_stats;
}

static void mlx4_en_set_default_moderation(struct mlx4_en_priv *priv)
{
	struct mlx4_en_cq *cq;
	int i;

	/* If we haven't received a specific coalescing setting
	 * (module param), we set the moderation parameters as follows:
	 * - moder_cnt is set to the number of mtu sized packets to
	 *   satisfy our coelsing target.
	 * - moder_time is set to a fixed value.
	 */
	priv->rx_frames = MLX4_EN_RX_COAL_TARGET / priv->dev->mtu + 1;
	priv->rx_usecs = MLX4_EN_RX_COAL_TIME;
	priv->tx_frames = MLX4_EN_TX_COAL_PKTS;
	priv->tx_usecs = MLX4_EN_TX_COAL_TIME;
	en_dbg(INTR, priv, "Default coalesing params for mtu:%d - rx_frames:%d rx_usecs:%d\n",
	       priv->dev->mtu, priv->rx_frames, priv->rx_usecs);

	/* Setup cq moderation params */
	for (i = 0; i < priv->rx_ring_num; i++) {
		cq = priv->rx_cq[i];
		cq->moder_cnt = priv->rx_frames;
		cq->moder_time = priv->rx_usecs;
		priv->last_moder_time[i] = MLX4_EN_AUTO_CONF;
		priv->last_moder_packets[i] = 0;
		priv->last_moder_bytes[i] = 0;
	}

	for (i = 0; i < priv->tx_ring_num; i++) {
		cq = priv->tx_cq[i];
		cq->moder_cnt = priv->tx_frames;
		cq->moder_time = priv->tx_usecs;
	}

	/* Reset auto-moderation params */
	priv->pkt_rate_low = MLX4_EN_RX_RATE_LOW;
	priv->rx_usecs_low = MLX4_EN_RX_COAL_TIME_LOW;
	priv->pkt_rate_high = MLX4_EN_RX_RATE_HIGH;
	priv->rx_usecs_high = MLX4_EN_RX_COAL_TIME_HIGH;
	priv->sample_interval = MLX4_EN_SAMPLE_INTERVAL;
	priv->adaptive_rx_coal = 1;
	priv->last_moder_jiffies = 0;
	priv->last_cstate_jiffies = 0;
	priv->last_moder_tx_packets = 0;
}

static void mlx4_en_auto_moderation(struct mlx4_en_priv *priv)
{
	unsigned long period = (unsigned long) (jiffies - priv->last_moder_jiffies);
	struct mlx4_en_cq *cq;
	unsigned long packets;
	unsigned long rate;
	unsigned long avg_pkt_size;
	unsigned long rx_packets;
	unsigned long rx_bytes;
	unsigned long rx_pkt_diff;
	int moder_time;
	int ring, err;

	if (!priv->adaptive_rx_coal || period < priv->sample_interval * HZ)
		return;

	for (ring = 0; ring < priv->rx_ring_num; ring++) {
		spin_lock_bh(&priv->stats_lock);
		rx_packets = priv->rx_ring[ring]->packets;
		rx_bytes = priv->rx_ring[ring]->bytes;
		spin_unlock_bh(&priv->stats_lock);

		rx_pkt_diff = ((unsigned long) (rx_packets -
				priv->last_moder_packets[ring]));
		packets = rx_pkt_diff;
		rate = packets * HZ / period;
		avg_pkt_size = packets ? ((unsigned long) (rx_bytes -
				priv->last_moder_bytes[ring])) / packets : 0;

		/* Apply auto-moderation only when packet rate
		 * exceeds a rate that it matters */
		if (rate > (MLX4_EN_RX_RATE_THRESH / priv->rx_ring_num) &&
		    avg_pkt_size > MLX4_EN_AVG_PKT_SMALL) {
			if (avg_pkt_size > priv->dev->mtu -
					sizeof(struct iphdr) -
					sizeof(struct udphdr))
				moder_time = priv->rx_usecs_high;
			else if (rate < priv->pkt_rate_low)
				moder_time = priv->rx_usecs_low;
			else if (rate > priv->pkt_rate_high)
				moder_time = priv->rx_usecs_high;
			else
				moder_time = (rate - priv->pkt_rate_low) *
					(priv->rx_usecs_high - priv->rx_usecs_low) /
					(priv->pkt_rate_high - priv->pkt_rate_low) +
					priv->rx_usecs_low;
		} else {
			moder_time = priv->rx_usecs_low;
		}

		if (moder_time != priv->last_moder_time[ring]) {
			priv->last_moder_time[ring] = moder_time;
			cq = priv->rx_cq[ring];
			cq->moder_time = moder_time;
			err = mlx4_en_set_cq_moder(priv, cq);
			if (err)
				en_err(priv, "Failed modifying moderation for cq:%d\n",
				       ring);
		}
		priv->last_moder_packets[ring] = rx_packets;
		priv->last_moder_bytes[ring] = rx_bytes;
	}

	priv->last_moder_jiffies = jiffies;
}

static void mlx4_en_default_cstate(struct mlx4_en_priv *priv)
{
	unsigned long period = (unsigned long)
				(jiffies - priv->last_cstate_jiffies);
	unsigned long packets = 0;
	unsigned long global_rate = 0;
	int ring;

	if (!(priv->pflags & MLX4_EN_PRIV_FLAGS_PM_QOS) ||
	    period < priv->sample_interval * HZ)
		return;

	for (ring = 0; ring < priv->rx_ring_num; ring++)
		packets += priv->rx_ring[ring]->packets;

	global_rate = (packets - priv->last_packets) * HZ / period;
	priv->last_packets = packets;

	if ((global_rate < MLX4_EN_RX_RATE_THRESH) &&
	    (priv->last_cpu_dma_latency == 0)) {
		en_dbg(DRV, priv, "PM Qos update to default\n");
		pm_qos_update_request(&priv->pm_qos_req,
				      PM_QOS_DEFAULT_VALUE);
		priv->last_cpu_dma_latency = PM_QOS_DEFAULT_VALUE;
	}

	if ((global_rate > MLX4_EN_RX_RATE_THRESH) &&
	    (priv->last_cpu_dma_latency != 0)) {
		en_dbg(DRV, priv, "PM Qos update to low latency\n");
		pm_qos_update_request(&priv->pm_qos_req, 0);
		priv->last_cpu_dma_latency = 0;
	}

	priv->last_cstate_jiffies = jiffies;
}

static void mlx4_en_do_get_stats(struct work_struct *work)
{
	struct delayed_work *delay = to_delayed_work(work);
	struct mlx4_en_priv *priv = container_of(delay, struct mlx4_en_priv,
						 stats_task);
	struct mlx4_en_dev *mdev = priv->mdev;
	int err;

	mutex_lock(&mdev->state_lock);
	if (mdev->device_up) {
		if (priv->port_up) {
			if (mlx4_is_slave(mdev->dev))
				err = mlx4_en_get_vport_stats(mdev, priv->port);
			else
				err = mlx4_en_DUMP_ETH_STATS(mdev, priv->port, 0);
			if (err)
				en_dbg(HW, priv, "Could not update stats\n");

			mlx4_en_auto_moderation(priv);
			mlx4_en_default_cstate(priv);
		}

		queue_delayed_work(mdev->workqueue, &priv->stats_task, STATS_DELAY);
	}
	if (mdev->mac_removed[MLX4_MAX_PORTS + 1 - priv->port]) {
		mlx4_en_do_set_mac(priv, priv->current_mac);
		mdev->mac_removed[MLX4_MAX_PORTS + 1 - priv->port] = 0;
	}
	mutex_unlock(&mdev->state_lock);
}

/* mlx4_en_service_task - Run service task for tasks that needed to be done
 * periodically
 */
static void mlx4_en_service_task(struct work_struct *work)
{
	struct delayed_work *delay = to_delayed_work(work);
	struct mlx4_en_priv *priv = container_of(delay, struct mlx4_en_priv,
						 service_task);
	struct mlx4_en_dev *mdev = priv->mdev;

	mutex_lock(&mdev->state_lock);
	if (mdev->device_up) {
		if (mdev->dev->caps.flags2 & MLX4_DEV_CAP_FLAG2_TS)
			mlx4_en_ptp_overflow_check(mdev);

		queue_delayed_work(mdev->workqueue, &priv->service_task,
				   SERVICE_TASK_DELAY);
	}
	mutex_unlock(&mdev->state_lock);
}

static void mlx4_en_linkstate(struct work_struct *work)
{
	struct mlx4_en_priv *priv = container_of(work, struct mlx4_en_priv,
						 linkstate_task);
	struct mlx4_en_dev *mdev = priv->mdev;
	int linkstate = priv->link_state;

	mutex_lock(&mdev->state_lock);
	/* If observable port state changed set carrier state and
	 * report to system log */
	if (priv->last_link_state != linkstate) {
		if (linkstate == MLX4_DEV_EVENT_PORT_DOWN) {
			en_info(priv, "Link Down\n");
			netif_carrier_off(priv->dev);
		} else {
			en_info(priv, "Link Up\n");
			netif_carrier_on(priv->dev);
		}
	}
	priv->last_link_state = linkstate;
	mutex_unlock(&mdev->state_lock);
}

static int mlx4_en_restore_qos(struct net_device *dev)
{
	int err = 0;
#ifndef CONFIG_COMPAT_DISABLE_DCB
#ifdef CONFIG_MLX4_EN_DCB
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int res;
	struct ieee_ets ets;
	struct ieee_maxrate maxrate;

	if (mdev->dev->caps.flags2 & MLX4_DEV_CAP_FLAG2_ETS_CFG) {
		err |= res = mlx4_en_dcbnl_ieee_getets(dev, &ets);
		if (!res && mlx4_en_dcbnl_ieee_setets(dev, &ets))
			mlx4_warn(mdev->dev, "Failed to restore ets configuration\n");
	}
	err |= res = mlx4_en_restorepfc(dev);
	if (res)
		mlx4_warn(mdev->dev, "Failed to restore pfc configuration\n");
	if (mdev->dev->caps.flags2 & MLX4_DEV_CAP_FLAG2_ETS_CFG) {
		err |= res = mlx4_en_dcbnl_ieee_getmaxrate(dev, &maxrate);
		if (!res && mlx4_en_dcbnl_ieee_setmaxrate(dev, &maxrate))
			mlx4_warn(mdev->dev, "Failed to restore max rate configuration\n");
	}
#endif
#endif
	return err;
}

static void mlx4_en_reset_affinity_masks(struct mlx4_en_priv *priv)
{
	priv->current_mask = cpumask_empty(&priv->numa_mask) ?
				cpu_online_mask : &priv->numa_mask;
	priv->current_cpu = cpumask_first(priv->current_mask);
}

static void mlx4_en_set_affinity_masks(struct mlx4_en_priv *priv)
{
	if (priv->mdev->dev->numa_node == -1)
		goto out;

	priv->current_mask = cpumask_of_node(priv->mdev->dev->numa_node);
	if (!priv->current_mask)
		goto out;

	if (!cpumask_and(&priv->numa_mask, cpu_online_mask,
			 priv->current_mask))
		goto out;

	cpumask_xor(&priv->non_numa_mask, cpu_online_mask, priv->current_mask);
	mlx4_en_reset_affinity_masks(priv);
	return;

out:
	en_warn(priv, "Failed to find online cores for numa, using all online CPUs\n");
	priv->current_mask = cpu_online_mask;
	cpumask_clear(&priv->numa_mask);
	cpumask_clear(&priv->non_numa_mask);
	mlx4_en_reset_affinity_masks(priv);
	return;
}

int mlx4_en_start_port(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_en_cq *cq;
	struct mlx4_en_tx_ring *tx_ring;
	int rx_index = 0;
	int tx_index = 0;
	int err = 0;
	int i;
	int j;
	u8 mc_list[16] = {0};

	if (priv->port_up) {
		en_dbg(DRV, priv, "start port called while port already up\n");
		return 0;
	}

	INIT_LIST_HEAD(&priv->mc_list);
	INIT_LIST_HEAD(&priv->curr_list);
	INIT_LIST_HEAD(&priv->ethtool_list);
	memset(&priv->ethtool_rules[0], 0,
	       sizeof(struct ethtool_flow_id) * MAX_NUM_OF_FS_RULES);

	/* Calculate Rx buf size */
	dev->mtu = min(dev->mtu, priv->max_mtu);
	mlx4_en_calc_rx_buf(dev);
	en_dbg(DRV, priv, "Rx buf size:%d\n", priv->rx_skb_size);

	/* Configure rx cq's and rings */
	err = mlx4_en_activate_rx_rings(priv);
	if (err) {
		en_err(priv, "Failed to activate RX rings\n");
		return err;
	}

	for (i = 0; i < priv->rx_ring_num; i++) {
		cq = priv->rx_cq[i];

		mlx4_en_cq_init_lock(cq);

		err = mlx4_en_activate_cq(priv, cq, i);
		if (err) {
			en_err(priv, "Failed activating Rx CQ\n");
			goto cq_err;
		}
		for (j = 0; j < cq->size; j++)
			cq->buf[j].owner_sr_opcode = MLX4_CQE_OWNER_MASK;

		cq->moder_cnt = priv->rx_frames;
		cq->moder_time = priv->rx_usecs;
		priv->last_moder_time[i] = MLX4_EN_AUTO_CONF;
		err = mlx4_en_set_cq_moder(priv, cq);
		if (err) {
			en_err(priv, "Failed setting cq moderation parameters");
			mlx4_en_deactivate_cq(priv, cq);
			goto cq_err;
		}
		mlx4_en_arm_cq(priv, cq);
		priv->rx_ring[i]->cqn = cq->mcq.cqn;
		++rx_index;
	}

	/* Set qp number */
	en_dbg(DRV, priv, "Getting qp number for port %d\n", priv->port);
	memcpy(priv->current_mac, dev->dev_addr, sizeof(priv->current_mac));
	err = mlx4_en_get_qp(priv);
	if (err) {
		en_err(priv, "Failed getting eth qp\n");
		goto cq_err;
	}
	mdev->mac_removed[priv->port] = 0;

	/* gets default allocated counter index from func cap */
	/* or sink counter index if no resources */
	priv->counter_index = mdev->dev->caps.def_counter_index[priv->port - 1];

	en_dbg(DRV, priv, "%s: default counter index %d for port %d\n",
	       __func__, priv->counter_index, priv->port);

	err = mlx4_en_config_rss_steer(priv);
	if (err) {
		en_err(priv, "Failed configuring rss steering\n");
		goto mac_err;
	}

	if (mdev->dev->caps.steering_mode != MLX4_STEERING_MODE_A0) {
		err = mlx4_en_set_rss_steer_rules(priv);
		if (err) {
			en_err(priv, "Failed setting steering rules\n");
			goto rss_err;
		}
	}

	err = mlx4_en_create_drop_qp(priv);
	if (err)
		goto steer_err;

	/* Configure tx cq's and rings */
	for (i = 0; i < priv->tx_ring_num; i++) {
		/* Configure cq */
		cq = priv->tx_cq[i];
		err = mlx4_en_activate_cq(priv, cq, i);
		if (err) {
			en_err(priv, "Failed allocating Tx CQ\n");
			goto tx_err;
		}

		cq->moder_cnt = priv->tx_frames;
		cq->moder_time = priv->tx_usecs;
		err = mlx4_en_set_cq_moder(priv, cq);
		if (err) {
			en_err(priv, "Failed setting cq moderation parameters");
			mlx4_en_deactivate_cq(priv, cq);
			goto tx_err;
		}
		en_dbg(DRV, priv, "Resetting index of collapsed CQ:%d to -1\n", i);
		cq->buf->wqe_index = cpu_to_be16(0xffff);

		/* Configure ring */
		tx_ring = priv->tx_ring[i];

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)) || defined (CONFIG_COMPAT_NEW_TX_RING_SCHEME)
		err = mlx4_en_activate_tx_ring(priv, tx_ring, cq->mcq.cqn,
					       i / priv->num_tx_rings_p_up,
					       MLX4_EN_NO_VLAN);
#else
		err = mlx4_en_activate_tx_ring(priv, tx_ring, cq->mcq.cqn,
					       MLX4_EN_NO_VLAN);
#endif
		if (err) {
			en_err(priv, "Failed allocating Tx ring\n");
			mlx4_en_deactivate_cq(priv, cq);
			goto tx_err;
		}
		tx_ring->tx_queue = netdev_get_tx_queue(dev, i);

		/* Arm CQ for TX completions */
		mlx4_en_arm_cq(priv, cq);

		/* Set initial ownership of all Tx TXBBs to SW (1) */
		for (j = 0; j < tx_ring->buf_size; j += STAMP_STRIDE)
			*((u32 *) (tx_ring->buf + j)) = 0xffffffff;
		++tx_index;
	}

	/* Configure port */
	err = mlx4_SET_PORT_general(mdev->dev, priv->port,
				    priv->rx_skb_size,
				    priv->prof->tx_pause,
				    priv->prof->tx_ppp,
				    priv->prof->rx_pause,
				    priv->prof->rx_ppp);
	if (err) {
		en_err(priv, "Failed setting port general configurations for port %d, with error %d\n",
		       priv->port, err);
		goto tx_err;
	}
	/* Set default qp number */
	err = mlx4_SET_PORT_qpn_calc(mdev->dev, priv->port, priv->base_qpn, 0);
	if (err) {
		en_err(priv, "Failed setting default qp numbers\n");
		goto tx_err;
	}

#ifdef CONFIG_COMPAT_VXLAN_ENABLED
	if (mdev->dev->caps.tunnel_offload_mode == MLX4_TUNNEL_OFFLOAD_MODE_VXLAN) {
		err = mlx4_SET_PORT_VXLAN(mdev->dev, priv->port,
					  VXLAN_STEER_BY_OUTER_MAC,
					  VXLAN_ENABLE);
		if (err) {
			en_err(priv, "Failed setting port L2 tunnel configuration, err %d\n",
			       err);
			goto tx_err;
		}
	}
#endif

	/* Init port */
	en_dbg(HW, priv, "Initializing port\n");
	err = mlx4_INIT_PORT(mdev->dev, priv->port);
	if (err) {
		en_err(priv, "Failed Initializing port\n");
		goto tx_err;
	}

	/* Attach rx QP to broadcast address */
	memset(&mc_list[10], 0xff, ETH_ALEN);
	mc_list[5] = priv->port; /* needed for B0 steering support */
	if (mlx4_multicast_attach(mdev->dev, &priv->rss_map.indir_qp, mc_list,
				  priv->port, 0, MLX4_PROT_ETH,
				  &priv->broadcast_id))
		mlx4_warn(mdev, "Failed Attaching Broadcast\n");

	/* Must redo promiscuous mode setup. */
	priv->flags &= ~(MLX4_EN_FLAG_PROMISC | MLX4_EN_FLAG_MC_PROMISC);

	/* Schedule multicast task to populate multicast list */
	queue_work(mdev->workqueue, &priv->rx_mode_task);

#ifdef CONFIG_COMPAT_VXLAN_ENABLED
#ifdef CONFIG_COMPAT_VXLAN_DYNAMIC_PORT
	if (priv->mdev->dev->caps.flags2 & MLX4_DEV_CAP_FLAG2_VXLAN_OFFLOADS)
		vxlan_get_rx_port(dev);
#endif
#endif
	priv->port_up = true;

	/* Process all completions if exist to prevent
	 * the queues freezing if they are full
	 */
	local_bh_disable();
	for (i = 0; i < priv->rx_ring_num; i++)
		napi_schedule(&priv->rx_cq[i]->napi);
	local_bh_enable();

	netif_tx_start_all_queues(dev);
	if (mlx4_en_restore_qos(dev))
		mlx4_warn(mdev, "Couldn't restore QOS settings\n");
#ifdef CONFIG_DEBUG_FS
	mlx4_en_create_debug_files(priv);
#endif
	return 0;

tx_err:
	while (tx_index--) {
		mlx4_en_deactivate_tx_ring(priv, priv->tx_ring[tx_index]);
		mlx4_en_deactivate_cq(priv, priv->tx_cq[tx_index]);
	}
	mlx4_en_destroy_drop_qp(priv);
steer_err:
	mlx4_en_delete_rss_steer_rules(priv);
rss_err:
	mlx4_en_release_rss_steer(priv);
mac_err:
	mlx4_en_put_qp(priv);
cq_err:
	while (rx_index--)
		mlx4_en_deactivate_cq(priv, priv->rx_cq[rx_index]);
	for (i = 0; i < priv->rx_ring_num; i++)
		mlx4_en_deactivate_rx_ring(priv, priv->rx_ring[i]);

	return err; /* need to close devices */
}


void mlx4_en_stop_port(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_en_mc_list *mclist, *tmp;
	int i;
	u8 mc_list[16] = {0};

	if (!priv->port_up) {
		en_dbg(DRV, priv, "stop port called while port already down\n");
		return;
	}

#ifdef CONFIG_DEBUG_FS
	mlx4_en_delete_debug_files(priv);
#endif

	/* close port*/
	mlx4_CLOSE_PORT(mdev->dev, priv->port);

	/* Synchronize with tx routine */
	netif_tx_lock_bh(dev);
	netif_carrier_off(dev);
	netif_tx_stop_all_queues(dev);
	netif_tx_unlock_bh(dev);

	/* Set port as not active */
	priv->port_up = false;

	/* Promsicuous mode */
	if (mdev->dev->caps.steering_mode ==
	    MLX4_STEERING_MODE_DEVICE_MANAGED) {
		priv->flags &= ~(MLX4_EN_FLAG_PROMISC |
				 MLX4_EN_FLAG_MC_PROMISC);
		mlx4_flow_steer_promisc_remove(mdev->dev,
					       priv->port,
					       MLX4_FS_ALL_DEFAULT);
		mlx4_flow_steer_promisc_remove(mdev->dev,
					       priv->port,
					       MLX4_FS_MC_DEFAULT);
	} else if (priv->flags & MLX4_EN_FLAG_PROMISC) {
		priv->flags &= ~MLX4_EN_FLAG_PROMISC;

		/* Disable promiscouos mode */
		mlx4_unicast_promisc_remove(mdev->dev, priv->base_qpn,
					    priv->port);

		/* Disable Multicast promisc */
		if (priv->flags & MLX4_EN_FLAG_MC_PROMISC) {
			mlx4_multicast_promisc_remove(mdev->dev, priv->base_qpn,
						      priv->port);
			priv->flags &= ~MLX4_EN_FLAG_MC_PROMISC;
		}
	}

	/* Detach All multicasts */
	memset(&mc_list[10], 0xff, ETH_ALEN);
	mc_list[5] = priv->port; /* needed for B0 steering support */
	mlx4_multicast_detach(mdev->dev, &priv->rss_map.indir_qp, mc_list,
			      MLX4_PROT_ETH, priv->broadcast_id);
	list_for_each_entry(mclist, &priv->curr_list, list) {
		memcpy(&mc_list[10], mclist->addr, ETH_ALEN);
		mc_list[5] = priv->port;
		mlx4_multicast_detach(mdev->dev, &priv->rss_map.indir_qp,
				      mc_list, MLX4_PROT_ETH, mclist->reg_id);
#ifdef CONFIG_COMPAT_VXLAN_ENABLED
		if (mclist->tunnel_reg_id)
			mlx4_flow_detach(mdev->dev, mclist->tunnel_reg_id);
#endif
	}
	mlx4_en_clear_list(dev);
	list_for_each_entry_safe(mclist, tmp, &priv->curr_list, list) {
		list_del(&mclist->list);
		kfree(mclist);
	}

	/* Flush multicast filter */
	mlx4_SET_MCAST_FLTR(mdev->dev, priv->port, 0, 1, MLX4_MCAST_CONFIG);

	/* Free TX Rings */
	for (i = 0; i < priv->tx_ring_num; i++) {
		mlx4_en_deactivate_tx_ring(priv, priv->tx_ring[i]);
		mlx4_en_deactivate_cq(priv, priv->tx_cq[i]);
	}
	msleep(10);

	for (i = 0; i < priv->tx_ring_num; i++)
		mlx4_en_free_tx_buf(dev, priv->tx_ring[i]);

	mlx4_en_destroy_drop_qp(priv);

	/* Delete flow rules added from ethtool */
	mlx4_en_remove_ethtool_rules(priv);

	/* Delete flow rules if exists */
	if (mdev->dev->caps.steering_mode != MLX4_STEERING_MODE_A0)
		mlx4_en_delete_rss_steer_rules(priv);

#ifdef CONFIG_RFS_ACCEL
	/* Free RFS rules */
	mlx4_en_cleanup_filters(priv);
#endif

	/* Free RSS qps */
	mlx4_en_release_rss_steer(priv);

	/* Unregister Mac address for the port */
	mlx4_en_put_qp(priv);
	if (!(mdev->dev->caps.flags2 & MLX4_DEV_CAP_FLAG2_REASSIGN_MAC_EN))
		mdev->mac_removed[priv->port] = 1;

	/* Free RX Rings */
	for (i = 0; i < priv->rx_ring_num; i++) {
		struct mlx4_en_cq *cq = priv->rx_cq[i];

		local_bh_disable();
		while (!mlx4_en_cq_lock_napi(cq)) {
			pr_info("CQ %d locked\n", i);
			mdelay(1);
		}
		local_bh_enable();

		while (test_bit(NAPI_STATE_SCHED, &cq->napi.state))
			msleep(1);
		mlx4_en_deactivate_rx_ring(priv, priv->rx_ring[i]);
		mlx4_en_deactivate_cq(priv, cq);
	}
}

static void mlx4_en_restart(struct work_struct *work)
{
	struct mlx4_en_priv *priv = container_of(work, struct mlx4_en_priv,
						 watchdog_task);
	struct mlx4_en_dev *mdev = priv->mdev;
	struct net_device *dev = priv->dev;
	int i;

	en_dbg(DRV, priv, "Watchdog task called for port %d\n", priv->port);

	mutex_lock(&mdev->state_lock);
	if (priv->port_up) {
		mlx4_en_stop_port(dev);
		for (i = 0; i < priv->tx_ring_num; i++)
			netdev_tx_reset_queue(priv->tx_ring[i]->tx_queue);
		if (mlx4_en_start_port(dev))
			en_err(priv, "Failed restarting port %d\n", priv->port);
	}
	mutex_unlock(&mdev->state_lock);
}

static void mlx4_en_clear_stats(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int i;

	if (!mlx4_is_slave(mdev->dev))
		if (mlx4_en_DUMP_ETH_STATS(mdev, priv->port, 1))
			en_dbg(HW, priv, "Failed dumping statistics\n");

	memset(&priv->stats, 0, sizeof(priv->stats));
	memset(&priv->pstats, 0, sizeof(priv->pstats));
	memset(&priv->pkstats, 0, sizeof(priv->pkstats));
	memset(&priv->port_stats, 0, sizeof(priv->port_stats));
	memset(&priv->vport_stats, 0, sizeof(priv->vport_stats));
	memset(&priv->vf_stats, 0, sizeof(priv->vf_stats));

	for (i = 0; i < priv->tx_ring_num; i++) {
		priv->tx_ring[i]->bytes = 0;
		priv->tx_ring[i]->packets = 0;
		priv->tx_ring[i]->tso_packets = 0;
		priv->tx_ring[i]->tx_csum = 0;
	}
	for (i = 0; i < priv->rx_ring_num; i++) {
		priv->rx_ring[i]->bytes = 0;
		priv->rx_ring[i]->packets = 0;
		priv->rx_ring[i]->csum_ok = 0;
		priv->rx_ring[i]->csum_none = 0;
		priv->rx_ring[i]->no_reuse_cnt = 0;
	}
}

static int mlx4_en_open(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int err = 0;

	mutex_lock(&mdev->state_lock);

	if (!mdev->device_up) {
		en_err(priv, "Cannot open - device down/disabled\n");
		err = -EBUSY;
		goto out;
	}

	/* Reset HW statistics and SW counters */
	mlx4_en_clear_stats(dev);

	err = mlx4_en_start_port(dev);
	if (err)
		en_err(priv, "Failed starting port:%d\n", priv->port);

out:
	mutex_unlock(&mdev->state_lock);
	return err;
}


static int mlx4_en_close(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;

	en_dbg(IFDOWN, priv, "Close port called\n");

	mutex_lock(&mdev->state_lock);

	mlx4_en_stop_port(dev);
	netif_carrier_off(dev);

	mutex_unlock(&mdev->state_lock);
	return 0;
}

void mlx4_en_free_resources(struct mlx4_en_priv *priv)
{
	int i;

#ifdef CONFIG_RFS_ACCEL
#ifdef CONFIG_COMPAT_IS_NETDEV_EXTENDED
	free_irq_cpu_rmap(mlx4_en_rx_cpu_rmap(priv));
	mlx4_en_rx_cpu_rmap(priv) = NULL;
#else
	if (priv->dev->rx_cpu_rmap) {
		free_irq_cpu_rmap(priv->dev->rx_cpu_rmap);
		priv->dev->rx_cpu_rmap = NULL;
	}
#endif
#endif

	for (i = 0; i < priv->tx_ring_num; i++) {
		if (priv->tx_ring && priv->tx_ring[i])
			mlx4_en_destroy_tx_ring(priv, &priv->tx_ring[i]);
		if (priv->tx_cq && priv->tx_cq[i])
			mlx4_en_destroy_cq(priv, &priv->tx_cq[i]);
	}

	for (i = 0; i < priv->rx_ring_num; i++) {
		if (priv->rx_ring[i])
			mlx4_en_destroy_rx_ring(priv, &priv->rx_ring[i],
				priv->prof->rx_ring_size, priv->stride);
		if (priv->rx_cq[i])
			mlx4_en_destroy_cq(priv, &priv->rx_cq[i]);
	}
}

int mlx4_en_alloc_resources(struct mlx4_en_priv *priv)
{
	struct mlx4_en_port_profile *prof = priv->prof;
	int i;
	int node;

	/* Create rx Rings */
	mlx4_en_reset_affinity_masks(priv);
	for (i = 0; i < priv->rx_ring_num; i++) {
		node = cpu_to_node(priv->current_cpu);
		if (mlx4_en_create_cq(priv, &priv->rx_cq[i],
				      prof->rx_ring_size, i, RX, node))
			goto err;
		mlx4_en_set_cq_affinity(priv, priv->rx_cq[i]);

		if (mlx4_en_create_rx_ring(priv, &priv->rx_ring[i],
					   prof->rx_ring_size,
					   node))
			goto err;
	}

	/* Create tx Rings */
	mlx4_en_reset_affinity_masks(priv);
	for (i = 0; i < priv->tx_ring_num; i++) {
		node = cpu_to_node(priv->current_cpu);
		if (mlx4_en_create_cq(priv, &priv->tx_cq[i],
				      prof->tx_ring_size, i, TX, node))
			goto err;
		mlx4_en_set_cq_affinity(priv, priv->tx_cq[i]);

		if (mlx4_en_create_tx_ring(priv, &priv->tx_ring[i],
					   prof->tx_ring_size, TXBB_SIZE,
					   node))
			goto err;
	}

#ifdef CONFIG_RFS_ACCEL
#ifdef CONFIG_COMPAT_IS_NETDEV_EXTENDED
	mlx4_en_rx_cpu_rmap(priv) = alloc_irq_cpu_rmap(priv->rx_ring_num);
	if (!mlx4_en_rx_cpu_rmap(priv))
		goto err;
#else
	priv->dev->rx_cpu_rmap = alloc_irq_cpu_rmap(priv->rx_ring_num);
	if (!priv->dev->rx_cpu_rmap)
		goto err;
#endif
#endif

	return 0;

err:
	en_err(priv, "Failed to allocate NIC resources\n");
	for (i = 0; i < priv->rx_ring_num; i++) {
		if (priv->rx_ring[i])
			mlx4_en_destroy_rx_ring(priv, &priv->rx_ring[i],
						prof->rx_ring_size,
						priv->stride);
		if (priv->rx_cq[i])
			mlx4_en_destroy_cq(priv, &priv->rx_cq[i]);
	}
	for (i = 0; i < priv->tx_ring_num; i++) {
		if (priv->tx_ring[i])
			mlx4_en_destroy_tx_ring(priv, &priv->tx_ring[i]);
		if (priv->tx_cq[i])
			mlx4_en_destroy_cq(priv, &priv->tx_cq[i]);
	}
	priv->port_up = false;
	return -ENOMEM;
}

static const char fmt_u64[] = "%llu\n";

struct en_stats_attribute {
	struct attribute attr;
	ssize_t (*show)(struct en_port *,
			struct en_stats_attribute *,
			char *buf);
	ssize_t (*store)(struct en_port *,
			 struct en_stats_attribute *,
			 char *buf,
			 size_t count);
};

struct en_port_attribute {
	struct attribute attr;
	ssize_t (*show)(struct en_port *,
			struct en_port_attribute *,
			char *buf);
	ssize_t (*store)(struct en_port *,
			 struct en_port_attribute *,
			 const char *buf,
			 size_t count);
};

#define EN_PORT_ATTR(_name, _mode, _show, _store) \
struct en_stats_attribute en_stats_attr_##_name = \
	__ATTR(_name, _mode, _show, _store)


/* Show a given an attribute in the statistics group */
static ssize_t mlx4_en_show_vf_statistics(struct en_port *en_p,
			    struct en_stats_attribute *attr, char *buf,
			    unsigned long offset)
{
	ssize_t ret = -EINVAL;
	struct net_device_stats link_stats;
	memset(&link_stats, 0xff, sizeof(struct net_device_stats));

	mlx4_get_vf_statistics(en_p->dev, en_p->port_num, en_p->vport_num, &link_stats);

	ret = sprintf(buf, fmt_u64, *(u64 *)(((u8 *)&link_stats) + offset));

	return ret;
}

/* generate a read-only statistics attribute */
#define VFSTAT_ENTRY(name)						\
static ssize_t name##_show(struct en_port *en_p,			\
			   struct en_stats_attribute *attr, char *buf)	\
{									\
	return mlx4_en_show_vf_statistics(en_p, attr, buf,		\
			    offsetof(struct net_device_stats, name));	\
}									\
static EN_PORT_ATTR(name, S_IRUGO, name##_show, NULL)

VFSTAT_ENTRY(rx_packets);
VFSTAT_ENTRY(tx_packets);
VFSTAT_ENTRY(rx_bytes);
VFSTAT_ENTRY(tx_bytes);
VFSTAT_ENTRY(rx_errors);
VFSTAT_ENTRY(tx_errors);
VFSTAT_ENTRY(rx_dropped);
VFSTAT_ENTRY(tx_dropped);
VFSTAT_ENTRY(multicast);
VFSTAT_ENTRY(collisions);
VFSTAT_ENTRY(rx_length_errors);
VFSTAT_ENTRY(rx_over_errors);
VFSTAT_ENTRY(rx_crc_errors);
VFSTAT_ENTRY(rx_frame_errors);
VFSTAT_ENTRY(rx_fifo_errors);
VFSTAT_ENTRY(rx_missed_errors);
VFSTAT_ENTRY(tx_aborted_errors);
VFSTAT_ENTRY(tx_carrier_errors);
VFSTAT_ENTRY(tx_fifo_errors);
VFSTAT_ENTRY(tx_heartbeat_errors);
VFSTAT_ENTRY(tx_window_errors);
VFSTAT_ENTRY(rx_compressed);
VFSTAT_ENTRY(tx_compressed);

static struct attribute *vfstat_attrs[] = {
	&en_stats_attr_rx_packets.attr,
	&en_stats_attr_tx_packets.attr,
	&en_stats_attr_rx_bytes.attr,
	&en_stats_attr_tx_bytes.attr,
	&en_stats_attr_rx_errors.attr,
	&en_stats_attr_tx_errors.attr,
	&en_stats_attr_rx_dropped.attr,
	&en_stats_attr_tx_dropped.attr,
	&en_stats_attr_multicast.attr,
	&en_stats_attr_collisions.attr,
	&en_stats_attr_rx_length_errors.attr,
	&en_stats_attr_rx_over_errors.attr,
	&en_stats_attr_rx_crc_errors.attr,
	&en_stats_attr_rx_frame_errors.attr,
	&en_stats_attr_rx_fifo_errors.attr,
	&en_stats_attr_rx_missed_errors.attr,
	&en_stats_attr_tx_aborted_errors.attr,
	&en_stats_attr_tx_carrier_errors.attr,
	&en_stats_attr_tx_fifo_errors.attr,
	&en_stats_attr_tx_heartbeat_errors.attr,
	&en_stats_attr_tx_window_errors.attr,
	&en_stats_attr_rx_compressed.attr,
	&en_stats_attr_tx_compressed.attr,
	NULL
};


static ssize_t en_stats_show(struct kobject *kobj,
				 struct attribute *attr, char *buf)
{
	struct en_stats_attribute *en_stats_attr =
		container_of(attr, struct en_stats_attribute, attr);
	struct en_port *p = container_of(kobj, struct en_port, kobj_stats);

	if (!en_stats_attr->show)
		return -EIO;

	return en_stats_attr->show(p, en_stats_attr, buf);
}

#ifdef CONFIG_COMPAT_SYSFS_OPS_CONST
static const struct sysfs_ops en_port_stats_sysfs_ops = {
#else
static struct sysfs_ops en_port_stats_sysfs_ops = {
#endif
	.show = en_stats_show
};

static struct kobj_type en_port_stats = {
	.sysfs_ops  = &en_port_stats_sysfs_ops,
	.default_attrs = vfstat_attrs,
};

static ssize_t mlx4_en_show_vf_link_state(struct en_port *en_p,
					  struct en_port_attribute *attr,
					  char *buf)
{
	const char *str[] = { "auto", "enable", "disable" };
	int link_state;
	ssize_t len = 0;

	link_state = mlx4_get_vf_link_state(en_p->dev, en_p->port_num,
					    en_p->vport_num);
	if (link_state >= 0)
		len += sprintf(&buf[len], "%s\n", str[link_state]);

	return len;
}

static ssize_t mlx4_en_store_vf_link_state(struct en_port *en_p,
					   struct en_port_attribute *attr,
					   const char *buf, size_t count)
{
	int err, link_state;

	if (count > 128)
		return -EINVAL;

	if (strstr(buf, "auto"))
		link_state = IFLA_VF_LINK_STATE_AUTO;
	else if (strstr(buf, "enable"))
		link_state = IFLA_VF_LINK_STATE_ENABLE;
	else if (strstr(buf, "disable"))
		link_state = IFLA_VF_LINK_STATE_DISABLE;
	else
		return -EINVAL;

	err = mlx4_set_vf_link_state(en_p->dev, en_p->port_num,
				     en_p->vport_num, link_state);
	return err ? err : count;
}

struct en_port_attribute en_port_attr_link_state = __ATTR(link_state,
						S_IRUGO | S_IWUSR,
						mlx4_en_show_vf_link_state,
						mlx4_en_store_vf_link_state);

static ssize_t en_port_show(struct kobject *kobj,
			    struct attribute *attr, char *buf)
{
	struct en_port_attribute *en_port_attr =
		container_of(attr, struct en_port_attribute, attr);
	struct en_port *p = container_of(kobj, struct en_port, kobj_vf);

	if (!en_port_attr->show)
		return -EIO;

	return en_port_attr->show(p, en_port_attr, buf);
}

static ssize_t en_port_store(struct kobject *kobj,
			     struct attribute *attr,
			     const char *buf, size_t count)
{
	struct en_port_attribute *en_port_attr =
		container_of(attr, struct en_port_attribute, attr);
	struct en_port *p = container_of(kobj, struct en_port, kobj_vf);

	if (!en_port_attr->store)
		return -EIO;

	return en_port_attr->store(p, en_port_attr, buf, count);
}

#ifdef CONFIG_COMPAT_SYSFS_OPS_CONST
static const struct sysfs_ops en_port_vf_ops = {
#else
static struct sysfs_ops en_port_vf_ops = {
#endif
	.show = en_port_show,
	.store = en_port_store,
};

static ssize_t mlx4_en_show_vlan_set(struct en_port *en_p,
				     struct en_port_attribute *attr,
				     char *buf)
{
	return mlx4_get_vf_vlan_set(en_p->dev, en_p->port_num,
				    en_p->vport_num, buf);
}

static ssize_t mlx4_en_store_vlan_set(struct en_port *en_p,
				      struct en_port_attribute *attr,
				      const char *buf, size_t count)
{
	int err;
	u16 vlan;
	struct mlx4_dev *dev = en_p->dev;
	int port = en_p->port_num;
	int vf = en_p->vport_num;
	char save;

	/* Max symbols per VLAN 4 * MAX_VLANS + (MAX_VLANS - 1) for spaces */
	if (count > MAX_VLANS * 4 + (MAX_VLANS - 1))
		return -EINVAL;

	err = mlx4_reset_vlan_policy(dev, port, vf);
	if (err)
		return err;

	do {
		int len;

		len = strcspn(buf, " ");

		/* nul-terminate and parse */
		save = buf[len];
		((char *)buf)[len] = '\0';

		if (sscanf(buf, "%hu", &vlan) != 1 ||
		    vlan > VLAN_MAX_VALUE) {
			if (!strcmp(buf, "\n"))
				err = 1;
			else
				err = -EINVAL;
			return err;
		}
		err = mlx4_set_vf_vlan_next(dev, port, vf, vlan);
		if (err) {
			mlx4_reset_vlan_policy(dev, port, vf);
			mlx4_warn(dev,
				  "Setting VLAN policy for VF %d failed\n",
				  vf);
			return err;
		}

		buf += len+1;
	} while (save == ' ');

	return count;
}

struct en_port_attribute en_port_attr_vlan_set = __ATTR(vlan_set,
						S_IRUGO | S_IWUSR,
						mlx4_en_show_vlan_set,
						mlx4_en_store_vlan_set);
static struct attribute *vf_attrs[] = {
	&en_port_attr_link_state.attr,
	&en_port_attr_vlan_set.attr,
	NULL
};

static struct kobj_type en_port_type = {
	.sysfs_ops  = &en_port_vf_ops,
	.default_attrs = vf_attrs,
};

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 5, 0))
static ssize_t mlx4_en_show_fdb(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct net_device *netdev = to_net_dev(dev);
	ssize_t len = 0;
	struct netdev_hw_addr *ha;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))
	struct netdev_hw_addr *mc;
#else
	struct dev_addr_list *mc;
#endif

	netif_addr_lock_bh(netdev);

	netdev_for_each_uc_addr(ha, netdev) {
		len += sprintf(&buf[len], "%02x:%02x:%02x:%02x:%02x:%02x\n",
				ha->addr[0], ha->addr[1], ha->addr[2],
				ha->addr[3], ha->addr[4], ha->addr[5]);
	}

	netdev_for_each_mc_addr(mc, netdev) {
		len += sprintf(&buf[len], "%02x:%02x:%02x:%02x:%02x:%02x\n",
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))
				mc->addr[0], mc->addr[1], mc->addr[2],
				mc->addr[3], mc->addr[4], mc->addr[5]);
#else
				mc->da_addr[0], mc->da_addr[1], mc->da_addr[2],
				mc->da_addr[3], mc->da_addr[4], mc->da_addr[5]);
#endif
	}

	netif_addr_unlock_bh(netdev);

	return len;
}

static ssize_t mlx4_en_set_fdb(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct net_device *netdev = to_net_dev(dev);
	struct mlx4_en_priv *priv = netdev_priv(netdev);
	unsigned char mac[ETH_ALEN];
	unsigned int tmp[ETH_ALEN];
	int add = 0;
	int err, i;

	if (count < sizeof("-01:02:03:04:05:06"))
		return -EINVAL;

	switch (buf[0]) {
	case '-':
		break;
	case '+':
		add = 1;
		break;
	default:
		return -EINVAL;
	}

	err = sscanf(&buf[1], "%02x:%02x:%02x:%02x:%02x:%02x",
		     &tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4], &tmp[5]);

	if (err != ETH_ALEN)
		return -EINVAL;

	for (i = 0; i < ETH_ALEN; ++i)
		mac[i] = tmp[i] & 0xff;

	rtnl_lock();
	if (is_unicast_ether_addr(mac)) {
		if (add)
			err = dev_uc_add_excl(netdev, mac);
		else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))
			err = dev_uc_del(netdev, mac);
#else
			err = dev_unicast_delete(netdev, mac);
#endif
	} else if (is_multicast_ether_addr(mac)) {
		if (add)
			err = dev_mc_add_excl(netdev, mac);
		else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))
			err = dev_mc_del(netdev, mac);
#else
			err = dev_mc_delete(netdev, mac, ETH_ALEN, true);
#endif
	} else {
		rtnl_unlock();
		return -EINVAL;
	}
	rtnl_unlock();

	en_dbg(DRV, priv, "Port:%d: %s %pM\n", priv->port,
	       (add ? "adding" : "removing"), mac);

	return err ? err : count;
}

static DEVICE_ATTR(fdb, S_IRUGO | 002, mlx4_en_show_fdb, mlx4_en_set_fdb);
#endif

void mlx4_en_destroy_netdev(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int i;

	en_dbg(DRV, priv, "Destroying netdev on port:%d\n", priv->port);

#ifdef CONFIG_COMPAT_EN_SYSFS
	if (priv->sysfs_group_initialized)
		mlx4_en_sysfs_remove(dev);
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 5, 0))
	if (mlx4_is_mfunc(priv->mdev->dev))
		device_remove_file(&dev->dev, &dev_attr_fdb);
#endif
	/* Unregister device - this will close the port if it was up */
	if (priv->registered)
		unregister_netdev(dev);

	if (priv->allocated) {
		mlx4_free_hwq_res(mdev->dev, &priv->res, MLX4_EN_PAGE_SIZE);

		/* Remove pm_qos request, after reseting to default value */
		pm_qos_update_request(&priv->pm_qos_req, PM_QOS_DEFAULT_VALUE);
		pm_qos_remove_request(&priv->pm_qos_req);
	}

	cancel_delayed_work(&priv->stats_task);
	cancel_delayed_work(&priv->service_task);
	/* flush any pending task for this netdev */
	flush_workqueue(mdev->workqueue);

	/* Detach the netdev so tasks would not attempt to access it */
	mutex_lock(&mdev->state_lock);
	mdev->pndev[priv->port] = NULL;
	mutex_unlock(&mdev->state_lock);

	if (mlx4_is_master(priv->mdev->dev)) {
		for (i = 0; i < priv->mdev->dev->num_vfs; i++) {
			if (priv->vf_ports[i]) {
				kobject_put(&priv->vf_ports[i]->kobj_stats);
				kobject_put(&priv->vf_ports[i]->kobj_vf);
				kfree(priv->vf_ports[i]);
				priv->vf_ports[i] = NULL;
			}
		}
	}

	mlx4_en_free_resources(priv);

	kfree(priv->tx_ring);
	kfree(priv->tx_cq);

	free_netdev(dev);
}

static int mlx4_en_change_mtu(struct net_device *dev, int new_mtu)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int err = 0;

	en_dbg(DRV, priv, "Change MTU called - current:%d new:%d\n",
		 dev->mtu, new_mtu);

	if ((new_mtu < MLX4_EN_MIN_MTU) || (new_mtu > priv->max_mtu)) {
		en_err(priv, "Bad MTU size:%d.\n", new_mtu);
		return -EINVAL;
	}
	dev->mtu = new_mtu;

	if (netif_running(dev)) {
		mutex_lock(&mdev->state_lock);
		if (!mdev->device_up) {
			/* NIC is probably restarting - let watchdog task reset
			 * the port */
			en_dbg(DRV, priv, "Change MTU called with card down!?\n");
		} else {
			mlx4_en_stop_port(dev);
			err = mlx4_en_start_port(dev);
			if (err) {
				en_err(priv, "Failed restarting port:%d\n",
					 priv->port);
				queue_work(mdev->workqueue, &priv->watchdog_task);
			}
		}
		mutex_unlock(&mdev->state_lock);
	}
	return 0;
}

static int mlx4_en_hwtstamp_ioctl(struct net_device *dev, struct ifreq *ifr)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	struct hwtstamp_config hwts_config;

	if (copy_from_user(&hwts_config, ifr->ifr_data, sizeof(hwts_config)))
		return -EFAULT;

	/* reserved for future extensions */
	if (hwts_config.flags)
		return -EINVAL;

	/* device doesn't support time stamping */
	if (!(mdev->dev->caps.flags2 & MLX4_DEV_CAP_FLAG2_TS))
		return -EINVAL;

	/* TX HW timestamp */
	switch (hwts_config.tx_type) {
	case HWTSTAMP_TX_OFF:
	case HWTSTAMP_TX_ON:
		break;
	default:
		return -ERANGE;
	}

	/* RX HW timestamp */
	switch (hwts_config.rx_filter) {
	case HWTSTAMP_FILTER_NONE:
		break;
	case HWTSTAMP_FILTER_ALL:
	case HWTSTAMP_FILTER_SOME:
	case HWTSTAMP_FILTER_PTP_V1_L4_EVENT:
	case HWTSTAMP_FILTER_PTP_V1_L4_SYNC:
	case HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ:
	case HWTSTAMP_FILTER_PTP_V2_L4_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_L4_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ:
	case HWTSTAMP_FILTER_PTP_V2_L2_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_L2_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_L2_DELAY_REQ:
	case HWTSTAMP_FILTER_PTP_V2_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_DELAY_REQ:
		hwts_config.rx_filter = HWTSTAMP_FILTER_ALL;
		break;
	default:
		return -ERANGE;
	}

	priv->config.hwtstamp = hwts_config; /* Copy new HW time stamping config */
	if (mlx4_en_reset_config(dev)) {
		hwts_config.tx_type = HWTSTAMP_TX_OFF;
		hwts_config.rx_filter = HWTSTAMP_FILTER_NONE;
	}

	return copy_to_user(ifr->ifr_data, &hwts_config,
			    sizeof(hwts_config)) ? -EFAULT : 0;
}

static int mlx4_en_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	switch (cmd) {
	case SIOCSHWTSTAMP:
		return mlx4_en_hwtstamp_ioctl(dev, ifr);
	default:
		return -EOPNOTSUPP;
	}
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39) || defined(CONFIG_COMPAT_LOOPBACK))
#ifndef CONFIG_COMPAT_LOOPBACK
static
#endif
int mlx4_en_set_features(struct net_device *dev,
			 netdev_features_t features)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);

	if ((features & NETIF_F_HW_VLAN_RX) &&
	    !(dev->features & NETIF_F_HW_VLAN_RX)) {
		priv->config.flags |= MLX4_EN_RX_VLAN_OFFLOAD; /* Turn ON RX vlan strip offload */
		en_info(priv, "Turn ON RX vlan strip offload\n");
		mlx4_en_reset_config(dev);
	} else if (!(features & NETIF_F_HW_VLAN_RX) &&
		  (dev->features & NETIF_F_HW_VLAN_RX)) {
		priv->config.flags &= ~MLX4_EN_RX_VLAN_OFFLOAD; /* Turn OFF RX vlan strip offload */
		en_info(priv, "Turn OFF RX vlan strip offload\n");
		mlx4_en_reset_config(dev);
	}

	if (features & NETIF_F_LOOPBACK)
		priv->ctrl_flags |= cpu_to_be32(MLX4_WQE_CTRL_FORCE_LOOPBACK);
	else
		priv->ctrl_flags &=
			cpu_to_be32(~MLX4_WQE_CTRL_FORCE_LOOPBACK);

	mlx4_en_update_loopback_state(dev, features);

	return 0;
}
#endif

#ifdef CONFIG_COMPAT_NDO_VF_MAC_VLAN
static int mlx4_en_set_vf_mac(struct net_device *dev, int queue, u8 *mac)
{
	struct mlx4_en_priv *en_priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = en_priv->mdev;

	if (!is_valid_ether_addr(mac))
		return -EINVAL;

	return mlx4_set_vf_mac(mdev->dev, en_priv->port, queue, mac);
}

static int mlx4_en_set_vf_vlan(struct net_device *dev, int vf, u16 vlan, u8 qos)
{
	struct mlx4_en_priv *en_priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = en_priv->mdev;

	return mlx4_set_vf_vlan(mdev->dev, en_priv->port, vf, vlan, qos);
}
#endif

#ifdef CONFIG_COMPAT_IS_VF_INFO_SPOOFCHK
static int mlx4_en_set_vf_spoofchk(struct net_device *dev, int vf, bool setting)
{
	struct mlx4_en_priv *en_priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = en_priv->mdev;

	return mlx4_set_vf_spoofchk(mdev->dev, en_priv->port, vf, setting);
}
#endif

#ifdef CONFIG_COMPAT_NDO_VF_MAC_VLAN
int mlx4_en_get_vf_config(struct net_device *dev, int vf, struct ifla_vf_info *ivf)
{
	struct mlx4_en_priv *en_priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = en_priv->mdev;

	return mlx4_get_vf_config(mdev->dev, en_priv->port, vf, ivf);
}
#endif

#ifdef CONFIG_COMPAT_IS_VF_INFO_LINKSTATE
static int mlx4_en_set_vf_link_state(struct net_device *dev, int vf, int link_state)
{
	struct mlx4_en_priv *en_priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = en_priv->mdev;

	return mlx4_set_vf_link_state(mdev->dev, en_priv->port, vf, link_state);
}
#endif

#ifdef CONFIG_COMPAT_FDB_API_EXISTS
#ifdef CONFIG_COMPAT_FDB_ADD_NLATTR
static int mlx4_en_fdb_add(struct ndmsg *ndm, struct nlattr *tb[],
#else
static int mlx4_en_fdb_add(struct ndmsg *ndm,
#endif
			   struct net_device *dev,
#ifdef CONFIG_COMPAT_FDB_CONST_ADDR
			   const unsigned char *addr, u16 flags)
#else
			   unsigned char *addr, u16 flags)
#endif
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_dev *mdev = priv->mdev->dev;
	int err;

	if (!mlx4_is_mfunc(mdev))
		return -EOPNOTSUPP;

	/* Hardware does not support aging addresses so if a
	 * ndm_state is given only allow permanent addresses
	 */
	if (ndm->ndm_state && !(ndm->ndm_state & NUD_PERMANENT)) {
		en_info(priv, "Add FDB only supports static addresses\n");
		return -EINVAL;
	}

	if (is_unicast_ether_addr(addr))
		err = dev_uc_add_excl(dev, addr);
	else if (is_multicast_ether_addr(addr))
		err = dev_mc_add_excl(dev, addr);
	else
		err = -EINVAL;

	/* Only return duplicate errors if NLM_F_EXCL is set */
	if (err == -EEXIST && !(flags & NLM_F_EXCL))
		err = 0;

	return err;
}

#ifdef CONFIG_COMPAT_FDB_DEL_NLATTR
static int mlx4_en_fdb_del(struct ndmsg *ndm, struct nlattr *tb[],
#else
static int mlx4_en_fdb_del(struct ndmsg *ndm,
#endif
			   struct net_device *dev,
#ifdef CONFIG_COMPAT_FDB_CONST_ADDR
			   const unsigned char *addr)
#else
 			   unsigned char *addr)
#endif
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_dev *mdev = priv->mdev->dev;
	int err;

	if (!mlx4_is_mfunc(mdev))
		return -EOPNOTSUPP;

	if (ndm->ndm_state && !(ndm->ndm_state & NUD_PERMANENT)) {
		en_info(priv, "Del FDB only supports static addresses\n");
		return -EINVAL;
	}

	if (is_unicast_ether_addr(addr))
		err = dev_uc_del(dev, addr);
	else if (is_multicast_ether_addr(addr))
		err = dev_mc_del(dev, addr);
	else
		err = -EINVAL;

	return err;
}

static int mlx4_en_fdb_dump(struct sk_buff *skb,
			    struct netlink_callback *cb,
			    struct net_device *dev, int idx)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_dev *mdev = priv->mdev->dev;

	if (mlx4_is_mfunc(mdev))
		idx = ndo_dflt_fdb_dump(skb, cb, dev, idx);

	return idx;
}
#endif

#ifdef CONFIG_COMPAT_VXLAN_ENABLED
static void mlx4_en_add_vxlan_offloads(struct work_struct *work)
{
	int ret;
	struct mlx4_en_priv *priv = container_of(work, struct mlx4_en_priv,
						 vxlan_add_task);

	ret = mlx4_config_vxlan_port(priv->mdev->dev, priv->vxlan_port);
	if (ret)
		goto out;

	ret = mlx4_SET_PORT_VXLAN(priv->mdev->dev, priv->port,
				  VXLAN_STEER_BY_OUTER_MAC, VXLAN_ENABLE);
out:
	if (ret)
		en_err(priv, "failed setting L2 tunnel configuration ret %d\n", ret);
}

static void mlx4_en_del_vxlan_offloads(struct work_struct *work)
{
	int ret;
	struct mlx4_en_priv *priv = container_of(work, struct mlx4_en_priv,
						 vxlan_del_task);

	ret = mlx4_SET_PORT_VXLAN(priv->mdev->dev, priv->port,
				  VXLAN_STEER_BY_OUTER_MAC, VXLAN_DISABLE);
	if (ret)
		en_err(priv, "failed setting L2 tunnel configuration ret %d\n", ret);

	priv->vxlan_port = 0;
}

#ifdef CONFIG_COMPAT_VXLAN_DYNAMIC_PORT
static void mlx4_en_add_vxlan_port(struct  net_device *dev,
				   sa_family_t sa_family, __be16 port)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	__be16 current_port;

	if (priv->mdev->dev->caps.tunnel_offload_mode != MLX4_TUNNEL_OFFLOAD_MODE_VXLAN)
		return;

	if (sa_family == AF_INET6)
		return;

	current_port = priv->vxlan_port;
	if (current_port && current_port != port) {
		en_warn(priv, "vxlan port %d configured, can't add port %d\n",
			ntohs(current_port), ntohs(port));
		return;
	}

	priv->vxlan_port = port;
	queue_work(priv->mdev->workqueue, &priv->vxlan_add_task);
}

static void mlx4_en_del_vxlan_port(struct  net_device *dev,
				   sa_family_t sa_family, __be16 port)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	__be16 current_port;

	if (priv->mdev->dev->caps.tunnel_offload_mode != MLX4_TUNNEL_OFFLOAD_MODE_VXLAN)
		return;

	if (sa_family == AF_INET6)
		return;

	current_port = priv->vxlan_port;
	if (current_port != port) {
		en_dbg(DRV, priv, "vxlan port %d isn't configured, ignoring\n", ntohs(port));
		return;
	}

	queue_work(priv->mdev->workqueue, &priv->vxlan_del_task);
}
#endif
#endif

static const struct net_device_ops mlx4_netdev_ops = {
	.ndo_open		= mlx4_en_open,
	.ndo_stop		= mlx4_en_close,
	.ndo_start_xmit		= mlx4_en_xmit,
	.ndo_select_queue	= mlx4_en_select_queue,
	.ndo_get_stats		= mlx4_en_get_stats,
	.ndo_set_rx_mode	= mlx4_en_set_rx_mode,
	.ndo_set_mac_address	= mlx4_en_set_mac,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_change_mtu		= mlx4_en_change_mtu,
	.ndo_do_ioctl		= mlx4_en_ioctl,
	.ndo_tx_timeout		= mlx4_en_tx_timeout,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0))
	.ndo_vlan_rx_register	= mlx4_en_vlan_rx_register,
#endif
	.ndo_vlan_rx_add_vid	= mlx4_en_vlan_rx_add_vid,
	.ndo_vlan_rx_kill_vid	= mlx4_en_vlan_rx_kill_vid,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= mlx4_en_netpoll,
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39))
	.ndo_set_features	= mlx4_en_set_features,
	.ndo_setup_tc		= mlx4_en_setup_tc,
#endif
#ifdef CONFIG_RFS_ACCEL
#ifndef CONFIG_COMPAT_IS_NETDEV_EXTENDED
	.ndo_rx_flow_steer	= mlx4_en_filter_rfs,
#endif
#endif
#ifdef CONFIG_NET_RX_BUSY_POLL
#ifndef CONFIG_COMPAT_IS_NETDEV_EXTENDED
	.ndo_busy_poll		= mlx4_en_low_latency_recv,
#endif
#endif
#ifdef CONFIG_COMPAT_FDB_API_EXISTS
	.ndo_fdb_add		= mlx4_en_fdb_add,
	.ndo_fdb_del		= mlx4_en_fdb_del,
	.ndo_fdb_dump		= mlx4_en_fdb_dump,
#endif
};

static const struct net_device_ops mlx4_netdev_ops_master = {
	.ndo_open		= mlx4_en_open,
	.ndo_stop		= mlx4_en_close,
	.ndo_start_xmit		= mlx4_en_xmit,
	.ndo_select_queue	= mlx4_en_select_queue,
	.ndo_get_stats		= mlx4_en_get_stats,
	.ndo_set_rx_mode	= mlx4_en_set_rx_mode,
	.ndo_set_mac_address	= mlx4_en_set_mac,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_change_mtu		= mlx4_en_change_mtu,
	.ndo_do_ioctl		= mlx4_en_ioctl,
	.ndo_tx_timeout		= mlx4_en_tx_timeout,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0))
	.ndo_vlan_rx_register	= mlx4_en_vlan_rx_register,
#endif
	.ndo_vlan_rx_add_vid	= mlx4_en_vlan_rx_add_vid,
	.ndo_vlan_rx_kill_vid	= mlx4_en_vlan_rx_kill_vid,
#ifdef CONFIG_COMPAT_NDO_VF_MAC_VLAN
	.ndo_set_vf_mac		= mlx4_en_set_vf_mac,
	.ndo_set_vf_vlan	= mlx4_en_set_vf_vlan,
#endif
#ifdef CONFIG_COMPAT_IS_VF_INFO_SPOOFCHK
#ifndef CONFIG_COMPAT_IS_NETDEV_OPS_EXTENDED
	.ndo_set_vf_spoofchk	= mlx4_en_set_vf_spoofchk,
#endif
#endif
#ifdef CONFIG_COMPAT_IS_VF_INFO_LINKSTATE
#ifndef CONFIG_COMPAT_IS_NETDEV_OPS_EXTENDED
	.ndo_set_vf_link_state	= mlx4_en_set_vf_link_state,
#endif
#endif
#ifdef CONFIG_COMPAT_NDO_VF_MAC_VLAN
	.ndo_get_vf_config	= mlx4_en_get_vf_config,
#endif
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= mlx4_en_netpoll,
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39))
	.ndo_set_features	= mlx4_en_set_features,
	.ndo_setup_tc		= mlx4_en_setup_tc,
#endif
#ifdef CONFIG_RFS_ACCEL
#ifndef CONFIG_COMPAT_IS_NETDEV_EXTENDED
	.ndo_rx_flow_steer	= mlx4_en_filter_rfs,
#endif
#endif
#ifdef CONFIG_COMPAT_FDB_API_EXISTS
	.ndo_fdb_add		= mlx4_en_fdb_add,
	.ndo_fdb_del		= mlx4_en_fdb_del,
	.ndo_fdb_dump		= mlx4_en_fdb_dump,
#endif
#ifdef CONFIG_COMPAT_VXLAN_ENABLED
#ifdef CONFIG_COMPAT_VXLAN_DYNAMIC_PORT
	.ndo_add_vxlan_port	= mlx4_en_add_vxlan_port,
	.ndo_del_vxlan_port	= mlx4_en_del_vxlan_port,
#endif
#endif
};

#ifdef CONFIG_COMPAT_IS_NETDEV_OPS_EXTENDED
static const struct net_device_ops_ext mlx4_netdev_ops_master_ext = {
	.size			= sizeof(struct net_device_ops_ext),
	.ndo_set_vf_spoofchk	= mlx4_en_set_vf_spoofchk,
	.ndo_set_vf_link_state	= mlx4_en_set_vf_link_state,
};
#endif

int mlx4_en_init_netdev(struct mlx4_en_dev *mdev, int port,
			struct mlx4_en_port_profile *prof)
{
	struct net_device *dev;
	struct mlx4_en_priv *priv;
	int i;
	int err;
	u64 mac_u64;
#ifndef CONFIG_COMPAT_DISABLE_DCB
#ifdef CONFIG_MLX4_EN_DCB
	struct tc_configuration *tc;
	u8 config = 0;
#endif
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)) || defined(CONFIG_COMPAT_NEW_TX_RING_SCHEME)
	dev = alloc_etherdev_mqs(sizeof(struct mlx4_en_priv),
				 MAX_TX_RINGS, MAX_RX_RINGS);
#else
	dev = alloc_etherdev_mq(sizeof(struct mlx4_en_priv), MAX_TX_RINGS);
#endif
	if (dev == NULL)
		return -ENOMEM;

#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)) || defined (CONFIG_COMPAT_IS_NUM_TX_QUEUES) || defined (CONFIG_X86_XEN)) && \
	!defined (CONFIG_COMPAT_DISABLE_REAL_NUM_TXQ)
	netif_set_real_num_tx_queues(dev, prof->tx_ring_num);
#else
	dev->real_num_tx_queues = prof->tx_ring_num;
#endif
	netif_set_real_num_rx_queues(dev, prof->rx_ring_num);

	SET_NETDEV_DEV(dev, &mdev->dev->pdev->dev);
	dev->dev_id =  port - 1;

	/*
	 * Initialize driver private data
	 */

	priv = netdev_priv(dev);
	memset(priv, 0, sizeof(struct mlx4_en_priv));
	priv->counter_index = 0xff;
	mlx4_set_stats_bitmap(mdev->dev, priv->stats_bitmap);
	spin_lock_init(&priv->stats_lock);
	INIT_WORK(&priv->rx_mode_task, mlx4_en_do_set_rx_mode);
	INIT_WORK(&priv->watchdog_task, mlx4_en_restart);
	INIT_WORK(&priv->linkstate_task, mlx4_en_linkstate);
	INIT_DELAYED_WORK(&priv->stats_task, mlx4_en_do_get_stats);
	INIT_DELAYED_WORK(&priv->service_task, mlx4_en_service_task);
#ifdef CONFIG_COMPAT_VXLAN_ENABLED
	INIT_WORK(&priv->vxlan_add_task, mlx4_en_add_vxlan_offloads);
	INIT_WORK(&priv->vxlan_del_task, mlx4_en_del_vxlan_offloads);
#endif
#ifdef CONFIG_RFS_ACCEL
	INIT_LIST_HEAD(&priv->filters);
	spin_lock_init(&priv->filters_lock);
#endif

	priv->msg_enable = MLX4_EN_MSG_LEVEL;
	priv->dev = dev;
	priv->mdev = mdev;
	priv->ddev = &mdev->pdev->dev;
	priv->prof = prof;
	priv->port = port;
	priv->port_up = false;
	priv->flags = prof->flags;
	priv->pflags = MLX4_EN_PRIV_FLAGS_BLUEFLAME;
	priv->ctrl_flags = cpu_to_be32(MLX4_WQE_CTRL_CQ_UPDATE |
			MLX4_WQE_CTRL_SOLICITED);
	priv->num_tx_rings_p_up =
		mdev->profile.prof[priv->port].num_tx_rings_p_up;
	priv->tx_ring_num = prof->tx_ring_num;

	priv->tx_ring = kcalloc(MAX_TX_RINGS,
				sizeof(struct mlx4_en_tx_ring *), GFP_KERNEL);
	if (!priv->tx_ring) {
		err = -ENOMEM;
		goto out;
	}
	priv->tx_cq = kcalloc(sizeof(struct mlx4_en_cq *), MAX_TX_RINGS,
			GFP_KERNEL);
	if (!priv->tx_cq) {
		err = -ENOMEM;
		goto out;
	}
	priv->rx_ring_num = prof->rx_ring_num;
	priv->cqe_factor = (mdev->dev->caps.cqe_size == 64) ? 1 : 0;
	priv->cqe_size = mdev->dev->caps.cqe_size;
	priv->mac_index = -1;
	priv->last_ifq_jiffies = 0;
	priv->if_counters_rx_errors = 0;
	priv->if_counters_rx_no_buffer = 0;
#ifndef CONFIG_COMPAT_DISABLE_DCB
#ifdef CONFIG_MLX4_EN_DCB
	if (!mlx4_is_slave(priv->mdev->dev)) {
		priv->dcbx_cap = DCB_CAP_DCBX_VER_CEE | DCB_CAP_DCBX_HOST;
		priv->flags |= MLX4_EN_FLAG_DCB_ENABLED;
		priv->dcb_cfg.pfc_mode_enable = false;
		priv->dcb_set_bitmap = 0x00;

		for (i = 0; i < MLX4_EN_NUM_UP; i++) {
			tc = &priv->dcb_cfg.tc_config[i];
			tc->dcb_pfc = pfc_disabled;
		}

		if (mdev->dev->caps.flags2 & MLX4_DEV_CAP_FLAG2_ETS_CFG) {
			dev->dcbnl_ops = &mlx4_en_dcbnl_ops;
		} else {
			en_info(priv, "QoS disabled - no HW support\n");
			dev->dcbnl_ops = &mlx4_en_dcbnl_pfc_ops;
		}
		/* Query for defalut disable_32_14_4_e value for qcn */
		err = mlx4_disable_32_14_4_e_read(priv->mdev->dev, &config, priv->port);
		if (!err) {
			if (config)
				priv->pflags |= MLX4_EN_PRIV_FLAGS_DISABLE_32_14_4_E;
			else
				priv->pflags &= ~MLX4_EN_PRIV_FLAGS_DISABLE_32_14_4_E;
		} else {
			if (err == -EOPNOTSUPP) {
				en_info(priv, "QCN disabled - no HW support\n");
			} else {
				en_err(priv, "Failed to query QCN status\n");
			}
		}
	}
#endif
#endif

	for (i = 0; i < MLX4_EN_MAC_HASH_SIZE; ++i)
		INIT_HLIST_HEAD(&priv->mac_hash[i]);

	/* Query for default mac and max mtu */
	priv->max_mtu = mdev->dev->caps.eth_mtu_cap[priv->port];

	/* Query CONFIG_DEV parameters */
	priv->rx_csum_mode_port = mdev->dev->caps.rx_checksum_flags_port[priv->port];

	/* Set default MAC */
	dev->addr_len = ETH_ALEN;
	mlx4_en_u64_to_mac(dev->dev_addr, mdev->dev->caps.def_mac[priv->port]);
	if (!is_valid_ether_addr(dev->dev_addr)) {
		if (mlx4_is_slave(priv->mdev->dev)) {
			eth_hw_addr_random(dev);
			en_warn(priv, "Assigned random MAC address %pM\n", dev->dev_addr);
			mac_u64 = mlx4_mac_to_u64(dev->dev_addr);
			mdev->dev->caps.def_mac[priv->port] = mac_u64;
		} else {
			en_err(priv, "Port: %d, invalid mac burned: %pM, quiting\n",
			       priv->port, dev->dev_addr);
			err = -EINVAL;
			goto out;
		}
	}

	memcpy(dev->perm_addr, dev->dev_addr, ETH_ALEN);
	memcpy(priv->current_mac, dev->dev_addr, sizeof(priv->current_mac));

	priv->stride = roundup_pow_of_two(sizeof(struct mlx4_en_rx_desc) +
					  DS_SIZE * MLX4_EN_MAX_RX_FRAGS);

	mlx4_en_set_affinity_masks(priv);
	/* Initialize time stamping config */
	priv->config.hwtstamp.flags = 0;
	priv->config.hwtstamp.tx_type = HWTSTAMP_TX_OFF;
	priv->config.hwtstamp.rx_filter = HWTSTAMP_FILTER_NONE;
	priv->config.flags = MLX4_EN_RX_VLAN_OFFLOAD | MLX4_EN_TX_VLAN_OFFLOAD;

	err = mlx4_en_alloc_resources(priv);
	if (err)
		goto out;

	/* Allocate page for receive rings */
	err = mlx4_alloc_hwq_res(mdev->dev, &priv->res,
				MLX4_EN_PAGE_SIZE, MLX4_EN_PAGE_SIZE);
	if (err) {
		en_err(priv, "Failed to allocate page for rx qps\n");
		goto out;
	}

	/* Initialize pm_qos request object */
	priv->last_cpu_dma_latency = PM_QOS_DEFAULT_VALUE;
	pm_qos_add_request(&priv->pm_qos_req,
			   PM_QOS_CPU_DMA_LATENCY,
			   PM_QOS_DEFAULT_VALUE);

	priv->allocated = 1;

	/*
	 * Initialize netdev entry points
	 */
	if (mlx4_is_master(priv->mdev->dev))
		dev->netdev_ops = &mlx4_netdev_ops_master;
	else
		dev->netdev_ops = &mlx4_netdev_ops;

#ifdef CONFIG_COMPAT_IS_NETDEV_OPS_EXTENDED
	if (mlx4_is_master(priv->mdev->dev))
		set_netdev_ops_ext(dev, &mlx4_netdev_ops_master_ext);
#endif

	dev->watchdog_timeo = MLX4_EN_WATCHDOG_TIMEOUT;
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)) || defined (CONFIG_COMPAT_IS_NUM_TX_QUEUES) || defined (CONFIG_X86_XEN)) && \
	!defined (CONFIG_COMPAT_DISABLE_REAL_NUM_TXQ)
	netif_set_real_num_tx_queues(dev, priv->tx_ring_num);
#else
	dev->real_num_tx_queues = priv->tx_ring_num;
#endif
	netif_set_real_num_rx_queues(dev, priv->rx_ring_num);

#ifdef CONFIG_RFS_ACCEL
#ifdef CONFIG_COMPAT_IS_NETDEV_EXTENDED
	netdev_extended(dev)->rfs_data.ndo_rx_flow_steer = mlx4_en_filter_rfs;
#endif
#endif
#ifdef CONFIG_NET_RX_BUSY_POLL
#ifdef CONFIG_COMPAT_IS_NETDEV_EXTENDED
	netdev_extended(dev)->ndo_busy_poll = mlx4_en_low_latency_recv;
#endif
#endif

	SET_ETHTOOL_OPS(dev, &mlx4_en_ethtool_ops);

#ifdef CONFIG_COMPAT_ETHTOOL_OPS_EXT
	set_ethtool_ops_ext(dev, &mlx4_en_ethtool_ops_ext);
#endif

	/*
	 * Set driver features
	 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39))
#ifdef CONFIG_COMPAT_LRO_ENABLED
	dev->hw_features = NETIF_F_SG | NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM |
		NETIF_F_GRO | NETIF_F_LRO;
#else
	dev->hw_features = NETIF_F_SG | NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM | NETIF_F_GRO;
#endif
	if (mdev->LSO_support)
		dev->hw_features |= NETIF_F_TSO | NETIF_F_TSO6;

	dev->vlan_features = dev->hw_features;

#ifdef CONFIG_COMPAT_NETIF_F_RXHASH
	dev->hw_features |= NETIF_F_RXCSUM | NETIF_F_RXHASH;
#else
	dev->hw_features |= NETIF_F_RXCSUM;
#endif
	dev->features = dev->hw_features | NETIF_F_HIGHDMA |
			NETIF_F_HW_VLAN_TX | NETIF_F_HW_VLAN_RX |
			NETIF_F_HW_VLAN_FILTER;
	dev->hw_features |= NETIF_F_LOOPBACK;
	dev->hw_features |= NETIF_F_HW_VLAN_RX;

	if (mdev->dev->caps.steering_mode ==
	    MLX4_STEERING_MODE_DEVICE_MANAGED)
		dev->hw_features |= NETIF_F_NTUPLE;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0))
	if (mdev->dev->caps.steering_mode != MLX4_STEERING_MODE_A0)
		dev->priv_flags |= IFF_UNICAST_FLT;
#endif

#else
#ifdef CONFIG_COMPAT_LRO_ENABLED
	dev->features = NETIF_F_SG | NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM |
		NETIF_F_GRO | NETIF_F_LRO;
#else
	dev->features = NETIF_F_SG | NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM | NETIF_F_GRO;
#endif
	if (mdev->LSO_support)
		dev->features |= NETIF_F_TSO | NETIF_F_TSO6;

	dev->vlan_features = dev->features;

#ifdef CONFIG_COMPAT_NETIF_F_RXHASH
	dev->features |= NETIF_F_RXCSUM | NETIF_F_RXHASH;
#else
	dev->features |= NETIF_F_RXCSUM;
#endif
	dev->features |= NETIF_F_HIGHDMA |
		NETIF_F_HW_VLAN_TX | NETIF_F_HW_VLAN_RX |
		NETIF_F_HW_VLAN_FILTER;

	if (mdev->dev->caps.steering_mode ==
			MLX4_STEERING_MODE_DEVICE_MANAGED)
		dev->features |= NETIF_F_NTUPLE;
#endif

#ifdef CONFIG_COMPAT_VXLAN_ENABLED
	if (mdev->dev->caps.tunnel_offload_mode == MLX4_TUNNEL_OFFLOAD_MODE_VXLAN) {
		dev->hw_enc_features |= NETIF_F_IP_CSUM | NETIF_F_RXCSUM |
					NETIF_F_TSO | NETIF_F_GSO_UDP_TUNNEL;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39))
		dev->hw_features |= NETIF_F_GSO_UDP_TUNNEL;
#endif
		dev->features    |= NETIF_F_GSO_UDP_TUNNEL;
	}
#endif

	mdev->pndev[port] = dev;

	netif_carrier_off(dev);
	mlx4_en_set_default_moderation(priv);

	err = register_netdev(dev);
	if (err) {
		en_err(priv, "Netdev registration failed for port %d\n", port);
		goto out;
	}
	priv->registered = 1;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 5, 0))
	if (mlx4_is_mfunc(priv->mdev->dev)) {
		err = device_create_file(&dev->dev, &dev_attr_fdb);
		if (err) {
			en_err(priv, "Sysfs registration failed for port %d\n", port);
			goto out;
		}
	}
#endif

	en_warn(priv, "Using %d TX rings\n", prof->tx_ring_num);
	en_warn(priv, "Using %d RX rings\n", prof->rx_ring_num);

	if (mlx4_is_master(priv->mdev->dev)) {
		for (i = 0; i < priv->mdev->dev->num_vfs; i++) {
			priv->vf_ports[i] = kzalloc(sizeof(struct en_port), GFP_KERNEL);
			if (!priv->vf_ports[i]) {
				err = -ENOMEM;
				goto out;
			}
			priv->vf_ports[i]->dev = priv->mdev->dev;
			priv->vf_ports[i]->port_num = port & 0xff;
			priv->vf_ports[i]->vport_num = i & 0xff;
			err = kobject_init_and_add(&priv->vf_ports[i]->kobj_vf,
						   &en_port_type,
						   &dev->dev.kobj,
						   "vf%d", i);
			if (err) {
				kfree(priv->vf_ports[i]);
				priv->vf_ports[i] = NULL;
				goto out;
			}
			err = kobject_init_and_add(&priv->vf_ports[i]->kobj_stats,
						   &en_port_stats,
						   &priv->vf_ports[i]->kobj_vf,
						   "statistics");
			if (err) {
				kobject_put(&priv->vf_ports[i]->kobj_vf);
				kfree(priv->vf_ports[i]);
				priv->vf_ports[i] = NULL;
				goto out;
			}
		}
	}

	mlx4_en_update_loopback_state(priv->dev, priv->dev->features);

	/* Configure port */
	priv->rx_skb_size = dev->mtu + ETH_HLEN + VLAN_HLEN + ETH_FCS_LEN;
	err = mlx4_SET_PORT_general(mdev->dev, priv->port,
				    priv->rx_skb_size,
				    prof->tx_pause, prof->tx_ppp,
				    prof->rx_pause, prof->rx_ppp);
	if (err) {
		en_err(priv, "Failed setting port general configurations "
		       "for port %d, with error %d\n", priv->port, err);
		goto out;
	}

#ifdef CONFIG_COMPAT_VXLAN_ENABLED
	if (mdev->dev->caps.tunnel_offload_mode == MLX4_TUNNEL_OFFLOAD_MODE_VXLAN) {
		err = mlx4_SET_PORT_VXLAN(mdev->dev, priv->port,
					  VXLAN_STEER_BY_OUTER_MAC,
					  VXLAN_ENABLE);
		if (err) {
			en_err(priv, "Failed setting port L2 tunnel configuration, err %d\n",
			       err);
			goto out;
		}
	}
#endif

	/* Init port */
	en_warn(priv, "Initializing port\n");
	err = mlx4_INIT_PORT(mdev->dev, priv->port);
	if (err) {
		en_err(priv, "Failed Initializing port\n");
		goto out;
	}
	queue_delayed_work(mdev->workqueue, &priv->stats_task, STATS_DELAY);
	if (mdev->dev->caps.steering_mode ==
	    MLX4_STEERING_MODE_DEVICE_MANAGED) {
		priv->pflags |= MLX4_EN_PRIV_FLAGS_FS_EN_L2;
		if (mdev->dev->caps.dmfs_high_steer_mode != MLX4_STEERING_DMFS_A0_STATIC)
			priv->pflags |= MLX4_EN_PRIV_FLAGS_FS_EN_IPV4	|
					MLX4_EN_PRIV_FLAGS_FS_EN_TCP	|
					MLX4_EN_PRIV_FLAGS_FS_EN_UDP;
	}
	if (mdev->dev->caps.flags2 & MLX4_DEV_CAP_FLAG2_TS)
		queue_delayed_work(mdev->workqueue, &priv->service_task,
				   SERVICE_TASK_DELAY);

#ifdef CONFIG_COMPAT_EN_SYSFS
	err = mlx4_en_sysfs_create(dev);
	if (err)
		goto out;
	priv->sysfs_group_initialized = 1;
#endif

	return 0;

out:
	mlx4_en_destroy_netdev(dev);
	return err;
}

int mlx4_en_reset_config(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int port_up = 0;
	int n_stats, err = 0;
	u64 *data = NULL;

	err = mlx4_en_pre_config(priv);
	if (err)
		return err;

	mutex_lock(&mdev->state_lock);
	if (priv->port_up) {
		port_up = 1;
		mlx4_en_stop_port(dev);
	}

	/* Cache port statistics */
	n_stats = mlx4_en_get_sset_count(dev, ETH_SS_STATS);
	if (n_stats > 0) {
		data = kmalloc(n_stats * sizeof(u64), GFP_KERNEL);
		if (data)
			mlx4_en_get_ethtool_stats(dev, NULL, data);
	}

	mlx4_en_free_resources(priv);

	if (priv->config.hwtstamp.rx_filter != HWTSTAMP_FILTER_NONE &&
	    priv->config.flags & MLX4_EN_RX_VLAN_OFFLOAD) {
		en_warn(priv,
			"Can't turn ON both rxvlan offload and HW time stamping, turning off rxvlan\n");
		priv->config.flags &= ~MLX4_EN_RX_VLAN_OFFLOAD;
	}

	en_info(priv, "Changing Interface configuration HWTstamp_rx(%d) HWTstamp_tx(%d) rxvlan(%d) txvlan(%d)\n",
		priv->config.hwtstamp.rx_filter != HWTSTAMP_FILTER_NONE,
		priv->config.hwtstamp.tx_type != HWTSTAMP_TX_OFF,
		priv->config.flags & MLX4_EN_RX_VLAN_OFFLOAD,
		priv->config.flags & MLX4_EN_TX_VLAN_OFFLOAD);

	err = mlx4_en_alloc_resources(priv);
	if (err) {
		en_err(priv, "Failed reallocating port resources\n");
		goto out;
	}

	/* Restore port statistics */
	if (n_stats > 0 && data)
		mlx4_en_restore_ethtool_stats(priv, data);

	if (port_up) {
		err = mlx4_en_start_port(dev);
		if (err)
			en_err(priv, "Failed starting port\n");
	}

out:
	kfree(data);
	mutex_unlock(&mdev->state_lock);
	return err;
}
