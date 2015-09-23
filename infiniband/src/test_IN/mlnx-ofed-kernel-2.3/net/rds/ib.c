/*
 * Copyright (c) 2006 Oracle.  All rights reserved.
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
#include <linux/in.h>
#include <linux/if.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/if_arp.h>
#include <net/arp.h>
#include <linux/delay.h>
#include <rdma/ib_cache.h>
#include <net/sock.h>
#include <net/route.h>
#include <net/inet_common.h>
#include <linux/rtnetlink.h>
#include <linux/module.h>

#include "rds.h"
#include "ib.h"
#include "tcp.h"
#include <linux/time.h>

unsigned int rds_ib_fmr_1m_pool_size = RDS_FMR_1M_POOL_SIZE;
unsigned int rds_ib_fmr_8k_pool_size = RDS_FMR_8K_POOL_SIZE;
unsigned int rds_ib_retry_count = RDS_IB_DEFAULT_RETRY_COUNT;
unsigned int rds_ib_apm_enabled = 0;
unsigned int rds_ib_apm_fallback = 1;
unsigned int rds_ib_active_bonding_enabled;
unsigned int rds_ib_active_bonding_fallback = 1;
unsigned int rds_ib_active_bonding_reconnect_delay = 1;
unsigned int rds_ib_apm_timeout = RDS_IB_DEFAULT_TIMEOUT;
unsigned int rds_ib_rnr_retry_count = RDS_IB_DEFAULT_RNR_RETRY_COUNT;
static char *rds_ib_active_bonding_failover_groups;
unsigned int rds_ib_active_bonding_arps = RDS_IB_DEFAULT_NUM_ARPS;
static char *rds_ib_active_bonding_excl_ips = "169.254/16,172.10/16";

module_param(rds_ib_fmr_1m_pool_size, int, 0444);
MODULE_PARM_DESC(rds_ib_fmr_1m_pool_size, " Max number of 1m fmr per HCA");
module_param(rds_ib_fmr_8k_pool_size, int, 0444);
MODULE_PARM_DESC(rds_ib_fmr_8k_pool_size, " Max number of 8k fmr per HCA");
module_param(rds_ib_retry_count, int, 0444);
MODULE_PARM_DESC(rds_ib_retry_count, " Number of hw retries before reporting an error");
module_param(rds_ib_apm_enabled, int, 0444);
MODULE_PARM_DESC(rds_ib_apm_enabled, " APM Enabled");
module_param(rds_ib_active_bonding_enabled, int, 0444);
MODULE_PARM_DESC(rds_ib_active_bonding_enabled, " High Availability IP enabled");
module_param(rds_ib_apm_timeout, int, 0444);
MODULE_PARM_DESC(rds_ib_apm_timeout, " APM timeout");
module_param(rds_ib_rnr_retry_count, int, 0444);
MODULE_PARM_DESC(rds_ib_rnr_retry_count, " QP rnr retry count");
module_param(rds_ib_apm_fallback, int, 0444);
MODULE_PARM_DESC(rds_ib_apm_fallback, " APM failback enabled");
module_param(rds_ib_active_bonding_fallback, int, 0444);
MODULE_PARM_DESC(rds_ib_active_bonding_fallback, " HAIP failback Enabled");
module_param(rds_ib_active_bonding_failover_groups, charp, 0444);
MODULE_PARM_DESC(rds_ib_active_bonding_failover_groups,
	"<ifname>[,<ifname>]*[;<ifname>[,<ifname>]*]*");
module_param(rds_ib_active_bonding_reconnect_delay, int, 0444);
MODULE_PARM_DESC(rds_ib_active_bonding_reconnect_delay, " Active Bonding reconnect delay");
module_param(rds_ib_active_bonding_arps, int, 0444);
MODULE_PARM_DESC(rds_ib_active_bonding_arps, " Num ARPs to be sent when IP moved");
module_param(rds_ib_active_bonding_excl_ips, charp, 0444);
MODULE_PARM_DESC(rds_ib_active_bonding_excl_ips,
	"[<IP>/<prefix>][,<IP>/<prefix>]*");

/*
 * we have a clumsy combination of RCU and a rwsem protecting this list
 * because it is used both in the get_mr fast path and while blocking in
 * the FMR flushing path.
 */
DECLARE_RWSEM(rds_ib_devices_lock);
struct list_head rds_ib_devices;

/* NOTE: if also grabbing ibdev lock, grab this first */
DEFINE_SPINLOCK(ib_nodev_conns_lock);
LIST_HEAD(ib_nodev_conns);

struct workqueue_struct *rds_aux_wq;

struct socket	*rds_ib_inet_socket;

static struct rds_ib_port *ip_config;
static u8	ip_port_cnt = 0;
static u8	ip_port_max = RDS_IB_MAX_PORTS;

static struct rds_ib_excl_ips excl_ips_tbl[RDS_IB_MAX_EXCL_IPS];
static u8       excl_ips_cnt;

void rds_ib_nodev_connect(void)
{
	struct rds_ib_connection *ic;

	spin_lock(&ib_nodev_conns_lock);
	list_for_each_entry(ic, &ib_nodev_conns, ib_node)
		rds_conn_connect_if_down(ic->conn);
	spin_unlock(&ib_nodev_conns_lock);
}

void rds_ib_dev_shutdown(struct rds_ib_device *rds_ibdev)
{
	struct rds_ib_connection *ic;
	unsigned long flags;

	spin_lock_irqsave(&rds_ibdev->spinlock, flags);
	list_for_each_entry(ic, &rds_ibdev->conn_list, ib_node)
		rds_conn_drop(ic->conn);
	spin_unlock_irqrestore(&rds_ibdev->spinlock, flags);
}

/*
 * rds_ib_destroy_mr_pool() blocks on a few things and mrs drop references
 * from interrupt context so we push freing off into a work struct in krdsd.
 */
static void rds_ib_dev_free(struct work_struct *work)
{
	struct rds_ib_ipaddr *i_ipaddr, *i_next;
	struct rds_ib_device *rds_ibdev = container_of(work,
					struct rds_ib_device, free_work);

	if (rds_ibdev->dev) {
		if (rds_ibdev->mr_8k_pool)
			rds_ib_destroy_mr_pool(rds_ibdev->mr_8k_pool);
		if (rds_ibdev->mr_1m_pool)
			rds_ib_destroy_mr_pool(rds_ibdev->mr_1m_pool);
		if (rds_ibdev->mr)
			ib_dereg_mr(rds_ibdev->mr);
		if (rds_ibdev->pd)
			ib_dealloc_pd(rds_ibdev->pd);
	}
	kfree(rds_ibdev->srq);

	list_for_each_entry_safe(i_ipaddr, i_next, &rds_ibdev->ipaddr_list, list) {
		list_del(&i_ipaddr->list);
		kfree(i_ipaddr);
	}

	if (rds_ibdev->vector_load)
		kfree(rds_ibdev->vector_load);

	if (rds_ibdev->dev) {
		WARN_ON(!waitqueue_active(&rds_ibdev->wait));

		rds_ibdev->done = 1;
		wake_up(&rds_ibdev->wait);
	} else
		kfree(rds_ibdev);
}

void rds_ib_dev_put(struct rds_ib_device *rds_ibdev)
{
	BUG_ON(atomic_read(&rds_ibdev->refcount) <= 0);
	if (atomic_dec_and_test(&rds_ibdev->refcount))
		queue_work(rds_wq, &rds_ibdev->free_work);
}

