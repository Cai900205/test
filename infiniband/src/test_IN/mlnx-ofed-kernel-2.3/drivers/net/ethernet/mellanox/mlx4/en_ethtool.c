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

#include <linux/kernel.h>
#include <linux/ethtool.h>
#include <linux/netdevice.h>
#include <linux/mlx4/driver.h>
#include <linux/in.h>
#include <net/ip.h>
#include <linux/bitmap.h>

#include "mlx4_en.h"
#include "en_port.h"

#define EN_ETHTOOL_QP_ATTACH (1ull << 63)

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0)
/* Add missing defines for supported and advertised speed features */
#define SUPPORTED_40000baseKR4_Full     (1 << 23)
#define SUPPORTED_40000baseCR4_Full     (1 << 24)
#define SUPPORTED_40000baseSR4_Full     (1 << 25)
#define SUPPORTED_40000baseLR4_Full     (1 << 26)
#define ADVERTISED_40000baseKR4_Full    (1 << 23)
#define ADVERTISED_40000baseCR4_Full    (1 << 24)
#define ADVERTISED_40000baseSR4_Full    (1 << 25)
#define ADVERTISED_40000baseLR4_Full    (1 << 26)
#endif

union mlx4_ethtool_flow_union {
	struct ethtool_tcpip4_spec		tcp_ip4_spec;
	struct ethtool_tcpip4_spec		udp_ip4_spec;
	struct ethtool_tcpip4_spec		sctp_ip4_spec;
	struct ethtool_ah_espip4_spec		ah_ip4_spec;
	struct ethtool_ah_espip4_spec		esp_ip4_spec;
	struct ethtool_usrip4_spec		usr_ip4_spec;
	struct ethhdr				ether_spec;
	__u8					hdata[52];
};

struct mlx4_ethtool_flow_ext {
	__u8		padding[2];
	unsigned char	h_dest[ETH_ALEN];
	__be16		vlan_etype;
	__be16		vlan_tci;
	__be32		data[2];
};

struct mlx4_ethtool_rx_flow_spec {
	__u32		flow_type;
	union mlx4_ethtool_flow_union h_u;
	struct mlx4_ethtool_flow_ext h_ext;
	union mlx4_ethtool_flow_union m_u;
	struct mlx4_ethtool_flow_ext m_ext;
	__u64		ring_cookie;
	__u32		location;
};

struct mlx4_ethtool_rxnfc {
	__u32				cmd;
	__u32				flow_type;
	__u64				data;
	struct mlx4_ethtool_rx_flow_spec	fs;
	__u32				rule_cnt;
	__u32				rule_locs[0];
};

#ifndef FLOW_MAC_EXT
#define	FLOW_MAC_EXT	0x40000000
#endif

#ifndef FLOW_EXT
#define	FLOW_EXT	0x80000000
#endif

#ifndef ETHER_FLOW
#define	ETHER_FLOW	0x12	/* spec only (ether_spec) */
#endif

static void
mlx4_en_get_drvinfo(struct net_device *dev, struct ethtool_drvinfo *drvinfo)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;

	strlcpy(drvinfo->driver, DRV_NAME, sizeof(drvinfo->driver));
	strlcpy(drvinfo->version, DRV_VERSION " (" DRV_RELDATE ")",
		sizeof(drvinfo->version));
	snprintf(drvinfo->fw_version, sizeof(drvinfo->fw_version),
		"%d.%d.%d",
		(u16) (mdev->dev->caps.fw_ver >> 32),
		(u16) ((mdev->dev->caps.fw_ver >> 16) & 0xffff),
		(u16) (mdev->dev->caps.fw_ver & 0xffff));
	strlcpy(drvinfo->bus_info, pci_name(mdev->dev->pdev),
		sizeof(drvinfo->bus_info));
	drvinfo->n_stats = 0;
	drvinfo->regdump_len = 0;
	drvinfo->eedump_len = 0;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0))
static u32 mlx4_en_get_tso(struct net_device *dev)
{
       return (dev->features & NETIF_F_TSO) != 0;
}

static int mlx4_en_set_tso(struct net_device *dev, u32 data)
{
       struct mlx4_en_priv *priv = netdev_priv(dev);

       if (data) {
               if (!priv->mdev->LSO_support)
                       return -EPERM;
               dev->features |= (NETIF_F_TSO | NETIF_F_TSO6);
#ifdef HAVE_NETDEV_VLAN_FEATURES
               dev->vlan_features |= (NETIF_F_TSO | NETIF_F_TSO6);
#else
               if (priv->vlgrp) {
                       int i;
                       struct net_device *vdev;
                       for (i = 0; i < VLAN_N_VID; i++) {
                               vdev = vlan_group_get_device(priv->vlgrp, i);
                               if (vdev) {
                                       vdev->features |= (NETIF_F_TSO | NETIF_F_TSO6);
                                       vlan_group_set_device(priv->vlgrp, i, vdev);
                               }
                       }
               }
#endif
       } else {
               dev->features &= ~(NETIF_F_TSO | NETIF_F_TSO6);
#ifdef HAVE_NETDEV_VLAN_FEATURES
               dev->vlan_features &= ~(NETIF_F_TSO | NETIF_F_TSO6);
#else
               if (priv->vlgrp) {
                       int i;
                       struct net_device *vdev;
                       for (i = 0; i < VLAN_N_VID; i++) {
                               vdev = vlan_group_get_device(priv->vlgrp, i);
                               if (vdev) {
                                       vdev->features &= ~(NETIF_F_TSO | NETIF_F_TSO6);
                                       vlan_group_set_device(priv->vlgrp, i, vdev);
                               }
                       }
               }
#endif
       }
       return 0;
}

static u32 mlx4_en_get_rx_csum(struct net_device *dev)
{
       return dev->features & NETIF_F_RXCSUM;
}

static int mlx4_en_set_rx_csum(struct net_device *dev, u32 data)
{
       if (!data) {
               dev->features &= ~NETIF_F_RXCSUM;
               return 0;
       }
       dev->features |= NETIF_F_RXCSUM;
       return 0;
}
#endif

static const char main_strings[][ETH_GSTRING_LEN] = {
	/* packet statistics */
	"rx_packets",
	"rx_bytes",
	"rx_multicast_packets",
	"rx_broadcast_packets",
	"rx_errors",
	"rx_dropped",
	"rx_length_errors",
	"rx_over_errors",
	"rx_crc_errors",
	"rx_jabbers",
	"rx_in_range_length_error",
	"rx_out_range_length_error",
	"rx_lt_64_bytes_packets",
	"rx_127_bytes_packets",
	"rx_255_bytes_packets",
	"rx_511_bytes_packets",
	"rx_1023_bytes_packets",
	"rx_1518_bytes_packets",
	"rx_1522_bytes_packets",
	"rx_1548_bytes_packets",
	"rx_gt_1548_bytes_packets",
	"tx_packets",
	"tx_bytes",
	"tx_multicast_packets",
	"tx_broadcast_packets",
	"tx_errors",
	"tx_dropped",
	"tx_lt_64_bytes_packets",
	"tx_127_bytes_packets",
	"tx_255_bytes_packets",
	"tx_511_bytes_packets",
	"tx_1023_bytes_packets",
	"tx_1518_bytes_packets",
	"tx_1522_bytes_packets",
	"tx_1548_bytes_packets",
	"tx_gt_1548_bytes_packets",
	"rx_prio_0_packets", "rx_prio_0_bytes",
	"rx_prio_1_packets", "rx_prio_1_bytes",
	"rx_prio_2_packets", "rx_prio_2_bytes",
	"rx_prio_3_packets", "rx_prio_3_bytes",
	"rx_prio_4_packets", "rx_prio_4_bytes",
	"rx_prio_5_packets", "rx_prio_5_bytes",
	"rx_prio_6_packets", "rx_prio_6_bytes",
	"rx_prio_7_packets", "rx_prio_7_bytes",
	"rx_novlan_packets", "rx_novlan_bytes",
	"tx_prio_0_packets", "tx_prio_0_bytes",
	"tx_prio_1_packets", "tx_prio_1_bytes",
	"tx_prio_2_packets", "tx_prio_2_bytes",
	"tx_prio_3_packets", "tx_prio_3_bytes",
	"tx_prio_4_packets", "tx_prio_4_bytes",
	"tx_prio_5_packets", "tx_prio_5_bytes",
	"tx_prio_6_packets", "tx_prio_6_bytes",
	"tx_prio_7_packets", "tx_prio_7_bytes",
	"tx_novlan_packets", "tx_novlan_bytes",

	/* flow control statistics */
	"rx_pause_prio_0", "rx_pause_duration_prio_0",
	"rx_pause_transition_prio_0", "tx_pause_prio_0",
	"tx_pause_duration_prio_0", "tx_pause_transition_prio_0",
	"rx_pause_prio_1", "rx_pause_duration_prio_1",
	"rx_pause_transition_prio_1", "tx_pause_prio_1",
	"tx_pause_duration_prio_1", "tx_pause_transition_prio_1",
	"rx_pause_prio_2", "rx_pause_duration_prio_2",
	"rx_pause_transition_prio_2", "tx_pause_prio_2",
	"tx_pause_duration_prio_2", "tx_pause_transition_prio_2",
	"rx_pause_prio_3", "rx_pause_duration_prio_3",
	"rx_pause_transition_prio_3", "tx_pause_prio_3",
	"tx_pause_duration_prio_3", "tx_pause_transition_prio_3",
	"rx_pause_prio_4", "rx_pause_duration_prio_4",
	"rx_pause_transition_prio_4", "tx_pause_prio_4",
	"tx_pause_duration_prio_4", "tx_pause_transition_prio_4",
	"rx_pause_prio_5", "rx_pause_duration_prio_5",
	"rx_pause_transition_prio_5", "tx_pause_prio_5",
	"tx_pause_duration_prio_5", "tx_pause_transition_prio_5",
	"rx_pause_prio_6", "rx_pause_duration_prio_6",
	"rx_pause_transition_prio_6", "tx_pause_prio_6",
	"tx_pause_duration_prio_6", "tx_pause_transition_prio_6",
	"rx_pause_prio_7", "rx_pause_duration_prio_7",
	"rx_pause_transition_prio_7", "tx_pause_prio_7",
	"tx_pause_duration_prio_7", "tx_pause_transition_prio_7",

	/* VF statistics */
	"rx_packets",
	"rx_bytes",
	"rx_multicast_packets",
	"rx_broadcast_packets",
	"rx_filtered",
	"rx_dropped",
	"tx_packets",
	"tx_bytes",
	"tx_multicast_packets",
	"tx_broadcast_packets",
	"tx_errors",

	/* VPort statistics */
	"vport_rx_unicast_packets",
	"vport_rx_unicast_bytes",
	"vport_rx_multicast_packets",
	"vport_rx_multicast_bytes",
	"vport_rx_broadcast_packets",
	"vport_rx_broadcast_bytes",
	"vport_rx_dropped",
	"vport_rx_filtered",
	"vport_tx_unicast_packets",
	"vport_tx_unicast_bytes",
	"vport_tx_multicast_packets",
	"vport_tx_multicast_bytes",
	"vport_tx_broadcast_packets",
	"vport_tx_broadcast_bytes",
	"vport_tx_errors",

	/* port statistics */
#ifdef CONFIG_COMPAT_LRO_ENABLED
	"rx_lro_aggregated", "rx_lro_flushed", "rx_lro_no_desc",
#endif
	"tx_tso_packets",
	"tx_queue_stopped", "tx_wake_queue", "tx_timeout", "rx_alloc_failed",
	"rx_csum_good", "rx_csum_none", "tx_chksum_offload", "rx_replacement",

	/* perf statistics */
	"tx_poll",
	"tx_pktsz_avg",
	"inflight_avg",
	"tx_coal_avg",
	"rx_coal_avg",
	"napi_quota",
};

