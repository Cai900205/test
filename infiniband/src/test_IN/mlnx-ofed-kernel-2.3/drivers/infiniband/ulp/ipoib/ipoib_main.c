/*
 * Copyright (c) 2004 Topspin Communications.  All rights reserved.
 * Copyright (c) 2005 Sun Microsystems, Inc. All rights reserved.
 * Copyright (c) 2004 Voltaire, Inc. All rights reserved.
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

#include "ipoib.h"

#include <linux/module.h>

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>

#include <linux/if_arp.h>	/* For ARPHRD_xxx */

#include <linux/ip.h>
#include <linux/in.h>

#include <linux/jhash.h>
#include <net/arp.h>
#include <net/dst.h>

const char ipoib_driver_version[] = DRV_VERSION "_"DRV_RELDATE;

MODULE_AUTHOR("Roland Dreier");
MODULE_DESCRIPTION("IP-over-InfiniBand net driver");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(DRV_VERSION "_"DRV_RELDATE);

int ipoib_sendq_size __read_mostly = IPOIB_TX_RING_SIZE;
int ipoib_recvq_size __read_mostly = IPOIB_RX_RING_SIZE;

module_param_named(send_queue_size, ipoib_sendq_size, int, 0444);
MODULE_PARM_DESC(send_queue_size, "Number of descriptors in send queue (default = 512) (2-8192)");
module_param_named(recv_queue_size, ipoib_recvq_size, int, 0444);
MODULE_PARM_DESC(recv_queue_size, "Number of descriptors in receive queue (default = 512) (2-8192)");

#ifdef CONFIG_COMPAT_LRO_ENABLED
static int lro = 1;
module_param_named(lro, lro, int, 0444);
MODULE_PARM_DESC(lro,  "Enable LRO (Large Receive Offload) (default = 1) (0-1)");

static int lro_max_aggr = IPOIB_LRO_MAX_AGGR;
module_param_named(lro_max_aggr, lro_max_aggr, int, 0444);
MODULE_PARM_DESC(lro_max_aggr, "LRO: Max packets to be aggregated must be power of 2"
                               "(default = 64) (2-64)");
#endif

#ifdef CONFIG_INFINIBAND_IPOIB_DEBUG
int ipoib_debug_level;

module_param_named(debug_level, ipoib_debug_level, int, 0644);
MODULE_PARM_DESC(debug_level, "Enable debug tracing if > 0 (default: 0) (0-1)");
#endif

struct ipoib_path_iter {
	struct net_device *dev;
	struct ipoib_path  path;
};

static const u8 ipv4_bcast_addr[] = {
	0x00, 0xff, 0xff, 0xff,
	0xff, 0x12, 0x40, 0x1b,	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,	0xff, 0xff, 0xff, 0xff
};

struct workqueue_struct *ipoib_workqueue;
struct workqueue_struct *ipoib_auto_moder_workqueue;
struct ib_sa_client ipoib_sa_client;

static void ipoib_add_one(struct ib_device *device);
static void ipoib_remove_one(struct ib_device *device);
static void ipoib_neigh_reclaim(struct rcu_head *rp);
#ifdef CONFIG_COMPAT_LRO_ENABLED
static void ipoib_lro_setup(struct ipoib_recv_ring *recv_ring,
				struct ipoib_dev_priv *priv);
#endif

static struct ib_client ipoib_client = {
	.name   = "ipoib",
	.add    = ipoib_add_one,
	.remove = ipoib_remove_one
};

/* used to avoid get/set client_data at the same time */
spinlock_t client_data_lock;

int ipoib_open(struct net_device *dev)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);

	ipoib_dbg(priv, "bringing up interface\n");

	set_bit(IPOIB_FLAG_ADMIN_UP, &priv->flags);


	ipoib_pkey_dev_check_presence(dev);

	if (!test_bit(IPOIB_PKEY_ASSIGNED, &priv->flags))
		return 0;

	if (ipoib_ib_dev_open(dev, 1))
		goto err_disable;

	if (ipoib_ib_dev_up(dev))
		goto err_stop;

	if (!test_bit(IPOIB_FLAG_SUBINTERFACE, &priv->flags)) {
		struct ipoib_dev_priv *cpriv;

		/* Bring up any child interfaces too */
		down_read(&priv->vlan_rwsem);
		list_for_each_entry(cpriv, &priv->child_intfs, list) {
			int flags;

			flags = cpriv->dev->flags;
			if (flags & IFF_UP)
				continue;

			dev_change_flags(cpriv->dev, flags | IFF_UP);
		}
		up_read(&priv->vlan_rwsem);
	}

	netif_tx_start_all_queues(dev);

	if (priv->ethtool.use_adaptive_rx_coalesce) {
		set_bit(IPOIB_FLAG_AUTO_MODER, &priv->flags);
		queue_delayed_work(ipoib_auto_moder_workqueue,
					   &priv->adaptive_moder_task,
					   ADAPT_MODERATION_DELAY);
	}

	return 0;

err_stop:
	ipoib_ib_dev_stop(dev, 1);

err_disable:
	clear_bit(IPOIB_FLAG_ADMIN_UP, &priv->flags);

	return -EINVAL;
}

static int ipoib_stop(struct net_device *dev)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);

	ipoib_dbg(priv, "stopping interface\n");
	mutex_lock(&priv->state_lock);
	clear_bit(IPOIB_FLAG_ADMIN_UP, &priv->flags);
	mutex_unlock(&priv->state_lock);

	netif_tx_stop_all_queues(dev);

	ipoib_ib_dev_down(dev, 1);
	ipoib_ib_dev_stop(dev, 0);

	if (!test_bit(IPOIB_FLAG_SUBINTERFACE, &priv->flags)) {
		struct ipoib_dev_priv *cpriv;

		/* Bring down any child interfaces too */
		down_read(&priv->vlan_rwsem);
		list_for_each_entry(cpriv, &priv->child_intfs, list) {
			int flags;

			flags = cpriv->dev->flags;
			if (!(flags & IFF_UP))
				continue;

			dev_change_flags(cpriv->dev, flags & ~IFF_UP);
		}
		up_read(&priv->vlan_rwsem);
	}

	return 0;
}

void ipoib_uninit(struct net_device *dev)
{
	ipoib_dev_cleanup(dev);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39))
static netdev_features_t ipoib_fix_features(struct net_device *dev, netdev_features_t features)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);

	if (test_bit(IPOIB_FLAG_ADMIN_CM, &priv->flags))
		features &= ~(NETIF_F_SG | NETIF_F_IP_CSUM | NETIF_F_TSO |
			      NETIF_F_RXCSUM);

	return features;
}
#endif

static int ipoib_change_mtu(struct net_device *dev, int new_mtu)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);

	/* dev->mtu > 2K ==> connected mode */
	if (ipoib_cm_admin_enabled(dev)) {
		if (new_mtu > ipoib_cm_max_mtu(dev))
			return -EINVAL;

		if (new_mtu > priv->mcast_mtu)
			ipoib_warn(priv, "mtu > %d will cause multicast packet drops.\n",
				   priv->mcast_mtu);

		dev->mtu = new_mtu;
		return 0;
	}

	if (new_mtu > IPOIB_UD_MTU(priv->max_ib_mtu))
		return -EINVAL;

	priv->admin_mtu = new_mtu;

	dev->mtu = min(priv->mcast_mtu, priv->admin_mtu);
	if (dev->mtu < new_mtu) {
		ipoib_warn(priv, "mtu must be smaller than mcast_mtu (%d)\n",
			   priv->mcast_mtu);
		return -EINVAL;
	}

	return 0;
}

int ipoib_set_mode(struct net_device *dev, const char *buf)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	struct ipoib_send_ring *send_ring;
	int i;

	if ((test_bit(IPOIB_FLAG_ADMIN_CM, &priv->flags) &&
	     !strcmp(buf, "connected\n")) ||
	    (!test_bit(IPOIB_FLAG_ADMIN_CM, &priv->flags) &&
	     !strcmp(buf, "datagram\n"))) {
		ipoib_dbg(priv, "already in that mode, goes out.\n");
		return 0;
	}

	if (priv->dev->flags & IFF_UP) {
		ipoib_warn(priv, "interface is up, cannot change mode\n");
		return -EINVAL;
	}

	/* flush paths if we switch modes so that connections are restarted */
	if (IPOIB_CM_SUPPORTED(dev->dev_addr) && !strcmp(buf, "connected\n")) {
		set_bit(IPOIB_FLAG_ADMIN_CM, &priv->flags);
		ipoib_warn(priv, "enabling connected mode "
			   "will cause multicast packet drops\n");
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39))
		netdev_update_features(dev);
#else
                dev->features &= ~(NETIF_F_IP_CSUM | NETIF_F_SG | NETIF_F_TSO);
                if (ipoib_cm_max_mtu(dev) > priv->mcast_mtu)
                        ipoib_warn(priv, "mtu > %d will cause multicast packet drops.\n",
                                   priv->mcast_mtu);
#endif
 		dev_set_mtu(dev, ipoib_cm_max_mtu(dev));
		rtnl_unlock();

		send_ring = priv->send_ring;
		for (i = 0; i < priv->num_tx_queues; i++) {
			send_ring->tx_wr.send_flags &= ~IB_SEND_IP_CSUM;
			send_ring++;
		}

		ipoib_flush_paths(dev);

		if (!rtnl_trylock())
			return -EBUSY;

		return 0;
	}

	if (!strcmp(buf, "datagram\n")) {
		clear_bit(IPOIB_FLAG_ADMIN_CM, &priv->flags);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39))
		netdev_update_features(dev);
#else
		if (test_bit(IPOIB_FLAG_CSUM, &priv->flags)) {
			dev->features |= NETIF_F_IP_CSUM | NETIF_F_SG;

			if (priv->hca_caps & IB_DEVICE_UD_TSO)
				dev->features |= NETIF_F_TSO;
		}
#endif
		dev_set_mtu(dev, min(priv->mcast_mtu, dev->mtu));
		rtnl_unlock();
		ipoib_flush_paths(dev);

		if (!rtnl_trylock())
			return -EBUSY;

		return 0;
	}

	return -EINVAL;
}

static struct ipoib_path *__path_find(struct net_device *dev, void *gid)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	struct rb_node *n = priv->path_tree.rb_node;
	struct ipoib_path *path;
	int ret;

	while (n) {
		path = rb_entry(n, struct ipoib_path, rb_node);

		ret = memcmp(gid, path->pathrec.dgid.raw,
			     sizeof (union ib_gid));

		if (ret < 0)
			n = n->rb_left;
		else if (ret > 0)
			n = n->rb_right;
		else
			return path;
	}

	return NULL;
}

static int __path_add(struct net_device *dev, struct ipoib_path *path)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	struct rb_node **n = &priv->path_tree.rb_node;
	struct rb_node *pn = NULL;
	struct ipoib_path *tpath;
	int ret;

	while (*n) {
		pn = *n;
		tpath = rb_entry(pn, struct ipoib_path, rb_node);

		ret = memcmp(path->pathrec.dgid.raw, tpath->pathrec.dgid.raw,
			     sizeof (union ib_gid));
		if (ret < 0)
			n = &pn->rb_left;
		else if (ret > 0)
			n = &pn->rb_right;
		else
			return -EEXIST;
	}

	rb_link_node(&path->rb_node, pn, n);
	rb_insert_color(&path->rb_node, &priv->path_tree);

	list_add_tail(&path->list, &priv->path_list);

	return 0;
}

static void path_free(struct net_device *dev, struct ipoib_path *path)
{
	struct sk_buff *skb;

	while ((skb = __skb_dequeue(&path->queue)))
		dev_kfree_skb_irq(skb);
	if (path->pathrec.dlid)
		ipoib_path_del_notify(netdev_priv(dev), &path->pathrec);

	ipoib_dbg(netdev_priv(dev), "path_free\n");

	/* remove all neigh connected to this path */
	ipoib_del_neighs_by_gid(dev, path->pathrec.dgid.raw);

	if (path->ah)
		ipoib_put_ah(path->ah);

	kfree(path);
}

#ifdef CONFIG_INFINIBAND_IPOIB_DEBUG

struct ipoib_neigh_iter *ipoib_neigh_iter_init(struct net_device *dev)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	struct ipoib_neigh_table *ntbl = &priv->ntbl;
	struct ipoib_neigh_iter *iter;
	struct ipoib_neigh_hash *htbl;
	struct ipoib_neigh *neigh;
	struct ipoib_neigh __rcu **np;

	if (!netif_carrier_ok(dev))
		return NULL;
	iter = kmalloc(sizeof(*iter), GFP_KERNEL);
	if (!iter)
		return NULL;

	iter->dev = dev;
	iter->htbl_index = 0;

	rcu_read_lock_bh();
	htbl = rcu_dereference_bh(ntbl->htbl);

	if (!htbl) {
		ipoib_warn(priv, "%s: failed to dereference htbl\n",
			   __func__);
		goto out_error;
	}

	/* find the first neigh in the hash table */
	while (iter->htbl_index < htbl->size) {
		np = &htbl->buckets[iter->htbl_index];
		neigh = rcu_dereference_bh(*np);
		if (neigh) {
			if (!atomic_inc_not_zero(&neigh->refcnt)) {
				/* neigh was deleted */
				neigh = NULL;
				iter->htbl_index++;
				continue;
			}
			iter->neigh = neigh;
			goto out_unlock;
		}
		iter->htbl_index++;
	}
	/* Getting here means there are no neighs */
	goto out_error;