/*
 * New connections use this to find the device to associate with the
 * connection.  It's not in the fast path so we're not concerned about the
 * performance of the IB call.  (As of this writing, it uses an interrupt
 * blocking spinlock to serialize walking a per-device list of all registered
 * clients.)
 *
 * RCU is used to handle incoming connections racing with device teardown.
 * Rather than use a lock to serialize removal from the client_data and
 * getting a new reference, we use an RCU grace period.  The destruction
 * path removes the device from client_data and then waits for all RCU
 * readers to finish.
 *
 * A new connection can get NULL from this if its arriving on a
 * device that is in the process of being removed.
 */
struct rds_ib_device *rds_ib_get_client_data(struct ib_device *device)
{
	struct rds_ib_device *rds_ibdev;

	rcu_read_lock();
	rds_ibdev = ib_get_client_data(device, &rds_ib_client);
	if (rds_ibdev)
		atomic_inc(&rds_ibdev->refcount);
	rcu_read_unlock();
	return rds_ibdev;
}

/*
 * The IB stack is letting us know that a device is going away.  This can
 * happen if the underlying HCA driver is removed or if PCI hotplug is removing
 * the pci function, for example.
 *
 * This can be called at any time and can be racing with any other RDS path.
 */
void rds_ib_remove_one(struct ib_device *device)
{
	struct rds_ib_device *rds_ibdev;
	int i;

	rds_ibdev = ib_get_client_data(device, &rds_ib_client);
	if (!rds_ibdev)
		return;

	if (rds_ib_active_bonding_enabled) {
		for (i = 1; i <= ip_port_cnt; i++) {
			if (ip_config[i].rds_ibdev == rds_ibdev)
				ip_config[i].rds_ibdev = NULL;
		}
		ib_unregister_event_handler(&rds_ibdev->event_handler);
	}

	rds_ib_dev_shutdown(rds_ibdev);

	/* stop connection attempts from getting a reference to this device. */
	ib_set_client_data(device, &rds_ib_client, NULL);

	down_write(&rds_ib_devices_lock);
	list_del_rcu(&rds_ibdev->list);
	up_write(&rds_ib_devices_lock);

	/*
	 * This synchronize rcu is waiting for readers of both the ib
	 * client data and the devices list to finish before we drop
	 * both of those references.
	 */
	synchronize_rcu();
	rds_ib_dev_put(rds_ibdev);
	rds_ib_dev_put(rds_ibdev);

	if (!wait_event_timeout(rds_ibdev->wait, rds_ibdev->done, 30*HZ)) {
		printk(KERN_WARNING "RDS/IB: device cleanup timed out after "
			" 30 secs (refcount=%d)\n",
			 atomic_read(&rds_ibdev->refcount));
		/* when rds_ib_remove_one return, driver's mlx4_ib_removeone
		 * function destroy ib_device, so we must clear rds_ibdev->dev
		 * to NULL, or will cause crash when rds connection be released,
		 * at the moment rds_ib_dev_free through ib_device
		 * .i.e rds_ibdev->dev to release mr and fmr, reusing the
		 * released ib_device will cause crash.
		 */
		rds_ibdev->dev = NULL;
	} else
		kfree(rds_ibdev);
}

struct ib_client rds_ib_client = {
	.name   = "rds_ib",
	.add    = rds_ib_add_one,
	.remove = rds_ib_remove_one
};

static int rds_ib_conn_info_visitor(struct rds_connection *conn,
				    void *buffer)
{
	struct rds_info_rdma_connection *iinfo = buffer;
	struct rds_ib_connection *ic;

	/* We will only ever look at IB transports */
	if (conn->c_trans != &rds_ib_transport)
		return 0;

	iinfo->src_addr = conn->c_laddr;
	iinfo->dst_addr = conn->c_faddr;

	memset(&iinfo->src_gid, 0, sizeof(iinfo->src_gid));
	memset(&iinfo->dst_gid, 0, sizeof(iinfo->dst_gid));
	if (rds_conn_state(conn) == RDS_CONN_UP) {
		struct rds_ib_device *rds_ibdev;
		struct rdma_dev_addr *dev_addr;

		ic = conn->c_transport_data;
		if (rds_ib_apm_enabled) {
			memcpy((union ib_gid *) &iinfo->src_gid,
				&ic->i_cur_path.p_sgid, sizeof(union ib_gid));
			memcpy((union ib_gid *) &iinfo->dst_gid,
				&ic->i_cur_path.p_dgid, sizeof(union ib_gid));
		} else {
			dev_addr = &ic->i_cm_id->route.addr.dev_addr;
			rdma_addr_get_sgid(dev_addr,
				(union ib_gid *) &iinfo->src_gid);
			rdma_addr_get_dgid(dev_addr,
				(union ib_gid *) &iinfo->dst_gid);
		}

		rds_ibdev = ic->rds_ibdev;
		iinfo->max_send_wr = ic->i_send_ring.w_nr;
		iinfo->max_recv_wr = ic->i_recv_ring.w_nr;
		iinfo->max_send_sge = rds_ibdev->max_sge;
		rds_ib_get_mr_info(rds_ibdev, iinfo);
		iinfo->tos = ic->conn->c_tos;
		iinfo->sl = ic->i_sl;
		iinfo->cache_allocs = atomic_read(&ic->i_cache_allocs);
	}
	return 1;
}

static void rds_ib_ic_info(struct socket *sock, unsigned int len,
			   struct rds_info_iterator *iter,
			   struct rds_info_lengths *lens)
{
	rds_for_each_conn_info(sock, len, iter, lens,
				rds_ib_conn_info_visitor,
				sizeof(struct rds_info_rdma_connection));
}


/*
 * Early RDS/IB was built to only bind to an address if there is an IPoIB
 * device with that address set.
 *
 * If it were me, I'd advocate for something more flexible.  Sending and
 * receiving should be device-agnostic.  Transports would try and maintain
 * connections between peers who have messages queued.  Userspace would be
 * allowed to influence which paths have priority.  We could call userspace
 * asserting this policy "routing".
 */
static int rds_ib_laddr_check(__be32 addr)
{
	int ret;
	struct rdma_cm_id *cm_id;
	struct sockaddr_in sin;

	/* Create a CMA ID and try to bind it. This catches both
	 * IB and iWARP capable NICs.
	 */
	cm_id = rdma_create_id(NULL, NULL, RDMA_PS_TCP, IB_QPT_RC);
	if (IS_ERR(cm_id))
		return -EADDRNOTAVAIL;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = addr;

	/* rdma_bind_addr will only succeed for IB & iWARP devices */
	ret = rdma_bind_addr(cm_id, (struct sockaddr *)&sin);
	/* due to this, we will claim to support iWARP devices unless we
	   check node_type. */
	if (ret || !cm_id->device || cm_id->device->node_type != RDMA_NODE_IB_CA)
		ret = -EADDRNOTAVAIL;

	rdsdebug("addr %pI4 ret %d node type %d\n",
		&addr, ret,
		cm_id->device ? cm_id->device->node_type : -1);

	rdma_destroy_id(cm_id);

	return ret;
}

static u8 rds_ib_get_failover_port(u8 port)
{
	u8	i;

	for (i = 1; i <= ip_port_cnt; i++) {
		if (i != port &&
			ip_config[i].failover_group ==
				ip_config[port].failover_group &&
			ip_config[i].port_state == RDS_IB_PORT_UP) {
			return i;
		}
	}

	for (i = 1; i <= ip_port_cnt; i++) {
		if (i != port &&
			ip_config[i].port_state == RDS_IB_PORT_UP) {
				return i;
			}
	}

	return 0;
}

static void rds_ib_send_gratuitous_arp(struct net_device	*out_dev,
					unsigned char		*dev_addr,
					__be32			ip_addr)
{
	int i;

	/* Send multiple ARPs to improve reliability */
	for (i = 0; i < rds_ib_active_bonding_arps; i++) {
		arp_send(ARPOP_REQUEST, ETH_P_ARP,
			ip_addr, out_dev,
			ip_addr, NULL,
			dev_addr, NULL);
	}
}