static const char mlx4_en_test_names[][ETH_GSTRING_LEN]= {
	"Interrupt Test",
	"Link Test",
	"Speed Test",
	"Register Test",
	"Loopback Test",
};

static const char mlx4_en_priv_flags[][ETH_GSTRING_LEN] = {
	"pm_qos_request_low_latency",
	"mlx4_rss_xor_hash_function",
	"mlx4_flow_steering_ethernet_l2",
	"mlx4_flow_steering_ipv4",
	"mlx4_flow_steering_tcp",
	"mlx4_flow_steering_udp",
#ifdef CONFIG_MLX4_EN_DCB
	"qcn_disable_32_14_4_e",
#endif
	"blueflame",
};

static u32 mlx4_en_get_msglevel(struct net_device *dev)
{
	return ((struct mlx4_en_priv *) netdev_priv(dev))->msg_enable;
}

static void mlx4_en_set_msglevel(struct net_device *dev, u32 val)
{
	((struct mlx4_en_priv *) netdev_priv(dev))->msg_enable = val;
}

static void mlx4_en_get_wol(struct net_device *netdev,
			    struct ethtool_wolinfo *wol)
{
	struct mlx4_en_priv *priv = netdev_priv(netdev);
	int err = 0;
	u64 config = 0;
	u64 mask;

	if ((priv->port < 1) || (priv->port > 2)) {
		en_err(priv, "Failed to get WoL information\n");
		return;
	}

	mask = (priv->port == 1) ? MLX4_DEV_CAP_FLAG_WOL_PORT1 :
		MLX4_DEV_CAP_FLAG_WOL_PORT2;

	if (!(priv->mdev->dev->caps.flags & mask)) {
		wol->supported = 0;
		wol->wolopts = 0;
		return;
	}

	err = mlx4_wol_read(priv->mdev->dev, &config, priv->port);
	if (err) {
		en_err(priv, "Failed to get WoL information\n");
		return;
	}

	if (config & MLX4_EN_WOL_MAGIC)
		wol->supported = WAKE_MAGIC;
	else
		wol->supported = 0;

	if (config & MLX4_EN_WOL_ENABLED)
		wol->wolopts = WAKE_MAGIC;
	else
		wol->wolopts = 0;
}

static int mlx4_en_set_wol(struct net_device *netdev,
			    struct ethtool_wolinfo *wol)
{
	struct mlx4_en_priv *priv = netdev_priv(netdev);
	u64 config = 0;
	int err = 0;
	u64 mask;

	if ((priv->port < 1) || (priv->port > 2))
		return -EOPNOTSUPP;

	mask = (priv->port == 1) ? MLX4_DEV_CAP_FLAG_WOL_PORT1 :
		MLX4_DEV_CAP_FLAG_WOL_PORT2;

	if (!(priv->mdev->dev->caps.flags & mask))
		return -EOPNOTSUPP;

	if (wol->supported & ~WAKE_MAGIC)
		return -EINVAL;

	err = mlx4_wol_read(priv->mdev->dev, &config, priv->port);
	if (err) {
		en_err(priv, "Failed to get WoL info, unable to modify\n");
		return err;
	}

	if (wol->wolopts & WAKE_MAGIC) {
		config |= MLX4_EN_WOL_DO_MODIFY | MLX4_EN_WOL_ENABLED |
				MLX4_EN_WOL_MAGIC;
	} else {
		config &= ~(MLX4_EN_WOL_ENABLED | MLX4_EN_WOL_MAGIC);
		config |= MLX4_EN_WOL_DO_MODIFY;
	}

	err = mlx4_wol_write(priv->mdev->dev, config, priv->port);
	if (err)
		en_err(priv, "Failed to set WoL information\n");

	return err;
}

struct bitmap_sim_iterator {
	bool advance_array;
	unsigned long *stats_bitmap;
	unsigned int count;
	unsigned int j;
};

static inline void bitmap_sim_iterator_init(struct bitmap_sim_iterator *h,
					    unsigned long *stats_bitmap,
					    int count)
{
	h->j = 0;
	h->advance_array = !bitmap_empty(stats_bitmap, count);
	h->count = h->advance_array ? bitmap_weight(stats_bitmap, count)
		: count;
	h->stats_bitmap = stats_bitmap;
}

static inline int bitmap_sim_iterator_test(struct bitmap_sim_iterator *h)
{
	return !h->advance_array ? 1 : test_bit(h->j, h->stats_bitmap);
}

static inline int bitmap_sim_iterator_inc(struct bitmap_sim_iterator *h)
{
	return h->j++;
}

static inline unsigned int bitmap_sim_iterator_count(
		struct bitmap_sim_iterator *h)
{
	return h->count;
}

int mlx4_en_get_sset_count(struct net_device *dev, int sset)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct bitmap_sim_iterator it;

	bitmap_sim_iterator_init(&it, priv->stats_bitmap, NUM_ALL_STATS);

	switch (sset) {
	case ETH_SS_STATS:
		return bitmap_sim_iterator_count(&it) +
			(priv->tx_ring_num * 2) +
#ifdef LL_EXTENDED_STATS
			(priv->rx_ring_num * 5);
#else
			(priv->rx_ring_num * 2);
#endif
	case ETH_SS_TEST:
		return MLX4_EN_NUM_SELF_TEST - !(priv->mdev->dev->caps.flags
					& MLX4_DEV_CAP_FLAG_UC_LOOPBACK) * 2;
	case ETH_SS_PRIV_FLAGS:
		return MLX4_EN_PRIV_NUM_FLAGS;

	default:
		return -EOPNOTSUPP;
	}
}

#ifdef CONFIG_COMPAT_LRO_ENABLED
static void mlx4_en_update_lro_stats(struct mlx4_en_priv *priv)
{
	int i;

	priv->port_stats.lro_aggregated = 0;
	priv->port_stats.lro_flushed = 0;
	priv->port_stats.lro_no_desc = 0;

	for (i = 0; i < priv->rx_ring_num; i++) {
		priv->port_stats.lro_aggregated += priv->rx_ring[i]->lro.lro_mgr.stats.aggregated;
		priv->port_stats.lro_flushed += priv->rx_ring[i]->lro.lro_mgr.stats.flushed;
		priv->port_stats.lro_no_desc += priv->rx_ring[i]->lro.lro_mgr.stats.no_desc;
	}
}
#endif

void mlx4_en_get_ethtool_stats(struct net_device *dev,
		struct ethtool_stats *stats, u64 *data)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	int index = 0;
	int i, data_len;
	struct bitmap_sim_iterator it;

	bitmap_sim_iterator_init(&it, priv->stats_bitmap, NUM_ALL_STATS);

	if (!data)
		return;

	if (!priv->port_up) {
		data_len = mlx4_en_get_sset_count(dev, ETH_SS_STATS);
		memset(data, 0, data_len*sizeof(u64));
		return;
	}

	spin_lock_bh(&priv->stats_lock);

#ifdef CONFIG_COMPAT_LRO_ENABLED
	mlx4_en_update_lro_stats(priv);
#endif

	for (i = 0; i < NUM_PKT_STATS; i++,
			bitmap_sim_iterator_inc(&it))
		if (bitmap_sim_iterator_test(&it))
			data[index++] = ((unsigned long *)&priv->pkstats)[i];

	for (i = 0; i < NUM_FLOW_STATS; i++,
			bitmap_sim_iterator_inc(&it))
		if (bitmap_sim_iterator_test(&it))
			data[index++] = ((u64 *)&priv->flowstats)[i];

	for (i = 0; i < NUM_VF_STATS; i++,
			bitmap_sim_iterator_inc(&it))
		if (bitmap_sim_iterator_test(&it))
			data[index++] = ((unsigned long *)&priv->vf_stats)[i];

	for (i = 0; i < NUM_VPORT_STATS; i++,
			bitmap_sim_iterator_inc(&it))
		if (bitmap_sim_iterator_test(&it))
			data[index++] = ((unsigned long *)&priv->vport_stats)[i];

	for (i = 0; i < NUM_PORT_STATS; i++,
			bitmap_sim_iterator_inc(&it))
		if (bitmap_sim_iterator_test(&it))
			data[index++] = ((unsigned long *)&priv->port_stats)[i];

	for (i = 0; i < NUM_PERF_STATS; i++, bitmap_sim_iterator_inc(&it))
		if (bitmap_sim_iterator_test(&it))
			data[index++] = ((unsigned long *)&priv->pstats)[i];

	for (i = 0; i < priv->tx_ring_num; i++) {
		data[index++] = priv->tx_ring[i]->packets;
		data[index++] = priv->tx_ring[i]->bytes;
	}
	for (i = 0; i < priv->rx_ring_num; i++) {
		data[index++] = priv->rx_ring[i]->packets;
		data[index++] = priv->rx_ring[i]->bytes;
#ifdef LL_EXTENDED_STATS
		data[index++] = priv->rx_ring[i]->yields;
		data[index++] = priv->rx_ring[i]->misses;
		data[index++] = priv->rx_ring[i]->cleaned;
#endif
	}
	spin_unlock_bh(&priv->stats_lock);

}

void mlx4_en_restore_ethtool_stats(struct mlx4_en_priv *priv, u64 *data)
{
	int index = 0;
	int i;
	struct bitmap_sim_iterator it;

	bitmap_sim_iterator_init(&it, priv->stats_bitmap, NUM_ALL_STATS);

	if (!data || !priv->port_up)
		return;

	spin_lock_bh(&priv->stats_lock);

	for (i = 0; i < NUM_PKT_STATS; i++,
	      bitmap_sim_iterator_inc(&it))
			if (bitmap_sim_iterator_test(&it))
				((unsigned long *)&priv->pkstats)[i] =
					data[index++];

	for (i = 0; i < NUM_FLOW_STATS; i++,
	      bitmap_sim_iterator_inc(&it))
		if (bitmap_sim_iterator_test(&it))
			((u64 *)&priv->flowstats)[i] = data[index++];

	for (i = 0; i < NUM_VF_STATS; i++,
	      bitmap_sim_iterator_inc(&it))
		if (bitmap_sim_iterator_test(&it))
			((unsigned long *)&priv->vf_stats)[i] = data[index++];

	for (i = 0; i < NUM_VPORT_STATS; i++,
	      bitmap_sim_iterator_inc(&it))
		if (bitmap_sim_iterator_test(&it))
			((unsigned long *)&priv->vport_stats)[i] =
			data[index++];

	for (i = 0; i < NUM_PORT_STATS; i++,
	      bitmap_sim_iterator_inc(&it))
			if (bitmap_sim_iterator_test(&it))
				((unsigned long *)&priv->port_stats)[i] =
				data[index++];

	for (i = 0; i < NUM_PERF_STATS; i++, bitmap_sim_iterator_inc(&it))
			if (bitmap_sim_iterator_test(&it))
				((unsigned long *)&priv->pstats)[i] =
				data[index++];

	for (i = 0; i < priv->tx_ring_num; i++) {
		priv->tx_ring[i]->packets = data[index++];
		priv->tx_ring[i]->bytes = data[index++];
	}
	for (i = 0; i < priv->rx_ring_num; i++) {
		priv->rx_ring[i]->packets = data[index++];
		priv->rx_ring[i]->bytes = data[index++];
	}
	spin_unlock_bh(&priv->stats_lock);
}