out_error:
	kfree(iter);
	iter = NULL;
out_unlock:
	rcu_read_unlock_bh();
	return iter;
}

/*
* Find the next neigh in the hash table:
* First, see if the current neigh has a 'hnext' neigh (in current hash
* bucket).
* If not, find the next hash bucket that contains a neigh.
* Returns 0 if a neigh is found, non-zero otherwise.
*/
int ipoib_neigh_iter_next(struct ipoib_neigh_iter *iter)
{
	struct ipoib_dev_priv *priv = netdev_priv(iter->dev);
	struct ipoib_neigh_table *ntbl = &priv->ntbl;
	struct ipoib_neigh_hash *htbl;
	struct ipoib_neigh *neigh;
	struct ipoib_neigh __rcu **np;
	int ret = -EINVAL;
	int table_size = 0;

	if (!iter) {
		ipoib_warn(priv, "%s, got NULL iter\n", __func__);
		goto out_no_lock;
	}

	if (!iter->neigh) {
		ipoib_warn(priv, "neigh can't be NULL in %s\n",
			   __func__);
		goto out_no_lock;
	}

	rcu_read_lock_bh();

	/* Check if the current neigh has a next one in the same hash
	 * bucket */
	neigh = rcu_dereference_bh(iter->neigh->hnext);
	if (neigh != NULL) {
		iter->neigh = neigh;
		/* removing ref on current neigh */
		ipoib_neigh_put(neigh);
		/* getting ref on next neigh */
		atomic_inc(&iter->neigh->refcnt);
		iter->dev = neigh->dev;
		ret = 0;
		goto out_unlock;
	}

	/* No hnext - look for the next bucket */
	htbl = rcu_dereference_bh(ntbl->htbl);
	table_size = htbl->size;

	if (!htbl || IS_ERR(htbl)) {
		ipoib_warn(priv, "failed to dereference htbl\n");
		goto out_unlock;
	}

	iter->htbl_index++;
	while (iter->htbl_index < table_size) {
		np = &htbl->buckets[iter->htbl_index];
		neigh = rcu_dereference_bh(*np);
		if (neigh != NULL) {
			/* removing ref on current neigh */
			ipoib_neigh_put(iter->neigh);
			iter->neigh = neigh;
			iter->dev = neigh->dev;
			/* getting ref on next neigh */
			atomic_inc(&iter->neigh->refcnt);
			ret = 0;
			goto out_unlock;
		}
		iter->htbl_index++;
	}

	/* getting here means there are no more neighs in the hash */
	ipoib_neigh_put(iter->neigh);

out_unlock:
	rcu_read_unlock_bh();
out_no_lock:
	return ret;
}

void ipoib_neigh_iter_read(struct ipoib_neigh_iter *iter,
			  struct ipoib_neigh *neigh)
{
	neigh->dev = iter->neigh->dev;
	neigh->index = iter->neigh->index;
	neigh->refcnt = iter->neigh->refcnt;
	neigh->alive = iter->neigh->alive;
	memcpy(neigh->daddr, iter->neigh->daddr, INFINIBAND_ALEN);
}

struct ipoib_path_iter *ipoib_path_iter_init(struct net_device *dev)
{
	struct ipoib_path_iter *iter;

	iter = kmalloc(sizeof *iter, GFP_KERNEL);
	if (!iter)
		return NULL;

	iter->dev = dev;
	memset(iter->path.pathrec.dgid.raw, 0, 16);

	if (ipoib_path_iter_next(iter)) {
		kfree(iter);
		return NULL;
	}

	return iter;
}

int ipoib_path_iter_next(struct ipoib_path_iter *iter)
{
	struct ipoib_dev_priv *priv = netdev_priv(iter->dev);
	struct rb_node *n;
	struct ipoib_path *path;
	int ret = 1;

	spin_lock_irq(&priv->lock);

	n = rb_first(&priv->path_tree);

	while (n) {
		path = rb_entry(n, struct ipoib_path, rb_node);

		if (memcmp(iter->path.pathrec.dgid.raw, path->pathrec.dgid.raw,
			   sizeof (union ib_gid)) < 0) {
			iter->path = *path;
			ret = 0;
			break;
		}

		n = rb_next(n);
	}

	spin_unlock_irq(&priv->lock);

	return ret;
}

void ipoib_path_iter_read(struct ipoib_path_iter *iter,
			  struct ipoib_path *path)
{
	*path = iter->path;
}

#endif /* CONFIG_INFINIBAND_IPOIB_DEBUG */

void ipoib_mark_paths_invalid(struct net_device *dev)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	struct ipoib_path *path, *tp;

	spin_lock_irq(&priv->lock);

	list_for_each_entry_safe(path, tp, &priv->path_list, list) {
		ipoib_dbg(priv, "mark path LID 0x%04x GID %pI6 invalid\n",
			be16_to_cpu(path->pathrec.dlid),
			path->pathrec.dgid.raw);
		path->valid =  0;
		ipoib_path_del_notify(netdev_priv(dev), &path->pathrec);
	}

	spin_unlock_irq(&priv->lock);
}

void ipoib_flush_paths(struct net_device *dev)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	struct ipoib_path *path, *tp;
	LIST_HEAD(remove_list);
	unsigned long flags;

	netif_tx_lock_bh(dev);
	spin_lock_irqsave(&priv->lock, flags);

	list_splice_init(&priv->path_list, &remove_list);

	list_for_each_entry(path, &remove_list, list)
		rb_erase(&path->rb_node, &priv->path_tree);

	list_for_each_entry_safe(path, tp, &remove_list, list) {
		if (path->query)
			ib_sa_cancel_query(path->query_id, path->query);
		spin_unlock_irqrestore(&priv->lock, flags);
		netif_tx_unlock_bh(dev);
		wait_for_completion(&path->done);
		path_free(dev, path);
		netif_tx_lock_bh(dev);
		spin_lock_irqsave(&priv->lock, flags);
	}

	spin_unlock_irqrestore(&priv->lock, flags);
	netif_tx_unlock_bh(dev);
}

static void path_rec_completion(int status,
				struct ib_sa_path_rec *pathrec,
				void *path_ptr)
{
	struct ipoib_path *path = path_ptr;
	struct net_device *dev = path->dev;
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	struct ipoib_ah *ah = NULL;
	struct ipoib_ah *old_ah = NULL;
	struct ipoib_neigh *neigh, *tn;
	struct sk_buff_head skqueue;
	struct sk_buff *skb;
	unsigned long flags;
	int ret;

	if (!status)
		ipoib_dbg(priv, "PathRec LID 0x%04x for GID %pI6\n",
			  be16_to_cpu(pathrec->dlid), pathrec->dgid.raw);
	else
		ipoib_dbg(priv, "PathRec status %d for GID %pI6\n",
			  status, path->pathrec.dgid.raw);

	skb_queue_head_init(&skqueue);

	if (!status) {
		struct ib_ah_attr av;

		if (!ib_init_ah_from_path(priv->ca, priv->port, pathrec, &av))
			ah = ipoib_create_ah(dev, priv->pd, &av);
		ipoib_path_add_notify(priv, pathrec);
	}

	spin_lock_irqsave(&priv->lock, flags);

	if (!IS_ERR_OR_NULL(ah)) {
		path->pathrec = *pathrec;

		old_ah   = path->ah;
		path->ah = ah;

		ipoib_dbg(priv, "created address handle %p for LID 0x%04x, SL %d\n",
			  ah, be16_to_cpu(pathrec->dlid), pathrec->sl);

		while ((skb = __skb_dequeue(&path->queue)))
			__skb_queue_tail(&skqueue, skb);

		list_for_each_entry_safe(neigh, tn, &path->neigh_list, list) {
			if (neigh->ah) {
				WARN_ON(neigh->ah != old_ah);
				/*
				 * Dropping the ah reference inside
				 * priv->lock is safe here, because we
				 * will hold one more reference from
				 * the original value of path->ah (ie
				 * old_ah).
				 */
				ipoib_put_ah(neigh->ah);
			}
			kref_get(&path->ah->ref);
			neigh->ah = path->ah;

			if (ipoib_cm_enabled(dev, neigh->daddr)) {
				if (!ipoib_cm_get(neigh))
					ipoib_cm_set(neigh, ipoib_cm_create_tx(dev,
									       path,
									       neigh));
				if (!ipoib_cm_get(neigh)) {
					list_del_init(&neigh->list);
					ipoib_neigh_free(neigh);
					continue;
				}
			}

			while ((skb = __skb_dequeue(&neigh->queue)))
				__skb_queue_tail(&skqueue, skb);
		}
		path->valid = 1;
	}

	path->query = NULL;
	complete(&path->done);

	spin_unlock_irqrestore(&priv->lock, flags);

	if (old_ah)
		ipoib_put_ah(old_ah);

	while ((skb = __skb_dequeue(&skqueue))) {
		skb->dev = dev;
		ret = dev_queue_xmit(skb);
		if (ret)
			ipoib_warn(priv, "%s: dev_queue_xmit failed "
				   "to requeue packet, ret:%d\n", __func__, ret);
	}
}

static struct ipoib_path *path_rec_create(struct net_device *dev, void *gid)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	struct ipoib_path *path;

	if (!priv->broadcast)
		return NULL;

	path = kzalloc(sizeof *path, GFP_ATOMIC);
	if (!path)
		return NULL;

	path->dev = dev;

	skb_queue_head_init(&path->queue);

	INIT_LIST_HEAD(&path->neigh_list);

	memcpy(path->pathrec.dgid.raw, gid, sizeof (union ib_gid));
	path->pathrec.sgid	    = priv->local_gid;
	path->pathrec.pkey	    = cpu_to_be16(priv->pkey);
	path->pathrec.numb_path     = 1;
	path->pathrec.traffic_class = priv->broadcast->mcmember.traffic_class;

	return path;
}

static int path_rec_start(struct net_device *dev,
			  struct ipoib_path *path)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);

	ipoib_dbg(priv, "Start path record lookup for %pI6\n",
		  path->pathrec.dgid.raw);

	init_completion(&path->done);

	path->query_id =
		ib_sa_path_rec_get(&ipoib_sa_client, priv->ca, priv->port,
				   &path->pathrec,
				   IB_SA_PATH_REC_DGID		|
				   IB_SA_PATH_REC_SGID		|
				   IB_SA_PATH_REC_NUMB_PATH	|
				   IB_SA_PATH_REC_TRAFFIC_CLASS |
				   IB_SA_PATH_REC_PKEY,
				   1000, GFP_ATOMIC,
				   path_rec_completion,
				   path, &path->query);
	if (path->query_id < 0) {
		ipoib_warn(priv, "ib_sa_path_rec_get failed: %d\n", path->query_id);
		path->query = NULL;
		complete(&path->done);
		return path->query_id;
	}

	return 0;
}

static struct ipoib_neigh *neigh_add_path(struct sk_buff *skb, u8 *daddr,
					  struct net_device *dev)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	struct ipoib_path *path;
	struct ipoib_neigh *neigh;
	unsigned long flags;
	int index;

	spin_lock_irqsave(&priv->lock, flags);
	neigh = ipoib_neigh_alloc(daddr, dev);
	if (!neigh) {
		spin_unlock_irqrestore(&priv->lock, flags);
		index = skb_get_queue_mapping(skb);
		priv->send_ring[index].stats.tx_dropped++;
		dev_kfree_skb_any(skb);
		return NULL;
	}

	/* Under TX MQ it is possible that more than one skb transmission
	 * triggered a call to create the ipoib neigh. But only one actually
	 * created the neigh structure, where the other instances found it
	 * in the hash. We must make sure that the neigh will be added
	 * only once to the path list, since double insertion will lead to
	 * an infinite loop in path_rec_completion.
	 */
	if (unlikely(!list_empty(&neigh->list))) {
		spin_unlock_irqrestore(&priv->lock, flags);
		return neigh;
	}

	path = __path_find(dev, daddr + 4);
	if (!path) {
		path = path_rec_create(dev, daddr + 4);
		if (!path)
			goto err_path;

		__path_add(dev, path);
	}

	list_add_tail(&neigh->list, &path->neigh_list);

	if (path->ah) {
		kref_get(&path->ah->ref);
		neigh->ah = path->ah;

		if (ipoib_cm_enabled(dev, neigh->daddr)) {
			if (!ipoib_cm_get(neigh))
				ipoib_cm_set(neigh, ipoib_cm_create_tx(dev, path, neigh));
			if (!ipoib_cm_get(neigh)) {
				list_del_init(&neigh->list);
				ipoib_neigh_free(neigh);
				goto err_drop;
			}
			if (skb_queue_len(&neigh->queue) < IPOIB_MAX_PATH_REC_QUEUE)
				__skb_queue_tail(&neigh->queue, skb);
			else {
				ipoib_warn(priv, "queue length limit %d. Packet drop.\n",
					   skb_queue_len(&neigh->queue));
				goto err_drop;
			}
		} else {
			spin_unlock_irqrestore(&priv->lock, flags);
			ipoib_send(dev, skb, path->ah, IPOIB_QPN(daddr));
			ipoib_neigh_put(neigh);
			return NULL;
		}
	} else {
		neigh->ah  = NULL;

		if (!path->query && path_rec_start(dev, path))
			goto err_list;

		__skb_queue_tail(&neigh->queue, skb);
	}

	spin_unlock_irqrestore(&priv->lock, flags);
	ipoib_neigh_put(neigh);
	return NULL;

