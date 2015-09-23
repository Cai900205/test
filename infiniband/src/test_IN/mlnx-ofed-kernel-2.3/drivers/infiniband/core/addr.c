/*
 * Copyright (c) 2005 Voltaire Inc.  All rights reserved.
 * Copyright (c) 2002-2005, Network Appliance, Inc. All rights reserved.
 * Copyright (c) 1999-2005, Mellanox Technologies, Inc. All rights reserved.
 * Copyright (c) 2005 Intel Corporation.  All rights reserved.
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

#include <linux/mutex.h>
#include <linux/inetdevice.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <net/arp.h>
#include <net/neighbour.h>
#include <net/route.h>
#include <net/netevent.h>
#include <net/addrconf.h>
#include <net/ip6_route.h>
#include <rdma/ib_addr.h>

MODULE_AUTHOR("Sean Hefty");
MODULE_DESCRIPTION("IB Address Translation");
MODULE_LICENSE("Dual BSD/GPL");

struct addr_req {
	struct list_head list;
	struct sockaddr_storage src_addr;
	struct sockaddr_storage dst_addr;
	struct rdma_dev_addr *addr;
	struct rdma_addr_client *client;
	void *context;
	void (*callback)(int status, struct sockaddr *src_addr,
			 struct rdma_dev_addr *addr, void *context);
	unsigned long timeout;
	int status;
};

static void process_req(struct work_struct *work);

static DEFINE_MUTEX(lock);
static LIST_HEAD(req_list);
static DECLARE_DELAYED_WORK(work, process_req);
static struct workqueue_struct *addr_wq;

static struct rdma_addr_client self;
void rdma_addr_register_client(struct rdma_addr_client *client)
{
	atomic_set(&client->refcount, 1);
	init_completion(&client->comp);
}
EXPORT_SYMBOL(rdma_addr_register_client);

static inline void put_client(struct rdma_addr_client *client)
{
	if (atomic_dec_and_test(&client->refcount))
		complete(&client->comp);
}

void rdma_addr_unregister_client(struct rdma_addr_client *client)
{
	put_client(client);
	wait_for_completion(&client->comp);
}
EXPORT_SYMBOL(rdma_addr_unregister_client);

int rdma_copy_addr(struct rdma_dev_addr *dev_addr, struct net_device *dev,
		     const unsigned char *dst_dev_addr)
{
	dev_addr->dev_type = dev->type;
	memcpy(dev_addr->src_dev_addr, dev->dev_addr, MAX_ADDR_LEN);
	memcpy(dev_addr->broadcast, dev->broadcast, MAX_ADDR_LEN);
	if (dst_dev_addr)
		memcpy(dev_addr->dst_dev_addr, dst_dev_addr, MAX_ADDR_LEN);
	dev_addr->bound_dev_if = dev->ifindex;
	return 0;
}
EXPORT_SYMBOL(rdma_copy_addr);

int rdma_translate_ip(struct sockaddr *addr, struct rdma_dev_addr *dev_addr,
		      u16 *vlan_id)
{
	struct net_device *dev;
	int ret = -EADDRNOTAVAIL;

	if (dev_addr->bound_dev_if) {
		dev = dev_get_by_index(&init_net, dev_addr->bound_dev_if);
		if (!dev)
			return -ENODEV;
		ret = rdma_copy_addr(dev_addr, dev, NULL);
		dev_put(dev);
		return ret;
	}

	switch (addr->sa_family) {
	case AF_INET:
		dev = ip_dev_find(&init_net,
			((struct sockaddr_in *) addr)->sin_addr.s_addr);

		if (!dev)
			return ret;

		ret = rdma_copy_addr(dev_addr, dev, NULL);
		if (vlan_id)
			*vlan_id = rdma_vlan_dev_vlan_id(dev);
		dev_put(dev);
		break;

#if IS_ENABLED(CONFIG_IPV6)
	case AF_INET6:
		rcu_read_lock();
		for_each_netdev_rcu(&init_net, dev) {
			if (ipv6_chk_addr(&init_net,
					  &((struct sockaddr_in6 *) addr)->sin6_addr,
					  dev, 1)) {
				ret = rdma_copy_addr(dev_addr, dev, NULL);
				if (vlan_id)
					*vlan_id = rdma_vlan_dev_vlan_id(dev);
				break;
			}
		}
		rcu_read_unlock();
		break;
#endif
	}
	return ret;
}
EXPORT_SYMBOL(rdma_translate_ip);

static void set_timeout(unsigned long time)
{
	unsigned long delay;

	delay = time - jiffies;
	if ((long)delay <= 0)
		delay = 1;

	mod_delayed_work(addr_wq, &work, delay);
}

static void queue_req(struct addr_req *req)
{
	struct addr_req *temp_req;

	mutex_lock(&lock);
	list_for_each_entry_reverse(temp_req, &req_list, list) {
		if (time_after_eq(req->timeout, temp_req->timeout))
			break;
	}

	list_add(&req->list, &temp_req->list);

	if (req_list.next == &req->list)
		set_timeout(req->timeout);
	mutex_unlock(&lock);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,1,0)
static int dst_fetch_ha(struct dst_entry *dst, struct rdma_dev_addr *dev_addr, void *daddr)
#else
static int dst_fetch_ha(struct dst_entry *dst, struct rdma_dev_addr *addr)
#endif
{
	struct neighbour *n;
	int ret;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,1,0)
	n = dst_neigh_lookup(dst, daddr);
#endif

	rcu_read_lock();
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
	n = dst_get_neighbour(dst);
#endif
	if (!n || !(n->nud_state & NUD_VALID)) {
		if (n)
			neigh_event_send(n, NULL);
		ret = -ENODATA;
	} else {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,1,0)
		ret = rdma_copy_addr(dev_addr, dst->dev, n->ha);
#else
		ret = rdma_copy_addr(addr, dst->dev, n->ha);
#endif
	}
	rcu_read_unlock();

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,1,0)
	if (n)
		neigh_release(n);
#endif

	return ret;
}
#endif

static int addr4_resolve(struct sockaddr_in *src_in,
			 struct sockaddr_in *dst_in,
			 struct rdma_dev_addr *addr)
{
	__be32 src_ip = src_in->sin_addr.s_addr;
	__be32 dst_ip = dst_in->sin_addr.s_addr;
	struct rtable *rt;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)
	struct flowi4 fl4;
#else
	struct flowi fl;
	struct neighbour *neigh;
#endif
	int ret;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)
	memset(&fl4, 0, sizeof(fl4));
	fl4.daddr = dst_ip;
	fl4.saddr = src_ip;
	fl4.flowi4_oif = addr->bound_dev_if;
	rt = ip_route_output_key(&init_net, &fl4);
	if (IS_ERR(rt)) {
		ret = PTR_ERR(rt);
		goto out;
	}
#else
	memset(&fl, 0, sizeof(fl));
	fl.nl_u.ip4_u.daddr = dst_ip;
	fl.nl_u.ip4_u.saddr = src_ip;
	fl.oif = addr->bound_dev_if;
	ret = ip_route_output_key(&init_net, &rt, &fl);
	if (ret)
		goto out;
#endif
	src_in->sin_family = AF_INET;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)
	src_in->sin_addr.s_addr = fl4.saddr;

	if (rt->dst.dev->flags & IFF_LOOPBACK) {
#else
	src_in->sin_addr.s_addr = rt->rt_src;

#if defined(CONFIG_IS_RTABLE_IDEV)
	if (rt->idev->dev->flags & IFF_LOOPBACK) {
#else
	if (rt->dst.dev->flags & IFF_LOOPBACK) {
#endif
#endif
		ret = rdma_translate_ip((struct sockaddr *)dst_in, addr, NULL);
		if (!ret)
			memcpy(addr->dst_dev_addr, addr->src_dev_addr, MAX_ADDR_LEN);
		goto put;
	}

	/* If the device does ARP internally, return 'done' */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)
	if (rt->dst.dev->flags & IFF_NOARP) {
		ret = rdma_copy_addr(addr, rt->dst.dev, NULL);
		goto put;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,1,0)
	ret = dst_fetch_ha(&rt->dst, addr, &fl4.daddr);