static void mlx4_en_self_test(struct net_device *dev,
			      struct ethtool_test *etest, u64 *buf)
{
	mlx4_en_ex_selftest(dev, &etest->flags, buf);
}

static void mlx4_en_get_strings(struct net_device *dev,
				uint32_t stringset, uint8_t *data)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	int index = 0;
	int i, strings = 0;
	struct bitmap_sim_iterator it;

	bitmap_sim_iterator_init(&it, priv->stats_bitmap, NUM_ALL_STATS);

	switch (stringset) {
	case ETH_SS_PRIV_FLAGS:
		for (i = 0; i < MLX4_EN_PRIV_NUM_FLAGS; i++)
			strcpy(data + i * ETH_GSTRING_LEN,
			       mlx4_en_priv_flags[i]);
		break;

	case ETH_SS_TEST:
		for (i = 0; i < MLX4_EN_NUM_SELF_TEST - 2; i++)
			strcpy(data + i * ETH_GSTRING_LEN, mlx4_en_test_names[i]);
		if (priv->mdev->dev->caps.flags & MLX4_DEV_CAP_FLAG_UC_LOOPBACK)
			for (; i < MLX4_EN_NUM_SELF_TEST; i++)
				strcpy(data + i * ETH_GSTRING_LEN, mlx4_en_test_names[i]);
		break;

	case ETH_SS_STATS:
		/* Add main counters */
		for (i = 0; i < NUM_PKT_STATS; i++, strings++,
		     bitmap_sim_iterator_inc(&it))
			if (bitmap_sim_iterator_test(&it))
				strcpy(data + (index++) * ETH_GSTRING_LEN,
				       main_strings[strings]);

		for (i = 0; i < NUM_FLOW_STATS; i++, strings++,
		     bitmap_sim_iterator_inc(&it))
			if (bitmap_sim_iterator_test(&it))
				strcpy(data + (index++) * ETH_GSTRING_LEN,
				       main_strings[strings]);

		for (i = 0; i < NUM_VF_STATS; i++, strings++,
		     bitmap_sim_iterator_inc(&it))
			if (bitmap_sim_iterator_test(&it))
				strcpy(data + (index++) * ETH_GSTRING_LEN,
				       main_strings[strings]);

		for (i = 0; i < NUM_VPORT_STATS; i++, strings++,
		     bitmap_sim_iterator_inc(&it))
			if (bitmap_sim_iterator_test(&it))
				strcpy(data + (index++) * ETH_GSTRING_LEN,
				       main_strings[strings]);

		for (i = 0; i < NUM_PORT_STATS; i++, strings++,
		     bitmap_sim_iterator_inc(&it))
			if (bitmap_sim_iterator_test(&it))
				strcpy(data + (index++) * ETH_GSTRING_LEN,
				       main_strings[strings]);

		for (i = 0; i < NUM_PERF_STATS; i++, strings++,
		     bitmap_sim_iterator_inc(&it))
			if (bitmap_sim_iterator_test(&it))
				strcpy(data + (index++) * ETH_GSTRING_LEN,
				       main_strings[strings]);

		for (i = 0; i < priv->tx_ring_num; i++) {
			sprintf(data + (index++) * ETH_GSTRING_LEN,
				"tx%d_packets", i);
			sprintf(data + (index++) * ETH_GSTRING_LEN,
				"tx%d_bytes", i);
		}
		for (i = 0; i < priv->rx_ring_num; i++) {
			sprintf(data + (index++) * ETH_GSTRING_LEN,
				"rx%d_packets", i);
			sprintf(data + (index++) * ETH_GSTRING_LEN,
				"rx%d_bytes", i);
#ifdef LL_EXTENDED_STATS
			sprintf(data + (index++) * ETH_GSTRING_LEN,
				"rx%d_napi_yield", i);
			sprintf(data + (index++) * ETH_GSTRING_LEN,
				"rx%d_misses", i);
			sprintf(data + (index++) * ETH_GSTRING_LEN,
				"rx%d_cleaned", i);
#endif
		}
		break;
	}
}

static u32 mlx4_en_autoneg_get(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	u32 autoneg = AUTONEG_DISABLE;

	if ((mdev->dev->caps.flags2 & MLX4_DEV_CAP_FLAG2_ETH_BACKPL_AN_REP) &&
	    priv->port_state.autoneg) {
		autoneg = AUTONEG_ENABLE;
	}

	return autoneg;
}

static u32 ptys_get_supported_port(struct mlx4_ptys_reg *ptys_reg)
{
	u32 eth_proto = be32_to_cpu(ptys_reg->eth_proto_cap);

	if (eth_proto & (MLX4_EN_PROT_10GBase_T
			 | MLX4_EN_PROT_1000Base_T
			 | MLX4_EN_PROT_100Base_TX)) {
			return SUPPORTED_TP;
	}

	if (eth_proto & (MLX4_EN_PROT_10GBase_CR
			 | MLX4_EN_PROT_10GBase_SR
			 | MLX4_EN_PROT_56GBase_SR4
			 | MLX4_EN_PROT_40GBase_CR4
			 | MLX4_EN_PROT_40GBase_SR4
			 | MLX4_EN_PROT_1000Base_CX_SGMII)) {
			return SUPPORTED_FIBRE;
	}

	if (eth_proto & (MLX4_EN_PROT_56GBase_KR4
			 | MLX4_EN_PROT_40GBase_KR4
			 | MLX4_EN_PROT_20GBase_KR2
			 | MLX4_EN_PROT_10GBase_KR
			 | MLX4_EN_PROT_10GBase_KX4
			 | MLX4_EN_PROT_1000Base_KX)) {
			return SUPPORTED_Backplane;
	}
	return 0;
}

static u32 ptys_get_active_port(struct mlx4_ptys_reg *ptys_reg)
{
	u32 eth_proto = be32_to_cpu(ptys_reg->eth_proto_oper);
	if (!eth_proto) /* link down*/
		eth_proto = be32_to_cpu(ptys_reg->eth_proto_cap);

	if (eth_proto & (MLX4_EN_PROT_10GBase_T
			 | MLX4_EN_PROT_1000Base_T
			 | MLX4_EN_PROT_100Base_TX)) {
			return PORT_TP;
	}

	if (eth_proto & (MLX4_EN_PROT_10GBase_SR
			 | MLX4_EN_PROT_56GBase_SR4
			 | MLX4_EN_PROT_40GBase_SR4
			 | MLX4_EN_PROT_1000Base_CX_SGMII)) {
			return PORT_FIBRE;
	}

	if (eth_proto & (MLX4_EN_PROT_10GBase_CR
			 | MLX4_EN_PROT_56GBase_CR4
			 | MLX4_EN_PROT_40GBase_CR4)) {
			return PORT_DA;
	}

	if (eth_proto & (MLX4_EN_PROT_56GBase_KR4
			 | MLX4_EN_PROT_40GBase_KR4
			 | MLX4_EN_PROT_20GBase_KR2
			 | MLX4_EN_PROT_10GBase_KR
			 | MLX4_EN_PROT_10GBase_KX4
			 | MLX4_EN_PROT_1000Base_KX)) {
			return PORT_NONE;
	}
	return PORT_OTHER;
}

#ifndef SUPPORTED_20000baseMLD2_Full
#define SUPPORTED_20000baseMLD2_Full  0
#define ADVERTISED_20000baseMLD2_Full 0
#endif

#ifndef SUPPORTED_20000baseKR2_Full
#define SUPPORTED_20000baseKR2_Full  0
#define ADVERTISED_20000baseKR2_Full 0
#endif

#define ptys2ethtool_link_modes(eth_proto, type) ({	\
	u32 link_modes = 0;					\
	link_modes |= (eth_proto & MLX4_EN_PROT_100Base_TX) ?	\
		type##_100baseT_Full : 0;			\
								\
	link_modes |= (eth_proto & MLX4_EN_PROT_1000Base_T) ?	\
		type##_1000baseT_Full : 0;			\
								\
	link_modes |= (eth_proto & MLX4_EN_PROT_10GBase_T) ?	\
		type##_10000baseT_Full : 0;			\
								\
	link_modes |= (eth_proto & (MLX4_EN_PROT_1000Base_KX|	\
				    MLX4_EN_PROT_1000Base_CX_SGMII)) ? \
		type##_1000baseKX_Full : 0;			\
								\
	link_modes |= (eth_proto & (MLX4_EN_PROT_10GBase_KX4|	\
				   MLX4_EN_PROT_10GBase_CX4)) ?	\
		type##_10000baseKX4_Full : 0;			\
								\
	link_modes |= (eth_proto & (MLX4_EN_PROT_10GBase_KR|	\
				    MLX4_EN_PROT_10GBase_CR|	\
				    MLX4_EN_PROT_10GBase_SR)) ?	\
		type##_10000baseKR_Full : 0;			\
								\
	link_modes |= (eth_proto & MLX4_EN_PROT_10GBase_KR) ?	\
		type##_10000baseR_FEC : 0;			\
								\
	link_modes |= (eth_proto & MLX4_EN_PROT_20GBase_KR2) ?	\
		type##_20000baseMLD2_Full | type##_20000baseKR2_Full : 0;\
								\
	link_modes |= (eth_proto & MLX4_EN_PROT_40GBase_KR4) ?	\
		type##_40000baseKR4_Full : 0;			\
								\
	link_modes |= (eth_proto & MLX4_EN_PROT_40GBase_CR4) ?	\
		type##_40000baseCR4_Full : 0;			\
								\
	link_modes |= (eth_proto & MLX4_EN_PROT_40GBase_SR4) ?	\
		type##_40000baseSR4_Full : 0;			\
								\
	link_modes;						\
})