err_list:
	list_del_init(&neigh->list);

err_path:
	ipoib_neigh_free(neigh);
err_drop:
	index = skb_get_queue_mapping(skb);
	priv->send_ring[index].stats.tx_dropped++;
	dev_kfree_skb_any(skb);

	spin_unlock_irqrestore(&priv->lock, flags);
	ipoib_neigh_put(neigh);

	return NULL;
}

/*
 * clean_path_from_cache: free path from both caches
 * (list and rb tree)
 * call that function under lock. (netif_tx_lock_bh && priv->lock)
 */
static inline void clean_path_from_cache(struct ipoib_path *path,
				  struct ipoib_dev_priv *priv)
{
	list_del(&path->list);
	rb_erase(&path->rb_node, &priv->path_tree);
	if (path->query)
		ib_sa_cancel_query(path->query_id, path->query);

}

/*
 * clean_path_dependencies: free path from neigths.
 * Do not call this function under locks.
 */
static inline void clean_path_references(struct ipoib_path *path,
				  struct net_device *dev)
{
	wait_for_completion(&path->done);
	path_free(dev, path);
}

/*
 * ipoib_repath_ah: for each arp response/request:
 *		 check that the lid ipoib kept for this gid
 *		 is the same as it has in the arp packet.
 *		 if not, delete that path from the cache.
 */
void ipoib_repath_ah(struct work_struct *work)
{
	struct ipoib_arp_repath *repath =
		container_of(work, struct ipoib_arp_repath, work);

	struct net_device *dev = repath->dev;
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	struct ipoib_path *path_from_cache;
	u16 lid_from_cache;
	unsigned long flags;

	netif_tx_lock_bh(dev);
	spin_lock_irqsave(&priv->lock, flags);

	path_from_cache = __path_find(dev, &repath->sgid);

	if (path_from_cache) {
		lid_from_cache = be16_to_cpu(path_from_cache->pathrec.dlid);
		/*check if we have the same path in the path cache:*/
		if ((lid_from_cache && repath->lid) &&
		    (repath->lid != lid_from_cache)) {
			ipoib_warn(priv, "Found gid with mismatch lids."
					 "(cache:%d,from arp: %d)\n",
				   lid_from_cache, repath->lid);
			clean_path_from_cache(path_from_cache, priv);
			spin_unlock_irqrestore(&priv->lock, flags);
			netif_tx_unlock_bh(dev);
			clean_path_references(path_from_cache, dev);
			goto free_res;
		}
	}

	spin_unlock_irqrestore(&priv->lock, flags);
	netif_tx_unlock_bh(dev);

free_res:
	kfree(repath);
}

static void unicast_arp_send(struct sk_buff *skb, struct net_device *dev,
			     struct ipoib_cb *cb)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	struct ipoib_path *path;
	unsigned long flags;
	int index = skb_get_queue_mapping(skb);

	spin_lock_irqsave(&priv->lock, flags);

	path = __path_find(dev, cb->hwaddr + 4);
	if (!path || !path->valid) {
		int new_path = 0;

		if (!path) {
			path = path_rec_create(dev, cb->hwaddr + 4);
			new_path = 1;
		}
		if (path) {
			__skb_queue_tail(&path->queue, skb);

			if (!path->query && path_rec_start(dev, path)) {
				spin_unlock_irqrestore(&priv->lock, flags);
				if (new_path)
					path_free(dev, path);
				return;
			} else
				__path_add(dev, path);
		} else {
			priv->send_ring[index].stats.tx_dropped++;
			dev_kfree_skb_any(skb);
		}

		spin_unlock_irqrestore(&priv->lock, flags);
		return;
	}

	if (path->ah) {
		ipoib_dbg(priv, "Send unicast ARP to %04x\n",
			  be16_to_cpu(path->pathrec.dlid));

		spin_unlock_irqrestore(&priv->lock, flags);
		ipoib_send(dev, skb, path->ah, IPOIB_QPN(cb->hwaddr));
		return;
	} else if ((path->query || !path_rec_start(dev, path)) &&
		   skb_queue_len(&path->queue) < IPOIB_MAX_PATH_REC_QUEUE) {
		__skb_queue_tail(&path->queue, skb);
	} else {
		priv->send_ring[index].stats.tx_dropped++;
		dev_kfree_skb_any(skb);
	}

	spin_unlock_irqrestore(&priv->lock, flags);
}

static int ipoib_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	struct ipoib_neigh *neigh;
	struct ipoib_cb *cb = (struct ipoib_cb *) skb->cb;
	struct ipoib_header *header;
	struct ipoib_send_ring *send_ring;
	unsigned long flags;

	send_ring = priv->send_ring + skb_get_queue_mapping(skb);
	header = (struct ipoib_header *) skb->data;

	if (unlikely(cb->hwaddr[4] == 0xff)) {
		/* multicast, arrange "if" according to probability */
		if ((header->proto != htons(ETH_P_IP)) &&
		    (header->proto != htons(ETH_P_IPV6)) &&
		    (header->proto != htons(ETH_P_ARP)) &&
		    (header->proto != htons(ETH_P_RARP))) {
			/* ethertype not supported by IPoIB */
			++send_ring->stats.tx_dropped;
			dev_kfree_skb_any(skb);
			return NETDEV_TX_OK;
		}
		/* Add in the P_Key for multicast*/
		cb->hwaddr[8] = (priv->pkey >> 8) & 0xff;
		cb->hwaddr[9] = priv->pkey & 0xff;

		neigh = ipoib_neigh_get(dev, cb->hwaddr);
		if (likely(neigh))
			goto send_using_neigh;
		ipoib_mcast_send(dev, cb->hwaddr, skb);
		return NETDEV_TX_OK;
	}

	/* unicast, arrange "switch" according to probability */
	switch (header->proto) {
	case htons(ETH_P_IP):
	case htons(ETH_P_IPV6):
		neigh = ipoib_neigh_get(dev, cb->hwaddr);
		if (unlikely(!neigh)) {
			/* If more than one thread of execution tries to create
			 * the neigh, only one would succeed, where all the
			 * others got the neigh from the hash and should
			 * continue as usual
			 */
			neigh = neigh_add_path(skb, cb->hwaddr, dev);
			if (likely(!neigh))
				return NETDEV_TX_OK;
		}
		break;
	case htons(ETH_P_ARP):
	case htons(ETH_P_RARP):
		/* for unicast ARP and RARP should always perform path find */
		unicast_arp_send(skb, dev, cb);
		return NETDEV_TX_OK;
	default:
		/* ethertype not supported by IPoIB */
		++dev->stats.tx_dropped;
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

send_using_neigh:
	skb_orphan(skb);
	skb_dst_drop(skb);
	/* note we now hold a ref to neigh */
	if (ipoib_cm_get(neigh)) {
		/* in select queue cm wasn't enabled ring is likely wrong */
		if (!IPOIB_CM_SUPPORTED(cb->hwaddr)) {
			ipoib_dbg(priv, "CM NOT supported,ring likely wrong, sending via UD\n");
			ipoib_send(dev, skb, neigh->ah, IPOIB_QPN(cb->hwaddr));
			goto unref;
		}

		if (ipoib_cm_up(neigh)) {
			ipoib_cm_send(dev, skb, ipoib_cm_get(neigh));
			goto unref;
		}
	} else if (neigh->ah) {
		/* in select queue cm was enabled ring is likely wrong */
		if (IPOIB_CM_SUPPORTED(cb->hwaddr) && priv->num_tx_queues > 1) {
				ipoib_dbg(priv, "CM supported,ring likely wrong, dropping.cm is: %d\n",
					   ipoib_cm_admin_enabled(dev));
			++send_ring->stats.tx_dropped;

			ipoib_cm_skb_too_long(dev, skb, priv->mcast_mtu);

			dev_kfree_skb_any(skb);
			goto unref;
		}
		ipoib_send(dev, skb, neigh->ah, IPOIB_QPN(cb->hwaddr));
		goto unref;
	}

	/*requeue the packet that misses path*/
	if (skb_queue_len(&neigh->queue) < IPOIB_MAX_PATH_REC_QUEUE) {
		spin_lock_irqsave(&priv->lock, flags);
		__skb_queue_tail(&neigh->queue, skb);
		spin_unlock_irqrestore(&priv->lock, flags);
	} else {
		++dev->stats.tx_dropped;
		dev_kfree_skb_any(skb);
	}

unref:
	ipoib_neigh_put(neigh);

	return NETDEV_TX_OK;
}

#ifdef CONFIG_COMPAT_SELECT_QUEUE_ACCEL
static u16 ipoib_select_queue_hw(struct net_device *dev, struct sk_buff *skb,
#ifdef CONFIG_COMPAT_SELECT_QUEUE_FALLBACK
 			 	 void *accel_priv, select_queue_fallback_t fallback)
#else
				 void *accel_priv)
#endif
#else /* CONFIG_COMPAT_SELECT_QUEUE_ACCEL */
static u16 ipoib_select_queue_hw(struct net_device *dev, struct sk_buff *skb)
#endif
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	struct ipoib_cb *cb = (struct ipoib_cb *) skb->cb;

	/* (BC/MC), stay on this core */
	if (unlikely(cb->hwaddr[4] == 0xff))
		return smp_processor_id() % priv->tss_qp_num;

	/* is CM in use */
	if (IPOIB_CM_SUPPORTED(cb->hwaddr)) {
		if (ipoib_cm_admin_enabled(dev)) {
			/* use remote QP for hash, so we use the same ring */
			u32 *d32 = (u32 *)cb->hwaddr;
			u32 hv = jhash_1word(*d32 & cpu_to_be32(0xFFFFFF), 0);
			return hv % priv->tss_qp_num;
		}
		else
			/* the ADMIN CM might be up until transmit, and
			 * we might transmit on CM QP not from it's
			 * designated ring */
			cb->hwaddr[0] &= ~IPOIB_FLAGS_RC;
	}
	return skb_tx_hash(dev, skb);
}

#ifdef CONFIG_COMPAT_SELECT_QUEUE_ACCEL
static u16 ipoib_select_queue_sw(struct net_device *dev, struct sk_buff *skb,
#ifdef CONFIG_COMPAT_SELECT_QUEUE_FALLBACK
				 void* accel_priv, select_queue_fallback_t fallback)
#else
				 void* accel_priv)
#endif
#else /* CONFIG_COMPAT_SELECT_QUEUE_ACCEL */
static u16 ipoib_select_queue_sw(struct net_device *dev, struct sk_buff *skb)
#endif
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	struct ipoib_cb *cb = (struct ipoib_cb *) skb->cb;
	struct ipoib_header *header;

	/* (BC/MC) use designated QDISC -> parent QP */
	if (unlikely(cb->hwaddr[4] == 0xff))
		return priv->tss_qp_num;

	/* is CM in use */
	if (IPOIB_CM_SUPPORTED(cb->hwaddr)) {
		if (ipoib_cm_admin_enabled(dev)) {
			/* use remote QP for hash, so we use the same ring */
			u32 *d32 = (u32 *)cb->hwaddr;
			u32 hv = jhash_1word(*d32 & cpu_to_be32(0xFFFFFF), 0);
			return hv % priv->tss_qp_num;
		}
		else
			/* the ADMIN CM might be up until transmit, and
			 * we might transmit on CM QP not from it's
			 * designated ring */
			cb->hwaddr[0] &= ~IPOIB_FLAGS_RC;
	}

	/* Did neighbour advertise TSS support */
	if (unlikely(!IPOIB_TSS_SUPPORTED(cb->hwaddr)))
		return priv->tss_qp_num;

	/* We are after ipoib_hard_header so skb->data is O.K. */
	header = (struct ipoib_header *) skb->data;
	header->tss_qpn_mask_sz |= priv->tss_qpn_mask_sz;

	/* don't use special ring in TX */
#ifdef CONFIG_COMPAT_IS___SKB_TX_HASH
	return __skb_tx_hash(dev, skb, priv->tss_qp_num);
#else
	return skb_tx_hash(dev, skb);
#endif
}