#else
	ret = dst_fetch_ha(&rt->dst, addr);
#endif
#else
#if defined(CONFIG_IS_RTABLE_IDEV)
	if (rt->idev->dev->flags & IFF_NOARP) {
		ret = rdma_copy_addr(addr, rt->idev->dev, NULL);
#else
	if (rt->dst.dev->flags & IFF_NOARP) {
		ret = rdma_copy_addr(addr, rt->dst.dev, NULL);
#endif
		goto put;
	}

#if defined(CONFIG_IS_RTABLE_IDEV)
	neigh = neigh_lookup(&arp_tbl, &rt->rt_gateway, rt->idev->dev);
#else
	neigh = neigh_lookup(&arp_tbl, &rt->rt_gateway, rt->dst.dev);
#endif
	if (!neigh || !(neigh->nud_state & NUD_VALID)) {
#if defined(CONFIG_IS_RTABLE_IDEV)
		neigh_event_send(rt->u.dst.neighbour, NULL);
#else
		neigh_event_send(rt->dst.neighbour, NULL);
#endif
		ret = -ENODATA;
		if (neigh)
			goto release;
		goto put;
	}

	ret = rdma_copy_addr(addr, neigh->dev, neigh->ha);
release:
	neigh_release(neigh);
#endif

put:
	ip_rt_put(rt);
out:
	return ret;
}