static int ptys_get_ethtool_settings(struct net_device *dev,
				     struct ethtool_cmd *cmd)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	int ret;
	u32 eth_proto;
	struct mlx4_ptys_reg ptys_reg;

	memset(&ptys_reg, 0, sizeof(ptys_reg));
	ptys_reg.local_port = priv->port;
	ptys_reg.proto_mask = MLX4_PTYS_EN;
	ret = mlx4_ACCESS_PTYS_REG(priv->mdev->dev,
			     MLX4_ACCESS_REG_QUERY, &ptys_reg);
	if (ret) {
		en_warn(priv, "Failed to run mlx4_ACCESS_PTYS_REG status(%x)",
			  ret);
		return ret;
	}
	en_dbg(DRV, priv, "ptys_reg.proto_mask       %x\n",
	       ptys_reg.proto_mask);
	en_dbg(DRV, priv, "ptys_reg.eth_proto_cap    %x\n",
	       be32_to_cpu(ptys_reg.eth_proto_cap));
	en_dbg(DRV, priv, "ptys_reg.eth_proto_admin  %x\n",
	       be32_to_cpu(ptys_reg.eth_proto_admin));
	en_dbg(DRV, priv, "ptys_reg.eth_proto_oper   %x\n",
	       be32_to_cpu(ptys_reg.eth_proto_oper));
	en_dbg(DRV, priv, "ptys_reg.eth_proto_lp_adv %x\n",
	       be32_to_cpu(ptys_reg.eth_proto_lp_adv));

	cmd->supported = 0;
	cmd->advertising = 0;

	cmd->supported |= ptys_get_supported_port(&ptys_reg);

	eth_proto = be32_to_cpu(ptys_reg.eth_proto_cap);
	cmd->supported |= ptys2ethtool_link_modes(eth_proto, SUPPORTED);

	eth_proto = be32_to_cpu(ptys_reg.eth_proto_admin);
	cmd->advertising |= ptys2ethtool_link_modes(eth_proto, ADVERTISED);

	cmd->supported |= SUPPORTED_Pause | SUPPORTED_Asym_Pause;
	cmd->advertising |= (priv->prof->tx_pause) ? ADVERTISED_Pause : 0;

	cmd->advertising |= (priv->prof->tx_pause ^ priv->prof->rx_pause) ?
		ADVERTISED_Asym_Pause : 0;

	cmd->port = ptys_get_active_port(&ptys_reg);
	cmd->transceiver = (SUPPORTED_TP & cmd->supported) ?
		XCVR_EXTERNAL : XCVR_INTERNAL;

	if (mlx4_en_autoneg_get(dev)) {
		cmd->supported |= SUPPORTED_Autoneg;
		cmd->advertising |= ADVERTISED_Autoneg;
	}

	cmd->autoneg = (priv->port_state.flags & MLX4_EN_PORT_ANC) ?
		AUTONEG_ENABLE : AUTONEG_DISABLE;

	eth_proto = be32_to_cpu(ptys_reg.eth_proto_lp_adv);
	cmd->lp_advertising = ptys2ethtool_link_modes(eth_proto, ADVERTISED);

	/* TODO: once arch is done report ADVERTISE_Pause/ADVERTISE_Asym_Pause
	 * in cmd->lp_advertising according to QUERY_PORT.pause_lp_adv.
	 */
	cmd->lp_advertising |= (priv->port_state.flags & MLX4_EN_PORT_ANC) ?
			ADVERTISED_Autoneg : 0;

	cmd->phy_address = 0;
	cmd->mdio_support = 0;
	cmd->maxtxpkt = 0;
	cmd->maxrxpkt = 0;
	cmd->eth_tp_mdix = ETH_TP_MDI_INVALID;

#if defined(ETH_TP_MDI_AUTO)
	cmd->eth_tp_mdix_ctrl = ETH_TP_MDI_AUTO;
#endif
	return ret;
}

static int mlx4_en_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	int trans_type;
	int ret = 1;

	if (mlx4_en_QUERY_PORT(priv->mdev, priv->port))
		return -ENOMEM;
	en_dbg(DRV, priv, "query_port.flags (ANC) %x\n",
	       priv->port_state.flags & MLX4_EN_PORT_ANC);

	if (priv->mdev->dev->caps.flags2 & MLX4_DEV_CAP_FLAG2_ETH_PROT_CTRL)
		ret = ptys_get_ethtool_settings(dev, cmd);

	if (ret) { /* ETH PROT CRTL is not supported or PTYS CMD failed */
		/* SUPPORTED_1000baseT_Half isn't supported */
		cmd->supported = SUPPORTED_1000baseT_Full
				|SUPPORTED_10000baseT_Full;

		cmd->advertising = ADVERTISED_1000baseT_Full
				  |ADVERTISED_10000baseT_Full;

		cmd->supported |= SUPPORTED_1000baseKX_Full
				|SUPPORTED_10000baseKX4_Full
				|SUPPORTED_10000baseKR_Full
				|SUPPORTED_10000baseR_FEC
				|SUPPORTED_40000baseKR4_Full
				|SUPPORTED_40000baseCR4_Full
				|SUPPORTED_40000baseSR4_Full
				|SUPPORTED_40000baseLR4_Full;

		/* ADVERTISED_1000baseT_Half isn't advertised */
		cmd->advertising |= ADVERTISED_1000baseKX_Full
				  |ADVERTISED_10000baseKX4_Full
				  |ADVERTISED_10000baseKR_Full
				  |ADVERTISED_10000baseR_FEC
				  |ADVERTISED_40000baseKR4_Full
				  |ADVERTISED_40000baseCR4_Full
				  |ADVERTISED_40000baseSR4_Full
				  |ADVERTISED_40000baseLR4_Full;

		trans_type = priv->port_state.transceiver;
		if (trans_type > 0 && trans_type <= 0xC) {
			cmd->port = PORT_FIBRE;
			cmd->transceiver = XCVR_EXTERNAL;
			cmd->supported |= SUPPORTED_FIBRE;
			cmd->advertising |= ADVERTISED_FIBRE;
		} else if (trans_type == 0x80 || trans_type == 0) {
			cmd->port = PORT_TP;
			cmd->transceiver = XCVR_INTERNAL;
			cmd->supported |= SUPPORTED_TP;
			cmd->advertising |= ADVERTISED_TP;
		} else  {
			cmd->port = -1;
			cmd->transceiver = -1;
		}

		cmd->autoneg = mlx4_en_autoneg_get(dev);
		if (cmd->autoneg == AUTONEG_ENABLE) {
			cmd->supported |= SUPPORTED_Autoneg;
			cmd->advertising |= ADVERTISED_Autoneg;
		}
	}

	if (netif_carrier_ok(dev)) {
		ethtool_cmd_speed_set(cmd, priv->port_state.link_speed);
		cmd->duplex = DUPLEX_FULL;
	} else {
		ethtool_cmd_speed_set(cmd, -1);
		cmd->duplex = -1;
	}
	return 0;
}

static const char *mlx4_en_duplex_to_string(int duplex)
{
	switch (duplex) {
	case DUPLEX_FULL:
		return "FULL";
	case DUPLEX_HALF:
		return "HALF";
	default:
		break;
	}
	return "UNKNOWN";
}

static int mlx4_en_set_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_port_state *port_state = &priv->port_state;

	if ((cmd->autoneg != port_state->autoneg) ||
	    (ethtool_cmd_speed(cmd) != port_state->link_speed) ||
	    (cmd->duplex != DUPLEX_FULL)) {
		en_info(priv, "Changing port state properties (auto-negotiation"
			      " , speed/duplex) is not supported. Current:"
			      " auto-negotiation=%d speed/duplex=%d/%s\n",
			      port_state->autoneg, port_state->link_speed,
			      mlx4_en_duplex_to_string(DUPLEX_FULL));
		return -EOPNOTSUPP;
	}

	/* User provided same port state properties that are currently set.
	 * Nothing to change
	 */
	return 0;
}

static int mlx4_en_get_coalesce(struct net_device *dev,
			      struct ethtool_coalesce *coal)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);

	coal->tx_coalesce_usecs = priv->tx_usecs;
	coal->tx_max_coalesced_frames = priv->tx_frames;
	coal->rx_coalesce_usecs = priv->rx_usecs;
	coal->rx_max_coalesced_frames = priv->rx_frames;

	coal->pkt_rate_low = priv->pkt_rate_low;
	coal->rx_coalesce_usecs_low = priv->rx_usecs_low;
	coal->pkt_rate_high = priv->pkt_rate_high;
	coal->rx_coalesce_usecs_high = priv->rx_usecs_high;
	coal->rate_sample_interval = priv->sample_interval;
	coal->use_adaptive_rx_coalesce = priv->adaptive_rx_coal;
	return 0;
}

static int mlx4_en_set_coalesce(struct net_device *dev,
			      struct ethtool_coalesce *coal)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	int err, i;

	priv->rx_frames = (coal->rx_max_coalesced_frames ==
			   MLX4_EN_AUTO_CONF) ?
				MLX4_EN_RX_COAL_TARGET /
				priv->dev->mtu + 1 :
				coal->rx_max_coalesced_frames;
	priv->rx_usecs = (coal->rx_coalesce_usecs ==
			  MLX4_EN_AUTO_CONF) ?
				MLX4_EN_RX_COAL_TIME :
				coal->rx_coalesce_usecs;

	/* Setting TX coalescing parameters */
	if (coal->tx_coalesce_usecs != priv->tx_usecs ||
	    coal->tx_max_coalesced_frames != priv->tx_frames) {
		priv->tx_usecs = coal->tx_coalesce_usecs;
		priv->tx_frames = coal->tx_max_coalesced_frames;
		if (priv->port_up) {
			for (i = 0; i < priv->tx_ring_num; i++) {
				priv->tx_cq[i]->moder_cnt = priv->tx_frames;
				priv->tx_cq[i]->moder_time = priv->tx_usecs;
				if (mlx4_en_set_cq_moder(priv, priv->tx_cq[i]))
					en_warn(priv, "Failed changing moderation for TX cq %d\n", i);
			}
		}
	}

	/* Set adaptive coalescing params */
	priv->pkt_rate_low = coal->pkt_rate_low;
	priv->rx_usecs_low = coal->rx_coalesce_usecs_low;
	priv->pkt_rate_high = coal->pkt_rate_high;
	priv->rx_usecs_high = coal->rx_coalesce_usecs_high;
	priv->sample_interval = coal->rate_sample_interval;
	priv->adaptive_rx_coal = coal->use_adaptive_rx_coalesce;

	if (priv->port_up) {
		for (i = 0; i < priv->rx_ring_num; i++) {
			priv->rx_cq[i]->moder_cnt = priv->rx_frames;
			priv->rx_cq[i]->moder_time = priv->rx_usecs;
			if (priv->adaptive_rx_coal)
				continue;
			priv->last_moder_time[i] = priv->rx_usecs;
			err = mlx4_en_set_cq_moder(priv, priv->rx_cq[i]);
			if (err)
				return err;
		}
	}

	return 0;
}