static void ipoib_timeout(struct net_device *dev)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	struct ipoib_send_ring *send_ring;
	u16 index;
	int is_stopped;

	ipoib_warn(priv, "transmit timeout: latency %d msecs\n",
		   jiffies_to_msecs(jiffies - dev->trans_start));

	for (index = 0; index < priv->num_tx_queues; index++) {
		is_stopped = __netif_subqueue_stopped(dev, index);
		send_ring = priv->send_ring + index;
		ipoib_warn(priv,
			"queue (%d) stopped=%d, tx_head %u, tx_tail %u "
			"tx_outstanding %u ipoib_sendq_size: %d\n",
			index, is_stopped,
			send_ring->tx_head, send_ring->tx_tail,
			send_ring->tx_outstanding, priv->sendq_size);

		if (is_stopped && unlikely(send_ring->tx_outstanding <
			    priv->sendq_size >> 1) &&
			test_bit(IPOIB_FLAG_ADMIN_UP, &priv->flags)) {
			ipoib_warn(priv, "%s: waking the queue\n",
					__func__);
			netif_wake_subqueue(dev, index);
		}

	}
	/* XXX reset QP, etc. */
}

static struct net_device_stats *ipoib_get_stats(struct net_device *dev)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	struct net_device_stats *stats = &dev->stats;
	struct net_device_stats local_stats;
	int i;

	/* if rings are not ready yet return last values */
	if (!down_read_trylock(&priv->rings_rwsem))
		return stats;

	/* check we are not after ipoib_dev_uninit that already released them*/
	if (!priv->recv_ring || !priv->send_ring) {
		up_read(&priv->rings_rwsem);
		return stats;
	}

	memset(&local_stats, 0, sizeof(struct net_device_stats));

	for (i = 0; i < priv->num_rx_queues; i++) {
		struct ipoib_rx_ring_stats *rstats = &priv->recv_ring[i].stats;
		local_stats.rx_packets += rstats->rx_packets;
		local_stats.rx_bytes   += rstats->rx_bytes;
		local_stats.rx_errors  += rstats->rx_errors;
		local_stats.rx_dropped += rstats->rx_dropped;
	}

	for (i = 0; i < priv->num_tx_queues; i++) {
		struct ipoib_tx_ring_stats *tstats = &priv->send_ring[i].stats;
		local_stats.tx_packets += tstats->tx_packets;
		local_stats.tx_bytes   += tstats->tx_bytes;
		local_stats.tx_errors  += tstats->tx_errors;
		local_stats.tx_dropped += tstats->tx_dropped;
	}

	up_read(&priv->rings_rwsem);

	stats->rx_packets = local_stats.rx_packets;
	stats->rx_bytes   = local_stats.rx_bytes;
	stats->rx_errors  = local_stats.rx_errors;
	stats->rx_dropped = local_stats.rx_dropped;

	stats->tx_packets = local_stats.tx_packets;
	stats->tx_bytes   = local_stats.tx_bytes;
	stats->tx_errors  = local_stats.tx_errors;
	stats->tx_dropped = local_stats.tx_dropped;

	return stats;
}

static int ipoib_hard_header(struct sk_buff *skb,
			     struct net_device *dev,
			     unsigned short type,
			     const void *daddr, const void *saddr, unsigned len)
{
	struct ipoib_header *header;
	struct ipoib_cb *cb = (struct ipoib_cb *) skb->cb;

	header = (struct ipoib_header *) skb_push(skb, sizeof *header);

	header->proto = htons(type);
	header->tss_qpn_mask_sz = 0;

	/*
	 * we don't rely on dst_entry structure,  always stuff the
	 * destination address into skb->cb so we can figure out where
	 * to send the packet later.
	 */
	memcpy(cb->hwaddr, daddr, INFINIBAND_ALEN);

	return sizeof(*header);
}

static void ipoib_set_mcast_list(struct net_device *dev)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);

	if (!test_bit(IPOIB_FLAG_OPER_UP, &priv->flags)) {
		ipoib_dbg(priv, "IPOIB_FLAG_OPER_UP not set");
		return;
	}

	queue_work(ipoib_workqueue, &priv->restart_task);
}

static u32 ipoib_addr_hash(struct ipoib_neigh_hash *htbl, u8 *daddr)
{
	/*
	 * Use only the address parts that contributes to spreading
	 * The subnet prefix is not used as one can not connect to
	 * same remote port (GUID) using the same remote QPN via two
	 * different subnets.
	 */
	 /* qpn octets[1:4) & port GUID octets[12:20) */
	u32 *d32 = (u32 *)daddr;
	u32 hv;

	hv = jhash_3words(d32[3], d32[4], cpu_to_be32(0xFFFFFF) & d32[0], 0);
	return hv & htbl->mask;
}

struct ipoib_neigh *ipoib_neigh_get(struct net_device *dev, u8 *daddr)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	struct ipoib_neigh_table *ntbl = &priv->ntbl;
	struct ipoib_neigh_hash *htbl;
	struct ipoib_neigh *neigh = NULL;
	u32 hash_val;

	rcu_read_lock_bh();

	htbl = rcu_dereference_bh(ntbl->htbl);

	if (!htbl)
		goto out_unlock;

	hash_val = ipoib_addr_hash(htbl, daddr);
	for (neigh = rcu_dereference_bh(htbl->buckets[hash_val]);
	     neigh != NULL;
	     neigh = rcu_dereference_bh(neigh->hnext)) {
		if (memcmp(daddr+1, neigh->daddr+1, INFINIBAND_ALEN-1) == 0) {
			/* found, take one ref on behalf of the caller */
			if (!atomic_inc_not_zero(&neigh->refcnt)) {
				/* deleted */
				neigh = NULL;
				goto out_unlock;
			}
			neigh->alive = jiffies;
			goto out_unlock;
		}
	}

out_unlock:
	rcu_read_unlock_bh();
	return neigh;
}

static void __ipoib_reap_neigh(struct ipoib_dev_priv *priv)
{
	struct ipoib_neigh_table *ntbl = &priv->ntbl;
	struct ipoib_neigh_hash *htbl;
	unsigned long neigh_obsolete;
	unsigned long dt;
	unsigned long flags;
	int i;

	if (test_bit(IPOIB_STOP_NEIGH_GC, &priv->flags))
		return;

	spin_lock_irqsave(&priv->lock, flags);

	htbl = rcu_dereference_protected(ntbl->htbl,
					 lockdep_is_held(&priv->lock));

	if (!htbl)
		goto out_unlock;

	/* neigh is obsolete if it was idle for two GC periods */
	dt = 2 * arp_tbl.gc_interval;
	neigh_obsolete = jiffies - dt;
	/* handle possible race condition */
	if (test_bit(IPOIB_STOP_NEIGH_GC, &priv->flags))
		goto out_unlock;

	for (i = 0; i < htbl->size; i++) {
		struct ipoib_neigh *neigh;
		struct ipoib_neigh __rcu **np = &htbl->buckets[i];

		while ((neigh = rcu_dereference_protected(*np,
							  lockdep_is_held(&priv->lock))) != NULL) {
			/* was the neigh idle for two GC periods */
			if (time_after(neigh_obsolete, neigh->alive)) {
				rcu_assign_pointer(*np,
						   rcu_dereference_protected(neigh->hnext,
									     lockdep_is_held(&priv->lock)));
				/* remove from path/mc list */
				list_del_init(&neigh->list);
				call_rcu(&neigh->rcu, ipoib_neigh_reclaim);
			} else {
				np = &neigh->hnext;
			}

		}
	}

out_unlock:
	spin_unlock_irqrestore(&priv->lock, flags);
}

static void ipoib_reap_neigh(struct work_struct *work)
{
	struct ipoib_dev_priv *priv =
		container_of(work, struct ipoib_dev_priv, neigh_reap_task.work);

	__ipoib_reap_neigh(priv);

	if (!test_bit(IPOIB_STOP_NEIGH_GC, &priv->flags))
		queue_delayed_work(ipoib_workqueue, &priv->neigh_reap_task,
				   IPOIB_NEIGH_GC_TIME);
}


static struct ipoib_neigh *ipoib_neigh_ctor(u8 *daddr,
				      struct net_device *dev)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	struct ipoib_neigh *neigh;

	neigh = kzalloc(sizeof *neigh, GFP_ATOMIC);
	if (!neigh)
		return NULL;

	neigh->dev = dev;
	memcpy(&neigh->daddr, daddr, sizeof(neigh->daddr));
	skb_queue_head_init(&neigh->queue);
	INIT_LIST_HEAD(&neigh->list);
	ipoib_cm_set(neigh, NULL);
	/* one ref on behalf of the caller */
	atomic_set(&neigh->refcnt, 1);

	/*
	 * ipoib_neigh_alloc can be called from neigh_add_path without
	 * the protection of spin lock or from ipoib_mcast_send under
	 * spin lock protection. thus there is a need to use atomic
	 */
	if (priv->num_rx_queues > 1)
		neigh->index = atomic_inc_return(&priv->tx_ring_ind)
			% priv->num_rx_queues;
	else
		neigh->index = 0;

	return neigh;
}

struct ipoib_neigh *ipoib_neigh_alloc(u8 *daddr,
				      struct net_device *dev)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	struct ipoib_neigh_table *ntbl = &priv->ntbl;
	struct ipoib_neigh_hash *htbl;
	struct ipoib_neigh *neigh;
	u32 hash_val;

	htbl = rcu_dereference_protected(ntbl->htbl,
					 lockdep_is_held(&priv->lock));
	if (!htbl) {
		neigh = NULL;
		goto out_unlock;
	}

	/* need to add a new neigh, but maybe some other thread succeeded?
	 * recalc hash, maybe hash resize took place so we do a search
	 */
	hash_val = ipoib_addr_hash(htbl, daddr);
	for (neigh = rcu_dereference_protected(htbl->buckets[hash_val],
					       lockdep_is_held(&priv->lock));
	     neigh != NULL;
	     neigh = rcu_dereference_protected(neigh->hnext,
					       lockdep_is_held(&priv->lock))) {
		if (memcmp(daddr, neigh->daddr, INFINIBAND_ALEN) == 0) {
			/* found, take one ref on behalf of the caller */
			if (!atomic_inc_not_zero(&neigh->refcnt)) {
				/* deleted */
				neigh = NULL;
				break;
			}
			neigh->alive = jiffies;
			goto out_unlock;
		}
	}

	neigh = ipoib_neigh_ctor(daddr, dev);
	if (!neigh)
		goto out_unlock;

	/* one ref on behalf of the hash table */
	atomic_inc(&neigh->refcnt);
	neigh->alive = jiffies;
	/* put in hash */
	rcu_assign_pointer(neigh->hnext,
			   rcu_dereference_protected(htbl->buckets[hash_val],
						     lockdep_is_held(&priv->lock)));
	rcu_assign_pointer(htbl->buckets[hash_val], neigh);
	atomic_inc(&ntbl->entries);

out_unlock:

	return neigh;
}

void ipoib_neigh_dtor(struct ipoib_neigh *neigh)
{
	/* neigh reference count was dropprd to zero */
	struct net_device *dev = neigh->dev;
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	struct sk_buff *skb;

	if (neigh->ah)
		ipoib_put_ah(neigh->ah);

	spin_lock_irq(&priv->lock);
	while ((skb = __skb_dequeue(&neigh->queue))) {
		++dev->stats.tx_dropped;
		dev_kfree_skb_any(skb);
	}
	spin_unlock_irq(&priv->lock);

	if (ipoib_cm_get(neigh))
		ipoib_cm_destroy_tx(ipoib_cm_get(neigh));
	ipoib_dbg(netdev_priv(dev),
		  "neigh free for %06x %pI6\n",
		  IPOIB_QPN(neigh->daddr),
		  neigh->daddr + 4);
	kfree(neigh);
	if (atomic_dec_and_test(&priv->ntbl.entries)) {
		if (test_bit(IPOIB_NEIGH_TBL_FLUSH, &priv->flags))
			complete(&priv->ntbl.flushed);
	}
}

static void ipoib_neigh_reclaim(struct rcu_head *rp)
{
	/* Called as a result of removal from hash table */
	struct ipoib_neigh *neigh = container_of(rp, struct ipoib_neigh, rcu);
	/* note TX context may hold another ref */
	ipoib_neigh_put(neigh);
}

void ipoib_neigh_free(struct ipoib_neigh *neigh)
{
	struct net_device *dev = neigh->dev;
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	struct ipoib_neigh_table *ntbl = &priv->ntbl;
	struct ipoib_neigh_hash *htbl;
	struct ipoib_neigh __rcu **np;
	struct ipoib_neigh *n;
	u32 hash_val;

	htbl = rcu_dereference_protected(ntbl->htbl,
					lockdep_is_held(&priv->lock));
	if (!htbl)
		return;

	hash_val = ipoib_addr_hash(htbl, neigh->daddr);
	np = &htbl->buckets[hash_val];
	for (n = rcu_dereference_protected(*np,
					    lockdep_is_held(&priv->lock));
	     n != NULL;
	     n = rcu_dereference_protected(*np,
					lockdep_is_held(&priv->lock))) {
		if (n == neigh) {
			/* found */
			rcu_assign_pointer(*np,
					   rcu_dereference_protected(neigh->hnext,
								     lockdep_is_held(&priv->lock)));
			call_rcu(&neigh->rcu, ipoib_neigh_reclaim);
			return;
		} else {
			np = &n->hnext;
		}
	}
}