static int rds_ib_set_ip(struct net_device	*out_dev,
			unsigned char		*dev_addr,
			char			*if_name,
			__be32			addr,
			__be32			bcast,
			__be32			mask)
{
	struct ifreq		*ir;
	struct sockaddr_in	*sin;
	struct page		*page;
	int			ret = 0;

	page = alloc_page(GFP_HIGHUSER);
	if (!page) {
		printk(KERN_ERR "RDS/IB: alloc_page failed .. NO MEM\n");
		return 1;
	}

	ir = (struct ifreq *)kmap(page);
	memset(ir, 0, sizeof(struct ifreq));
	sin = (struct sockaddr_in *)&ir->ifr_addr;
	sin->sin_family = AF_INET;

	strcpy(ir->ifr_ifrn.ifrn_name, if_name);

	sin->sin_addr.s_addr = addr;
	ret = inet_ioctl(rds_ib_inet_socket, SIOCSIFADDR, (unsigned long) ir);
	if (ret && addr) {
		printk(KERN_ERR
			"RDS/IB: inet_ioctl(SIOCSIFADDR) on %s failed (%d)\n",
			if_name, ret);
		goto out;
	}

	if (!addr)
		goto out;

	sin->sin_addr.s_addr = bcast;
	ret = inet_ioctl(rds_ib_inet_socket, SIOCSIFBRDADDR,
			(unsigned long) ir);
	if (ret) {
		printk(KERN_ERR
			"RDS/IB: inet_ioctl(SIOCSIFBRDADDR) on %s failed (%d)\n",
			if_name, ret);
		goto out;
	}

	sin->sin_addr.s_addr = mask;
	ret = inet_ioctl(rds_ib_inet_socket, SIOCSIFNETMASK,
			(unsigned long) ir);
	if (ret) {
		printk(KERN_ERR
			"RDS/IB: inet_ioctl(SIOCSIFBRDADDR) on %s failed (%d)\n",
			if_name, ret);
		goto out;
	}

	rds_ib_send_gratuitous_arp(out_dev, dev_addr, addr);

out:
	kunmap(page);
	__free_page(page);

	return ret;
}

static int rds_ib_addr_exist(struct net_device *ndev,
				__be32		addr,
				char		*if_name)
{
	struct in_device        *in_dev;
	struct in_ifaddr        *ifa;
	struct in_ifaddr        **ifap;
	int			found = 0;

	rtnl_lock();
	in_dev = in_dev_get(ndev);
	if (in_dev) {
		for (ifap = &in_dev->ifa_list; (ifa = *ifap);
			ifap = &ifa->ifa_next) {
			if (ifa->ifa_address == addr) {
				found = 1;
				if (if_name)
					strcpy(if_name, ifa->ifa_label);
				break;
			}
		}
		in_dev_put(in_dev);
	}
	rtnl_unlock();

	return found;
}

static void rds_ib_update_arp_cache(struct net_device      *out_dev,
					unsigned char      *dev_addr,
					__be32             ip_addr)
{
	int ret = 0;
	struct neighbour *neigh;

	neigh = __neigh_lookup_errno(&arp_tbl, &ip_addr, out_dev);
	if (!IS_ERR(neigh)) {
		ret = neigh_update(neigh, dev_addr, NUD_STALE,
					NEIGH_UPDATE_F_OVERRIDE |
					NEIGH_UPDATE_F_ADMIN);
		if (ret)
			printk(KERN_ERR "RDS/IB: neigh_update failed (%d) "
				"for out_dev %s IP %u.%u.%u.%u\n",
				ret, out_dev->name, NIPQUAD(ip_addr));
		neigh_release(neigh);
	}
}

static void rds_ib_conn_drop(struct work_struct *_work)
{
	struct rds_ib_conn_drop_work    *work =
		container_of(_work, struct rds_ib_conn_drop_work, work.work);
	struct rds_connection   *conn = (struct rds_connection *)work->conn;

	rds_conn_drop(conn);

	kfree(work);
}

static int rds_ib_move_ip(char			*from_dev,
			char			*to_dev,
			u8			from_port,
			u8			to_port,
			u8			arp_port,
			__be32			addr,
			__be32			bcast,
			__be32			mask,
			int			event_type,
			int                     alias,
			int			failover)
{
	struct ifreq		*ir;
	struct sockaddr_in	*sin;
	struct page		*page;
	char			from_dev2[2*IFNAMSIZ + 1];
	char			to_dev2[2*IFNAMSIZ + 1];
	char                    *tmp_str;
	int			ret = 0;
	u8			active_port, i, j, port = 0;
	struct in_device	*in_dev;
	struct rds_ib_connection *ic, *ic2;
	struct rds_ib_device *rds_ibdev;
	struct rds_ib_conn_drop_work *work;

	page = alloc_page(GFP_HIGHUSER);
	if (!page) {
		printk(KERN_ERR "RDS/IB: alloc_page failed .. NO MEM\n");
		return 1;
	}

	ir = (struct ifreq *)kmap(page);
	memset(ir, 0, sizeof(struct ifreq));
	sin = (struct sockaddr_in *)&ir->ifr_addr;
	sin->sin_family = AF_INET;

	/* Set the primary IP if it hasn't been set */
	if (ip_config[to_port].ip_addr) {
		strcpy(ir->ifr_ifrn.ifrn_name, ip_config[to_port].dev->name);
		ret = inet_ioctl(rds_ib_inet_socket, SIOCGIFADDR,
					(unsigned long) ir);
		if (ret == -EADDRNOTAVAIL) {
			sin->sin_addr.s_addr = ip_config[to_port].ip_addr;
			ret = inet_ioctl(rds_ib_inet_socket, SIOCSIFADDR,
						(unsigned long) ir);
			if (ret) {
				printk(KERN_ERR
					"RDS/IB: inet_ioctl(SIOCSIFADDR) "
					"failed (%d)\n", ret);
				goto out;
			}
		} else if (ret) {
			printk(KERN_ERR
				"RDS/IB: inet_ioctl(SIOCGIFADDR) "
				"failed (%d)\n", ret);
			goto out;
		}
	}

	if (failover) {
		in_dev = in_dev_get(ip_config[to_port].dev);
		if (in_dev && !in_dev->ifa_list) {
			strcpy(to_dev2, to_dev);
		} else {
			strcpy(to_dev2, to_dev);
			strcat(to_dev2, ":");
			strcat(to_dev2, ip_config[from_port].port_label);
			if (alias) {
				tmp_str = strchr(from_dev, ':');
				strcat(to_dev2, tmp_str);
			}
			to_dev2[IFNAMSIZ-1] = 0;
		}
		if (in_dev)
			in_dev_put(in_dev);

		/* Bailout if IP already exists on target port */
		if (rds_ib_addr_exist(ip_config[to_port].dev, addr, NULL))
			goto out;

		active_port = ip_config[from_port].ip_active_port;
		if (alias || active_port == from_port) {
			strcpy(from_dev2, from_dev);
		} else if (ip_config[active_port].port_state ==
				RDS_IB_PORT_UP) {
			if (!rds_ib_addr_exist(ip_config[active_port].dev,
						addr, from_dev2)) {
				strcpy(from_dev2,
					ip_config[active_port].dev->name);
				strcat(from_dev2, ":");
				strcat(from_dev2,
					ip_config[from_port].port_label);
				from_dev2[IFNAMSIZ-1] = 0;
			}
		} else {
			strcpy(from_dev2, from_dev);
		}
	} else {
		if (!rds_ib_addr_exist(ip_config[from_port].dev,
						addr, from_dev2)) {
			strcpy(from_dev2, from_dev);
			strcat(from_dev2, ":");
			strcat(from_dev2, ip_config[to_port].port_label);
			from_dev2[IFNAMSIZ-1] = 0;
		}
		strcpy(to_dev2, to_dev);
	}

	/* Clear the IP on old port */
	ret = rds_ib_set_ip(NULL, NULL, from_dev2, 0, 0, 0);

	/* Set the IP on new port */
	ret = rds_ib_set_ip(ip_config[arp_port].dev,
				ip_config[to_port].dev->dev_addr,
				to_dev2, addr, bcast, mask);

	if (ret) {
		printk(KERN_NOTICE
			"RDS/IP: failed to move IP %u.%u.%u.%u "
			"from %s to %s\n",
			NIPQUAD(addr), from_dev2, to_dev2);
	} else {
		printk(KERN_NOTICE
			"RDS/IB: IP %u.%u.%u.%u migrated from %s to %s\n",
				NIPQUAD(addr), from_dev2, to_dev2);

		rds_ibdev = ip_config[from_port].rds_ibdev;
		if (!rds_ibdev)
			goto out;

		spin_lock_bh(&rds_ibdev->spinlock);
		list_for_each_entry(ic, &rds_ibdev->conn_list, ib_node) {
			if (ic->conn->c_laddr == addr) {
				if (rds_ib_apm_enabled) {
					if (!memcmp(
						&ic->i_cur_path.p_sgid,
						&ip_config[to_port].gid,
						sizeof(union ib_gid))) {
						continue;
					}
				}

				/* if local connection, update the ARP cache */
				if (ic->conn->c_loopback) {
					for (i = 1; i <= ip_port_cnt; i++) {
						if (ip_config[i].ip_addr ==
							ic->conn->c_faddr) {
							port = i;
							break;
						}

	for (j = 0; j < ip_config[i].alias_cnt; j++) {
		if (ip_config[i].aliases[j].ip_addr == ic->conn->c_faddr) {
								port = i;
								break;
							}
						}
					}

					BUG_ON(!port);

					rds_ib_update_arp_cache(
						ip_config[from_port].dev,
						ip_config[port].dev->dev_addr,
						ic->conn->c_faddr);

					rds_ib_update_arp_cache(
						ip_config[to_port].dev,
						ip_config[port].dev->dev_addr,
						ic->conn->c_faddr);

					rds_ib_update_arp_cache(
						ip_config[from_port].dev,
					ip_config[to_port].dev->dev_addr,
						ic->conn->c_laddr);

					rds_ib_update_arp_cache(
						ip_config[to_port].dev,
					ip_config[to_port].dev->dev_addr,
						ic->conn->c_laddr);

					list_for_each_entry(ic2,
						&rds_ibdev->conn_list,
							ib_node) {
						if (ic2->conn->c_laddr ==
							ic->conn->c_faddr &&
							ic2->conn->c_faddr ==
							ic->conn->c_laddr) {
							rds_conn_drop(ic2->conn);
						}
					}
				}

				if (event_type == RDS_IB_PORT_EVENT_IB &&
					failover) {
					work =
					kzalloc(sizeof *work, GFP_ATOMIC);
					if (!work) {
						printk(KERN_ERR
							"RDS/IB: failed to allocate connection drop work\n");
					spin_unlock_bh(&rds_ibdev->spinlock);
							goto out;
					}

			work->conn = (struct rds_ib_connection *)ic->conn;
			INIT_DELAYED_WORK(&work->work, rds_ib_conn_drop);
					queue_delayed_work(rds_wq, &work->work,
		msecs_to_jiffies(1000 * rds_ib_active_bonding_reconnect_delay));
				} else
					rds_conn_drop(ic->conn);
			}
		}
		spin_unlock_bh(&rds_ibdev->spinlock);
	}

out:
	kunmap(page);
	__free_page(page);

	return ret;
}