static int mlx4_en_set_pauseparam(struct net_device *dev,
				struct ethtool_pauseparam *pause)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int err;

	if (pause->autoneg)
		return -EOPNOTSUPP;

	priv->prof->tx_pause = pause->tx_pause != 0;
	priv->prof->rx_pause = pause->rx_pause != 0;
	err = mlx4_SET_PORT_general(mdev->dev, priv->port,
				    priv->rx_skb_size + ETH_FCS_LEN,
				    priv->prof->tx_pause,
				    priv->prof->tx_ppp,
				    priv->prof->rx_pause,
				    priv->prof->rx_ppp);
	if (err)
		en_err(priv, "Failed setting pause params\n");

	return err;
}

static void mlx4_en_get_pauseparam(struct net_device *dev,
				 struct ethtool_pauseparam *pause)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);

	pause->tx_pause = priv->prof->tx_pause;
	pause->rx_pause = priv->prof->rx_pause;
	pause->autoneg = 0;
}

/* rtnl lock must be taken before calling */
int mlx4_en_pre_config(struct mlx4_en_priv *priv)
{
#ifdef CONFIG_RFS_ACCEL
#ifdef CONFIG_COMPAT_IS_NETDEV_EXTENDED
	if (!mlx4_en_rx_cpu_rmap(priv))
		return 0;

	rtnl_unlock();
	free_irq_cpu_rmap(mlx4_en_rx_cpu_rmap(priv));
	rtnl_lock();

	mlx4_en_rx_cpu_rmap(priv) = NULL;
#else
	struct cpu_rmap *rmap;

	if (!priv->dev->rx_cpu_rmap)
		return 0;

	/* Disable RFS events
	 * Must have all RFS jobs flushed before freeing resources
	 */
	rmap = priv->dev->rx_cpu_rmap;
	priv->dev->rx_cpu_rmap = NULL;

	rtnl_unlock();
	free_irq_cpu_rmap(rmap);
	rtnl_lock();

	if (priv->dev->rx_cpu_rmap)
		return -EBUSY; /* another configuration completed while lock
				* was free
				*/
#endif

	/* Make sure all currently running filter_work are being processed
	 * Other work will return immediatly because of disable_rfs
	 */
	flush_workqueue(priv->mdev->workqueue);

#endif

	return 0;
}

static int mlx4_en_set_ringparam(struct net_device *dev,
				 struct ethtool_ringparam *param)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	u32 rx_size, tx_size;
	int port_up = 0;
	int err = 0;
	int n_stats;
	u64 *data = NULL;

	if (!priv->port_up)
		return -ENOMEM;

	if (param->rx_jumbo_pending || param->rx_mini_pending)
		return -EINVAL;

	rx_size = roundup_pow_of_two(param->rx_pending);
	rx_size = max_t(u32, rx_size, MLX4_EN_MIN_RX_SIZE);
	rx_size = min_t(u32, rx_size, MLX4_EN_MAX_RX_SIZE);
	tx_size = roundup_pow_of_two(param->tx_pending);
	tx_size = max_t(u32, tx_size, MLX4_EN_MIN_TX_SIZE);
	tx_size = min_t(u32, tx_size, MLX4_EN_MAX_TX_SIZE);

	if (rx_size == (priv->port_up ? priv->rx_ring[0]->actual_size :
					priv->rx_ring[0]->size) &&
	    tx_size == priv->tx_ring[0]->size)
		return 0;
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

	priv->prof->tx_ring_size = tx_size;
	priv->prof->rx_ring_size = rx_size;

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
		if (err) {
			en_err(priv, "Failed starting port\n");
			goto out;
		}
	}

out:
	kfree(data);
	mutex_unlock(&mdev->state_lock);
	return err;
}

static void mlx4_en_get_ringparam(struct net_device *dev,
				  struct ethtool_ringparam *param)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);

	if (!priv->port_up)
		return;

	memset(param, 0, sizeof(*param));
	param->rx_max_pending = MLX4_EN_MAX_RX_SIZE;
	param->tx_max_pending = MLX4_EN_MAX_TX_SIZE;
	param->rx_pending = priv->port_up ?
		priv->rx_ring[0]->actual_size : priv->rx_ring[0]->size;
	param->tx_pending = priv->tx_ring[0]->size;
}

static void mlx4_en_decide_blueflame(struct mlx4_en_priv *priv, u32 data)
{
	int i;
	bool bf_enabled_new = !!(data & MLX4_EN_PRIV_FLAGS_BLUEFLAME);
	bool bf_enabled_old = !!(priv->pflags & MLX4_EN_PRIV_FLAGS_BLUEFLAME);

	if (bf_enabled_new == bf_enabled_old)
		return; /* Nothing to do */

	if (bf_enabled_new) {
		bool bf_supported = true;

		for (i = 0; i < priv->tx_ring_num; i++)
			bf_supported &= priv->tx_ring[i]->bf_alloced;

		if (!bf_supported) {
			en_err(priv, "BlueFlame is not supported\n");
			return;
		}

		priv->pflags |= MLX4_EN_PRIV_FLAGS_BLUEFLAME;
	} else {
		priv->pflags &= ~MLX4_EN_PRIV_FLAGS_BLUEFLAME;
	}

	for (i = 0; i < priv->tx_ring_num; i++)
		priv->tx_ring[i]->bf_enabled = bf_enabled_new;

	en_info(priv, "BlueFlame %s\n",
		bf_enabled_new ?  "Enabled" : "Disabled");
}

#ifndef CONFIG_COMPAT_INDIR_SETTING
static u32 mlx4_en_get_rxfh_indir_size(struct net_device *dev)
#else
u32 mlx4_en_get_rxfh_indir_size(struct net_device *dev)
#endif
{
	struct mlx4_en_priv *priv = netdev_priv(dev);

	return priv->rx_ring_num;
}

#ifndef CONFIG_COMPAT_INDIR_SETTING
static int mlx4_en_get_rxfh_indir(struct net_device *dev, u32 *ring_index)
#else
int mlx4_en_get_rxfh_indir(struct net_device *dev, u32 *ring_index)
#endif
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_rss_map *rss_map = &priv->rss_map;
	int rss_rings;
	size_t n = priv->rx_ring_num;
	int err = 0;

	rss_rings = priv->prof->rss_rings ?: priv->rx_ring_num;
	rss_rings = 1 << ilog2(rss_rings);

	while (n--) {
		ring_index[n] = rss_map->qps[n % rss_rings].qpn -
			rss_map->base_qpn;
	}

	return err;
}

#ifndef CONFIG_COMPAT_INDIR_SETTING
static int mlx4_en_set_rxfh_indir(struct net_device *dev,
		const u32 *ring_index)
#else
int mlx4_en_set_rxfh_indir(struct net_device *dev,
			   const u32 *ring_index)
#endif
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int port_up = 0;
	int err = 0;
	int i;
	int rss_rings = 0;

	/* Calculate RSS table size and make sure flows are spread evenly
	 * between rings
	 */
	for (i = 0; i < priv->rx_ring_num; i++) {
		if (i > 0 && !ring_index[i] && !rss_rings)
			rss_rings = i;

		if (ring_index[i] != (i % (rss_rings ?: priv->rx_ring_num)))
			return -EINVAL;
	}

	if (!rss_rings)
		rss_rings = priv->rx_ring_num;

	/* RSS table size must be an order of 2 */
	if (!is_power_of_2(rss_rings))
		return -EINVAL;

	mutex_lock(&mdev->state_lock);
	if (priv->port_up) {
		port_up = 1;
		mlx4_en_stop_port(dev);
	}

	priv->prof->rss_rings = rss_rings;

	if (port_up) {
		err = mlx4_en_start_port(dev);
		if (err)
			en_err(priv, "Failed starting port\n");
	}

	mutex_unlock(&mdev->state_lock);
	return err;
}

#define all_zeros_or_all_ones(field)		\
	((field) == 0 || (field) == (__force typeof(field))-1)

static int mlx4_en_validate_flow(struct net_device *dev,
				 struct mlx4_ethtool_rxnfc *cmd)
{
	struct ethtool_usrip4_spec *l3_mask;
	struct ethtool_tcpip4_spec *l4_mask;
	struct ethhdr *eth_mask;

	if (cmd->fs.location >= MAX_NUM_OF_FS_RULES)
		return -EINVAL;

	if (cmd->fs.flow_type & FLOW_MAC_EXT) {
		/* dest mac mask must be ff:ff:ff:ff:ff:ff */
		if (!is_broadcast_ether_addr(cmd->fs.m_ext.h_dest))
			return -EINVAL;
	}

	switch (cmd->fs.flow_type & ~(FLOW_EXT | FLOW_MAC_EXT)) {
	case TCP_V4_FLOW:
	case UDP_V4_FLOW:
		if (cmd->fs.m_u.tcp_ip4_spec.tos)
			return -EINVAL;
		l4_mask = &cmd->fs.m_u.tcp_ip4_spec;
		/* don't allow mask which isn't all 0 or 1 */
		if (!all_zeros_or_all_ones(l4_mask->ip4src) ||
		    !all_zeros_or_all_ones(l4_mask->ip4dst) ||
		    !all_zeros_or_all_ones(l4_mask->psrc) ||
		    !all_zeros_or_all_ones(l4_mask->pdst))
			return -EINVAL;
		break;
	case IP_USER_FLOW:
		l3_mask = &cmd->fs.m_u.usr_ip4_spec;
		if (l3_mask->l4_4_bytes || l3_mask->tos || l3_mask->proto ||
		    cmd->fs.h_u.usr_ip4_spec.ip_ver != ETH_RX_NFC_IP4 ||
		    (!l3_mask->ip4src && !l3_mask->ip4dst) ||
		    !all_zeros_or_all_ones(l3_mask->ip4src) ||
		    !all_zeros_or_all_ones(l3_mask->ip4dst))
			return -EINVAL;
		break;
	case ETHER_FLOW:
		eth_mask = &cmd->fs.m_u.ether_spec;
		/* source mac mask must not be set */
		if (!is_zero_ether_addr(eth_mask->h_source))
			return -EINVAL;

		/* dest mac mask must be ff:ff:ff:ff:ff:ff */
		if (!is_broadcast_ether_addr(eth_mask->h_dest))
			return -EINVAL;

		if (!all_zeros_or_all_ones(eth_mask->h_proto))
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}

	if ((cmd->fs.flow_type & FLOW_EXT)) {
		if (cmd->fs.m_ext.vlan_etype ||
		    !(cmd->fs.m_ext.vlan_tci == 0 ||
		      cmd->fs.m_ext.vlan_tci == cpu_to_be16(0xfff)))
			return -EINVAL;
		if (cmd->fs.m_ext.vlan_tci) {
			if (be16_to_cpu(cmd->fs.h_ext.vlan_tci) <
			    VLAN_MIN_VALUE ||
			    be16_to_cpu(cmd->fs.h_ext.vlan_tci) >
			    VLAN_MAX_VALUE)
				return -EINVAL;
		}
	}

	return 0;
}