static int ipoib_neigh_hash_init(struct ipoib_dev_priv *priv)
{
	struct ipoib_neigh_table *ntbl = &priv->ntbl;
	struct ipoib_neigh_hash *htbl;
	struct ipoib_neigh **buckets;
	u32 size;

	clear_bit(IPOIB_NEIGH_TBL_FLUSH, &priv->flags);
	ntbl->htbl = NULL;
	htbl = kzalloc(sizeof(*htbl), GFP_KERNEL);
	if (!htbl)
		return -ENOMEM;
	set_bit(IPOIB_STOP_NEIGH_GC, &priv->flags);
	size = roundup_pow_of_two(arp_tbl.gc_thresh3);
	buckets = kzalloc(size * sizeof(*buckets), GFP_KERNEL);
	if (!buckets) {
		kfree(htbl);
		return -ENOMEM;
	}
	htbl->size = size;
	htbl->mask = (size - 1);
	htbl->buckets = buckets;
	ntbl->htbl = htbl;
	htbl->ntbl = ntbl;
	atomic_set(&ntbl->entries, 0);

	/* start garbage collection */
	clear_bit(IPOIB_STOP_NEIGH_GC, &priv->flags);
	queue_delayed_work(ipoib_workqueue, &priv->neigh_reap_task,
			   IPOIB_NEIGH_GC_TIME);

	return 0;
}

static void neigh_hash_free_rcu(struct rcu_head *head)
{
	struct ipoib_neigh_hash *htbl = container_of(head,
						    struct ipoib_neigh_hash,
						    rcu);
	struct ipoib_neigh __rcu **buckets = htbl->buckets;
	struct ipoib_neigh_table *ntbl = htbl->ntbl;

	kfree(buckets);
	kfree(htbl);
	complete(&ntbl->deleted);
}

void ipoib_del_neighs_by_gid(struct net_device *dev, u8 *gid)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	struct ipoib_neigh_table *ntbl = &priv->ntbl;
	struct ipoib_neigh_hash *htbl;
	unsigned long flags;
	int i;

	/* remove all neigh connected to a given path or mcast */
	spin_lock_irqsave(&priv->lock, flags);

	htbl = rcu_dereference_protected(ntbl->htbl,
					 lockdep_is_held(&priv->lock));

	if (!htbl)
		goto out_unlock;

	for (i = 0; i < htbl->size; i++) {
		struct ipoib_neigh *neigh;
		struct ipoib_neigh __rcu **np = &htbl->buckets[i];

		while ((neigh = rcu_dereference_protected(*np,
							  lockdep_is_held(&priv->lock))) != NULL) {
			/* delete neighs belong to this parent */
			if (!memcmp(gid, neigh->daddr + 4, sizeof (union ib_gid))) {
				rcu_assign_pointer(*np,
						   rcu_dereference_protected(neigh->hnext,
									     lockdep_is_held(&priv->lock)));
				/* remove from parent list */
				list_del_init(&neigh->list);
				call_rcu(&neigh->rcu, ipoib_neigh_reclaim);
			} else {
				np = &neigh->hnext;
			}

		}
	}
out_unlock:
	spin_unlock_irqrestore(&priv->lock, flags);
}

static void ipoib_flush_neighs(struct ipoib_dev_priv *priv)
{
	struct ipoib_neigh_table *ntbl = &priv->ntbl;
	struct ipoib_neigh_hash *htbl;
	unsigned long flags;
	int i, wait_flushed = 0;

	init_completion(&priv->ntbl.flushed);

	spin_lock_irqsave(&priv->lock, flags);

	htbl = rcu_dereference_protected(ntbl->htbl,
					lockdep_is_held(&priv->lock));
	if (!htbl)
		goto out_unlock;

	wait_flushed = atomic_read(&priv->ntbl.entries);
	if (!wait_flushed)
		goto free_htbl;

	for (i = 0; i < htbl->size; i++) {
		struct ipoib_neigh *neigh;
		struct ipoib_neigh __rcu **np = &htbl->buckets[i];

		while ((neigh = rcu_dereference_protected(*np,
				       lockdep_is_held(&priv->lock))) != NULL) {
			rcu_assign_pointer(*np,
					   rcu_dereference_protected(neigh->hnext,
								     lockdep_is_held(&priv->lock)));
			/* remove from path/mc list */
			list_del_init(&neigh->list);
			call_rcu(&neigh->rcu, ipoib_neigh_reclaim);
		}
	}

free_htbl:
	rcu_assign_pointer(ntbl->htbl, NULL);
	call_rcu(&htbl->rcu, neigh_hash_free_rcu);

out_unlock:
	spin_unlock_irqrestore(&priv->lock, flags);
	if (wait_flushed)
		wait_for_completion(&priv->ntbl.flushed);
}

static void ipoib_neigh_hash_uninit(struct net_device *dev)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	int stopped;

	ipoib_dbg(priv, "ipoib_neigh_hash_uninit\n");
	init_completion(&priv->ntbl.deleted);
	set_bit(IPOIB_NEIGH_TBL_FLUSH, &priv->flags);

	/* Stop GC if called at init fail need to cancel work */
	stopped = test_and_set_bit(IPOIB_STOP_NEIGH_GC, &priv->flags);
	if (!stopped)
		cancel_delayed_work_sync(&priv->neigh_reap_task);

	ipoib_flush_neighs(priv);

	wait_for_completion(&priv->ntbl.deleted);
}




static void ipoib_set_default_moderation(struct ipoib_dev_priv *priv)
{
	struct ipoib_recv_ring *rx_ring;
	int i;
	/* If we haven't received a specific coalescing setting
	 * (module param), we set the moderation parameters as follows:
	 * - moder_cnt is set to the number of mtu sized packets to
	 *   satisfy our coaelscing target.
	 * - moder_time is set to a fixed value.
	 */
	priv->ethtool.rx_max_coalesced_frames = IPOIB_RX_COAL_TARGET;
	priv->ethtool.rx_coalesce_usecs = IPOIB_RX_COAL_TIME;
	printk(KERN_ERR "Default coalesing params for mtu:%d - "
			   "rx_frames:%d rx_usecs:%d\n",
	       priv->dev->mtu, priv->ethtool.rx_max_coalesced_frames,
	       priv->ethtool.rx_coalesce_usecs);

	/* Reset auto-moderation params */
	priv->ethtool.pkt_rate_low = IPOIB_RX_RATE_LOW;
	priv->ethtool.rx_coalesce_usecs_low = IPOIB_RX_COAL_TIME_LOW;
	priv->ethtool.pkt_rate_high = IPOIB_RX_RATE_HIGH;
	priv->ethtool.rx_coalesce_usecs_high = IPOIB_RX_COAL_TIME_HIGH;
	priv->ethtool.sample_interval = IPOIB_SAMPLE_INTERVAL;
	priv->ethtool.use_adaptive_rx_coalesce = 1;

	priv->ethtool.pkt_rate_low_per_ring = priv->ethtool.pkt_rate_low;
	priv->ethtool.pkt_rate_high_per_ring = priv->ethtool.pkt_rate_high;
	rx_ring = priv->recv_ring;
	for (i = 0; i < priv->num_rx_queues; i++) {
		rx_ring->ethtool.last_moder_time = IPOIB_AUTO_CONF;
		rx_ring->ethtool.last_moder_jiffies = 0;
		rx_ring->ethtool.last_moder_packets = 0;
		rx_ring->ethtool.last_moder_tx_packets = 0;
		rx_ring->ethtool.last_moder_bytes = 0;
		rx_ring++;
	}
}
/*
The function classifies the incoming traffic during each sampling interval
into classes. The rx_usec value (i.e., moderation time) is then adjusted
appropriately per class.
There are two classes defined:
	A. Bulk traffic: for heavy traffic consisting of packets of normal size.
	This class is further divided into two sub-classes:
		1. Traffic that is mainly BW bound
		- This traffic will get maximum moderation.
		2. Traffic that is mostly latency bound
		- For situations where low latency is vital
		- The rx_usec will be changed to a value in the range:
		(ethtool.pkt_rate_low  .. ethtool.pkt_rate_high)
		depending on sampled packet rate.
	B.  Low latency traffic: for minimal traffic, or small packets.
	- This traffic will get minimum moderation.
*/


static void ipoib_auto_moderation_ring(struct ipoib_recv_ring *rx_ring,
					struct ipoib_dev_priv *priv)
{
	unsigned long period = jiffies - rx_ring->ethtool.last_moder_jiffies;
	unsigned long rate;
	unsigned long avg_pkt_size;
	unsigned long rx_packets;
	unsigned long rx_bytes;
	unsigned long rx_pkt_diff;
	int moder_time;
	int ret;

	rx_packets = rx_ring->stats.rx_packets;
	rx_bytes = rx_ring->stats.rx_bytes;
	rx_pkt_diff = rx_packets - rx_ring->ethtool.last_moder_packets;
	rate = rx_pkt_diff * HZ / period;
	avg_pkt_size = rx_pkt_diff ?
		(rx_bytes - rx_ring->ethtool.last_moder_bytes) / rx_pkt_diff : 0;

	/* Apply auto-moderation only when packet rate exceeds a rate that
	 * it matters */
	if (rate > (IPOIB_RX_RATE_THRESH / priv->num_rx_queues)
			&& avg_pkt_size > IPOIB_AVG_PKT_SMALL) {
		/* If tx and rx packet rates are not balanced
		* (probably TCP stream, big data and small acks),
		* assume that traffic is mainly BW bound (maximum moderation).
		* Otherwise, moderate according to packet rate */

		if (rate < priv->ethtool.pkt_rate_low_per_ring)
			moder_time =
			priv->ethtool.rx_coalesce_usecs_low;
		else if (rate > priv->ethtool.pkt_rate_high_per_ring)
			moder_time =
				priv->ethtool.rx_coalesce_usecs_high;
		else
			moder_time =
			(rate - priv->ethtool.pkt_rate_low_per_ring) *
			(priv->ethtool.rx_coalesce_usecs_high -
			priv->ethtool.rx_coalesce_usecs_low) /
			(priv->ethtool.pkt_rate_high_per_ring -
			priv->ethtool.pkt_rate_low_per_ring) +
			priv->ethtool.rx_coalesce_usecs_low;

	} else
		moder_time = priv->ethtool.rx_coalesce_usecs_low;
	if (moder_time != rx_ring->ethtool.last_moder_time) {
		struct ib_cq_attr  attr;

		memset(&attr, 0, sizeof(attr));
		attr.moderation.cq_count = priv->ethtool.rx_max_coalesced_frames;
		attr.moderation.cq_period = moder_time;
		ipoib_dbg(priv, "%s: Rx moder_time changed from:%d to %d\n",
			__func__, rx_ring->ethtool.last_moder_time,
				moder_time);
		rx_ring->ethtool.last_moder_time = moder_time;
		ret = ib_modify_cq(rx_ring->recv_cq,
				   &attr,
				   IB_CQ_MODERATION);

		if (ret && ret != -ENOSYS)
			ipoib_warn(priv, "%s: failed modifying CQ (%d)\n",
			__func__, ret);

	}
	rx_ring->ethtool.last_moder_packets = rx_packets;
	rx_ring->ethtool.last_moder_bytes = rx_bytes;
	rx_ring->ethtool.last_moder_jiffies = jiffies;

}

static void ipoib_auto_moderation(struct ipoib_dev_priv *priv)
{
	int i;

	if (!priv->ethtool.use_adaptive_rx_coalesce)
		return;

	/* iterate over all the rings */
	for (i = 0; i < priv->num_rx_queues; i++)
		ipoib_auto_moderation_ring(&priv->recv_ring[i], priv);
}

static void ipoib_config_adapt_moder(struct work_struct *work)
{
	struct delayed_work *delay = to_delayed_work(work);
	struct ipoib_dev_priv *priv = container_of(delay,
						   struct ipoib_dev_priv,
						   adaptive_moder_task);

	if (!(netif_running(priv->dev) && netif_carrier_ok(priv->dev))) {
		ipoib_dbg(priv, "%s: port is not ACTIVE, no configuration"
				" for adaptive moderation\n",
			  __func__);
		return;
	}

	ipoib_auto_moderation(priv);

	if (test_bit(IPOIB_FLAG_AUTO_MODER, &priv->flags) &&
		priv->ethtool.use_adaptive_rx_coalesce)
		queue_delayed_work(ipoib_auto_moder_workqueue,
			&priv->adaptive_moder_task,
			ADAPT_MODERATION_DELAY);
}

