/*
 * Copyright 2012  Luis R. Rodriguez <mcgrof@do-not-panic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Compatibility file for Linux wireless for kernels 3.9.
 */

#include <linux/export.h>
#include <linux/compat.h>
#include <linux/netdevice.h>
#include <net/sock.h>
#include <linux/jhash.h>

#ifndef CONFIG_COMPAT_NETIF_HAS_PICK_TX

static inline int get_xps_queue(struct net_device *dev, struct sk_buff *skb)
{
	int queue_index = -1;
#ifdef CONFIG_COMPAT_NETIF_IS_XPS
	struct xps_dev_maps *dev_maps;
	struct xps_map *map;

	rcu_read_lock();
#ifdef CONFIG_COMPAT_IS_NETDEV_EXTENDED
	dev_maps = rcu_dereference(netdev_extended(dev)->xps_maps);
#else
	dev_maps = rcu_dereference(dev->xps_maps);
#endif

	if (dev_maps) {
		map = rcu_dereference(
		      dev_maps->cpu_map[raw_smp_processor_id()]);
		if (map) {
			if (map->len == 1)
				queue_index = map->queues[0];
			else {
				u32 hash;
				if (skb->sk && skb->sk->sk_hash)
					hash = skb->sk->sk_hash;
				else
					hash = (__force u16) skb->protocol ^
						skb->rxhash;
				hash = jhash_1word(hash, mlx4_en_hashrnd);
				queue_index = map->queues[
					((u64)hash * map->len) >> 32];
			}
			if (unlikely(queue_index >= dev->real_num_tx_queues))
				queue_index = -1;
		}
	}
	rcu_read_unlock();
#endif /* CONFIG_COMPAT_NETIF_IS_XPS */
	return queue_index;
}

#define __netdev_pick_tx LINUX_BACKPORT(__netdev_pick_tx)
u16 __netdev_pick_tx(struct net_device *dev, struct sk_buff *skb)
{
	int new_index;
#ifdef CONFIG_COMPAT_SOCK_HAS_QUEUE
	struct sock *sk = skb->sk;
	int queue_index = sk_tx_queue_get(sk);

	if (queue_index >= 0 && queue_index < dev->real_num_tx_queues) {
#ifdef CONFIG_COMPAT_NETIF_IS_XPS
		if (!skb->ooo_okay)
#endif /* CONFIG_COMPAT_NETIF_IS_XPS */
			return queue_index;
	}
#endif /* CONFIG_COMPAT_SOCK_HAS_QUEUE */

	new_index = get_xps_queue(dev, skb);
	if (new_index < 0)
		new_index = skb_tx_hash(dev, skb);

#ifdef CONFIG_COMPAT_SOCK_HAS_QUEUE
	if (queue_index != new_index && sk) {
		struct dst_entry *dst = rcu_dereference(sk->sk_dst_cache);
		if (dst && skb_dst(skb) == dst)
			sk_tx_queue_set(sk, new_index);
	}
#endif /* CONFIG_COMPAT_SOCK_HAS_QUEUE */

	return new_index;
}
EXPORT_SYMBOL(__netdev_pick_tx);

#endif /* CONFIG_COMPAT_NETIF_HAS_PICK_TX */

#ifndef CONFIG_COMPAT_NETIF_HAS_SET_XPS_QUEUE

#define netif_set_xps_queue LINUX_BACKPORT(netif_set_xps_queue)
int netif_set_xps_queue(struct net_device *dev, struct cpumask *msk, u16 idx)
{
#ifdef CONFIG_COMPAT_NETIF_IS_XPS
	int i, len, err;
	char buf[MAX_XPS_BUFFER_SIZE];
	struct attribute *attr = NULL;
	struct kobj_type *ktype = NULL;
	struct mlx4_en_netq_attribute *xps_attr = NULL;
	struct netdev_queue *txq = netdev_get_tx_queue(dev, idx);

#ifdef CONFIG_COMPAT_IS_NETDEV_EXTENDED
	struct netdev_tx_queue_extended *txq_ext =
					netdev_extended(dev)->_tx_ext + idx;
	ktype = txq_ext->kobj.ktype;
#else /* CONFIG_COMPAT_IS_NETDEV_EXTENDED */
	ktype = txq->kobj.ktype;
#endif /* CONFIG_COMPAT_IS_NETDEV_EXTENDED */
	if (!ktype)
		return -ENOMEM;

	for (i = 0; (attr = ktype->default_attrs[i]); i++) {
		if (!strcmp("xps_cpus", attr->name))
			break;
	}
	if (!attr)
		return -EINVAL;

	len = bitmap_scnprintf(buf, MAX_XPS_BUFFER_SIZE,
			       cpumask_bits(msk), MAX_XPS_CPUS);
	if (!len)
		return -ENOMEM;

	xps_attr = to_netq_attr(attr);
	err = xps_attr->store(txq, xps_attr, buf, len);
	if (err)
		return -EINVAL;

	return 0;
#else /* CONFIG_COMPAT_NETIF_IS_XPS */
	return -1;
#endif /* CONFIG_COMPAT_NETIF_IS_XPS */
}
EXPORT_SYMBOL(netif_set_xps_queue);
#endif /* CONFIG_COMPAT_NETIF_HAS_SET_XPS_QUEUE */