static int mlx4_en_ethtool_add_mac_rule(struct mlx4_ethtool_rxnfc *cmd,
					struct list_head *rule_list_h,
					struct mlx4_spec_list *spec_l2,
					unsigned char *mac)
{
	int err = 0;
	__be64 mac_msk = cpu_to_be64(MLX4_MAC_MASK << 16);

	spec_l2->id = MLX4_NET_TRANS_RULE_ID_ETH;
	memcpy(spec_l2->eth.dst_mac_msk, &mac_msk, ETH_ALEN);
	memcpy(spec_l2->eth.dst_mac, mac, ETH_ALEN);

	if ((cmd->fs.flow_type & FLOW_EXT) && cmd->fs.m_ext.vlan_tci) {
		spec_l2->eth.vlan_id = cmd->fs.h_ext.vlan_tci;
		spec_l2->eth.vlan_id_msk = cpu_to_be16(0xfff);
	}

	list_add_tail(&spec_l2->list, rule_list_h);

	return err;
}

static int mlx4_en_ethtool_add_mac_rule_by_ipv4(struct mlx4_en_priv *priv,
						struct mlx4_ethtool_rxnfc *cmd,
						struct list_head *rule_list_h,
						struct mlx4_spec_list *spec_l2,
						__be32 ipv4_dst)
{
	unsigned char mac[ETH_ALEN];

	if (!ipv4_is_multicast(ipv4_dst)) {
		if (cmd->fs.flow_type & FLOW_MAC_EXT)
			memcpy(&mac, cmd->fs.h_ext.h_dest, ETH_ALEN);
		else
			memcpy(&mac, priv->dev->dev_addr, ETH_ALEN);
	} else {
		ip_eth_mc_map(ipv4_dst, mac);
	}

	return mlx4_en_ethtool_add_mac_rule(cmd, rule_list_h, spec_l2, &mac[0]);
}

static int add_ip_rule(struct mlx4_en_priv *priv,
				struct mlx4_ethtool_rxnfc *cmd,
				struct list_head *list_h)
{
	struct mlx4_spec_list *spec_l2 = NULL;
	struct mlx4_spec_list *spec_l3 = NULL;
	struct ethtool_usrip4_spec *l3_mask = &cmd->fs.m_u.usr_ip4_spec;

	spec_l3 = kzalloc(sizeof(*spec_l3), GFP_KERNEL);
	spec_l2 = kzalloc(sizeof(*spec_l2), GFP_KERNEL);
	if (!spec_l2 || !spec_l3) {
		en_err(priv, "Fail to alloc ethtool rule.\n");
		kfree(spec_l2);
		kfree(spec_l3);
		return -ENOMEM;
	}

	mlx4_en_ethtool_add_mac_rule_by_ipv4(priv, cmd, list_h, spec_l2,
					     cmd->fs.h_u.
					     usr_ip4_spec.ip4dst);
	spec_l3->id = MLX4_NET_TRANS_RULE_ID_IPV4;
	spec_l3->ipv4.src_ip = cmd->fs.h_u.usr_ip4_spec.ip4src;
	if (l3_mask->ip4src)
		spec_l3->ipv4.src_ip_msk = MLX4_BE_WORD_MASK;
	spec_l3->ipv4.dst_ip = cmd->fs.h_u.usr_ip4_spec.ip4dst;
	if (l3_mask->ip4dst)
		spec_l3->ipv4.dst_ip_msk = MLX4_BE_WORD_MASK;
	list_add_tail(&spec_l3->list, list_h);

	return 0;
}

static int add_tcp_udp_rule(struct mlx4_en_priv *priv,
			     struct mlx4_ethtool_rxnfc *cmd,
			     struct list_head *list_h, int proto)
{
	struct mlx4_spec_list *spec_l2 = NULL;
	struct mlx4_spec_list *spec_l3 = NULL;
	struct mlx4_spec_list *spec_l4 = NULL;
	struct ethtool_tcpip4_spec *l4_mask = &cmd->fs.m_u.tcp_ip4_spec;

	spec_l2 = kzalloc(sizeof(*spec_l2), GFP_KERNEL);
	spec_l3 = kzalloc(sizeof(*spec_l3), GFP_KERNEL);
	spec_l4 = kzalloc(sizeof(*spec_l4), GFP_KERNEL);
	if (!spec_l2 || !spec_l3 || !spec_l4) {
		en_err(priv, "Fail to alloc ethtool rule.\n");
		kfree(spec_l2);
		kfree(spec_l3);
		kfree(spec_l4);
		return -ENOMEM;
	}

	spec_l3->id = MLX4_NET_TRANS_RULE_ID_IPV4;

	if (proto == TCP_V4_FLOW) {
		mlx4_en_ethtool_add_mac_rule_by_ipv4(priv, cmd, list_h,
						     spec_l2,
						     cmd->fs.h_u.
						     tcp_ip4_spec.ip4dst);
		spec_l4->id = MLX4_NET_TRANS_RULE_ID_TCP;
		spec_l3->ipv4.src_ip = cmd->fs.h_u.tcp_ip4_spec.ip4src;
		spec_l3->ipv4.dst_ip = cmd->fs.h_u.tcp_ip4_spec.ip4dst;
		spec_l4->tcp_udp.src_port = cmd->fs.h_u.tcp_ip4_spec.psrc;
		spec_l4->tcp_udp.dst_port = cmd->fs.h_u.tcp_ip4_spec.pdst;
	} else {
		mlx4_en_ethtool_add_mac_rule_by_ipv4(priv, cmd, list_h,
						     spec_l2,
						     cmd->fs.h_u.
						     udp_ip4_spec.ip4dst);
		spec_l4->id = MLX4_NET_TRANS_RULE_ID_UDP;
		spec_l3->ipv4.src_ip = cmd->fs.h_u.udp_ip4_spec.ip4src;
		spec_l3->ipv4.dst_ip = cmd->fs.h_u.udp_ip4_spec.ip4dst;
		spec_l4->tcp_udp.src_port = cmd->fs.h_u.udp_ip4_spec.psrc;
		spec_l4->tcp_udp.dst_port = cmd->fs.h_u.udp_ip4_spec.pdst;
	}

	if (l4_mask->ip4src)
		spec_l3->ipv4.src_ip_msk = MLX4_BE_WORD_MASK;
	if (l4_mask->ip4dst)
		spec_l3->ipv4.dst_ip_msk = MLX4_BE_WORD_MASK;

	if (l4_mask->psrc)
		spec_l4->tcp_udp.src_port_msk = MLX4_BE_SHORT_MASK;
	if (l4_mask->pdst)
		spec_l4->tcp_udp.dst_port_msk = MLX4_BE_SHORT_MASK;

	list_add_tail(&spec_l3->list, list_h);
	list_add_tail(&spec_l4->list, list_h);

	return 0;
}

static int mlx4_en_ethtool_to_net_trans_rule(struct net_device *dev,
					     struct mlx4_ethtool_rxnfc *cmd,
					     struct list_head *rule_list_h)
{
	int err;
	struct ethhdr *eth_spec;
	struct mlx4_spec_list *spec_l2;
	struct mlx4_en_priv *priv = netdev_priv(dev);

	err = mlx4_en_validate_flow(dev, cmd);
	if (err)
		return err;

	switch (cmd->fs.flow_type & ~(FLOW_EXT | FLOW_MAC_EXT)) {
	case ETHER_FLOW:
		spec_l2 = kzalloc(sizeof(*spec_l2), GFP_KERNEL);
		if (!spec_l2)
			return -ENOMEM;

		eth_spec = &cmd->fs.h_u.ether_spec;
		mlx4_en_ethtool_add_mac_rule(cmd, rule_list_h, spec_l2, &eth_spec->h_dest[0]);
		spec_l2->eth.ether_type = eth_spec->h_proto;
		if (eth_spec->h_proto)
			spec_l2->eth.ether_type_enable = 1;
		break;
	case IP_USER_FLOW:
		err = add_ip_rule(priv, cmd, rule_list_h);
		break;
	case TCP_V4_FLOW:
		err = add_tcp_udp_rule(priv, cmd, rule_list_h, TCP_V4_FLOW);
		break;
	case UDP_V4_FLOW:
		err = add_tcp_udp_rule(priv, cmd, rule_list_h, UDP_V4_FLOW);
		break;
	}

	return err;
}

static int mlx4_en_flow_replace(struct net_device *dev,
				struct mlx4_ethtool_rxnfc *cmd)
{
	int err;
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	struct ethtool_flow_id *loc_rule;
	struct mlx4_spec_list *spec, *tmp_spec;
	u32 qpn;
	u64 reg_id;

	struct mlx4_net_trans_rule rule = {
		.queue_mode = MLX4_NET_TRANS_Q_FIFO,
		.exclusive = 0,
		.allow_loopback = 1,
		.promisc_mode = MLX4_FS_REGULAR,
	};

	rule.port = priv->port;
	rule.priority = MLX4_DOMAIN_ETHTOOL | cmd->fs.location;
	INIT_LIST_HEAD(&rule.list);

	/* Allow direct QP attaches if the EN_ETHTOOL_QP_ATTACH flag is set */
	if (cmd->fs.ring_cookie == RX_CLS_FLOW_DISC)
		qpn = priv->drop_qp.qpn;
	else if (cmd->fs.ring_cookie & EN_ETHTOOL_QP_ATTACH) {
		qpn = cmd->fs.ring_cookie & (EN_ETHTOOL_QP_ATTACH - 1);
	} else {
		if (cmd->fs.ring_cookie >= priv->rx_ring_num) {
			en_warn(priv, "rxnfc: RX ring (%llu) doesn't exist.\n",
				cmd->fs.ring_cookie);
			return -EINVAL;
		}
		qpn = priv->rss_map.qps[cmd->fs.ring_cookie].qpn;
		if (!qpn) {
			en_warn(priv, "rxnfc: RX ring (%llu) is inactive.\n",
				cmd->fs.ring_cookie);
			return -EINVAL;
		}
	}
	rule.qpn = qpn;
	err = mlx4_en_ethtool_to_net_trans_rule(dev, cmd, &rule.list);
	if (err)
		goto out_free_list;

	mutex_lock(&mdev->state_lock);
	loc_rule = &priv->ethtool_rules[cmd->fs.location];
	if (loc_rule->id) {
		err = mlx4_flow_detach(priv->mdev->dev, loc_rule->id);
		if (err) {
			en_err(priv, "Fail to detach network rule at location %d. registration id = %llx\n",
			       cmd->fs.location, loc_rule->id);
			goto unlock;
		}
		loc_rule->id = 0;
		memset(&loc_rule->flow_spec, 0,
		       sizeof(struct ethtool_rx_flow_spec));
		list_del(&loc_rule->list);
	}
	err = mlx4_flow_attach(priv->mdev->dev, &rule, &reg_id);
	if (err) {
		en_err(priv, "Fail to attach network rule at location %d.\n",
		       cmd->fs.location);
		goto unlock;
	}
	loc_rule->id = reg_id;
	memcpy(&loc_rule->flow_spec, &cmd->fs,
	       sizeof(struct ethtool_rx_flow_spec));
	list_add_tail(&loc_rule->list, &priv->ethtool_list);

unlock:
	mutex_unlock(&mdev->state_lock);
out_free_list:
	list_for_each_entry_safe(spec, tmp_spec, &rule.list, list) {
		list_del(&spec->list);
		kfree(spec);
	}
	return err;
}