static void rds_ib_check_up_port(void)
{
	struct net_device *dev;
	int     downs;
	int     retries = 0;

retry:
	downs = 0;
	read_lock(&dev_base_lock);
	for_each_netdev(&init_net, dev) {
		if ((dev->type == ARPHRD_INFINIBAND) &&
				(dev->flags & IFF_UP) &&
				!(dev->flags & IFF_SLAVE) &&
				!(dev->flags & IFF_MASTER)) {
			if (dev->operstate != IF_OPER_UP)
				downs++;
		}
	}
	read_unlock(&dev_base_lock);

	if (downs) {
		if (retries++ <= 30) {
			msleep(1000);
			goto retry;
		} else {
			printk(KERN_ERR "RDS/IB: Some port(s) may not be "
					"operational\n");
		}
	}
}


static u8 rds_ib_init_port(struct rds_ib_device	*rds_ibdev,
				struct net_device	*net_dev,
				u8			port_num,
				union ib_gid		gid)
{
	const char *digits = "0123456789";

	if (ip_port_cnt >= ip_port_max) {
		printk(KERN_ERR
			"RDS/IB: Exceeded max ports (%d) for device %s\n",
				ip_port_max, rds_ibdev->dev->name);
		return 0;
	}

	ip_port_cnt++;
	ip_config[ip_port_cnt].port_num = port_num;
	ip_config[ip_port_cnt].port_label[0] = 'P';
	ip_config[ip_port_cnt].port_label[1] = digits[ip_port_cnt / 10];
	ip_config[ip_port_cnt].port_label[2] = digits[ip_port_cnt % 10];
	ip_config[ip_port_cnt].port_label[3] = 0;
	ip_config[ip_port_cnt].dev = net_dev;
	ip_config[ip_port_cnt].rds_ibdev = rds_ibdev;
	ip_config[ip_port_cnt].ip_active_port = 0;
	strcpy(ip_config[ip_port_cnt].if_name, net_dev->name);
	memcpy(&ip_config[ip_port_cnt].gid, &gid, sizeof(union ib_gid));

	if (net_dev->operstate == IF_OPER_UP)
		ip_config[ip_port_cnt].port_state = RDS_IB_PORT_UP;
	else
		ip_config[ip_port_cnt].port_state = RDS_IB_PORT_INIT;

	return ip_port_cnt;
}

static void rds_ib_set_port(struct rds_ib_device	*rds_ibdev,
				struct net_device	*net_dev,
				char			*if_name,
				u8			port,
				__be32			ip_addr,
				__be32			ip_bcast,
				__be32			ip_mask)
{
	unsigned int	idx, i;
	__be32          excl_addr = 0;

	for (i = 0; i < excl_ips_cnt; i++) {
		if (!((excl_ips_tbl[i].ip ^ ip_addr) &
			excl_ips_tbl[i].mask)) {
			excl_addr = 1;
			break;
		}
	}

	if (!strcmp(net_dev->name, if_name)) {
		if (excl_addr)
			ip_addr = ip_bcast = ip_mask = 0;

		strcpy(ip_config[port].if_name, if_name);
		ip_config[port].ip_addr = ip_addr;
		ip_config[port].ip_bcast = ip_bcast;
		ip_config[port].ip_mask = ip_mask;
		ip_config[port].ip_active_port = port;
	} else if (!excl_addr) {
		idx = ip_config[port].alias_cnt++;
		strcpy(ip_config[port].aliases[idx].if_name, if_name);
		ip_config[port].aliases[idx].ip_addr = ip_addr;
		ip_config[port].aliases[idx].ip_bcast = ip_bcast;
		ip_config[port].aliases[idx].ip_mask = ip_mask;
	}
}