#if IS_ENABLED(CONFIG_IPV6)
static int addr6_resolve(struct sockaddr_in6 *src_in,
			 struct sockaddr_in6 *dst_in,
			 struct rdma_dev_addr *addr)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)
	struct flowi6 fl6;
#else
	struct flowi fl;
	struct neighbour *neigh;
#endif
	struct dst_entry *dst;
	int ret;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)
	memset(&fl6, 0, sizeof fl6);
	fl6.daddr = dst_in->sin6_addr;
	fl6.saddr = src_in->sin6_addr;
	fl6.flowi6_oif = addr->bound_dev_if;

	dst = ip6_route_output(&init_net, NULL, &fl6);
	if ((ret = dst->error))
		goto put;

	if (ipv6_addr_any(&fl6.saddr)) {
		ret = ipv6_dev_get_saddr(&init_net, ip6_dst_idev(dst)->dev,
					 &fl6.daddr, 0, &fl6.saddr);
		if (ret)
			goto put;

		src_in->sin6_family = AF_INET6;
		src_in->sin6_addr = fl6.saddr;
	}
#else
	memset(&fl, 0, sizeof fl);
	ipv6_addr_copy(&fl.fl6_dst, &dst_in->sin6_addr);
	ipv6_addr_copy(&fl.fl6_src, &src_in->sin6_addr);
	fl.oif = addr->bound_dev_if;

	dst = ip6_route_output(&init_net, NULL, &fl);
	if ((ret = dst->error))
		goto put;

	if (ipv6_addr_any(&fl.fl6_src)) {
		ret = ipv6_dev_get_saddr(&init_net, ip6_dst_idev(dst)->dev,
					 &fl.fl6_dst, 0, &fl.fl6_src);
		if (ret)
			goto put;

		src_in->sin6_family = AF_INET6;
		ipv6_addr_copy(&src_in->sin6_addr, &fl.fl6_src);
	}
#endif

	if (dst->dev->flags & IFF_LOOPBACK) {
		ret = rdma_translate_ip((struct sockaddr *)dst_in, addr, NULL);
		if (!ret)
			memcpy(addr->dst_dev_addr, addr->src_dev_addr, MAX_ADDR_LEN);
		goto put;
	}

	/* If the device does ARP internally, return 'done' */
	if (dst->dev->flags & IFF_NOARP) {
		ret = rdma_copy_addr(addr, dst->dev, NULL);
		goto put;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,1,0)
	ret = dst_fetch_ha(dst, addr, &fl6.daddr);
#else
	ret = dst_fetch_ha(dst, addr);
#endif
#else
	neigh = dst->neighbour;
	if (!neigh || !(neigh->nud_state & NUD_VALID)) {
		neigh_event_send(dst->neighbour, NULL);
		ret = -ENODATA;
		goto put;
	}

	ret = rdma_copy_addr(addr, dst->dev, neigh->ha);