static int mlx4_en_flow_detach(struct net_device *dev,
			       struct mlx4_ethtool_rxnfc *cmd)
{
	int err = 0;
	struct ethtool_flow_id *rule;
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;

	if (cmd->fs.location >= MAX_NUM_OF_FS_RULES)
		return -EINVAL;

	mutex_lock(&mdev->state_lock);
	rule = &priv->ethtool_rules[cmd->fs.location];
	if (!rule->id) {
		err =  -ENOENT;
		goto out;
	}

	err = mlx4_flow_detach(priv->mdev->dev, rule->id);
	if (err) {
		en_err(priv, "Fail to detach network rule at location %d. registration id = 0x%llx\n",
		       cmd->fs.location, rule->id);
		goto out;
	}
	rule->id = 0;
	memset(&rule->flow_spec, 0, sizeof(struct ethtool_rx_flow_spec));

	list_del(&rule->list);
out:
	mutex_unlock(&mdev->state_lock);
	return err;

}

static int mlx4_en_get_flow(struct net_device *dev, struct mlx4_ethtool_rxnfc *cmd,
			    int loc)
{
	int err = 0;
	struct ethtool_flow_id *rule;
	struct mlx4_en_priv *priv = netdev_priv(dev);

	if (loc < 0 || loc >= MAX_NUM_OF_FS_RULES)
		return -EINVAL;

	rule = &priv->ethtool_rules[loc];
	if (rule->id)
		memcpy(&cmd->fs, &rule->flow_spec,
		       sizeof(struct ethtool_rx_flow_spec));
	else
		err = -ENOENT;

	return err;
}

static int mlx4_en_get_num_flows(struct mlx4_en_priv *priv)
{

	int i, res = 0;
	for (i = 0; i < MAX_NUM_OF_FS_RULES; i++) {
		if (priv->ethtool_rules[i].id)
			res++;
	}
	return res;

}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0))
static int mlx4_en_get_rxnfc(struct net_device *dev, struct ethtool_rxnfc *c,
			     u32 *rule_locs)
#else
static int mlx4_en_get_rxnfc(struct net_device *dev, struct ethtool_rxnfc *c,
			     void *rule_locs)
#endif
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int err = 0;
	int i = 0, priority = 0;
	struct mlx4_ethtool_rxnfc *cmd = (struct mlx4_ethtool_rxnfc *)c;

	if ((cmd->cmd == ETHTOOL_GRXCLSRLCNT ||
	     cmd->cmd == ETHTOOL_GRXCLSRULE ||
	     cmd->cmd == ETHTOOL_GRXCLSRLALL) &&
	    (mdev->dev->caps.steering_mode !=
	     MLX4_STEERING_MODE_DEVICE_MANAGED || !priv->port_up))
		return -EINVAL;

	switch (cmd->cmd) {
	case ETHTOOL_GRXRINGS:
		cmd->data = priv->rx_ring_num;
		break;
	case ETHTOOL_GRXCLSRLCNT:
		cmd->rule_cnt = mlx4_en_get_num_flows(priv);
		break;
	case ETHTOOL_GRXCLSRULE:
		err = mlx4_en_get_flow(dev, cmd, cmd->fs.location);
		break;
	case ETHTOOL_GRXCLSRLALL:
		while ((!err || err == -ENOENT) && priority < cmd->rule_cnt) {
			err = mlx4_en_get_flow(dev, cmd, i);
			if (!err) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0))
				rule_locs[priority++] = i;
#else
				((u32 *)(rule_locs))[priority++] = i;
#endif
			}
			i++;
		}
		err = 0;
		break;
	default:
		err = -EOPNOTSUPP;
		break;
	}

	return err;
}

static int mlx4_en_set_rxnfc(struct net_device *dev, struct ethtool_rxnfc *c)
{
	int err = 0;
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_ethtool_rxnfc *cmd = (struct mlx4_ethtool_rxnfc *)c;

	if (mdev->dev->caps.steering_mode !=
	    MLX4_STEERING_MODE_DEVICE_MANAGED || !priv->port_up)
		return -EINVAL;

	switch (cmd->cmd) {
	case ETHTOOL_SRXCLSRLINS:
		err = mlx4_en_flow_replace(dev, cmd);
		break;
	case ETHTOOL_SRXCLSRLDEL:
		err = mlx4_en_flow_detach(dev, cmd);
		break;
	default:
		en_warn(priv, "Unsupported ethtool command. (%d)\n", cmd->cmd);
		return -EINVAL;
	}

	return err;
}

void mlx4_en_remove_ethtool_rules(struct mlx4_en_priv *priv)
{
	struct ethtool_flow_id *flow, *tmp_flow;
	struct mlx4_dev *device = priv->mdev->dev;

	if (device->caps.steering_mode != MLX4_STEERING_MODE_DEVICE_MANAGED)
		return;

	list_for_each_entry_safe(flow, tmp_flow, &priv->ethtool_list, list) {
		mlx4_flow_detach(device, flow->id);
		list_del(&flow->list);
	}
}

#ifndef CONFIG_COMPAT_NUM_CHANNELS
static void mlx4_en_get_channels(struct net_device *dev,
				 struct ethtool_channels *channel)
#else
void mlx4_en_get_channels(struct net_device *dev,
			  struct ethtool_channels *channel)
#endif
{
	struct mlx4_en_priv *priv = netdev_priv(dev);

	memset(channel, 0, sizeof(*channel));

	channel->max_rx = MAX_RX_RINGS;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)) || defined (CONFIG_COMPAT_IS_NUM_TX_QUEUES)
	channel->max_tx = MLX4_EN_MAX_TX_RING_P_UP;
#else
	channel->max_tx = MLX4_EN_NUM_TX_RINGS * 2;
#endif

	channel->rx_count = priv->rx_ring_num;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)) || defined (CONFIG_COMPAT_IS_NUM_TX_QUEUES)
	channel->tx_count = priv->tx_ring_num / MLX4_EN_NUM_UP;
#else
	channel->tx_count = priv->tx_ring_num -
			    (!!priv->prof->rx_ppp) * MLX4_EN_NUM_PPP_RINGS;
#endif
}

#ifndef CONFIG_COMPAT_NUM_CHANNELS
static int mlx4_en_set_channels(struct net_device *dev,
				struct ethtool_channels *channel)
#else
int mlx4_en_set_channels(struct net_device *dev,
			 struct ethtool_channels *channel)
#endif
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int port_up = 0;
	int err = 0;

	if (channel->other_count || channel->combined_count ||
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)) || defined (CONFIG_COMPAT_IS_NUM_TX_QUEUES)
	    channel->tx_count > MLX4_EN_MAX_TX_RING_P_UP ||
#else
	    channel->tx_count > MLX4_EN_NUM_TX_RINGS * 2 ||
#endif
	    channel->rx_count > MAX_RX_RINGS ||
	    !channel->tx_count || !channel->rx_count)
		return -EINVAL;

	err = mlx4_en_pre_config(priv);
	if (err)
		return err;

	mutex_lock(&mdev->state_lock);
	if (priv->port_up) {
		port_up = 1;
		mlx4_en_stop_port(dev);
	}

	mlx4_en_free_resources(priv);

	priv->num_tx_rings_p_up = channel->tx_count;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)) || defined (CONFIG_COMPAT_IS_NUM_TX_QUEUES)
	priv->tx_ring_num = channel->tx_count * MLX4_EN_NUM_UP;
#else
	priv->tx_ring_num = channel->tx_count +
			    (!!priv->prof->rx_ppp) * MLX4_EN_NUM_PPP_RINGS;
#endif
	priv->rx_ring_num = channel->rx_count;

	err = mlx4_en_alloc_resources(priv);
	if (err) {
		en_err(priv, "Failed reallocating port resources\n");
		goto out;
	}

#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)) || defined (CONFIG_COMPAT_IS_NUM_TX_QUEUES)) && \
	!defined (CONFIG_COMPAT_DISABLE_REAL_NUM_TXQ)
	netif_set_real_num_tx_queues(dev, priv->tx_ring_num);
#else
	dev->real_num_tx_queues = priv->tx_ring_num;
#endif
	netif_set_real_num_rx_queues(dev, priv->rx_ring_num);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)) || defined (CONFIG_COMPAT_NEW_TX_RING_SCHEME)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39))
	if (dev->num_tc)
#else
	if (netdev_get_num_tc(dev))
#endif
		mlx4_en_setup_tc(dev, MLX4_EN_NUM_UP);
#endif

	en_warn(priv, "Using %d TX rings\n", priv->tx_ring_num);
	en_warn(priv, "Using %d RX rings\n", priv->rx_ring_num);

	if (port_up) {
		err = mlx4_en_start_port(dev);
		if (err)
			en_err(priv, "Failed starting port\n");
	}

out:
	mutex_unlock(&mdev->state_lock);
	return err;
}

#ifdef CONFIG_TIMESTAMP_ETHTOOL
static int mlx4_en_get_ts_info(struct net_device *dev,
			       struct ethtool_ts_info *info)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int ret;

	ret = ethtool_op_get_ts_info(dev, info);
	if (ret)
		return ret;

	if (mdev->dev->caps.flags2 & MLX4_DEV_CAP_FLAG2_TS) {
		info->so_timestamping |=
			SOF_TIMESTAMPING_TX_HARDWARE |
			SOF_TIMESTAMPING_RX_HARDWARE |
			SOF_TIMESTAMPING_RAW_HARDWARE;

		info->tx_types =
			(1 << HWTSTAMP_TX_OFF) |
			(1 << HWTSTAMP_TX_ON);

		info->rx_filters =
			(1 << HWTSTAMP_FILTER_NONE) |
			(1 << HWTSTAMP_FILTER_ALL);

#if defined (CONFIG_COMPAT_PTP_CLOCK) && (defined (CONFIG_PTP_1588_CLOCK) || defined (CONFIG_PTP_1588_CLOCK_MODULE))
		if (mdev->ptp_clock)
			info->phc_index = ptp_clock_index(mdev->ptp_clock);
#endif
	}

	return ret;
}
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39))
int mlx4_en_set_flags(struct net_device *dev, u32 data)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);

	if ((data & NETIF_F_HW_VLAN_RX) &&
	    !(dev->features & NETIF_F_HW_VLAN_RX)) {
		priv->config.flags |= MLX4_EN_RX_VLAN_OFFLOAD; /* Turn ON RX vlan strip offload */
		en_info(priv, "Turn ON RX vlan strip offload\n");
		mlx4_en_reset_config(dev);
	} else if (!(data & NETIF_F_HW_VLAN_RX) &&
		   (dev->features & NETIF_F_HW_VLAN_RX)) {
		priv->config.flags &= ~MLX4_EN_RX_VLAN_OFFLOAD; /* Turn OFF RX vlan strip offload */
		en_info(priv, "Turn OFF RX vlan strip offload\n");
		mlx4_en_reset_config(dev);
	}

	if (data & ETH_FLAG_LRO)
		dev->features |= NETIF_F_LRO;
	else
		dev->features &= ~NETIF_F_LRO;

	return 0;
}