static void rds_ib_do_failover(u8 from_port, u8 to_port, u8 arp_port,
				int event_type)
{
	u8      j;
	int	ret;

	if (!ip_config[from_port].ip_addr)
		return;

	if (!to_port)
		to_port = rds_ib_get_failover_port(from_port);

	if (!arp_port)
		arp_port = to_port;

	if (to_port) {
		if (!rds_ib_move_ip(
			ip_config[from_port].if_name,
			ip_config[to_port].if_name,
			from_port,
			to_port,
			arp_port,
			ip_config[from_port].ip_addr,
			ip_config[from_port].ip_bcast,
			ip_config[from_port].ip_mask,
			event_type,
			0,
			1)) {

			ip_config[from_port].ip_active_port = to_port;
			for (j = 0; j < ip_config[from_port].
				alias_cnt; j++) {

				ret = rds_ib_move_ip(
					ip_config[from_port].
						aliases[j].if_name,
					ip_config[to_port].if_name,
					from_port,
					to_port,
					arp_port,
					ip_config[from_port].
						aliases[j].ip_addr,
					ip_config[from_port].
						aliases[j].ip_bcast,
					ip_config[from_port].
						aliases[j].ip_mask,
					event_type,
					1,
					1);
			}
		}
	}
}

static void rds_ib_do_failback(u8 port, int event_type)
{
	u8      ip_active_port = ip_config[port].ip_active_port;
	u8      j;
	int     ret;

	if (!ip_config[port].ip_addr)
		return;

	if (port != ip_config[port].ip_active_port) {
		if (!rds_ib_move_ip(
			ip_config[ip_active_port].if_name,
			ip_config[port].if_name,
			ip_active_port,
			port,
			ip_active_port,
			ip_config[port].ip_addr,
			ip_config[port].ip_bcast,
			ip_config[port].ip_mask,
			event_type,
			0,
			0)) {

			ip_config[port].ip_active_port = port;
			for (j = 0; j < ip_config[port].
				alias_cnt; j++) {

				ret = rds_ib_move_ip(
					ip_config[ip_active_port].
						if_name,
					ip_config[port].
						aliases[j].if_name,
					ip_active_port,
					port,
					ip_active_port,
					ip_config[port].
						aliases[j].ip_addr,
					ip_config[port].
						aliases[j].ip_bcast,
					ip_config[port].
						aliases[j].ip_mask,
					event_type,
					1,
					0);
			}
		}
	}
}

static void rds_ib_failover(struct work_struct *_work)
{
	struct rds_ib_port_ud_work	*work =
		container_of(_work, struct rds_ib_port_ud_work, work.work);
	int				ret;
	u8				i;
	char				if_name[IFNAMSIZ];

	ip_config[work->port].port_state = RDS_IB_PORT_DOWN;

	for (i = 1; i <= ip_port_cnt; i++) {
		if (i != work->port &&
			ip_config[i].port_state == RDS_IB_PORT_DOWN &&
			ip_config[i].ip_active_port == work->port) {

			strcpy(if_name, ip_config[work->port].if_name);
			strcat(if_name, ":");
			strcat(if_name, ip_config[i].port_label);
			if_name[IFNAMSIZ-1] = 0;
			ret = rds_ib_set_ip(NULL, NULL, if_name, 0, 0, 0);

			rds_ib_do_failover(i, 0, 0, work->event_type);
		}
	}

	if (ip_config[work->port].ip_addr)
		rds_ib_do_failover(work->port, 0, 0, work->event_type);

	if (ip_config[work->port].ip_active_port == work->port) {
		ret = rds_ib_set_ip(NULL, NULL,
				ip_config[work->port].if_name,
				0, 0, 0);
	}

	kfree(work);
}

static void rds_ib_failback(struct work_struct *_work)
{
	struct rds_ib_port_ud_work	*work =
		container_of(_work, struct rds_ib_port_ud_work, work.work);
	u8				i, ip_active_port, port = work->port;

	if (ip_config[port].port_state == RDS_IB_PORT_INIT) {
		printk(KERN_NOTICE "RDS/IB: %s/port_%d/%s is ERROR\n",
			ip_config[port].rds_ibdev->dev->name,
			ip_config[port].port_num,
			ip_config[port].if_name);

		rds_ib_do_failover(port, 0, 0, work->event_type);
		if (ip_config[port].ip_active_port != port)
			ip_config[port].port_state = RDS_IB_PORT_DOWN;
		goto out;
	}

	ip_config[port].port_state = RDS_IB_PORT_UP;

	ip_active_port = ip_config[port].ip_active_port;

	rds_ib_do_failback(port, work->event_type);

	for (i = 1; i <= ip_port_cnt; i++) {
		if (i == port ||
			ip_config[i].port_state == RDS_IB_PORT_UP ||
			!ip_config[i].ip_addr)
			continue;

		if (ip_config[i].ip_active_port == i) {
			rds_ib_do_failover(i, 0, ip_active_port,
						work->event_type);
		} else if (ip_config[i].ip_active_port == port) {
			rds_ib_do_failover(i, port, ip_active_port,
						work->event_type);
		} else if (ip_config[ip_config[i].ip_active_port].port_state ==
				RDS_IB_PORT_DOWN) {
			rds_ib_do_failover(i, 0, ip_active_port,
						work->event_type);
		} else if (ip_config[port].failover_group ==
				ip_config[i].failover_group) {
			rds_ib_do_failover(i, port, ip_active_port,
						work->event_type);
		}
	}

	if (ip_active_port != ip_config[port].ip_active_port) {
		for (i = 1; i <= ip_port_cnt; i++) {
			if (ip_config[i].port_state == RDS_IB_PORT_DOWN &&
				i != ip_active_port && ip_config[i].ip_addr &&
				ip_config[i].ip_active_port == ip_active_port) {

				rds_ib_do_failover(i, ip_active_port,
							ip_active_port,
							work->event_type);
			}
		}
	}

out:
	kfree(work);
}

static int rds_ib_ip_config_down(void)
{
	u8	i;

	for (i = 1; i <= ip_port_cnt; i++) {
		if (ip_config[i].port_state == RDS_IB_PORT_UP)
			return 0;
	}

	return 1;
}

static void rds_ib_net_failback(struct work_struct *_work)
{
	struct rds_ib_port_ud_work	*work =
		container_of(_work, struct rds_ib_port_ud_work, work.work);
	struct in_device		*in_dev;

	in_dev = in_dev_get(ip_config[work->port].dev);
	if (in_dev && !in_dev->ifa_list &&
		ip_config[work->port].ip_addr &&
		work->timeout > 0) {
		INIT_DELAYED_WORK(&work->work, rds_ib_net_failback);
		work->timeout -= msecs_to_jiffies(100);
		queue_delayed_work(rds_wq, &work->work,
			msecs_to_jiffies(100));
	} else {
		rds_ib_failback((struct work_struct *)&work->work);
	}

	if (in_dev)
		in_dev_put(in_dev);
}

static void rds_ib_event_handler(struct ib_event_handler *handler,
				struct ib_event *event)
{
	struct rds_ib_device	*rds_ibdev =
		container_of(handler, typeof(*rds_ibdev), event_handler);
	u8	port;
	struct rds_ib_port_ud_work	*work;

	if (!rds_ib_active_bonding_enabled || !ip_port_cnt)
		return;

	if (event->event != IB_EVENT_PORT_ACTIVE &&
		event->event != IB_EVENT_PORT_ERR)
		return;

	for (port = 1; port <= ip_port_cnt; port++) {
		if (ip_config[port].port_num != event->element.port_num ||
			ip_config[port].rds_ibdev != rds_ibdev)
			continue;

		printk(KERN_NOTICE "RDS/IB: %s/port_%d/%s is %s\n",
			rds_ibdev->dev->name,
			event->element.port_num,
			ip_config[port].if_name,
			(event->event == IB_EVENT_PORT_ACTIVE) ?
				"ACTIVE" : "ERROR");

		work = kzalloc(sizeof *work, GFP_ATOMIC);
		if (!work) {
			printk(KERN_ERR
				"RDS/IB: failed to allocate port work\n");
			return;
		}

		work->port = port;
		work->event_type = RDS_IB_PORT_EVENT_IB;

		if (event->event == IB_EVENT_PORT_ACTIVE) {
			if (rds_ib_active_bonding_fallback) {
				INIT_DELAYED_WORK(&work->work, rds_ib_failback);
				queue_delayed_work(rds_wq, &work->work, 0);
			} else
				kfree(work);
		} else {
			INIT_DELAYED_WORK(&work->work, rds_ib_failover);
			queue_delayed_work(rds_wq, &work->work, 0);
		}
	}
}