#endif

put:
	dst_release(dst);
	return ret;
}
#else
static int addr6_resolve(struct sockaddr_in6 *src_in,
			 struct sockaddr_in6 *dst_in,
			 struct rdma_dev_addr *addr)
{
	return -EADDRNOTAVAIL;
}
#endif

static int addr_resolve(struct sockaddr *src_in,
			struct sockaddr *dst_in,
			struct rdma_dev_addr *addr)
{
	if (src_in->sa_family == AF_INET) {
		return addr4_resolve((struct sockaddr_in *) src_in,
			(struct sockaddr_in *) dst_in, addr);
	} else
		return addr6_resolve((struct sockaddr_in6 *) src_in,
			(struct sockaddr_in6 *) dst_in, addr);
}

static void process_req(struct work_struct *work)
{
	struct addr_req *req, *temp_req;
	struct sockaddr *src_in, *dst_in;
	struct list_head done_list;

	INIT_LIST_HEAD(&done_list);

	mutex_lock(&lock);
	list_for_each_entry_safe(req, temp_req, &req_list, list) {
		if (req->status == -ENODATA) {
			src_in = (struct sockaddr *) &req->src_addr;
			dst_in = (struct sockaddr *) &req->dst_addr;
			req->status = addr_resolve(src_in, dst_in, req->addr);
			if (req->status && time_after_eq(jiffies, req->timeout))
				req->status = -ETIMEDOUT;
			else if (req->status == -ENODATA)
				continue;
		}
		list_move_tail(&req->list, &done_list);
	}

	if (!list_empty(&req_list)) {
		req = list_entry(req_list.next, struct addr_req, list);
		set_timeout(req->timeout);
	}
	mutex_unlock(&lock);

	list_for_each_entry_safe(req, temp_req, &done_list, list) {
		list_del(&req->list);
		req->callback(req->status, (struct sockaddr *) &req->src_addr,
			req->addr, req->context);
		put_client(req->client);
		kfree(req);
	}
}