u32 mlx4_en_get_flags(struct net_device *dev)
{
	return ethtool_op_get_flags(dev) | (dev->features & NETIF_F_HW_VLAN_RX);
}
#endif

static u32 mlx4_en_get_priv_flags(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);

	return priv->pflags;
}

static int mlx4_en_set_priv_flags(struct net_device *dev, u32 data)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int restart_driver = 0;

	if ((data ^ priv->pflags) &
	    (MLX4_EN_PRIV_FLAGS_FS_EN_L2	|
	     MLX4_EN_PRIV_FLAGS_FS_EN_IPV4	|
	     MLX4_EN_PRIV_FLAGS_FS_EN_TCP	|
	     MLX4_EN_PRIV_FLAGS_FS_EN_UDP))
		return -EINVAL;

	if (data & MLX4_EN_PRIV_FLAGS_PM_QOS)
		priv->pflags |= MLX4_EN_PRIV_FLAGS_PM_QOS;

	/* User wants to disable the pm_qos request */
	else {
		mutex_lock(&mdev->state_lock);
		priv->pflags &= ~MLX4_EN_PRIV_FLAGS_PM_QOS;
		if (priv->last_cpu_dma_latency == 0) {
			en_dbg(DRV, priv, "PM Qos Update to Default\n");
			pm_qos_update_request(&priv->pm_qos_req,
					      PM_QOS_DEFAULT_VALUE);
			priv->last_cpu_dma_latency = PM_QOS_DEFAULT_VALUE;
		}
		mutex_unlock(&mdev->state_lock);
	}

	if ((data & MLX4_EN_PRIV_FLAGS_RSS_HASH_XOR) &&
	    !(priv->pflags & MLX4_EN_PRIV_FLAGS_RSS_HASH_XOR)) {
		priv->pflags |= MLX4_EN_PRIV_FLAGS_RSS_HASH_XOR;
#ifdef CONFIG_COMPAT_NETIF_F_RXHASH
		dev->features &= ~NETIF_F_RXHASH;
#endif
		restart_driver = 1;

	} else if (!(data & MLX4_EN_PRIV_FLAGS_RSS_HASH_XOR) &&
		   (priv->pflags & MLX4_EN_PRIV_FLAGS_RSS_HASH_XOR)) {
		priv->pflags &= ~MLX4_EN_PRIV_FLAGS_RSS_HASH_XOR;
#ifdef CONFIG_COMPAT_NETIF_F_RXHASH
		dev->features |= NETIF_F_RXHASH;
#endif
		restart_driver = 1;
	}

#ifndef CONFIG_COMPAT_DISABLE_DCB
#ifdef CONFIG_MLX4_EN_DCB
	if ((data & MLX4_EN_PRIV_FLAGS_DISABLE_32_14_4_E) &&
	    !(priv->pflags & MLX4_EN_PRIV_FLAGS_DISABLE_32_14_4_E)) {
		if (mlx4_disable_32_14_4_e_write(mdev->dev, 1, priv->port)) {
			en_err(priv, "Failed configure QCN parameter\n");
		} else {
			priv->pflags |= MLX4_EN_PRIV_FLAGS_DISABLE_32_14_4_E;
		}

	} else if (!(data & MLX4_EN_PRIV_FLAGS_DISABLE_32_14_4_E) &&
		   (priv->pflags & MLX4_EN_PRIV_FLAGS_DISABLE_32_14_4_E)) {
			if (mlx4_disable_32_14_4_e_write(mdev->dev, 0, priv->port)) {
				en_err(priv, "Failed configure QCN parameter\n");
			} else {
				priv->pflags &= ~MLX4_EN_PRIV_FLAGS_DISABLE_32_14_4_E;
			}
	}
#endif
#endif
	mlx4_en_decide_blueflame(priv, data);

	mutex_lock(&mdev->state_lock);
	if (restart_driver && priv->port_up) {
		mlx4_en_stop_port(dev);
		if (mlx4_en_start_port(dev))
			en_err(priv, "Failed restart port %d\n", priv->port);
	}
	mutex_unlock(&mdev->state_lock);

	return !(data == priv->pflags);
}

#ifdef CONFIG_MODULE_EEPROM_ETHTOOL
static int mlx4_en_get_module_info(struct net_device *dev,
				   struct ethtool_modinfo *modinfo)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int ret;
	u8 data[4];

	ret = mlx4_get_module_info(mdev->dev, priv->port, I2C_ADDR_LOW,
				   0/*offset*/, 4/*size*/, data);
	if (ret)
		return -EIO;

	switch (data[0] /* identifier */) {
	case MLX4_MODULE_ID_QSFP:
		modinfo->type = ETH_MODULE_SFF_8436;
		modinfo->eeprom_len = ETH_MODULE_SFF_8436_LEN;
		break;
	case MLX4_MODULE_ID_QSFP_PLUS:
		if (data[1] >= 0x3) { /* revision id */
			modinfo->type = ETH_MODULE_SFF_8636;
			modinfo->eeprom_len = ETH_MODULE_SFF_8636_LEN;
		} else {
			modinfo->type = ETH_MODULE_SFF_8436;
			modinfo->eeprom_len = ETH_MODULE_SFF_8436_LEN;
		}
		break;
	case MLX4_MODULE_ID_QSFP28:
		modinfo->type = ETH_MODULE_SFF_8636;
		modinfo->eeprom_len = ETH_MODULE_SFF_8636_LEN;
		break;
	case MLX4_MODULE_ID_SFP:
		modinfo->type = ETH_MODULE_SFF_8472;
		modinfo->eeprom_len = ETH_MODULE_SFF_8472_LEN;
		break;
	default:
		return -ENOSYS;
	}
	return 0;
}

static int mlx4_en_get_module_eeprom(struct net_device *dev,
				     struct ethtool_eeprom *ee,
				     u8 *data)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;

	int offset = ee->offset;
	int i = 0, ret;

	if (ee->len == 0)
		return -EINVAL;

	memset(data, 0, ee->len);

	while (i < ee->len) {
		u8 i2c_addr = I2C_ADDR_LOW;
		int read_offset = offset;
		u16 bytes_to_read  = ee->len - i >= MLX4_MODULE_INFO_MAX_READ ?
				MLX4_MODULE_INFO_MAX_READ :  ee->len - i;

		if (offset < I2C_PAGE_SIZE &&
		    offset + bytes_to_read > I2C_PAGE_SIZE)
			/* Cross pages reads are not allowed
			 * read until offset 256 in low page
			 * next round offset will be 256 (offset 0 in high page)
			 */
			bytes_to_read -= offset + bytes_to_read - I2C_PAGE_SIZE;

		if (offset >= I2C_PAGE_SIZE) { /* reset offset to high page */
			i2c_addr = I2C_ADDR_HIGH;
			read_offset = offset - I2C_PAGE_SIZE;
		}

		en_dbg(DRV, priv,
		       "mlx4_get_module_info i(%d) i2c_addr(0x%x) offset(%d) bytes_to_read(%d)\n",
			i, i2c_addr, offset, bytes_to_read);
		ret = mlx4_get_module_info(mdev->dev, priv->port, i2c_addr,
					   read_offset, bytes_to_read, data+i);
		if (ret) {
			if (i2c_addr == I2C_ADDR_HIGH &&
			    MAD_STATUS_2_CABLE_ERR(ret) == CABLE_INF_I2C_ADDR)
				/* Some SFP cables do not support i2c slave
				 * address 0x51, abort silently.
				 */
				return 0;

			en_err(priv,
			       "mlx4_get_module_info i(%d) i2c_addr(0x%x) offset(%d) bytes_to_read(%d) - FAILED (0x%x)\n",
			       i, i2c_addr, read_offset, bytes_to_read, ret);
			return 0;
		}

		offset += bytes_to_read;
		i += bytes_to_read;
	}
	return 0;
}
#endif

const struct ethtool_ops mlx4_en_ethtool_ops = {
	.get_drvinfo = mlx4_en_get_drvinfo,
	.get_settings = mlx4_en_get_settings,
	.set_settings = mlx4_en_set_settings,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0))
#ifdef NETIF_F_TSO
	.get_tso = mlx4_en_get_tso,
	.set_tso = mlx4_en_set_tso,
#endif
	.get_sg = ethtool_op_get_sg,
	.set_sg = ethtool_op_set_sg,
	.get_rx_csum = mlx4_en_get_rx_csum,
	.set_rx_csum = mlx4_en_set_rx_csum,
	.get_tx_csum = ethtool_op_get_tx_csum,
	.set_tx_csum = ethtool_op_set_tx_ipv6_csum,
#endif
	.get_link = ethtool_op_get_link,
	.get_strings = mlx4_en_get_strings,
	.get_sset_count = mlx4_en_get_sset_count,
	.get_ethtool_stats = mlx4_en_get_ethtool_stats,
	.self_test = mlx4_en_self_test,
	.get_wol = mlx4_en_get_wol,
	.set_wol = mlx4_en_set_wol,
	.get_msglevel = mlx4_en_get_msglevel,
	.set_msglevel = mlx4_en_set_msglevel,
	.get_coalesce = mlx4_en_get_coalesce,
	.set_coalesce = mlx4_en_set_coalesce,
	.get_pauseparam = mlx4_en_get_pauseparam,
	.set_pauseparam = mlx4_en_set_pauseparam,
	.get_ringparam = mlx4_en_get_ringparam,
	.set_ringparam = mlx4_en_set_ringparam,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39))
	.get_flags = mlx4_en_get_flags,
	.set_flags = mlx4_en_set_flags,
#endif
	.get_rxnfc = mlx4_en_get_rxnfc,
	.set_rxnfc = mlx4_en_set_rxnfc,
#ifndef CONFIG_COMPAT_INDIR_SETTING
	.get_rxfh_indir_size = mlx4_en_get_rxfh_indir_size,
	.get_rxfh_indir = mlx4_en_get_rxfh_indir,
	.set_rxfh_indir = mlx4_en_set_rxfh_indir,
#endif
	.get_priv_flags = mlx4_en_get_priv_flags,
	.set_priv_flags = mlx4_en_set_priv_flags,
#ifdef CONFIG_COMPAT_ETHTOOL_OPS_EXT
};

const struct ethtool_ops_ext mlx4_en_ethtool_ops_ext = {
	.size = sizeof(mlx4_en_ethtool_ops_ext),
#endif
#ifndef CONFIG_COMPAT_NUM_CHANNELS
	.get_channels = mlx4_en_get_channels,
	.set_channels = mlx4_en_set_channels,
#endif
#ifdef CONFIG_TIMESTAMP_ETHTOOL
	.get_ts_info = mlx4_en_get_ts_info,
#endif
#ifdef CONFIG_MODULE_EEPROM_ETHTOOL
	.get_module_info = mlx4_en_get_module_info,
	.get_module_eeprom = mlx4_en_get_module_eeprom,
#endif
};