static void rds_ib_dump_ip_config(void)
{
	int	i, j;

	if (!rds_ib_active_bonding_enabled || !ip_port_cnt)
		return;

	printk(KERN_ERR "RDS/IB: IP configuration ...\n");
	for (i = 1; i <= ip_port_cnt; i++) {
		printk(KERN_ERR "RDS/IB: %s/port_%d/%s: "
			"IP %d.%d.%d.%d/%d.%d.%d.%d/%d.%d.%d.%d "
			"state %s\n",
			((ip_config[i].rds_ibdev) ?
				((ip_config[i].rds_ibdev->dev) ?
					ip_config[i].rds_ibdev->dev->name :
						"No IB device") :
							"No RDS device"),
			ip_config[i].port_num,
			ip_config[i].if_name,
			NIPQUAD(ip_config[i].ip_addr),
			NIPQUAD(ip_config[i].ip_bcast),
			NIPQUAD(ip_config[i].ip_mask),
			(ip_config[i].port_state ==
				RDS_IB_PORT_UP ? "UP" : "DOWN"));

		for (j = 0; j < ip_config[i].alias_cnt; j++) {
			printk(KERN_ERR "Alias %s "
				"IP %d.%d.%d.%d/%d.%d.%d.%d/%d.%d.%d.%d\n",
				ip_config[i].aliases[j].if_name,
				NIPQUAD(ip_config[i].
					aliases[j].ip_addr),
				NIPQUAD(ip_config[i].
					aliases[j].ip_bcast),
				NIPQUAD(ip_config[i].
					aliases[j].ip_mask));
		}
	}
}

static int rds_ib_ip_config_init(void)
{
	struct net_device	*dev;
	struct in_ifaddr	*ifa;
	struct in_ifaddr	**ifap;
	struct in_device	*in_dev;
	struct rds_ib_device	*rds_ibdev;
	union ib_gid            gid;
	int                     ret = 0;
	u8                      port_num;
	u8                      port;

	if (!rds_ib_active_bonding_enabled)
		return 0;

	rds_ib_check_up_port();

	rcu_read_unlock();

	ip_config = kzalloc(sizeof(struct rds_ib_port) *
				(ip_port_max + 1), GFP_KERNEL);
	if (!ip_config) {
		printk(KERN_ERR "RDS/IB: failed to allocate IP config\n");
		return 1;
	}

	read_lock(&dev_base_lock);
	for_each_netdev(&init_net, dev) {
		in_dev = in_dev_get(dev);
		if ((dev->type == ARPHRD_INFINIBAND) &&
			(dev->flags & IFF_UP) &&
			!(dev->flags & IFF_SLAVE) &&
			!(dev->flags & IFF_MASTER) &&
			in_dev) {
			memcpy(&gid, dev->dev_addr + 4, sizeof gid);

			list_for_each_entry_rcu(rds_ibdev,
					&rds_ib_devices, list) {
				ret = ib_find_cached_gid(rds_ibdev->dev,
					&gid, &port_num, NULL);
				if (!ret)
					break;
			}

			if (ret) {
				printk(KERN_ERR "RDS/IB: GID "RDS_IB_GID_FMT
					" has no associated port\n",
					RDS_IB_GID_ARG(gid));
			} else {
				port = rds_ib_init_port(rds_ibdev, dev,
					port_num, gid);
				if (port > 0) {
					for (ifap = &in_dev->ifa_list;
						(ifa = *ifap);
						ifap = &ifa->ifa_next) {
						rds_ib_set_port(rds_ibdev, dev,
							ifa->ifa_label,
							port,
							ifa->ifa_address,
							ifa->ifa_broadcast,
							ifa->ifa_mask);
					}
				}
			}
		}
		if (in_dev)
			in_dev_put(in_dev);
	}

	rds_ib_dump_ip_config();
	read_unlock(&dev_base_lock);
	return ret;
}

static int rds_ib_excl_ip(char *str)
{
	char *tok, *nxt_tok, *end, *prefix_str;
	unsigned int octet_cnt = 0;
	unsigned long prefix = 0;
	__be32  ip = 0;

	prefix_str = strchr(str, '/');
	if (prefix_str) {
		*prefix_str = '\0';
		prefix_str++;
		prefix = simple_strtol(prefix_str, &end, 0);
		if (*end) {
			printk(KERN_WARNING "RDS/IP: Warning: IP prefix "
				"%s improperly formatted\n", prefix_str);
			goto err;
		} else if (prefix > 32) {
			printk(KERN_WARNING "RDS/IP: Warning: IP prefix "
				"%lu out of range\n", prefix);
			goto err;
		} else {
			tok = str;
			while (tok && octet_cnt < 4) {
				unsigned long octet;

				nxt_tok = strchr(tok, '.');
				if (nxt_tok) {
					*nxt_tok = '\0';
					nxt_tok++;
				}
				octet = simple_strtoul(tok, &end, 0);
				if (*end) {
					printk(KERN_WARNING "RDS/IP: Warning: "
						"IP octet %s improperly "
						" formatted\n", tok);
					goto err;
				} else if (octet > 255) {
					printk(KERN_WARNING "RDS/IP: Warning: "
						"IP octet %lu out of range\n",
						octet);
					goto err;
				} else {
					((unsigned char *)&ip)[octet_cnt] =
						(unsigned char)octet;
					octet_cnt++;
				}
				tok = nxt_tok;
			}

			if (tok) {
				printk(KERN_WARNING "RDS/IP: Warning: IP "
					"%s is improperly formatted\n", str);
				goto err;
			}
		}
	} else {
		printk(KERN_WARNING "RDS/IP: Warning: IP prefix not "
			"specified\n");
		goto err;
	}

	excl_ips_tbl[excl_ips_cnt].ip = ip;
	excl_ips_tbl[excl_ips_cnt].prefix = prefix;
	excl_ips_tbl[excl_ips_cnt].mask = inet_make_mask(prefix);

	excl_ips_cnt++;

	return 0;
err:
	return 1;
}

void rds_ib_ip_excl_ips_init(void)
{
	char *tok, *nxt_tok;
	char str[1024];

	if (rds_ib_active_bonding_excl_ips == NULL)
		return;

	strcpy(str, rds_ib_active_bonding_excl_ips);

	tok = str;
	while (tok) {
		nxt_tok = strchr(tok, ',');
		if (nxt_tok) {
			*nxt_tok = '\0';
			nxt_tok++;
		}

		if (rds_ib_excl_ip(tok))
			return;

		tok = nxt_tok;
	}
}