int ipoib_dev_init(struct net_device *dev, struct ib_device *ca, int port)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	struct ipoib_send_ring *send_ring;
	struct ipoib_recv_ring *recv_ring;
	int i, rx_allocated, tx_allocated;
	unsigned long alloc_size;

	if (ipoib_neigh_hash_init(priv) < 0)
		goto out;
	/* Allocate RX/TX "rings" to hold queued skbs */
	/* Multi queue initialization */
	priv->recv_ring = kzalloc(priv->num_rx_queues * sizeof(*recv_ring),
				GFP_KERNEL);
	if (!priv->recv_ring) {
		pr_warn("%s: failed to allocate RECV ring (%d entries)\n",
			ca->name, priv->num_rx_queues);
		goto out_neigh_hash_cleanup;
	}

	alloc_size = priv->recvq_size * sizeof(*recv_ring->rx_ring);
	rx_allocated = 0;
	recv_ring = priv->recv_ring;
	for (i = 0; i < priv->num_rx_queues; i++) {
		recv_ring->rx_ring = kzalloc(alloc_size, GFP_KERNEL);
		if (!recv_ring->rx_ring) {
			pr_warn("%s: failed to allocate RX ring (%d entries)\n",
			ca->name, priv->recvq_size);
			goto out_recv_ring_cleanup;
		}
		recv_ring->dev = dev;
		recv_ring->index = i;
#ifdef CONFIG_COMPAT_LRO_ENABLED
		ipoib_lro_setup(recv_ring, priv);
#endif
		recv_ring++;
		rx_allocated++;
	}

	priv->send_ring = kzalloc(priv->num_tx_queues * sizeof(*send_ring),
			GFP_KERNEL);
	if (!priv->send_ring) {
		pr_warn("%s: failed to allocate SEND ring (%d entries)\n",
			ca->name, priv->num_tx_queues);
		goto out_recv_ring_cleanup;
	}

	alloc_size = priv->sendq_size * sizeof(*send_ring->tx_ring);
	tx_allocated = 0;
	send_ring = priv->send_ring;
	for (i = 0; i < priv->num_tx_queues; i++) {
		send_ring->tx_ring = vzalloc(alloc_size);
		if (!send_ring->tx_ring) {
			pr_warn("%s: failed to allocate TX ring (%d entries)\n",
			ca->name, priv->sendq_size);
			goto out_send_ring_cleanup;
		}
		send_ring->dev = dev;
		send_ring->index = i;
		send_ring++;
		tx_allocated++;
	}

	/* priv->tx_head, tx_tail & tx_outstanding are already 0 */

	if (ipoib_ib_dev_init(dev, ca, port))
		goto out_send_ring_cleanup;


	ipoib_set_default_moderation(priv);
	/* access to rings allowed */
	up_write(&priv->rings_rwsem);

	/* For resiliency we ignore promics_mc_init failure */
	ipoib_promisc_mc_init(&priv->promisc);
	ipoib_genl_intf_init(&priv->netlink);
	ipoib_genl_intf_add(&priv->netlink);
	return 0;

out_send_ring_cleanup:
	for (i = 0; i < tx_allocated; i++)
		vfree(priv->send_ring[i].tx_ring);
	kfree(priv->send_ring);

out_recv_ring_cleanup:
	for (i = 0; i < rx_allocated; i++)
		kfree(priv->recv_ring[i].rx_ring);
	kfree(priv->recv_ring);

out_neigh_hash_cleanup:
	ipoib_neigh_hash_uninit(dev);
out:
	priv->send_ring = NULL;
	priv->recv_ring = NULL;

	return -ENOMEM;
}

void ipoib_dev_uninit(struct net_device *dev)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	int i;
	LIST_HEAD(head);

	ASSERT_RTNL();

	ipoib_genl_intf_del(&priv->netlink);
	ipoib_promisc_mc_stop(&priv->promisc);
	ipoib_promisc_mc_destroy(&priv->promisc);

	if (dev->reg_state != NETREG_UNINITIALIZED)
		ipoib_neigh_hash_uninit(dev);

	ipoib_ib_dev_cleanup(dev);

	/* no more access to rings */
	down_write(&priv->rings_rwsem);

	for (i = 0; i < priv->num_tx_queues; i++)
		vfree(priv->send_ring[i].tx_ring);
	kfree(priv->send_ring);

	for (i = 0; i < priv->num_rx_queues; i++)
		kfree(priv->recv_ring[i].rx_ring);
	kfree(priv->recv_ring);

	priv->recv_ring = NULL;
	priv->send_ring = NULL;
}

void ipoib_dev_cleanup(struct net_device *dev)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev), *cpriv, *tcpriv;

	LIST_HEAD(head);

	ASSERT_RTNL();

	ipoib_delete_debug_files(dev);

	/* Delete any child interfaces first */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)
        list_for_each_entry_safe_reverse(cpriv, tcpriv,
                                         &priv->child_intfs, list)
                unregister_netdevice_queue(cpriv->dev, &head);

	/*
 	 * the next function calls the ipoib_uninit which calls for
 	 * ipoib_dev_cleanup for each devices at the head list.
 	 */

        unregister_netdevice_many(&head);
#else
        list_for_each_entry_safe(cpriv, tcpriv,
				 &priv->child_intfs, list)
		unregister_netdevice(cpriv->dev);
#endif

	ipoib_dev_uninit(dev);
	/* ipoib_dev_uninit took rings lock can't release in case of reinit */
	up_write(&priv->rings_rwsem);
}

int ipoib_reinit(struct net_device *dev, int num_rx, int num_tx)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	int flags;
	int ret;

	flags = dev->flags;

	dev_close(dev);

	if (!test_bit(IPOIB_FLAG_SUBINTERFACE, &priv->flags)) {
		spin_lock_irq(&priv->lock);
		if (test_and_clear_bit(IPOIB_FLAG_EVENTS_REGISTERED, &priv->flags)) {
			ib_unregister_event_handler(&priv->event_handler);
		} else {
			/* no need to continue with the function,
			 * the driver is going down anyway,
			 */
			spin_unlock_irq(&priv->lock);
			pr_warn("%s %s: port %d module is going down\n",
				__func__, priv->ca->name, priv->port);
			return -EBUSY;
		}
		spin_unlock_irq(&priv->lock);
	}

	ipoib_dev_uninit(dev);
	priv->num_rx_queues = num_rx;
	priv->num_tx_queues = num_tx;
	if (num_rx == 1)
		priv->rss_qp_num = 0;
	else
		priv->rss_qp_num = num_rx;
	if (num_tx == 1 || !(priv->hca_caps & IB_DEVICE_UD_TSS))
		priv->tss_qp_num = num_tx - 1;
	else
		priv->tss_qp_num = num_tx;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)) || defined (CONFIG_COMPAT_IS_NUM_TX_QUEUES)
	netif_set_real_num_tx_queues(dev, num_tx);
#endif
	netif_set_real_num_rx_queues(dev, num_rx);
	/*
	 * prevent ipoib_ib_dev_init call ipoib_ib_dev_open
	 * let ipoib_open do it
	 */
	dev->flags &= ~IFF_UP;
	ret = ipoib_dev_init(dev, priv->ca, priv->port);
	if (ret) {
		pr_warn("%s: failed to reinitialize port %d (ret = %d)\n",
		       priv->ca->name, priv->port, ret);
		INIT_IB_EVENT_HANDLER(&priv->event_handler,
				      priv->ca, ipoib_event);
		return ret;
	}
	if (!test_bit(IPOIB_FLAG_SUBINTERFACE, &priv->flags)) {
		spin_lock_irq(&priv->lock);
		if (!test_and_set_bit(IPOIB_FLAG_EVENTS_REGISTERED, &priv->flags))
			ret = ib_register_event_handler(&priv->event_handler);
		spin_unlock_irq(&priv->lock);

		if (ret)
			pr_warn("%s: failed to rereg port %d (ret = %d)\n",
			priv->ca->name, priv->port, ret);
	}
	/* if the device was up bring it up again */
	if (flags & IFF_UP) {
		ret = dev_open(dev);
		if (ret)
			pr_warn("%s: failed to reopen port %d (ret = %d)\n",
			       priv->ca->name, priv->port, ret);
	}
	return ret;
}

#ifdef CONFIG_COMPAT_LRO_ENABLED
static int get_skb_hdr(struct sk_buff *skb, void **iphdr,
			void **tcph, u64 *hdr_flags, void *priv)
{
	unsigned int ip_len;
	struct iphdr *iph;

	if (unlikely(skb->protocol != htons(ETH_P_IP)))
		return -1;

	/*
	* In the future we may add an else clause that verifies the
	* checksum and allows devices which do not calculate checksum
	* to use LRO.
	*/
	if (unlikely(skb->ip_summed != CHECKSUM_UNNECESSARY))
		return -1;

	/* Check for non-TCP packet */
	skb_reset_network_header(skb);
	iph = ip_hdr(skb);
	if (iph->protocol != IPPROTO_TCP)
		return -1;

	ip_len = ip_hdrlen(skb);
	skb_set_transport_header(skb, ip_len);
	*tcph = tcp_hdr(skb);

	/* check if IP header and TCP header are complete */
	if (ntohs(iph->tot_len) < ip_len + tcp_hdrlen(skb))
		return -1;

	*hdr_flags = LRO_IPV4 | LRO_TCP;
	*iphdr = iph;

	return 0;
}


static void ipoib_lro_setup(struct ipoib_recv_ring *recv_ring,
				struct ipoib_dev_priv *priv)
{
	recv_ring->lro.lro_mgr.max_aggr  = lro_max_aggr;
	recv_ring->lro.lro_mgr.max_desc  = IPOIB_MAX_LRO_DESCRIPTORS;
	recv_ring->lro.lro_mgr.lro_arr   = recv_ring->lro.lro_desc;
	recv_ring->lro.lro_mgr.get_skb_header = get_skb_hdr;
	recv_ring->lro.lro_mgr.features  = LRO_F_NAPI;
	recv_ring->lro.lro_mgr.dev               = priv->dev;
	recv_ring->lro.lro_mgr.ip_summed_aggr = CHECKSUM_UNNECESSARY;
}

void set_lro_features_bit(struct ipoib_dev_priv *priv)
{
	int hw_support_lro = 0; 
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39))
		hw_support_lro = priv->dev->hw_features & NETIF_F_RXCSUM;
#else
		hw_support_lro = (priv->dev->features & NETIF_F_RXCSUM);
#endif
	if (lro && hw_support_lro) {
		priv->dev->features |= NETIF_F_LRO;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0))
		priv->dev->hw_features |= NETIF_F_LRO;
		priv->dev->wanted_features |= NETIF_F_LRO;
#endif
	}
}
#endif

static const struct header_ops ipoib_header_ops = {
	.create	= ipoib_hard_header,
};

static const struct net_device_ops ipoib_netdev_ops_no_tss = {
	.ndo_uninit		 = ipoib_uninit,
	.ndo_open		 = ipoib_open,
	.ndo_stop		 = ipoib_stop,
	.ndo_change_mtu		 = ipoib_change_mtu,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39))
	.ndo_fix_features	 = ipoib_fix_features,
#endif
	.ndo_start_xmit	 	 = ipoib_start_xmit,
	.ndo_tx_timeout		 = ipoib_timeout,
	.ndo_get_stats		= ipoib_get_stats,
	.ndo_set_rx_mode	 = ipoib_set_mcast_list,
};

static const struct net_device_ops ipoib_netdev_ops_hw_tss = {
	.ndo_uninit		 = ipoib_uninit,
	.ndo_open	= ipoib_open,
	.ndo_stop	= ipoib_stop,
	.ndo_change_mtu		= ipoib_change_mtu,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39))
	.ndo_fix_features		= ipoib_fix_features,
#endif
	.ndo_start_xmit		= ipoib_start_xmit,
	.ndo_select_queue		= ipoib_select_queue_hw,
	.ndo_tx_timeout		= ipoib_timeout,
	.ndo_get_stats		= ipoib_get_stats,
	.ndo_set_rx_mode		= ipoib_set_mcast_list,
};

static const struct net_device_ops ipoib_netdev_ops_sw_tss = {
	.ndo_uninit		 = ipoib_uninit,
	.ndo_open	= ipoib_open,
	.ndo_stop	= ipoib_stop,
	.ndo_change_mtu		= ipoib_change_mtu,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39))
	.ndo_fix_features		= ipoib_fix_features,
#endif
	.ndo_start_xmit		= ipoib_start_xmit,
	.ndo_select_queue		= ipoib_select_queue_sw,
	.ndo_tx_timeout		= ipoib_timeout,
	.ndo_get_stats		= ipoib_get_stats,
	.ndo_set_rx_mode		= ipoib_set_mcast_list,
};


static const struct net_device_ops *ipoib_netdev_ops;