int rdma_resolve_ip(struct rdma_addr_client *client,
		    struct sockaddr *src_addr, struct sockaddr *dst_addr,
		    struct rdma_dev_addr *addr, int timeout_ms,
		    void (*callback)(int status, struct sockaddr *src_addr,
				     struct rdma_dev_addr *addr, void *context),
		    void *context)
{
	struct sockaddr *src_in, *dst_in;
	struct addr_req *req;
	int ret = 0;

	req = kzalloc(sizeof *req, GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	src_in = (struct sockaddr *) &req->src_addr;
	dst_in = (struct sockaddr *) &req->dst_addr;

	if (src_addr) {
		if (src_addr->sa_family != dst_addr->sa_family) {
			ret = -EINVAL;
			goto err;
		}

		memcpy(src_in, src_addr, ip_addr_size(src_addr));
	} else {
		src_in->sa_family = dst_addr->sa_family;
	}

	memcpy(dst_in, dst_addr, ip_addr_size(dst_addr));
	req->addr = addr;
	req->callback = callback;
	req->context = context;
	req->client = client;
	atomic_inc(&client->refcount);

	req->status = addr_resolve(src_in, dst_in, addr);
	switch (req->status) {
	case 0:
		req->timeout = jiffies;
		queue_req(req);
		break;
	case -ENODATA:
		req->timeout = msecs_to_jiffies(timeout_ms) + jiffies;
		queue_req(req);
		break;
	default:
		ret = req->status;
		atomic_dec(&client->refcount);
		goto err;
	}
	return ret;
err:
	kfree(req);
	return ret;
}
EXPORT_SYMBOL(rdma_resolve_ip);

void rdma_addr_cancel(struct rdma_dev_addr *addr)
{
	struct addr_req *req, *temp_req;

	mutex_lock(&lock);
	list_for_each_entry_safe(req, temp_req, &req_list, list) {
		if (req->addr == addr) {
			req->status = -ECANCELED;
			req->timeout = jiffies;
			list_move(&req->list, &req_list);
			set_timeout(req->timeout);
			break;
		}
	}
	mutex_unlock(&lock);
}
EXPORT_SYMBOL(rdma_addr_cancel);

struct resolve_cb_context {
	struct rdma_dev_addr *addr;
	struct completion comp;
};

static void resolve_cb(int status, struct sockaddr *src_addr,
	     struct rdma_dev_addr *addr, void *context)
{
	if (!status)
		memcpy(((struct resolve_cb_context *)context)->addr, addr,
		       sizeof(struct rdma_dev_addr));
	else
		memset(
		((struct resolve_cb_context *)context)->addr->dst_dev_addr,
				0, sizeof(unsigned char) * MAX_ADDR_LEN);
	complete(&((struct resolve_cb_context *)context)->comp);
}

int rdma_addr_find_dmac_by_grh(union ib_gid *sgid, union ib_gid *dgid, u8 *dmac,
			       u16 *vlan_id)
{
	int ret = 0;
	struct rdma_dev_addr dev_addr;
	struct resolve_cb_context ctx;
	struct net_device *dev;

	union {
		struct sockaddr     _sockaddr;
		struct sockaddr_in  _sockaddr_in;
		struct sockaddr_in6 _sockaddr_in6;
	} sgid_addr, dgid_addr;


	ret = rdma_gid2ip(&sgid_addr._sockaddr, sgid);
	if (ret)
		return ret;

	ret = rdma_gid2ip(&dgid_addr._sockaddr, dgid);
	if (ret)
		return ret;

	memset(&dev_addr, 0, sizeof(dev_addr));

	ctx.addr = &dev_addr;
	init_completion(&ctx.comp);
	ret = rdma_resolve_ip(&self, &sgid_addr._sockaddr, &dgid_addr._sockaddr,
			&dev_addr, 1000, resolve_cb, &ctx);
	if (ret)
		return ret;

	wait_for_completion(&ctx.comp);

	memcpy(dmac, dev_addr.dst_dev_addr, ETH_ALEN);
	dev = dev_get_by_index(&init_net, dev_addr.bound_dev_if);
	if (!dev)
		return -ENODEV;
	if (vlan_id)
		*vlan_id = rdma_vlan_dev_vlan_id(dev);
	dev_put(dev);
	return ret;
}
EXPORT_SYMBOL(rdma_addr_find_dmac_by_grh);

int rdma_addr_find_smac_by_sgid(union ib_gid *sgid, u8 *smac, u16 *vlan_id)
{
	int ret = 0;
	struct rdma_dev_addr dev_addr;
	union {
		struct sockaddr     _sockaddr;
		struct sockaddr_in  _sockaddr_in;
		struct sockaddr_in6 _sockaddr_in6;
	} gid_addr;

	ret = rdma_gid2ip(&gid_addr._sockaddr, sgid);

	if (ret)
		return ret;
	memset(&dev_addr, 0, sizeof(dev_addr));
	ret = rdma_translate_ip(&gid_addr._sockaddr, &dev_addr, vlan_id);
	if (ret)
		return ret;

	memcpy(smac, dev_addr.src_dev_addr, ETH_ALEN);
	return ret;
}
EXPORT_SYMBOL(rdma_addr_find_smac_by_sgid);
static int netevent_callback(struct notifier_block *self, unsigned long event,
	void *ctx)
{
	if (event == NETEVENT_NEIGH_UPDATE) {
		struct neighbour *neigh = ctx;

		if (neigh->nud_state & NUD_VALID) {
			set_timeout(jiffies);
		}
	}
	return 0;
}

static struct notifier_block nb = {
	.notifier_call = netevent_callback
};

static int __init addr_init(void)
{
	addr_wq = create_singlethread_workqueue("ib_addr");
	if (!addr_wq)
		return -ENOMEM;

	register_netevent_notifier(&nb);
	rdma_addr_register_client(&self);
	return 0;
}

static void __exit addr_cleanup(void)
{
	rdma_addr_unregister_client(&self);
	unregister_netevent_notifier(&nb);
	destroy_workqueue(addr_wq);
}

module_init(addr_init);
module_exit(addr_cleanup);