void rds_ib_ip_failover_groups_init(void)
{
	char *tok, *grp, *nxt_tok, *nxt_grp;
	char str[1024];
	unsigned int	grp_id = 1;
	int i;
	struct rds_ib_device *rds_ibdev;

	if (!rds_ib_active_bonding_enabled)
		return;

	if (rds_ib_active_bonding_failover_groups == NULL) {
		list_for_each_entry_rcu(rds_ibdev, &rds_ib_devices, list) {
			for (i = 1; i <= ip_port_cnt; i++) {
				if (ip_config[i].rds_ibdev == rds_ibdev)
					ip_config[i].failover_group = grp_id;
			}
			grp_id++;
		}
		return;
	}

	strcpy(str, rds_ib_active_bonding_failover_groups);
	nxt_grp = strchr(str, ';');
	if (nxt_grp) {
		*nxt_grp = '\0';
		nxt_grp++;
	}
	grp = str;
	while (grp) {
		tok = grp;
		nxt_tok = strchr(tok, ',');
		if (nxt_tok) {
			*nxt_tok = '\0';
			nxt_tok++;
		}
		while (tok) {
			for (i = 1; i <= ip_port_cnt; i++) {
				if (!strcmp(tok, ip_config[i].if_name)) {
					if (!ip_config[i].failover_group)
						ip_config[i].failover_group =
							grp_id;
					else
						printk(KERN_WARNING "RDS/IB: %s is already part of another failover group\n", tok);
					break;
				}
			}
			tok = nxt_tok;
			nxt_tok = strchr(str, ',');
			if (nxt_tok) {
				*nxt_tok = '\0';
				nxt_tok++;
			}
		}

		grp = nxt_grp;
		nxt_grp = strchr(str, ';');
		if (nxt_grp) {
			*nxt_grp = '\0';
			nxt_grp++;
		}
		grp_id++;
	}
}

void rds_ib_add_one(struct ib_device *device)
{
	struct rds_ib_device *rds_ibdev;
	struct ib_device_attr *dev_attr;

	/* Only handle IB (no iWARP) devices */
	if (device->node_type != RDMA_NODE_IB_CA)
		return;

	dev_attr = kmalloc(sizeof *dev_attr, GFP_KERNEL);
	if (!dev_attr)
		return;

	if (ib_query_device(device, dev_attr)) {
		rdsdebug("Query device failed for %s\n", device->name);
		goto free_attr;
	}

	rds_ibdev = kzalloc_node(sizeof(struct rds_ib_device), GFP_KERNEL,
				ibdev_to_node(device));
	if (!rds_ibdev)
		goto free_attr;

	spin_lock_init(&rds_ibdev->spinlock);
	atomic_set(&rds_ibdev->refcount, 1);
	INIT_WORK(&rds_ibdev->free_work, rds_ib_dev_free);
	init_waitqueue_head(&rds_ibdev->wait);
	rds_ibdev->done = 0;

	rds_ibdev->max_wrs = dev_attr->max_qp_wr;
	rds_ibdev->max_sge = min(dev_attr->max_sge, RDS_IB_MAX_SGE);

	rds_ibdev->fmr_max_remaps = dev_attr->max_map_per_fmr?: 32;

	rds_ibdev->max_1m_fmrs = dev_attr->max_fmr ?
		min_t(unsigned int, dev_attr->max_fmr,
			rds_ib_fmr_1m_pool_size) :
			rds_ib_fmr_1m_pool_size;

	rds_ibdev->max_8k_fmrs = dev_attr->max_fmr ?
		min_t(unsigned int, dev_attr->max_fmr,
			rds_ib_fmr_8k_pool_size) :
			rds_ib_fmr_8k_pool_size;

	rds_ibdev->max_initiator_depth = dev_attr->max_qp_init_rd_atom;
	rds_ibdev->max_responder_resources = dev_attr->max_qp_rd_atom;

	rds_ibdev->dev = device;
	rds_ibdev->pd = ib_alloc_pd(device);
	if (IS_ERR(rds_ibdev->pd)) {
		rds_ibdev->pd = NULL;
		goto put_dev;
	}

	if (rds_ib_active_bonding_enabled) {
		INIT_IB_EVENT_HANDLER(&rds_ibdev->event_handler,
				rds_ibdev->dev, rds_ib_event_handler);
		if (ib_register_event_handler(&rds_ibdev->event_handler)) {
			printk(KERN_ERR
				"RDS/IB: ib_register_event_handler failed\n");
			goto put_dev;
		}
	}

	rds_ibdev->vector_load = kzalloc(sizeof(int) *
					device->num_comp_vectors, GFP_KERNEL);
	if (!rds_ibdev->vector_load) {
		printk(KERN_ERR "RDS/IB: failed to allocate vector memoru\n");
		goto put_dev;
	}

	rds_ibdev->mr = ib_get_dma_mr(rds_ibdev->pd, IB_ACCESS_LOCAL_WRITE);
	if (IS_ERR(rds_ibdev->mr)) {
		rds_ibdev->mr = NULL;
		goto put_dev;
	}

	rds_ibdev->mr_1m_pool =
		rds_ib_create_mr_pool(rds_ibdev, RDS_IB_MR_1M_POOL);
	if (IS_ERR(rds_ibdev->mr_1m_pool)) {
		rds_ibdev->mr_1m_pool = NULL;
		goto put_dev;
	}

	rds_ibdev->mr_8k_pool =
		rds_ib_create_mr_pool(rds_ibdev, RDS_IB_MR_8K_POOL);
	if (IS_ERR(rds_ibdev->mr_8k_pool)) {
		rds_ibdev->mr_8k_pool = NULL;
		goto put_dev;
	}

	rds_ibdev->srq = kmalloc(sizeof(struct rds_ib_srq), GFP_KERNEL);
	if (!rds_ibdev->srq)
		goto free_attr;

	INIT_LIST_HEAD(&rds_ibdev->ipaddr_list);
	INIT_LIST_HEAD(&rds_ibdev->conn_list);

	down_write(&rds_ib_devices_lock);
	list_add_tail_rcu(&rds_ibdev->list, &rds_ib_devices);
	up_write(&rds_ib_devices_lock);
	atomic_inc(&rds_ibdev->refcount);

	ib_set_client_data(device, &rds_ib_client, rds_ibdev);
	atomic_inc(&rds_ibdev->refcount);

	rds_ib_nodev_connect();
put_dev:
	rds_ib_dev_put(rds_ibdev);
free_attr:
	kfree(dev_attr);
}

static void rds_ib_unregister_client(void)
{
	ib_unregister_client(&rds_ib_client);
	/* wait for rds_ib_dev_free() to complete */
	flush_workqueue(rds_wq);
}

static void rds_ib_update_ip_config(void)
{
	struct net_device	*dev;
	struct in_device	*in_dev;
	int			i;

	read_lock(&dev_base_lock);
	for_each_netdev(&init_net, dev) {
		in_dev = in_dev_get(dev);
		if (in_dev) {
			for (i = 0; i <= ip_port_cnt; i++) {
				if (!strcmp(dev->name, ip_config[i].if_name)) {
					if (ip_config[i].dev != dev) {
						ip_config[i].dev = dev;
						printk(KERN_NOTICE "RDS/IB: "
							"dev %s/port_%d/%s updated",
					ip_config[i].rds_ibdev->dev->name,
							ip_config[i].port_num,
							dev->name);
					}
				}
			}
			in_dev_put(in_dev);
		}
	}
	read_unlock(&dev_base_lock);
}