void ipoib_setup(struct net_device *dev)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);

	/* Use correct ops (ndo_select_queue) */
	dev->netdev_ops		 = ipoib_netdev_ops;
	dev->header_ops		 = &ipoib_header_ops;

	ipoib_set_ethtool_ops(dev);

	dev->watchdog_timeo	 = 5 * HZ;

	dev->flags		|= IFF_BROADCAST | IFF_MULTICAST;

	dev->hard_header_len	 = IPOIB_ENCAP_LEN;
	dev->addr_len		 = INFINIBAND_ALEN;
	dev->type		 = ARPHRD_INFINIBAND;
	dev->tx_queue_len        = ipoib_sendq_size * 2;
	dev->features		 = (NETIF_F_VLAN_CHALLENGED	|
				    NETIF_F_HIGHDMA);
	dev->priv_flags		&= ~IFF_XMIT_DST_RELEASE;

	memcpy(dev->broadcast, ipv4_bcast_addr, INFINIBAND_ALEN);

	netif_carrier_off(dev);

	priv->dev = dev;

	spin_lock_init(&priv->lock);

	init_rwsem(&priv->vlan_rwsem);

	mutex_init(&priv->state_lock);
	mutex_init(&priv->ring_qp_lock);

	init_rwsem(&priv->rings_rwsem);
	/* read access to rings is disabled */
	down_write(&priv->rings_rwsem);

	INIT_LIST_HEAD(&priv->path_list);
	INIT_LIST_HEAD(&priv->child_intfs);
	INIT_LIST_HEAD(&priv->dead_ahs);
	INIT_LIST_HEAD(&priv->multicast_list);

	INIT_DELAYED_WORK(&priv->mcast_task,   ipoib_mcast_join_task);
	INIT_WORK(&priv->carrier_on_task, ipoib_mcast_carrier_on_task);
	INIT_WORK(&priv->flush_light,   ipoib_ib_dev_flush_light);
	INIT_WORK(&priv->flush_normal,   ipoib_ib_dev_flush_normal);
	INIT_WORK(&priv->flush_heavy,   ipoib_ib_dev_flush_heavy);
	INIT_WORK(&priv->restart_task, ipoib_mcast_restart_task);
	INIT_DELAYED_WORK(&priv->ah_reap_task, ipoib_reap_ah);
	INIT_DELAYED_WORK(&priv->neigh_reap_task, ipoib_reap_neigh);
	INIT_DELAYED_WORK(&priv->adaptive_moder_task, ipoib_config_adapt_moder);

}

struct ipoib_dev_priv *ipoib_intf_alloc(const char *name,
					struct ipoib_dev_priv *template_priv)
{
	struct net_device *dev;

	/* Use correct ops (ndo_select_queue) pass to ipoib_setup
	 * A child interface starts with the same numebr of queues
	 * as the parent but even if the parnet curently, has only
	 * one ring the MQ potential must be reserved
	 */
	if (template_priv->max_tx_queues > 1) {
		if (template_priv->hca_caps & IB_DEVICE_UD_TSS)
			ipoib_netdev_ops = &ipoib_netdev_ops_hw_tss;
		else
			ipoib_netdev_ops = &ipoib_netdev_ops_sw_tss;
	} else
		ipoib_netdev_ops = &ipoib_netdev_ops_no_tss;

	dev = alloc_netdev_mqs((int) sizeof(struct ipoib_dev_priv), name,
			   ipoib_setup,
			   template_priv->max_tx_queues,
			   template_priv->max_rx_queues);
	if (!dev)
		return NULL;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)) || defined (CONFIG_COMPAT_IS_NUM_TX_QUEUES)
	netif_set_real_num_tx_queues(dev, template_priv->num_tx_queues);
#endif
	netif_set_real_num_rx_queues(dev, template_priv->num_rx_queues);

	return netdev_priv(dev);
}

static ssize_t show_pkey(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct ipoib_dev_priv *priv = netdev_priv(to_net_dev(dev));

	return sprintf(buf, "0x%04x\n", priv->pkey);
}
static DEVICE_ATTR(pkey, S_IRUGO, show_pkey, NULL);

static ssize_t show_umcast(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct ipoib_dev_priv *priv = netdev_priv(to_net_dev(dev));

	return sprintf(buf, "%d\n", test_bit(IPOIB_FLAG_UMCAST, &priv->flags));
}

void ipoib_set_umcast(struct net_device *ndev, int umcast_val)
{
	struct ipoib_dev_priv *priv = netdev_priv(ndev);

	if (umcast_val > 0) {
		set_bit(IPOIB_FLAG_UMCAST, &priv->flags);
		ipoib_warn(priv, "ignoring multicast groups joined directly "
				"by userspace\n");
	} else
		clear_bit(IPOIB_FLAG_UMCAST, &priv->flags);
}

static ssize_t set_umcast(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf, size_t count)
{
	unsigned long umcast_val = simple_strtoul(buf, NULL, 0);

	ipoib_set_umcast(to_net_dev(dev), umcast_val);

	return count;
}
static DEVICE_ATTR(umcast, S_IWUSR | S_IRUGO, show_umcast, set_umcast);

int ipoib_add_umcast_attr(struct net_device *dev)
{
	return device_create_file(&dev->dev, &dev_attr_umcast);
}

int parse_child(struct device *dev, const char *buf, int *pkey,
		int *child_index)
{
	int ret;
	struct ipoib_dev_priv *priv = netdev_priv(to_net_dev(dev));

	*pkey = *child_index = -1;

	/* 'pkey' or 'pkey.child_index' or '.child_index' are allowed */
	ret = sscanf(buf, "%i.%i", pkey, child_index);
	if (ret == 1)  /* just pkey, implicit child index is 0 */
		*child_index = 0;
	else  if (ret != 2) { /* pkey same as parent, specified child index */
		*pkey = priv->pkey;
		ret  = sscanf(buf, ".%i", child_index);
		if (ret != 1 || *child_index == 0)
			return -EINVAL;
	}

	if (*child_index < 0 || *child_index > 0xff)
		return -EINVAL;

	if (*pkey <= 0 || *pkey > 0xffff || *pkey == 0x8000)
		return -EINVAL;

	ipoib_dbg(priv, "parse_child inp %s out pkey %04x index %d\n",
		buf, *pkey, *child_index);
	return 0;
}

static ssize_t create_child(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	int pkey, child_index;
	int ret;

	if (parse_child(dev, buf, &pkey, &child_index))
		return -EINVAL;

	/*
	 * Set the full membership bit, so that we join the right
	 * broadcast group, etc.
	 */
	pkey |= 0x8000;

	ret = ipoib_vlan_add(to_net_dev(dev), pkey, child_index);

	return ret ? ret : count;
}
static DEVICE_ATTR(create_child, S_IWUSR, NULL, create_child);

static ssize_t delete_child(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	int pkey, child_index;
	int ret;

	if (parse_child(dev, buf, &pkey, &child_index))
		return -EINVAL;

	ret = ipoib_vlan_delete(to_net_dev(dev), pkey, child_index);

	return ret ? ret : count;

}
static DEVICE_ATTR(delete_child, S_IWUSR, NULL, delete_child);

int ipoib_add_pkey_attr(struct net_device *dev)
{
	return device_create_file(&dev->dev, &dev_attr_pkey);
}

static ssize_t get_rx_chan(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct ipoib_dev_priv *priv = netdev_priv(to_net_dev(dev));

	return sprintf(buf, "%d\n", priv->num_rx_queues);
}

static ssize_t set_rx_chan(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct net_device *ndev = to_net_dev(dev);
	struct ipoib_dev_priv *priv = netdev_priv(ndev);
	int val, ret;

	ret = sscanf(buf, "%d", &val);
	if (ret != 1)
		return -EINVAL;
	if (val == 0 || val > priv->max_rx_queues)
		return -EINVAL;
	/* Nothing to do ? */
	if (val == priv->num_rx_queues)
		return count;
	if (!is_power_of_2(val))
		return -EINVAL;

	if (!rtnl_trylock())
		return restart_syscall();

	ret = ipoib_reinit(ndev, val, priv->num_tx_queues);

	rtnl_unlock();

	if (ret)
		return ret;

	return count;
}

static DEVICE_ATTR(rx_channels, S_IWUSR | S_IRUGO, get_rx_chan, set_rx_chan);

static ssize_t get_rx_max_channel(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct ipoib_dev_priv *priv = netdev_priv(to_net_dev(dev));

	return sprintf(buf, "%d\n", priv->max_rx_queues);
}

static DEVICE_ATTR(rx_max_channels, S_IRUGO, get_rx_max_channel, NULL);

static ssize_t get_tx_chan(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct ipoib_dev_priv *priv = netdev_priv(to_net_dev(dev));
	return sprintf(buf, "%d\n", priv->num_tx_queues);
}

static ssize_t set_tx_chan(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct net_device *ndev = to_net_dev(dev);
	struct ipoib_dev_priv *priv = netdev_priv(ndev);
	int val, ret;

	ret = sscanf(buf, "%d", &val);
	if (ret != 1)
		return -EINVAL;
	if (val == 0 || val > priv->max_tx_queues)
		return -EINVAL;
	/* Nothing to do ? */
	if (val == priv->num_tx_queues)
		return count;

	/* 1 is always O.K. */
	if (val > 1) {
		if (priv->hca_caps & IB_DEVICE_UD_TSS) {
			/* with HW TSS tx_count is 2^N */
			if (!is_power_of_2(val))
				return -EINVAL;
		} else {
			/*
			* with SW TSS tx_count = 1 + 2 ^ N.
			* 2 is not allowed, makes no sense,
			* if want to disable TSS use 1.
			*/
			if (!is_power_of_2(val - 1) || val == 2)
				return -EINVAL;
		}
	}

	if (!rtnl_trylock())
		return restart_syscall();

	ret = ipoib_reinit(ndev, priv->num_rx_queues, val);

	rtnl_unlock();

	if (ret)
		return ret;

	return count;
}

static DEVICE_ATTR(tx_channels, S_IWUSR | S_IRUGO, get_tx_chan, set_tx_chan);

static ssize_t get_tx_max_channel(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct ipoib_dev_priv *priv = netdev_priv(to_net_dev(dev));

	return sprintf(buf, "%d\n", priv->max_tx_queues);
}

static DEVICE_ATTR(tx_max_channels, S_IRUGO, get_tx_max_channel, NULL);

int ipoib_add_channels_attr(struct net_device *dev)
{
	int err = 0;
	err = device_create_file(&dev->dev, &dev_attr_tx_max_channels);
	if (err)
		return err;
	err = device_create_file(&dev->dev, &dev_attr_rx_max_channels);
	if (err)
		return err;
	err = device_create_file(&dev->dev, &dev_attr_tx_channels);
	if (err)
		return err;
	err = device_create_file(&dev->dev, &dev_attr_rx_channels);
	if (err)
		return err;
	return err;
}

static int ipoib_get_hca_features(struct ipoib_dev_priv *priv,
				  struct ib_device *hca)
{
	struct ib_device_attr *device_attr;
	int num_cores;
	int result = -ENOMEM;

	device_attr = kmalloc(sizeof *device_attr, GFP_KERNEL);
	if (!device_attr) {
		printk(KERN_WARNING "%s: allocation of %zu bytes failed\n",
		       hca->name, sizeof *device_attr);
		return result;
	}

	result = ib_query_device(hca, device_attr);
	if (result) {
		printk(KERN_WARNING "%s: ib_query_device failed (ret = %d)\n",
		       hca->name, result);
		kfree(device_attr);
		return result;
	}
	priv->hca_caps = device_attr->device_cap_flags;

	num_cores = num_online_cpus();
	if (num_cores == 1 || !(priv->hca_caps & IB_DEVICE_QPG)) {
		/* No additional QP, only one QP for RX & TX */
		priv->rss_qp_num = 0;
		priv->tss_qp_num = 0;
		priv->max_rx_queues = 1;
		priv->max_tx_queues = 1;
		priv->num_rx_queues = 1;
		priv->num_tx_queues = 1;
		kfree(device_attr);
		return 0;
	}
	num_cores = roundup_pow_of_two(num_cores);
	if (priv->hca_caps & IB_DEVICE_UD_RSS) {
		int max_rss_tbl_sz;
		max_rss_tbl_sz = min(device_attr->max_rss_tbl_sz,
				     IPOIB_MAX_RX_QUEUES);
		max_rss_tbl_sz = min(num_cores, max_rss_tbl_sz);
		max_rss_tbl_sz = rounddown_pow_of_two(max_rss_tbl_sz);
		priv->rss_qp_num    = max_rss_tbl_sz;
		priv->max_rx_queues = max_rss_tbl_sz;
	} else {
		/* No additional QP, only the parent QP for RX */
		priv->rss_qp_num = 0;
		priv->max_rx_queues = 1;
	}
	priv->num_rx_queues = priv->max_rx_queues;

	kfree(device_attr);

	priv->tss_qp_num = min(num_cores, IPOIB_MAX_TX_QUEUES);
	if (priv->hca_caps & IB_DEVICE_UD_TSS)
		/* TSS is supported by HW */
		priv->max_tx_queues = priv->tss_qp_num;
	else
		/* If TSS is not support by HW use the parent QP for ARP */
		priv->max_tx_queues = priv->tss_qp_num + 1;

	priv->num_tx_queues = priv->max_tx_queues;

	return 0;
}

int ipoib_set_dev_features(struct ipoib_dev_priv *priv, struct ib_device *hca)
{
	int result;

	result = ipoib_get_hca_features(priv, hca);
	if (result)
		return result;

	if (priv->hca_caps & IB_DEVICE_UD_IP_CSUM) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39))
		priv->dev->hw_features = NETIF_F_SG |
			NETIF_F_IP_CSUM | NETIF_F_RXCSUM;

		if (priv->hca_caps & IB_DEVICE_UD_TSO)
			priv->dev->hw_features |= NETIF_F_TSO;

		priv->dev->features |= priv->dev->hw_features;
#else
		set_bit(IPOIB_FLAG_CSUM, &priv->flags);
		priv->dev->features |= NETIF_F_SG |
			NETIF_F_IP_CSUM | NETIF_F_RXCSUM;

		if (priv->hca_caps & IB_DEVICE_UD_TSO)
			priv->dev->features |= NETIF_F_TSO;
#endif
	}

	return 0;
}

static struct net_device *ipoib_add_port(const char *format,
					 struct ib_device *hca, u8 port)
{
	struct ipoib_dev_priv *priv, *template_priv;
	struct ib_port_attr attr;
	int result = -ENOMEM;

	template_priv = kmalloc(sizeof *template_priv, GFP_KERNEL);
	if (!template_priv)
		goto alloc_mem_failed1;

	if (ipoib_get_hca_features(template_priv, hca))
		goto device_query_failed;

	priv = ipoib_intf_alloc(format, template_priv);
	if (!priv) {
		kfree(template_priv);
		goto alloc_mem_failed2;
	}
	kfree(template_priv);

	SET_NETDEV_DEV(priv->dev, hca->dma_device);
	priv->dev->dev_id = port - 1;

	if (!ib_query_port(hca, port, &attr)) {
		priv->max_ib_mtu = ib_mtu_enum_to_int(attr.max_mtu);
	} else {
		printk(KERN_WARNING "%s: ib_query_port %d failed\n",
		       hca->name, port);
		goto device_init_failed;
	}

	/* Initial ring params*/
	priv->sendq_size = ipoib_sendq_size;
	priv->recvq_size = ipoib_recvq_size;

	/* MTU will be reset when mcast join happens */
	priv->dev->mtu  = IPOIB_UD_MTU(priv->max_ib_mtu);
	priv->mcast_mtu  = priv->admin_mtu = priv->dev->mtu;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0))
	priv->dev->neigh_priv_len = sizeof(struct ipoib_neigh);
#endif

	result = ib_query_pkey(hca, port, 0, &priv->pkey);
	if (result) {
		printk(KERN_WARNING "%s: ib_query_pkey port %d failed (ret = %d)\n",
		       hca->name, port, result);
		goto device_init_failed;
	}

	result = ipoib_set_dev_features(priv, hca);
	if (result) {
		printk(KERN_WARNING "%s: couldn't set features for ipoib port %d; error %d\n",
		       hca->name, port, result);
		goto device_init_failed;
	}

	/*
	 * Set the full membership bit, so that we join the right
	 * broadcast group, etc.
	 */
	priv->pkey |= 0x8000;

	priv->dev->broadcast[8] = priv->pkey >> 8;
	priv->dev->broadcast[9] = priv->pkey & 0xff;

	result = ib_query_gid(hca, port, 0, &priv->local_gid);
	if (result) {
		printk(KERN_WARNING "%s: ib_query_gid port %d failed (ret = %d)\n",
		       hca->name, port, result);
		goto device_init_failed;
	} else
		memcpy(priv->dev->dev_addr + 4, priv->local_gid.raw, sizeof (union ib_gid));

	result = ipoib_dev_init(priv->dev, hca, port);
	if (result < 0) {
		printk(KERN_WARNING "%s: failed to initialize port %d (ret = %d)\n",
		       hca->name, port, result);
		goto device_init_failed;
	}

	INIT_IB_EVENT_HANDLER(&priv->event_handler,
			      priv->ca, ipoib_event);
	spin_lock_irq(&priv->lock);
	if (!test_and_set_bit(IPOIB_FLAG_EVENTS_REGISTERED, &priv->flags))
		ib_register_event_handler(&priv->event_handler);
	spin_unlock_irq(&priv->lock);

	result = register_netdev(priv->dev);
	if (result) {
		printk(KERN_WARNING "%s: couldn't register ipoib port %d; error %d\n",
		       hca->name, port, result);
		goto register_failed;
	}

#ifdef CONFIG_COMPAT_LRO_ENABLED
	/*force lro on the dev->features, because the function
	register_netdev disable it according to our private lro*/
	set_lro_features_bit(priv);
#endif

	ipoib_create_debug_files(priv->dev);

	result = -ENOMEM;

	if (ipoib_cm_add_mode_attr(priv->dev))
		goto sysfs_failed;
	if (ipoib_add_pkey_attr(priv->dev))
		goto sysfs_failed;
	if (ipoib_add_umcast_attr(priv->dev))
		goto sysfs_failed;
	if (device_create_file(&priv->dev->dev, &dev_attr_create_child))
		goto sysfs_failed;
	if (device_create_file(&priv->dev->dev, &dev_attr_delete_child))
		goto sysfs_failed;
	if (ipoib_add_channels_attr(priv->dev))
		goto sysfs_failed;

	return priv->dev;

sysfs_failed:
	ipoib_delete_debug_files(priv->dev);
	unregister_netdev(priv->dev);

register_failed:
	spin_lock_irq(&priv->lock);
	if (test_and_clear_bit(IPOIB_FLAG_EVENTS_REGISTERED, &priv->flags))
		ib_unregister_event_handler(&priv->event_handler);
	spin_unlock_irq(&priv->lock);

	/* Stop GC if started before flush */
	set_bit(IPOIB_STOP_NEIGH_GC, &priv->flags);
	cancel_delayed_work(&priv->neigh_reap_task);
	flush_workqueue(ipoib_workqueue);

	rtnl_lock();
	ipoib_dev_cleanup(priv->dev);
	rtnl_unlock();

device_init_failed:
	free_netdev(priv->dev);

alloc_mem_failed2:
	return ERR_PTR(result);

device_query_failed:
	kfree(template_priv);

alloc_mem_failed1:
	return ERR_PTR(result);
}

static void ipoib_add_one(struct ib_device *device)
{
	struct list_head *dev_list;
	struct net_device *dev;
	struct ipoib_dev_priv *priv;
	int s, e, p;
	int is_error_init = 0;
	unsigned long flags;

	if (rdma_node_get_transport(device->node_type) != RDMA_TRANSPORT_IB)
		return;

	dev_list = kmalloc(sizeof *dev_list, GFP_KERNEL);
	if (!dev_list)
		return;

	INIT_LIST_HEAD(dev_list);

	if (device->node_type == RDMA_NODE_IB_SWITCH) {
		s = 0;
		e = 0;
	} else {
		s = 1;
		e = device->phys_port_cnt;
	}

	for (p = s; p <= e; ++p) {
		if (rdma_port_get_link_layer(device, p) != IB_LINK_LAYER_INFINIBAND)
			continue;
		dev = ipoib_add_port("ib%d", device, p);
		if (!IS_ERR(dev)) {
			priv = netdev_priv(dev);
			list_add_tail(&priv->list, dev_list);
		} else
			is_error_init = 1;
	}
	spin_lock_irqsave(&client_data_lock, flags);
	ib_set_client_data(device, &ipoib_client, dev_list);
	spin_unlock_irqrestore(&client_data_lock, flags);

	if (is_error_init) {
		printk(KERN_ERR "%s: Failed to init ib port, removing it\n",
		       __func__);
		ipoib_remove_one(device);
	}
}

static void ipoib_remove_one(struct ib_device *device)
{
	struct ipoib_dev_priv *priv, *tmp;
	struct list_head *dev_list;
	unsigned long flags;

	if (rdma_node_get_transport(device->node_type) != RDMA_TRANSPORT_IB)
		return;

	/*
	 * protect against calling ipoib_remove_one multiple times
	 * for same device.
	 */
	spin_lock_irqsave(&client_data_lock, flags);
	dev_list = ib_get_client_data(device, &ipoib_client);
	if (!dev_list) {
		spin_unlock_irqrestore(&client_data_lock, flags);
		return;
	}

	ib_set_client_data(device, &ipoib_client, NULL);
	spin_unlock_irqrestore(&client_data_lock, flags);

	list_for_each_entry_safe(priv, tmp, dev_list, list) {
		spin_lock_irq(&priv->lock);
		if (test_and_clear_bit(IPOIB_FLAG_EVENTS_REGISTERED, &priv->flags))
			ib_unregister_event_handler(&priv->event_handler);
		spin_unlock_irq(&priv->lock);

		rtnl_lock();
		dev_change_flags(priv->dev, priv->dev->flags & ~IFF_UP);
		rtnl_unlock();

		/* mark interface in the middle of destruction */
		set_bit(IPOIB_FLAG_INTF_ON_DESTROY, &priv->flags);

		/* Stop GC */
		set_bit(IPOIB_STOP_NEIGH_GC, &priv->flags);
		cancel_delayed_work(&priv->neigh_reap_task);
		flush_workqueue(ipoib_workqueue);
		flush_workqueue(ipoib_auto_moder_workqueue);
		unregister_netdev(priv->dev);
		free_netdev(priv->dev);
	}

	kfree(dev_list);
}

static int __init ipoib_init_module(void)
{
	int ret;

	if (ipoib_recvq_size <= IPOIB_MAX_QUEUE_SIZE &&
	    ipoib_recvq_size >= IPOIB_MIN_QUEUE_SIZE) {
		ipoib_recvq_size = roundup_pow_of_two(ipoib_recvq_size);
		ipoib_recvq_size = min(ipoib_recvq_size, IPOIB_MAX_QUEUE_SIZE);
		ipoib_recvq_size = max(ipoib_recvq_size, IPOIB_MIN_QUEUE_SIZE);
	} else {
		pr_err(KERN_ERR "ipoib_recvq_size is out of bounds [%d-%d], seting to default %d\n",
		       IPOIB_MIN_QUEUE_SIZE, IPOIB_MAX_QUEUE_SIZE,
		       IPOIB_RX_RING_SIZE);
		ipoib_recvq_size  = IPOIB_RX_RING_SIZE;
	}

	if (ipoib_sendq_size <= IPOIB_MAX_QUEUE_SIZE &&
	    ipoib_sendq_size >= IPOIB_MIN_QUEUE_SIZE) {
		ipoib_sendq_size = roundup_pow_of_two(ipoib_sendq_size);
		ipoib_sendq_size = min(ipoib_sendq_size, IPOIB_MAX_QUEUE_SIZE);
		ipoib_sendq_size = max3(ipoib_sendq_size, 2 * MAX_SEND_CQE,
					IPOIB_MIN_QUEUE_SIZE);
	} else {
		pr_err(KERN_ERR "ipoib_sendq_size is out of bounds [%d-%d], seting to default %d\n",
		       IPOIB_MIN_QUEUE_SIZE, IPOIB_MAX_QUEUE_SIZE,
		       IPOIB_TX_RING_SIZE);
		ipoib_sendq_size  = IPOIB_TX_RING_SIZE;
	}

#ifdef CONFIG_INFINIBAND_IPOIB_DEBUG
	if (ipoib_debug_level < 0 || ipoib_debug_level > 1)
		ipoib_debug_level = 0;
#endif

#ifdef CONFIG_INFINIBAND_IPOIB_CM
	ipoib_max_conn_qp = min(ipoib_max_conn_qp, IPOIB_CM_MAX_CONN_QP);
#endif

#ifdef CONFIG_COMPAT_LRO_ENABLED
	if (lro < 0 || lro > 1)
		lro = 1;

	if (lro_max_aggr < 0 || lro_max_aggr > IPOIB_LRO_MAX_AGGR ||
	    (lro_max_aggr & (lro_max_aggr - 1)) != 0)
		lro_max_aggr = IPOIB_LRO_MAX_AGGR;
#endif


	/*
	 * When copying small received packets, we only copy from the
	 * linear data part of the SKB, so we rely on this condition.
	 */
	BUILD_BUG_ON(IPOIB_CM_COPYBREAK > IPOIB_CM_HEAD_SIZE);

	spin_lock_init(&client_data_lock);

	ret = ipoib_register_debugfs();
	if (ret)
		return ret;

	/*
	 * We create our own workqueue mainly because we want to be
	 * able to flush it when devices are being removed.  We can't
	 * use schedule_work()/flush_scheduled_work() because both
	 * unregister_netdev() and linkwatch_event take the rtnl lock,
	 * so flush_scheduled_work() can deadlock during device
	 * removal.
	 */
	ipoib_workqueue = create_singlethread_workqueue("ipoib");
	if (!ipoib_workqueue) {
		ret = -ENOMEM;
		goto err_fs;
	}

	ipoib_auto_moder_workqueue =
		create_singlethread_workqueue("ipoib_auto_moder");
	if (!ipoib_auto_moder_workqueue) {
		ret = -ENOMEM;
		goto err_am;
	}
	ib_sa_register_client(&ipoib_sa_client);

	if (ipoib_register_genl())
		pr_warn("IPoIB: ipoib_register_genl failed\n");

	ret = ib_register_client(&ipoib_client);
	if (ret)
		goto err_genl;

	return 0;

err_genl:
	ipoib_unregister_genl();
	ib_sa_unregister_client(&ipoib_sa_client);
	destroy_workqueue(ipoib_auto_moder_workqueue);
err_am:
	destroy_workqueue(ipoib_workqueue);

err_fs:
	ipoib_unregister_debugfs();

	return ret;
}

static void __exit ipoib_cleanup_module(void)
{
	ib_unregister_client(&ipoib_client);
	ipoib_unregister_genl();
	ib_sa_unregister_client(&ipoib_sa_client);
	ipoib_unregister_debugfs();
	destroy_workqueue(ipoib_workqueue);
	destroy_workqueue(ipoib_auto_moder_workqueue);
}

module_init(ipoib_init_module);
module_exit(ipoib_cleanup_module);