static int rds_ib_netdev_callback(struct notifier_block *self, unsigned long event, void *ctx)
{
	struct net_device *ndev = (struct net_device *)ctx;
	u8 port = 0;
	u8 i;
	struct rds_ib_port_ud_work *work;

	if (!rds_ib_active_bonding_enabled || !ip_port_cnt)
		return NOTIFY_DONE;

	if (event != NETDEV_UP && event != NETDEV_DOWN)
		return NOTIFY_DONE;

	for (i = 1; i <= ip_port_cnt; i++) {
		if (!strcmp(ndev->name, ip_config[i].if_name) &&
			ip_config[i].rds_ibdev) {
			if (event == NETDEV_UP && ip_config[i].dev != ndev)
				rds_ib_update_ip_config();
			port = i;
			break;
		}
	}

	if (!port)
		return NOTIFY_DONE;

	if (ip_config[port].rds_ibdev == NULL)
		return NOTIFY_DONE;

	work = kzalloc(sizeof *work, GFP_ATOMIC);
	if (!work) {
		printk(KERN_ERR "RDS/IB: failed to allocate port work\n");
		return NOTIFY_DONE;
	}

	printk(KERN_NOTICE "RDS/IB: %s/port_%d/%s is %s\n",
		ip_config[port].rds_ibdev->dev->name,
		ip_config[port].port_num, ndev->name,
		(event == NETDEV_UP) ? "UP" : "DOWN");

	work->dev = ndev;
	work->port = port;
	work->event_type = RDS_IB_PORT_EVENT_NET;

	switch (event) {
	case NETDEV_UP:
		if (rds_ib_active_bonding_fallback) {
			if (rds_ib_ip_config_down() ||
				ip_config[port].port_state ==
					RDS_IB_PORT_INIT) {
				INIT_DELAYED_WORK(&work->work,
					rds_ib_net_failback);
				work->timeout = msecs_to_jiffies(10000);
			} else {
				INIT_DELAYED_WORK(&work->work,
					rds_ib_net_failback);
				work->timeout = msecs_to_jiffies(1000);
			}
			queue_delayed_work(rds_wq, &work->work,
					msecs_to_jiffies(100));
		} else
			kfree(work);

		break;
	case NETDEV_DOWN:
		INIT_DELAYED_WORK(&work->work, rds_ib_failover);
		queue_delayed_work(rds_wq, &work->work, 0);
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block rds_ib_nb = {
	.notifier_call = rds_ib_netdev_callback
};

int rds_ib_init(void)
{
	int ret;

	INIT_LIST_HEAD(&rds_ib_devices);

	ret = sock_create_kern(PF_INET, SOCK_STREAM, IPPROTO_TCP,
				&rds_ib_inet_socket);
	if (ret < 0) {
		printk(KERN_ERR "RDS/IB: can't create TCP transport socket (%d).\n", -ret);
		goto out;
	}
	sock_net_set(rds_ib_inet_socket->sk, &init_net);

	ret = rds_ib_fmr_init();
	if (ret)
		goto out;

	ret = ib_register_client(&rds_ib_client);
	if (ret)
		goto out_fmr_exit;

	ret = rds_ib_sysctl_init();
	if (ret)
		goto out_ibreg;

	ret = rds_ib_recv_init();
	if (ret)
		goto out_sysctl;

	ret = rds_ib_srqs_init();
	if (ret) {
		printk(KERN_ERR "RDS/IB: Failed to init SRQ\n");
		goto out_recv;
	}

	rds_aux_wq = create_singlethread_workqueue("krdsd_aux");
	if (!rds_aux_wq) {
		printk(KERN_ERR "RDS/IB: failed to create aux workqueue\n");
		goto out_srq;
	}

	ret = rds_trans_register(&rds_ib_transport);
	if (ret)
		goto out_srq;

	rds_info_register_func(RDS_INFO_IB_CONNECTIONS, rds_ib_ic_info);

	rds_ib_ip_excl_ips_init();

	ret = rds_ib_ip_config_init();
	if (ret) {
		printk(KERN_ERR "RDS/IB: failed to init port\n");
		goto out_srq;
	}

	rds_ib_ip_failover_groups_init();

	register_netdevice_notifier(&rds_ib_nb);

	goto out;

out_srq:
	rds_ib_srqs_exit();
out_recv:
	rds_ib_recv_exit();
out_sysctl:
	rds_ib_sysctl_exit();
out_ibreg:
	rds_ib_unregister_client();
out_fmr_exit:
	rds_ib_fmr_exit();
out:
	return ret;
}


void rds_ib_exit(void)
{
	unregister_netdevice_notifier(&rds_ib_nb);
	rds_info_deregister_func(RDS_INFO_IB_CONNECTIONS, rds_ib_ic_info);
	rds_ib_unregister_client();
	rds_ib_destroy_nodev_conns();
	rds_ib_sysctl_exit();
	rds_ib_srqs_exit();
	rds_ib_recv_exit();
	flush_workqueue(rds_aux_wq);
	destroy_workqueue(rds_aux_wq);
	rds_trans_unregister(&rds_ib_transport);
	rds_ib_fmr_exit();

	if (ip_config)
		kfree(ip_config);
}

struct rds_transport rds_ib_transport = {
	.laddr_check		= rds_ib_laddr_check,
	.xmit_complete		= rds_ib_xmit_complete,
	.xmit			= rds_ib_xmit,
	.xmit_rdma		= rds_ib_xmit_rdma,
	.xmit_atomic		= rds_ib_xmit_atomic,
	.recv			= rds_ib_recv,
	.conn_alloc		= rds_ib_conn_alloc,
	.conn_free		= rds_ib_conn_free,
	.conn_connect		= rds_ib_conn_connect,
	.conn_shutdown		= rds_ib_conn_shutdown,
	.inc_copy_to_user	= rds_ib_inc_copy_to_user,
	.inc_free		= rds_ib_inc_free,
	.inc_to_skb		= rds_ib_inc_to_skb,
	.skb_local		= rds_skb_local,
	.cm_initiate_connect	= rds_ib_cm_initiate_connect,
	.cm_handle_connect	= rds_ib_cm_handle_connect,
	.cm_connect_complete	= rds_ib_cm_connect_complete,
	.stats_info_copy	= rds_ib_stats_info_copy,
	.exit			= rds_ib_exit,
	.get_mr			= rds_ib_get_mr,
	.sync_mr		= rds_ib_sync_mr,
	.free_mr		= rds_ib_free_mr,
	.flush_mrs		= rds_ib_flush_mrs,
	.check_migration        = rds_ib_check_migration,
	.t_owner		= THIS_MODULE,
	.t_name			= "infiniband",
	.t_type			= RDS_TRANS_IB
};

int rds_ib_inc_to_skb(struct rds_incoming *inc, struct sk_buff *skb)
{
	skb_frag_t *frag;
	int ret = 0, slen;
	u32 len;
	int i;
	struct rds_ib_incoming *ibinc;
	struct rds_page_frag *ibfrag;

	/* pull out initial pointers */
	ibinc  = container_of(inc, struct rds_ib_incoming, ii_inc);
	ibfrag = list_entry(ibinc->ii_frags.next, struct rds_page_frag, f_item);
	len    = be32_to_cpu(inc->i_hdr.h_len);
	slen   = len;
	i      = 0;

	/* run through the entire ib fragment list and save off the buffers */
	while (NULL != ibfrag && slen > 0) {
		/* one to one mapping of frags to sg structures */
		frag = &skb_shinfo(skb)->frags[i];

		/* save off all the sg pieces to the skb frags we are creating */
		frag->size        = ibfrag->f_sg.length;
		frag->page_offset = ibfrag->f_sg.offset;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 2, 0))
		frag->page        = sg_page(&ibfrag->f_sg);
#else
		frag->page.p      = sg_page(&ibfrag->f_sg);
#endif

		/* AA:  do we need to bump up the page reference */
		/* get_page(frag->page); */

		/* dec the amount of data we are consuming */
		slen -= frag->size;

		/* bump to the next entry */
		ibfrag = list_entry(ibfrag->f_item.next, struct rds_page_frag, f_item);
		i++;

		/* for now we will only have a single chain of fragments in the skb */
		if (i >= MAX_SKB_FRAGS) {
			rdsdebug("too many fragments in op %u > max %u, skb %p",
				 i, (int)MAX_SKB_FRAGS, skb);
			goto done;
		}
	}

	/* track the full message length too */
	skb->len = len;

	/* all good */
	ret = 1;

done:
	/* track all the fragments we saved */
	skb_shinfo(skb)->nr_frags = i;

	return ret;
}

MODULE_LICENSE("GPL");

