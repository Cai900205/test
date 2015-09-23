/*
 * Copyright (c) 2009-2012 Intel Corporation. All rights reserved.
 * Copyright (c) 2013 Mellanox Technologies LTD. All rights reserved.
 *
 * This software is available to you under the OpenIB.org BSD license
 * below:
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
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AWV
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <osd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <infiniband/acm.h>
#include <infiniband/umad.h>
#include <infiniband/verbs.h>
#include <dlist.h>
#include <search.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include "acm_mad.h"
#include "acm_util.h"

#define src_out     data[0]
#define src_index   data[1]
#define dst_index   data[2]

#define IB_LID_MCAST_START 0xc000

#define MAX_EP_ADDR 4
#define MAX_EP_MC   2

#ifndef SOCK_NONBLOCK
#define SOCK_NONBLOCK   O_NONBLOCK
#endif

enum acm_state {
	ACM_INIT,
	ACM_QUERY_ADDR,
	ACM_ADDR_RESOLVED,
	ACM_QUERY_ROUTE,
	ACM_READY
};

enum acm_addr_prot {
	ACM_ADDR_PROT_ACM
};

enum acm_route_prot {
	ACM_ROUTE_PROT_ACM,
	ACM_ROUTE_PROT_SA
};

enum acm_loopback_prot {
	ACM_LOOPBACK_PROT_NONE,
	ACM_LOOPBACK_PROT_LOCAL
};

enum acm_route_preload {
	ACM_ROUTE_PRELOAD_NONE,
	ACM_ROUTE_PRELOAD_OSM_FULL_V1
};

enum acm_addr_preload {
	ACM_ADDR_PRELOAD_NONE,
	ACM_ADDR_PRELOAD_HOSTS
};

/*
 * Nested locking order: dest -> ep, dest -> port
 */
struct acm_dest {
	uint8_t                address[ACM_MAX_ADDRESS]; /* keep first */
	char                   name[ACM_MAX_ADDRESS];
	struct ibv_ah          *ah;
	struct ibv_ah_attr     av;
	struct ibv_path_record path;
	union ibv_gid          mgid;
	uint64_t               req_id;
	DLIST_ENTRY            req_queue;
	uint32_t               remote_qpn;
	lock_t                 lock;
	enum acm_state         state;
	atomic_t               refcnt;
	uint64_t	       addr_timeout;
	uint64_t	       route_timeout;
	uint8_t                addr_type;
};

struct acm_port {
	struct acm_device   *dev;
	DLIST_ENTRY         ep_list;
	lock_t              lock;
	int                 mad_portid;
	int                 mad_agentid;
	struct acm_dest     sa_dest;
	union ibv_gid	    base_gid;
	enum ibv_port_state state;
	enum ibv_mtu        mtu;
	enum ibv_rate       rate;
	int                 subnet_timeout;
	int                 gid_cnt;
	uint16_t            default_pkey_ix;
	uint16_t            lid;
	uint16_t            lid_mask;
	uint8_t             port_num;
};

struct acm_device {
	struct ibv_context      *verbs;
	struct ibv_comp_channel *channel;
	struct ibv_pd           *pd;
	uint64_t                guid;
	DLIST_ENTRY             entry;
	int                     port_cnt;
	struct acm_port         port[0];
};

/* Maintain separate virtual send queues to avoid deadlock */
struct acm_send_queue {
	int                   credits;
	DLIST_ENTRY           pending;
};

struct acm_ep {
	struct acm_port       *port;
	struct ibv_cq         *cq;
	struct ibv_qp         *qp;
	struct ibv_mr         *mr;
	uint8_t               *recv_bufs;
	DLIST_ENTRY           entry;
	union acm_ep_info     addr[MAX_EP_ADDR];
	char                  name[MAX_EP_ADDR][ACM_MAX_ADDRESS];
	char		      id_string[ACM_MAX_ADDRESS];
	uint8_t               addr_type[MAX_EP_ADDR];
	void                  *dest_map[ACM_ADDRESS_RESERVED - 1];
	struct acm_dest       mc_dest[MAX_EP_MC];
	int                   mc_cnt;
	uint16_t              pkey_index;
	uint16_t              pkey;
	lock_t                lock;
	struct acm_send_queue resolve_queue;
	struct acm_send_queue sa_queue;
	struct acm_send_queue resp_queue;
	DLIST_ENTRY           active_queue;
	DLIST_ENTRY           wait_queue;
	enum acm_state        state;
};

struct acm_send_msg {
	DLIST_ENTRY          entry;
	struct acm_ep        *ep;
	struct acm_dest      *dest;
	struct ibv_ah        *ah;
	void                 *context;
	void                 (*resp_handler)(struct acm_send_msg *req,
	                                     struct ibv_wc *wc, struct acm_mad *resp);
	struct acm_send_queue *req_queue;
	struct ibv_mr        *mr;
	struct ibv_send_wr   wr;
	struct ibv_sge       sge;
	uint64_t             expires;
	int                  tries;
	uint8_t              data[ACM_SEND_SIZE];
};

struct acm_client {
	lock_t   lock;   /* acquire ep lock first */
	SOCKET   sock;
	int      index;
	atomic_t refcnt;
};

struct acm_request {
	struct acm_client *client;
	DLIST_ENTRY       entry;
	struct acm_msg    msg;
};

union socket_addr {
	struct sockaddr     sa;
	struct sockaddr_in  sin;
	struct sockaddr_in6 sin6;
};

static DLIST_ENTRY dev_list;

static atomic_t tid;
static DLIST_ENTRY timeout_list;
static event_t timeout_event;
static atomic_t wait_cnt;

static SOCKET listen_socket;
static SOCKET ip_mon_socket;
static struct acm_client client_array[FD_SETSIZE - 1];

static FILE *flog;
static lock_t log_lock;
PER_THREAD char log_data[ACM_MAX_ADDRESS];
static atomic_t counter[ACM_MAX_COUNTER];

/*
 * Service options - may be set through ibacm_opts.cfg file.
 */
static char *acme = BINDIR "/ib_acme -A";
static char *opts_file = ACM_CONF_DIR "/" ACM_OPTS_FILE;
static char *addr_file = ACM_CONF_DIR "/" ACM_ADDR_FILE;
static char route_data_file[128] = ACM_CONF_DIR "/ibacm_route.data";
static char addr_data_file[128] = ACM_CONF_DIR "/ibacm_hosts.data";
static char log_file[128] = "/var/log/ibacm.log";
static int log_level = 0;
static char lock_file[128] = "/var/run/ibacm.pid";
static enum acm_addr_prot addr_prot = ACM_ADDR_PROT_ACM;
static int addr_timeout = 1440;
static enum acm_route_prot route_prot = ACM_ROUTE_PROT_SA;
static int route_timeout = -1;
static enum acm_loopback_prot loopback_prot = ACM_LOOPBACK_PROT_LOCAL;
static short server_port = 6125;
static int timeout = 2000;
static int retries = 2;
static int resolve_depth = 1;
static int sa_depth = 1;
static int send_depth = 1;
static int recv_depth = 1024;
static uint8_t min_mtu = IBV_MTU_2048;
static uint8_t min_rate = IBV_RATE_10_GBPS;
static enum acm_route_preload route_preload;
static enum acm_addr_preload addr_preload;
static int support_ips_in_addr_cfg = 0;

void acm_write(int level, const char *format, ...)
{
	va_list args;
	struct timeval tv;

	if (level > log_level)
		return;

	gettimeofday(&tv, NULL);
	va_start(args, format);
	lock_acquire(&log_lock);
	fprintf(flog, "%u.%03u: ", (unsigned) tv.tv_sec, (unsigned) (tv.tv_usec / 1000));
	vfprintf(flog, format, args);
	fflush(flog);
	lock_release(&log_lock);
	va_end(args);
}

static void
acm_format_name(int level, char *name, size_t name_size,
		uint8_t addr_type, uint8_t *addr, size_t addr_size)
{
	struct ibv_path_record *path;

	if (level > log_level)
		return;

	switch (addr_type) {
	case ACM_EP_INFO_NAME:
		memcpy(name, addr, addr_size);
		break;
	case ACM_EP_INFO_ADDRESS_IP:
		inet_ntop(AF_INET, addr, name, name_size);
		break;
	case ACM_EP_INFO_ADDRESS_IP6:
	case ACM_ADDRESS_GID:
		inet_ntop(AF_INET6, addr, name, name_size);
		break;
	case ACM_EP_INFO_PATH:
		path = (struct ibv_path_record *) addr;
		if (path->dlid) {
			snprintf(name, name_size, "SLID(%u) DLID(%u)",
				ntohs(path->slid), ntohs(path->dlid));
		} else {
			acm_format_name(level, name, name_size, ACM_ADDRESS_GID,
					path->dgid.raw, sizeof path->dgid);
		}
		break;
	case ACM_ADDRESS_LID:
		snprintf(name, name_size, "LID(%u)", ntohs(*((uint16_t *) addr)));
		break;
	default:
		strcpy(name, "Unknown");
		break;
	}
}

static int ib_any_gid(union ibv_gid *gid)
{
	return ((gid->global.subnet_prefix | gid->global.interface_id) == 0);
}

static int acm_compare_dest(const void *dest1, const void *dest2)
{
	return memcmp(dest1, dest2, ACM_MAX_ADDRESS);
}

static void
acm_set_dest_addr(struct acm_dest *dest, uint8_t addr_type, uint8_t *addr, size_t size)
{
	memcpy(dest->address, addr, size);
	dest->addr_type = addr_type;
	acm_format_name(0, dest->name, sizeof dest->name, addr_type, addr, size);
}

static void
acm_init_dest(struct acm_dest *dest, uint8_t addr_type, uint8_t *addr, size_t size)
{
	DListInit(&dest->req_queue);
	atomic_init(&dest->refcnt);
	atomic_set(&dest->refcnt, 1);
	lock_init(&dest->lock);
	if (size)
		acm_set_dest_addr(dest, addr_type, addr, size);
}

static struct acm_dest *
acm_alloc_dest(uint8_t addr_type, uint8_t *addr)
{
	struct acm_dest *dest;

	dest = calloc(1, sizeof *dest);
	if (!dest) {
		acm_log(0, "ERROR - unable to allocate dest\n");
		return NULL;
	}

	acm_init_dest(dest, addr_type, addr, ACM_MAX_ADDRESS);
	acm_log(1, "%s\n", dest->name);
	return dest;
}

/* Caller must hold ep lock. */
static struct acm_dest *
acm_get_dest(struct acm_ep *ep, uint8_t addr_type, uint8_t *addr)
{
	struct acm_dest *dest, **tdest;

	tdest = tfind(addr, &ep->dest_map[addr_type - 1], acm_compare_dest);
	if (tdest) {
		dest = *tdest;
		(void) atomic_inc(&dest->refcnt);
		acm_log(2, "%s\n", dest->name);
	} else {
		dest = NULL;
		acm_format_name(2, log_data, sizeof log_data,
				addr_type, addr, ACM_MAX_ADDRESS);
		acm_log(2, "%s not found\n", log_data);
	}
	return dest;
}

static void
acm_put_dest(struct acm_dest *dest)
{
	acm_log(2, "%s\n", dest->name);
	if (atomic_dec(&dest->refcnt) == 0) {
		free(dest);
	}
}

static struct acm_dest *
acm_acquire_dest(struct acm_ep *ep, uint8_t addr_type, uint8_t *addr)
{
	struct acm_dest *dest;

	acm_format_name(2, log_data, sizeof log_data,
			addr_type, addr, ACM_MAX_ADDRESS);
	acm_log(2, "%s\n", log_data);
	lock_acquire(&ep->lock);
	dest = acm_get_dest(ep, addr_type, addr);
	if (!dest) {
		dest = acm_alloc_dest(addr_type, addr);
		if (dest) {
			tsearch(dest, &ep->dest_map[addr_type - 1], acm_compare_dest);
			(void) atomic_inc(&dest->refcnt);
		}
	}
	lock_release(&ep->lock);
	return dest;
}

static struct acm_dest *
acm_acquire_sa_dest(struct acm_port *port)
{
	struct acm_dest *dest;

	lock_acquire(&port->lock);
	if (port->state == IBV_PORT_ACTIVE) {
		dest = &port->sa_dest;
		atomic_inc(&port->sa_dest.refcnt);
	} else {
		dest = NULL;
	}
	lock_release(&port->lock);
	return dest;
}

static void acm_release_sa_dest(struct acm_dest *dest)
{
	atomic_dec(&dest->refcnt);
}

/* Caller must hold ep lock. */
//static void
//acm_remove_dest(struct acm_ep *ep, struct acm_dest *dest)
//{
//	acm_log(2, "%s\n", dest->name);
//	tdelete(dest->address, &ep->dest_map[dest->addr_type - 1], acm_compare_dest);
//	acm_put_dest(dest);
//}

static struct acm_request *
acm_alloc_req(struct acm_client *client, struct acm_msg *msg)
{
	struct acm_request *req;

	req = calloc(1, sizeof *req);
	if (!req) {
		acm_log(0, "ERROR - unable to alloc client request\n");
		return NULL;
	}

	req->client = client;
	memcpy(&req->msg, msg, sizeof(req->msg));
	acm_log(2, "client %d, req %p\n", client->index, req);
	return req;
}

static void
acm_free_req(struct acm_request *req)
{
	acm_log(2, "%p\n", req);
	free(req);
}

static struct acm_send_msg *
acm_alloc_send(struct acm_ep *ep, struct acm_dest *dest, size_t size)
{
	struct acm_send_msg *msg;

	msg = (struct acm_send_msg *) calloc(1, sizeof *msg);
	if (!msg) {
		acm_log(0, "ERROR - unable to allocate send buffer\n");
		return NULL;
	}

	msg->ep = ep;
	msg->mr = ibv_reg_mr(ep->port->dev->pd, msg->data, size, 0);
	if (!msg->mr) {
		acm_log(0, "ERROR - failed to register send buffer\n");
		goto err1;
	}

	if (!dest->ah) {
		msg->ah = ibv_create_ah(ep->port->dev->pd, &dest->av);
		if (!msg->ah) {
			acm_log(0, "ERROR - unable to create ah\n");
			goto err2;
		}
		msg->wr.wr.ud.ah = msg->ah;
	} else {
		msg->wr.wr.ud.ah = dest->ah;
	}

	acm_log(2, "get dest %s\n", dest->name);
	(void) atomic_inc(&dest->refcnt);
	msg->dest = dest;

	msg->wr.next = NULL;
	msg->wr.sg_list = &msg->sge;
	msg->wr.num_sge = 1;
	msg->wr.opcode = IBV_WR_SEND;
	msg->wr.send_flags = IBV_SEND_SIGNALED;
	msg->wr.wr_id = (uintptr_t) msg;
	msg->wr.wr.ud.remote_qpn = dest->remote_qpn;
	msg->wr.wr.ud.remote_qkey = ACM_QKEY;

	msg->sge.length = size;
	msg->sge.lkey = msg->mr->lkey;
	msg->sge.addr = (uintptr_t) msg->data;
	acm_log(2, "%p\n", msg);
	return msg;

err2:
	ibv_dereg_mr(msg->mr);
err1:
	free(msg);
	return NULL;
}

static void
acm_init_send_req(struct acm_send_msg *msg, void *context, 
	void (*resp_handler)(struct acm_send_msg *req,
		struct ibv_wc *wc, struct acm_mad *resp))
{
	acm_log(2, "%p\n", msg);
	msg->tries = retries + 1;
	msg->context = context;
	msg->resp_handler = resp_handler;
}

static void acm_free_send(struct acm_send_msg *msg)
{
	acm_log(2, "%p\n", msg);
	if (msg->ah)
		ibv_destroy_ah(msg->ah);
	ibv_dereg_mr(msg->mr);
	acm_put_dest(msg->dest);
	free(msg);
}

static void acm_post_send(struct acm_send_queue *queue, struct acm_send_msg *msg)
{
	struct acm_ep *ep = msg->ep;
	struct ibv_send_wr *bad_wr;

	msg->req_queue = queue;
	lock_acquire(&ep->lock);
	if (queue->credits) {
		acm_log(2, "posting send to QP\n");
		queue->credits--;
		DListInsertTail(&msg->entry, &ep->active_queue);
		ibv_post_send(ep->qp, &msg->wr, &bad_wr);
	} else {
		acm_log(2, "no sends available, queuing message\n");
		DListInsertTail(&msg->entry, &queue->pending);
	}
	lock_release(&ep->lock);
}

static void acm_post_recv(struct acm_ep *ep, uint64_t address)
{
	struct ibv_recv_wr wr, *bad_wr;
	struct ibv_sge sge;

	wr.next = NULL;
	wr.sg_list = &sge;
	wr.num_sge = 1;
	wr.wr_id = address;

	sge.length = ACM_RECV_SIZE;
	sge.lkey = ep->mr->lkey;
	sge.addr = address;

	ibv_post_recv(ep->qp, &wr, &bad_wr);
}

/* Caller must hold ep lock */
static void acm_send_available(struct acm_ep *ep, struct acm_send_queue *queue)
{
	struct acm_send_msg *msg;
	struct ibv_send_wr *bad_wr;
	DLIST_ENTRY *entry;

	if (DListEmpty(&queue->pending)) {
		queue->credits++;
	} else {
		acm_log(2, "posting queued send message\n");
		entry = queue->pending.Next;
		DListRemove(entry);
		msg = container_of(entry, struct acm_send_msg, entry);
		DListInsertTail(&msg->entry, &ep->active_queue);
		ibv_post_send(ep->qp, &msg->wr, &bad_wr);
	}
}

static void acm_complete_send(struct acm_send_msg *msg)
{
	struct acm_ep *ep = msg->ep;

	lock_acquire(&ep->lock);
	DListRemove(&msg->entry);
	if (msg->tries) {
		acm_log(2, "waiting for response\n");
		msg->expires = time_stamp_ms() + ep->port->subnet_timeout + timeout;
		DListInsertTail(&msg->entry, &ep->wait_queue);
		if (atomic_inc(&wait_cnt) == 1)
			event_signal(&timeout_event);
	} else {
		acm_log(2, "freeing\n");
		acm_send_available(ep, msg->req_queue);
		acm_free_send(msg);
	}
	lock_release(&ep->lock);
}

static struct acm_send_msg *acm_get_request(struct acm_ep *ep, uint64_t tid, int *free)
{
	struct acm_send_msg *msg, *req = NULL;
	struct acm_mad *mad;
	DLIST_ENTRY *entry, *next;

	acm_log(2, "\n");
	lock_acquire(&ep->lock);
	for (entry = ep->wait_queue.Next; entry != &ep->wait_queue; entry = next) {
		next = entry->Next;
		msg = container_of(entry, struct acm_send_msg, entry);
		mad = (struct acm_mad *) msg->data;
		if (mad->tid == tid) {
			acm_log(2, "match found in wait queue\n");
			req = msg;
			DListRemove(entry);
			(void) atomic_dec(&wait_cnt);
			acm_send_available(ep, msg->req_queue);
			*free = 1;
			goto unlock;
		}
	}

	for (entry = ep->active_queue.Next; entry != &ep->active_queue; entry = entry->Next) {
		msg = container_of(entry, struct acm_send_msg, entry);
		mad = (struct acm_mad *) msg->data;
		if (mad->tid == tid && msg->tries) {
			acm_log(2, "match found in active queue\n");
			req = msg;
			req->tries = 0;
			*free = 0;
			break;
		}
	}
unlock:
	lock_release(&ep->lock);
	return req;
}

static uint8_t acm_gid_index(struct acm_port *port, union ibv_gid *gid)
{
	union ibv_gid cmp_gid;
	uint8_t i;

	for (i = 0; i < port->gid_cnt; i++) {
		ibv_query_gid(port->dev->verbs, port->port_num, i, &cmp_gid);
		if (!memcmp(&cmp_gid, gid, sizeof cmp_gid))
			break;
	}
	return i;
}

static int acm_mc_index(struct acm_ep *ep, union ibv_gid *gid)
{
	int i;

	for (i = 0; i < ep->mc_cnt; i++) {
		if (!memcmp(&ep->mc_dest[i].address, gid, sizeof(*gid)))
			return i;
	}
	return -1;
}

/* Multicast groups are ordered lowest to highest preference. */
static int acm_best_mc_index(struct acm_ep *ep, struct acm_resolve_rec *rec)
{
	int i, index;

	for (i = min(rec->gid_cnt, ACM_MAX_GID_COUNT) - 1; i >= 0; i--) {
		index = acm_mc_index(ep, &rec->gid[i]);
		if (index >= 0) {
			return index;
		}
	}
	return -1;
}

static void
acm_record_mc_av(struct acm_port *port, struct ib_mc_member_rec *mc_rec,
	struct acm_dest *dest)
{
	uint32_t sl_flow_hop;

	sl_flow_hop = ntohl(mc_rec->sl_flow_hop);

	dest->av.dlid = ntohs(mc_rec->mlid);
	dest->av.sl = (uint8_t) (sl_flow_hop >> 28);
	dest->av.src_path_bits = port->sa_dest.av.src_path_bits;
	dest->av.static_rate = mc_rec->rate & 0x3F;
	dest->av.port_num = port->port_num;

	dest->av.is_global = 1;
	dest->av.grh.dgid = mc_rec->mgid;
	dest->av.grh.flow_label = (sl_flow_hop >> 8) & 0xFFFFF;
	dest->av.grh.sgid_index = acm_gid_index(port, &mc_rec->port_gid);
	dest->av.grh.hop_limit = (uint8_t) sl_flow_hop;
	dest->av.grh.traffic_class = mc_rec->tclass;

	dest->path.dgid = mc_rec->mgid;
	dest->path.sgid = mc_rec->port_gid;
	dest->path.dlid = mc_rec->mlid;
	dest->path.slid = htons(port->lid) | port->sa_dest.av.src_path_bits;
	dest->path.flowlabel_hoplimit = htonl(sl_flow_hop & 0xFFFFFFF);
	dest->path.tclass = mc_rec->tclass;
	dest->path.reversible_numpath = IBV_PATH_RECORD_REVERSIBLE | 1;
	dest->path.pkey = mc_rec->pkey;
	dest->path.qosclass_sl = htons((uint16_t) (sl_flow_hop >> 28));
	dest->path.mtu = mc_rec->mtu;
	dest->path.rate = mc_rec->rate;
	dest->path.packetlifetime = mc_rec->packet_lifetime;
}

/* Always send the GRH to transfer GID data to remote side */
static void
acm_init_path_av(struct acm_port *port, struct acm_dest *dest)
{
	uint32_t flow_hop;

	dest->av.dlid = ntohs(dest->path.dlid);
	dest->av.sl = ntohs(dest->path.qosclass_sl) & 0xF;
	dest->av.src_path_bits = dest->path.slid & 0x7F;
	dest->av.static_rate = dest->path.rate & 0x3F;
	dest->av.port_num = port->port_num;

	flow_hop = ntohl(dest->path.flowlabel_hoplimit);
	dest->av.is_global = 1;
	dest->av.grh.flow_label = (flow_hop >> 8) & 0xFFFFF;
	dest->av.grh.sgid_index = acm_gid_index(port, &dest->path.sgid);
	dest->av.grh.hop_limit = (uint8_t) flow_hop;
	dest->av.grh.traffic_class = dest->path.tclass;
}

static void acm_process_join_resp(struct acm_ep *ep, struct ib_user_mad *umad)
{
	struct acm_dest *dest;
	struct ib_mc_member_rec *mc_rec;
	struct ib_sa_mad *mad;
	int index, ret;

	mad = (struct ib_sa_mad *) umad->data;
	acm_log(1, "response status: 0x%x, mad status: 0x%x\n",
		umad->status, mad->status);
	lock_acquire(&ep->lock);
	if (umad->status) {
		acm_log(0, "ERROR - send join failed 0x%x\n", umad->status);
		goto err1;
	}
	if (mad->status) {
		acm_log(0, "ERROR - join response status 0x%x\n", mad->status);
		goto err1;
	}

	mc_rec = (struct ib_mc_member_rec *) mad->data;
	index = acm_mc_index(ep, &mc_rec->mgid);
	if (index < 0) {
		acm_log(0, "ERROR - MGID in join response not found\n");
		goto err1;
	}

	dest = &ep->mc_dest[index];
	dest->remote_qpn = IB_MC_QPN;
	dest->mgid = mc_rec->mgid;
	acm_record_mc_av(ep->port, mc_rec, dest);

	if (index == 0) {
		dest->ah = ibv_create_ah(ep->port->dev->pd, &dest->av);
		if (!dest->ah) {
			acm_log(0, "ERROR - unable to create ah\n");
			goto err1;
		}
		ret = ibv_attach_mcast(ep->qp, &mc_rec->mgid, mc_rec->mlid);
		if (ret) {
			acm_log(0, "ERROR - unable to attach QP to multicast group\n");
			goto err2;
		}
	}

	atomic_set(&dest->refcnt, 1);
	dest->state = ACM_READY;
	acm_log(1, "join successful\n");
	lock_release(&ep->lock);
	return;
err2:
	ibv_destroy_ah(dest->ah);
	dest->ah = NULL;
err1:
	lock_release(&ep->lock);
}

static void acm_mark_addr_invalid(struct acm_ep *ep,
				  struct acm_ep_addr_data *data)
{
	int i;

	lock_acquire(&ep->lock);
	for (i = 0; i < MAX_EP_ADDR; i++) {
		if (ep->addr_type[i] != data->type)
			continue;

		if ((data->type == ACM_ADDRESS_NAME &&
		    !strnicmp((char *) ep->addr[i].name,
			      (char *) data->info.addr, ACM_MAX_ADDRESS)) ||
		     !memcmp(ep->addr[i].addr, data->info.addr, ACM_MAX_ADDRESS)) {
			ep->addr_type[i] = ACM_ADDRESS_INVALID;
			break;
		}
	}
	lock_release(&ep->lock);
}

static int acm_addr_index(struct acm_ep *ep, uint8_t *addr, uint8_t addr_type)
{
	int i;

	for (i = 0; i < MAX_EP_ADDR; i++) {
		if (ep->addr_type[i] != addr_type)
			continue;

		if ((addr_type == ACM_ADDRESS_NAME &&
			!strnicmp((char *) ep->addr[i].name,
				(char *) addr, ACM_MAX_ADDRESS)) ||
			!memcmp(ep->addr[i].addr, addr, ACM_MAX_ADDRESS))
			return i;
	}
	return -1;
}

static uint8_t
acm_record_acm_route(struct acm_ep *ep, struct acm_dest *dest)
{
	int i;

	acm_log(2, "\n");
	for (i = 0; i < MAX_EP_MC; i++) {
		if (!memcmp(&dest->mgid, &ep->mc_dest[i].mgid, sizeof dest->mgid))
			break;
	}
	if (i == MAX_EP_MC) {
		acm_log(0, "ERROR - cannot match mgid\n");
		return ACM_STATUS_EINVAL;
	}

	dest->path = ep->mc_dest[i].path;
	dest->path.dgid = dest->av.grh.dgid;
	dest->path.dlid = htons(dest->av.dlid);
	dest->addr_timeout = time_stamp_min() + (unsigned) addr_timeout;
	dest->route_timeout = time_stamp_min() + (unsigned) route_timeout;
	dest->state = ACM_READY;
	return ACM_STATUS_SUCCESS;
}

static void acm_init_path_query(struct ib_sa_mad *mad)
{
	acm_log(2, "\n");
	mad->base_version = 1;
	mad->mgmt_class = IB_MGMT_CLASS_SA;
	mad->class_version = 2;
	mad->method = IB_METHOD_GET;
	mad->tid = htonll((uint64_t) atomic_inc(&tid));
	mad->attr_id = IB_SA_ATTR_PATH_REC;
}

static uint64_t acm_path_comp_mask(struct ibv_path_record *path)
{
	uint32_t fl_hop;
	uint16_t qos_sl;
	uint64_t comp_mask = 0;

	acm_log(2, "\n");
	if (path->service_id)
		comp_mask |= IB_COMP_MASK_PR_SERVICE_ID;
	if (!ib_any_gid(&path->dgid))
		comp_mask |= IB_COMP_MASK_PR_DGID;
	if (!ib_any_gid(&path->sgid))
		comp_mask |= IB_COMP_MASK_PR_SGID;
	if (path->dlid)
		comp_mask |= IB_COMP_MASK_PR_DLID;
	if (path->slid)
		comp_mask |= IB_COMP_MASK_PR_SLID;

	fl_hop = ntohl(path->flowlabel_hoplimit);
	if (fl_hop >> 8)
		comp_mask |= IB_COMP_MASK_PR_FLOW_LABEL;
	if (fl_hop & 0xFF)
		comp_mask |= IB_COMP_MASK_PR_HOP_LIMIT;

	if (path->tclass)
		comp_mask |= IB_COMP_MASK_PR_TCLASS;
	if (path->reversible_numpath & 0x80)
		comp_mask |= IB_COMP_MASK_PR_REVERSIBLE;
	if (path->pkey)
		comp_mask |= IB_COMP_MASK_PR_PKEY;

	qos_sl = ntohs(path->qosclass_sl);
	if (qos_sl >> 4)
		comp_mask |= IB_COMP_MASK_PR_QOS_CLASS;
	if (qos_sl & 0xF)
		comp_mask |= IB_COMP_MASK_PR_SL;

	if (path->mtu & 0xC0)
		comp_mask |= IB_COMP_MASK_PR_MTU_SELECTOR;
	if (path->mtu & 0x3F)
		comp_mask |= IB_COMP_MASK_PR_MTU;
	if (path->rate & 0xC0)
		comp_mask |= IB_COMP_MASK_PR_RATE_SELECTOR;
	if (path->rate & 0x3F)
		comp_mask |= IB_COMP_MASK_PR_RATE;
	if (path->packetlifetime & 0xC0)
		comp_mask |= IB_COMP_MASK_PR_PACKET_LIFETIME_SELECTOR;
	if (path->packetlifetime & 0x3F)
		comp_mask |= IB_COMP_MASK_PR_PACKET_LIFETIME;

	return comp_mask;
}

/* Caller must hold dest lock */
static uint8_t acm_resolve_path(struct acm_ep *ep, struct acm_dest *dest,
	void (*resp_handler)(struct acm_send_msg *req,
		struct ibv_wc *wc, struct acm_mad *resp))
{
	struct acm_send_msg *msg;
	struct ib_sa_mad *mad;
	uint8_t ret;

	acm_log(2, "%s\n", dest->name);
	if (!acm_acquire_sa_dest(ep->port)) {
		acm_log(1, "cannot acquire SA destination\n");
		ret = ACM_STATUS_EINVAL;
		goto err;
	}

	msg = acm_alloc_send(ep, &ep->port->sa_dest, sizeof(*mad));
	acm_release_sa_dest(&ep->port->sa_dest);
	if (!msg) {
		acm_log(0, "ERROR - cannot allocate send msg\n");
		ret = ACM_STATUS_ENOMEM;
		goto err;
	}

	(void) atomic_inc(&dest->refcnt);
	acm_init_send_req(msg, (void *) dest, resp_handler);
	mad = (struct ib_sa_mad *) msg->data;
	acm_init_path_query(mad);

	memcpy(mad->data, &dest->path, sizeof(dest->path));
	mad->comp_mask = acm_path_comp_mask(&dest->path);

	atomic_inc(&counter[ACM_CNTR_ROUTE_QUERY]);
	dest->state = ACM_QUERY_ROUTE;
	acm_post_send(&ep->sa_queue, msg);
	return ACM_STATUS_SUCCESS;
err:
	dest->state = ACM_INIT;
	return ret;
}

static uint8_t
acm_record_acm_addr(struct acm_ep *ep, struct acm_dest *dest, struct ibv_wc *wc,
	struct acm_resolve_rec *rec)
{
	int index;

	acm_log(2, "%s\n", dest->name);
	index = acm_best_mc_index(ep, rec);
	if (index < 0) {
		acm_log(0, "ERROR - no shared multicast groups\n");
		dest->state = ACM_INIT;
		return ACM_STATUS_ENODATA;
	}

	acm_log(2, "selecting MC group at index %d\n", index);
	dest->av = ep->mc_dest[index].av;
	dest->av.dlid = wc->slid;
	dest->av.src_path_bits = wc->dlid_path_bits;
	dest->av.grh.dgid = ((struct ibv_grh *) (uintptr_t) wc->wr_id)->sgid;
	
	dest->mgid = ep->mc_dest[index].mgid;
	dest->path.sgid = ep->mc_dest[index].path.sgid;
	dest->path.dgid = dest->av.grh.dgid;
	dest->path.tclass = ep->mc_dest[index].path.tclass;
	dest->path.pkey = ep->mc_dest[index].path.pkey;
	dest->remote_qpn = wc->src_qp;

	dest->state = ACM_ADDR_RESOLVED;
	return ACM_STATUS_SUCCESS;
}

static void
acm_record_path_addr(struct acm_ep *ep, struct acm_dest *dest,
	struct ibv_path_record *path)
{
	acm_log(2, "%s\n", dest->name);
	dest->path.pkey = htons(ep->pkey);
	dest->path.dgid = path->dgid;
	if (path->slid || !ib_any_gid(&path->sgid)) {
		dest->path.sgid = path->sgid;
		dest->path.slid = path->slid;
	} else {
		dest->path.slid = htons(ep->port->lid);
	}
	dest->path.dlid = path->dlid;
	dest->state = ACM_ADDR_RESOLVED;
}

static uint8_t acm_validate_addr_req(struct acm_mad *mad)
{
	struct acm_resolve_rec *rec;

	if (mad->method != IB_METHOD_GET) {
		acm_log(0, "ERROR - invalid method 0x%x\n", mad->method);
		return ACM_STATUS_EINVAL;
	}

	rec = (struct acm_resolve_rec *) mad->data;
	if (!rec->src_type || rec->src_type >= ACM_ADDRESS_RESERVED) {
		acm_log(0, "ERROR - unknown src type 0x%x\n", rec->src_type);
		return ACM_STATUS_EINVAL;
	}

	return ACM_STATUS_SUCCESS;
}

static void
acm_send_addr_resp(struct acm_ep *ep, struct acm_dest *dest)
{
	struct acm_resolve_rec *rec;
	struct acm_send_msg *msg;
	struct acm_mad *mad;

	acm_log(2, "%s\n", dest->name);
	msg = acm_alloc_send(ep, dest, sizeof (*mad));
	if (!msg) {
		acm_log(0, "ERROR - failed to allocate message\n");
		return;
	}

	mad = (struct acm_mad *) msg->data;
	rec = (struct acm_resolve_rec *) mad->data;

	mad->base_version = 1;
	mad->mgmt_class = ACM_MGMT_CLASS;
	mad->class_version = 1;
	mad->method = IB_METHOD_GET | IB_METHOD_RESP;
	mad->status = ACM_STATUS_SUCCESS;
	mad->control = ACM_CTRL_RESOLVE;
	mad->tid = dest->req_id;
	rec->gid_cnt = 1;
	memcpy(rec->gid, dest->mgid.raw, sizeof(union ibv_gid));

	acm_post_send(&ep->resp_queue, msg);
}

static int
acm_client_resolve_resp(struct acm_client *client, struct acm_msg *req_msg,
	struct acm_dest *dest, uint8_t status)
{
	struct acm_msg msg;
	int ret;

	acm_log(2, "client %d, status 0x%x\n", client->index, status);
	memset(&msg, 0, sizeof msg);

	if (status == ACM_STATUS_ENODATA)
		atomic_inc(&counter[ACM_CNTR_NODATA]);
	else if (status)
		atomic_inc(&counter[ACM_CNTR_ERROR]);

	lock_acquire(&client->lock);
	if (client->sock == INVALID_SOCKET) {
		acm_log(0, "ERROR - connection lost\n");
		ret = ACM_STATUS_ENOTCONN;
		goto release;
	}

	msg.hdr = req_msg->hdr;
	msg.hdr.opcode |= ACM_OP_ACK;
	msg.hdr.status = status;
	msg.hdr.length = ACM_MSG_HDR_LENGTH;
	memset(msg.hdr.data, 0, sizeof(msg.hdr.data));

	if (status == ACM_STATUS_SUCCESS) {
		msg.hdr.length += ACM_MSG_EP_LENGTH;
		msg.resolve_data[0].flags = IBV_PATH_FLAG_GMP |
			IBV_PATH_FLAG_PRIMARY | IBV_PATH_FLAG_BIDIRECTIONAL;
		msg.resolve_data[0].type = ACM_EP_INFO_PATH;
		msg.resolve_data[0].info.path = dest->path;

		if (req_msg->hdr.src_out) {
			msg.hdr.length += ACM_MSG_EP_LENGTH;
			memcpy(&msg.resolve_data[1],
				&req_msg->resolve_data[req_msg->hdr.src_index],
				ACM_MSG_EP_LENGTH);
		}
	}

	ret = send(client->sock, (char *) &msg, msg.hdr.length, 0);
	if (ret != msg.hdr.length)
		acm_log(0, "ERROR - failed to send response\n");
	else
		ret = 0;

release:
	lock_release(&client->lock);
	(void) atomic_dec(&client->refcnt);
	return ret;
}

static void
acm_complete_queued_req(struct acm_dest *dest, uint8_t status)
{
	struct acm_request *req;
	DLIST_ENTRY *entry;

	acm_log(2, "status %d\n", status);
	lock_acquire(&dest->lock);
	while (!DListEmpty(&dest->req_queue)) {
		entry = dest->req_queue.Next;
		DListRemove(entry);
		req = container_of(entry, struct acm_request, entry);
		lock_release(&dest->lock);

		acm_log(2, "completing request, client %d\n", req->client->index);
		acm_client_resolve_resp(req->client, &req->msg, dest, status);
		acm_free_req(req);

		lock_acquire(&dest->lock);
	}
	lock_release(&dest->lock);
}

static void
acm_dest_sa_resp(struct acm_send_msg *msg, struct ibv_wc *wc, struct acm_mad *mad)
{
	struct acm_dest *dest = (struct acm_dest *) msg->context;
	struct ib_sa_mad *sa_mad = (struct ib_sa_mad *) mad;
	uint8_t status;

	if (mad) {
		status = (uint8_t) (ntohs(mad->status) >> 8);
	} else {
		status = ACM_STATUS_ETIMEDOUT;
	}
	acm_log(2, "%s status=0x%x\n", dest->name, status);

	lock_acquire(&dest->lock);
	if (dest->state != ACM_QUERY_ROUTE) {
		acm_log(1, "notice - discarding SA response\n");
		lock_release(&dest->lock);
		return;
	}

	if (!status) {
		memcpy(&dest->path, sa_mad->data, sizeof(dest->path));
		acm_init_path_av(msg->ep->port, dest);
		dest->addr_timeout = time_stamp_min() + (unsigned) addr_timeout;
		dest->route_timeout = time_stamp_min() + (unsigned) route_timeout;
		acm_log(2, "timeout addr %llu route %llu\n", dest->addr_timeout, dest->route_timeout);
		dest->state = ACM_READY;
	} else {
		dest->state = ACM_INIT;
	}
	lock_release(&dest->lock);

	acm_complete_queued_req(dest, status);
}

static void
acm_resolve_sa_resp(struct acm_send_msg *msg, struct ibv_wc *wc, struct acm_mad *mad)
{
	struct acm_dest *dest = (struct acm_dest *) msg->context;
	int send_resp;

	acm_log(2, "\n");
	acm_dest_sa_resp(msg, wc, mad);

	lock_acquire(&dest->lock);
	send_resp = (dest->state == ACM_READY);
	lock_release(&dest->lock);

	if (send_resp)
		acm_send_addr_resp(msg->ep, dest);
}

static void
acm_process_addr_req(struct acm_ep *ep, struct ibv_wc *wc, struct acm_mad *mad)
{
	struct acm_resolve_rec *rec;
	struct acm_dest *dest;
	uint8_t status;
	int addr_index;

	acm_log(2, "\n");
	if ((status = acm_validate_addr_req(mad))) {
		acm_log(0, "ERROR - invalid request\n");
		return;
	}

	rec = (struct acm_resolve_rec *) mad->data;
	dest = acm_acquire_dest(ep, rec->src_type, rec->src);
	if (!dest) {
		acm_log(0, "ERROR - unable to add source\n");
		return;
	}
	
	addr_index = acm_addr_index(ep, rec->dest, rec->dest_type);
	if (addr_index >= 0)
		dest->req_id = mad->tid;

	lock_acquire(&dest->lock);
	acm_log(2, "dest state %d\n", dest->state);
	switch (dest->state) {
	case ACM_READY:
		if (dest->remote_qpn == wc->src_qp)
			break;

		acm_log(2, "src service has new qp, resetting\n");
		/* fall through */
	case ACM_INIT:
	case ACM_QUERY_ADDR:
		status = acm_record_acm_addr(ep, dest, wc, rec);
		if (status)
			break;
		/* fall through */
	case ACM_ADDR_RESOLVED:
		if (route_prot == ACM_ROUTE_PROT_ACM) {
			status = acm_record_acm_route(ep, dest);
			break;
		}
		if (addr_index >= 0 || !DListEmpty(&dest->req_queue)) {
			status = acm_resolve_path(ep, dest, acm_resolve_sa_resp);
			if (status)
				break;
		}
		/* fall through */
	default:
		lock_release(&dest->lock);
		acm_put_dest(dest);
		return;
	}
	lock_release(&dest->lock);
	acm_complete_queued_req(dest, status);

	if (addr_index >= 0 && !status) {
		acm_send_addr_resp(ep, dest);
	}
	acm_put_dest(dest);
}

static void
acm_process_addr_resp(struct acm_send_msg *msg, struct ibv_wc *wc, struct acm_mad *mad)
{
	struct acm_resolve_rec *resp_rec;
	struct acm_dest *dest = (struct acm_dest *) msg->context;
	uint8_t status;

	if (mad) {
		status = acm_class_status(mad->status);
		resp_rec = (struct acm_resolve_rec *) mad->data;
	} else {
		status = ACM_STATUS_ETIMEDOUT;
		resp_rec = NULL;
	}
	acm_log(2, "resp status 0x%x\n", status);

	lock_acquire(&dest->lock);
	if (dest->state != ACM_QUERY_ADDR) {
		lock_release(&dest->lock);
		goto put;
	}

	if (!status) {
		status = acm_record_acm_addr(msg->ep, dest, wc, resp_rec);
		if (!status) {
			if (route_prot == ACM_ROUTE_PROT_ACM) {
				status = acm_record_acm_route(msg->ep, dest);
			} else {
				status = acm_resolve_path(msg->ep, dest, acm_dest_sa_resp);
				if (!status) {
					lock_release(&dest->lock);
					goto put;
				}
			}
		}
	} else {
		dest->state = ACM_INIT;
	}
	lock_release(&dest->lock);

	acm_complete_queued_req(dest, status);
put:
	acm_put_dest(dest);
}

static void acm_process_acm_recv(struct acm_ep *ep, struct ibv_wc *wc, struct acm_mad *mad)
{
	struct acm_send_msg *req;
	struct acm_resolve_rec *rec;
	int free;

	acm_log(2, "\n");
	if (mad->base_version != 1 || mad->class_version != 1) {
		acm_log(0, "ERROR - invalid version %d %d\n",
			mad->base_version, mad->class_version);
		return;
	}
	
	if (mad->control != ACM_CTRL_RESOLVE) {
		acm_log(0, "ERROR - invalid control 0x%x\n", mad->control);
		return;
	}

	rec = (struct acm_resolve_rec *) mad->data;
	acm_format_name(2, log_data, sizeof log_data,
			rec->src_type, rec->src, sizeof rec->src);
	acm_log(2, "src  %s\n", log_data);
	acm_format_name(2, log_data, sizeof log_data,
			rec->dest_type, rec->dest, sizeof rec->dest);
	acm_log(2, "dest %s\n", log_data);
	if (mad->method & IB_METHOD_RESP) {
		acm_log(2, "received response\n");
		req = acm_get_request(ep, mad->tid, &free);
		if (!req) {
			acm_log(1, "notice - response did not match active request\n");
			return;
		}
		acm_log(2, "found matching request\n");
		req->resp_handler(req, wc, mad);
		if (free)
			acm_free_send(req);
	} else {
		acm_log(2, "unsolicited request\n");
		acm_process_addr_req(ep, wc, mad);
	}
}

static int
acm_client_query_resp(struct acm_client *client,
	struct acm_msg *msg, uint8_t status)
{
	int ret;

	acm_log(2, "status 0x%x\n", status);
	lock_acquire(&client->lock);
	if (client->sock == INVALID_SOCKET) {
		acm_log(0, "ERROR - connection lost\n");
		ret = ACM_STATUS_ENOTCONN;
		goto release;
	}

	msg->hdr.opcode |= ACM_OP_ACK;
	msg->hdr.status = status;

	ret = send(client->sock, (char *) msg, msg->hdr.length, 0);
	if (ret != msg->hdr.length)
		acm_log(0, "ERROR - failed to send response\n");
	else
		ret = 0;

release:
	lock_release(&client->lock);
	(void) atomic_dec(&client->refcnt);
	return ret;
}

static void
acm_client_sa_resp(struct acm_send_msg *msg, struct ibv_wc *wc, struct acm_mad *mad)
{
	struct acm_request *req = (struct acm_request *) msg->context;
	struct ib_sa_mad *sa_mad = (struct ib_sa_mad *) mad;
	uint8_t status;

	if (mad) {
		status = (uint8_t) (ntohs(sa_mad->status) >> 8);
		memcpy(&req->msg.resolve_data[0].info.path, sa_mad->data,
			sizeof(struct ibv_path_record));
	} else {
		status = ACM_STATUS_ETIMEDOUT;
	}
	acm_log(2, "status 0x%x\n", status);

	acm_client_query_resp(req->client, &req->msg, status);
	acm_free_req(req);
}

static void acm_process_sa_recv(struct acm_ep *ep, struct ibv_wc *wc, struct acm_mad *mad)
{
	struct ib_sa_mad *sa_mad = (struct ib_sa_mad *) mad;
	struct acm_send_msg *req;
	int free;

	acm_log(2, "\n");
	if (mad->base_version != 1 || mad->class_version != 2 ||
	    !(mad->method & IB_METHOD_RESP) || sa_mad->attr_id != IB_SA_ATTR_PATH_REC) {
		acm_log(0, "ERROR - unexpected SA MAD %d %d\n",
			mad->base_version, mad->class_version);
		return;
	}
	
	req = acm_get_request(ep, mad->tid, &free);
	if (!req) {
		acm_log(1, "notice - response did not match active request\n");
		return;
	}
	acm_log(2, "found matching request\n");
	req->resp_handler(req, wc, mad);
	if (free)
		acm_free_send(req);
}

static void acm_process_recv(struct acm_ep *ep, struct ibv_wc *wc)
{
	struct acm_mad *mad;

	acm_log(2, "base endpoint name %s\n", ep->id_string);
	mad = (struct acm_mad *) (uintptr_t) (wc->wr_id + sizeof(struct ibv_grh));
	switch (mad->mgmt_class) {
	case IB_MGMT_CLASS_SA:
		acm_process_sa_recv(ep, wc, mad);
		break;
	case ACM_MGMT_CLASS:
		acm_process_acm_recv(ep, wc, mad);
		break;
	default:
		acm_log(0, "ERROR - invalid mgmt class 0x%x\n", mad->mgmt_class);
		break;
	}

	acm_post_recv(ep, wc->wr_id);
}

static void acm_process_comp(struct acm_ep *ep, struct ibv_wc *wc)
{
	if (wc->status) {
		acm_log(0, "ERROR - work completion error\n"
			"\topcode %d, completion status %d\n",
			wc->opcode, wc->status);
		return;
	}

	if (wc->opcode & IBV_WC_RECV)
		acm_process_recv(ep, wc);
	else
		acm_complete_send((struct acm_send_msg *) (uintptr_t) wc->wr_id);
}

static void CDECL_FUNC acm_comp_handler(void *context)
{
	struct acm_device *dev = (struct acm_device *) context;
	struct acm_ep *ep;
	struct ibv_cq *cq;
	struct ibv_wc wc;
	int cnt;

	acm_log(1, "started\n");
	while (1) {
		ibv_get_cq_event(dev->channel, &cq, (void *) &ep);

		cnt = 0;
		while (ibv_poll_cq(cq, 1, &wc) > 0) {
			cnt++;
			acm_process_comp(ep, &wc);
		}

		ibv_req_notify_cq(cq, 0);
		while (ibv_poll_cq(cq, 1, &wc) > 0) {
			cnt++;
			acm_process_comp(ep, &wc);
		}

		ibv_ack_cq_events(cq, cnt);
	}
}

static void acm_format_mgid(union ibv_gid *mgid, uint16_t pkey, uint8_t tos,
	uint8_t rate, uint8_t mtu)
{
	mgid->raw[0] = 0xFF;
	mgid->raw[1] = 0x10 | 0x05;
	mgid->raw[2] = 0x40;
	mgid->raw[3] = 0x01;
	mgid->raw[4] = (uint8_t) (pkey >> 8);
	mgid->raw[5] = (uint8_t) pkey;
	mgid->raw[6] = tos;
	mgid->raw[7] = rate;
	mgid->raw[8] = mtu;
	mgid->raw[9] = 0;
	mgid->raw[10] = 0;
	mgid->raw[11] = 0;
	mgid->raw[12] = 0;
	mgid->raw[13] = 0;
	mgid->raw[14] = 0;
	mgid->raw[15] = 0;
}

static void acm_init_join(struct ib_sa_mad *mad, union ibv_gid *port_gid,
	uint16_t pkey, uint8_t tos, uint8_t tclass, uint8_t sl, uint8_t rate, uint8_t mtu)
{
	struct ib_mc_member_rec *mc_rec;

	acm_log(2, "\n");
	mad->base_version = 1;
	mad->mgmt_class = IB_MGMT_CLASS_SA;
	mad->class_version = 2;
	mad->method = IB_METHOD_SET;
	mad->tid = htonll((uint64_t) atomic_inc(&tid));
	mad->attr_id = IB_SA_ATTR_MC_MEMBER_REC;
	mad->comp_mask =
		IB_COMP_MASK_MC_MGID | IB_COMP_MASK_MC_PORT_GID |
		IB_COMP_MASK_MC_QKEY | IB_COMP_MASK_MC_MTU_SEL| IB_COMP_MASK_MC_MTU |
		IB_COMP_MASK_MC_TCLASS | IB_COMP_MASK_MC_PKEY | IB_COMP_MASK_MC_RATE_SEL |
		IB_COMP_MASK_MC_RATE | IB_COMP_MASK_MC_SL | IB_COMP_MASK_MC_FLOW |
		IB_COMP_MASK_MC_SCOPE | IB_COMP_MASK_MC_JOIN_STATE;

	mc_rec = (struct ib_mc_member_rec *) mad->data;
	acm_format_mgid(&mc_rec->mgid, pkey | 0x8000, tos, rate, mtu);
	mc_rec->port_gid = *port_gid;
	mc_rec->qkey = htonl(ACM_QKEY);
	mc_rec->mtu = 0x80 | mtu;
	mc_rec->tclass = tclass;
	mc_rec->pkey = htons(pkey);
	mc_rec->rate = 0x80 | rate;
	mc_rec->sl_flow_hop = htonl(((uint32_t) sl) << 28);
	mc_rec->scope_state = 0x51;
}

static void acm_join_group(struct acm_ep *ep, union ibv_gid *port_gid,
	uint8_t tos, uint8_t tclass, uint8_t sl, uint8_t rate, uint8_t mtu)
{
	struct acm_port *port;
	struct ib_sa_mad *mad;
	struct ib_user_mad *umad;
	struct ib_mc_member_rec *mc_rec;
	int ret, len;

	acm_log(2, "\n");
	len = sizeof(*umad) + sizeof(*mad);
	umad = (struct ib_user_mad *) calloc(1, len);
	if (!umad) {
		acm_log(0, "ERROR - unable to allocate MAD for join\n");
		return;
	}

	port = ep->port;
	umad->addr.qpn = htonl(port->sa_dest.remote_qpn);
	umad->addr.pkey_index = port->default_pkey_ix;
	umad->addr.lid = htons(port->sa_dest.av.dlid);
	umad->addr.sl = port->sa_dest.av.sl;
	umad->addr.path_bits = port->sa_dest.av.src_path_bits;

	acm_log(0, "%s %d pkey 0x%x, sl 0x%x, rate 0x%x, mtu 0x%x\n",
		ep->port->dev->verbs->device->name, ep->port->port_num,
		ep->pkey, sl, rate, mtu);
	ep->mc_dest[ep->mc_cnt].state = ACM_INIT;
	mad = (struct ib_sa_mad *) umad->data;
	acm_init_join(mad, port_gid, ep->pkey, tos, tclass, sl, rate, mtu);
	mc_rec = (struct ib_mc_member_rec *) mad->data;
	acm_set_dest_addr(&ep->mc_dest[ep->mc_cnt++], ACM_ADDRESS_GID,
		mc_rec->mgid.raw, sizeof(mc_rec->mgid));

	ret = umad_send(port->mad_portid, port->mad_agentid, (void *) umad,
		sizeof(*mad), timeout, retries);
	if (ret) {
		acm_log(0, "ERROR - failed to send multicast join request %d\n", ret);
		goto out;
	}

	acm_log(1, "waiting for response from SA to join request\n");
	ret = umad_recv(port->mad_portid, (void *) umad, &len, -1);
	if (ret < 0) {
		acm_log(0, "ERROR - recv error for multicast join response %d\n", ret);
		goto out;
	}

	acm_process_join_resp(ep, umad);
out:
	free(umad);
}

static void acm_ep_join(struct acm_ep *ep)
{
	struct acm_port *port;

	port = ep->port;
	acm_log(1, "%s\n", ep->id_string);

	ep->mc_cnt = 0;
	acm_join_group(ep, &port->base_gid, 0, 0, 0, min_rate, min_mtu);

	if ((ep->state = ep->mc_dest[0].state) != ACM_READY)
		return;

	if ((route_prot == ACM_ROUTE_PROT_ACM) &&
	    (port->rate != min_rate || port->mtu != min_mtu))
		acm_join_group(ep, &port->base_gid, 0, 0, 0, port->rate, port->mtu);

	acm_log(1, "join for %s complete\n", ep->id_string);
}

static void acm_port_join(struct acm_port *port)
{
	struct acm_ep *ep;
	DLIST_ENTRY *ep_entry;

	acm_log(1, "device %s port %d\n", port->dev->verbs->device->name,
		port->port_num);

	for (ep_entry = port->ep_list.Next; ep_entry != &port->ep_list;
		 ep_entry = ep_entry->Next) {

		ep = container_of(ep_entry, struct acm_ep, entry);
		acm_ep_join(ep);
	}
	acm_log(1, "joins for device %s port %d complete\n",
		port->dev->verbs->device->name, port->port_num);
}

static void acm_process_timeouts(void)
{
	DLIST_ENTRY *entry;
	struct acm_send_msg *msg;
	struct acm_resolve_rec *rec;
	struct acm_mad *mad;
	
	while (!DListEmpty(&timeout_list)) {
		entry = timeout_list.Next;
		DListRemove(entry);

		msg = container_of(entry, struct acm_send_msg, entry);
		mad = (struct acm_mad *) &msg->data[0];
		rec = (struct acm_resolve_rec *) mad->data;

		acm_format_name(0, log_data, sizeof log_data,
				rec->dest_type, rec->dest, sizeof rec->dest);
		acm_log(0, "notice - dest %s\n", log_data);
		msg->resp_handler(msg, NULL, NULL);
	}
}

static void acm_process_wait_queue(struct acm_ep *ep, uint64_t *next_expire)
{
	struct acm_send_msg *msg;
	DLIST_ENTRY *entry, *next;
	struct ibv_send_wr *bad_wr;

	for (entry = ep->wait_queue.Next; entry != &ep->wait_queue; entry = next) {
		next = entry->Next;
		msg = container_of(entry, struct acm_send_msg, entry);
		if (msg->expires < time_stamp_ms()) {
			DListRemove(entry);
			(void) atomic_dec(&wait_cnt);
			if (--msg->tries) {
				acm_log(1, "notice - retrying request\n");
				DListInsertTail(&msg->entry, &ep->active_queue);
				ibv_post_send(ep->qp, &msg->wr, &bad_wr);
			} else {
				acm_log(0, "notice - failing request\n");
				acm_send_available(ep, msg->req_queue);
				DListInsertTail(&msg->entry, &timeout_list);
			}
		} else {
			*next_expire = min(*next_expire, msg->expires);
			break;
		}
	}
}

static void CDECL_FUNC acm_retry_handler(void *context)
{
	struct acm_device *dev;
	struct acm_port *port;
	struct acm_ep *ep;
	DLIST_ENTRY *dev_entry, *ep_entry;
	uint64_t next_expire;
	int i, wait;

	acm_log(0, "started\n");
	while (1) {
		while (!atomic_get(&wait_cnt))
			event_wait(&timeout_event, -1);

		next_expire = -1;
		for (dev_entry = dev_list.Next; dev_entry != &dev_list;
			 dev_entry = dev_entry->Next) {

			dev = container_of(dev_entry, struct acm_device, entry);

			for (i = 0; i < dev->port_cnt; i++) {
				port = &dev->port[i];

				for (ep_entry = port->ep_list.Next;
					 ep_entry != &port->ep_list;
					 ep_entry = ep_entry->Next) {

					ep = container_of(ep_entry, struct acm_ep, entry);
					lock_acquire(&ep->lock);
					if (!DListEmpty(&ep->wait_queue))
						acm_process_wait_queue(ep, &next_expire);
					lock_release(&ep->lock);
				}
			}
		}

		acm_process_timeouts();
		wait = (int) (next_expire - time_stamp_ms());
		if (wait > 0 && atomic_get(&wait_cnt))
			event_wait(&timeout_event, wait);
	}
}

static void acm_init_server(void)
{
	FILE *f;
	int i;

	for (i = 0; i < FD_SETSIZE - 1; i++) {
		lock_init(&client_array[i].lock);
		client_array[i].index = i;
		client_array[i].sock = INVALID_SOCKET;
		atomic_init(&client_array[i].refcnt);
	}

	if (!(f = fopen("/var/run/ibacm.port", "w"))) {
		acm_log(0, "notice - cannot publish ibacm port number\n");
		return;
	}
	fprintf(f, "%hu\n", server_port);
	fclose(f);
}

static int acm_listen(void)
{
	struct sockaddr_in addr;
	int ret;

	acm_log(2, "\n");
	listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listen_socket == INVALID_SOCKET) {
		acm_log(0, "ERROR - unable to allocate listen socket\n");
		return socket_errno();
	}

	memset(&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(server_port);
	ret = bind(listen_socket, (struct sockaddr *) &addr, sizeof addr);
	if (ret == SOCKET_ERROR) {
		acm_log(0, "ERROR - unable to bind listen socket\n");
		return socket_errno();
	}
	
	ret = listen(listen_socket, 0);
	if (ret == SOCKET_ERROR) {
		acm_log(0, "ERROR - unable to start listen\n");
		return socket_errno();
	}

	acm_log(2, "listen active\n");
	return 0;
}

static void acm_disconnect_client(struct acm_client *client)
{
	lock_acquire(&client->lock);
	shutdown(client->sock, SHUT_RDWR);
	closesocket(client->sock);
	client->sock = INVALID_SOCKET;
	lock_release(&client->lock);
	(void) atomic_dec(&client->refcnt);
}

static void acm_svr_accept(void)
{
	SOCKET s;
	int i;

	acm_log(2, "\n");
	s = accept(listen_socket, NULL, NULL);
	if (s == INVALID_SOCKET) {
		acm_log(0, "ERROR - failed to accept connection\n");
		return;
	}

	for (i = 0; i < FD_SETSIZE - 1; i++) {
		if (!atomic_get(&client_array[i].refcnt))
			break;
	}

	if (i == FD_SETSIZE - 1) {
		acm_log(0, "ERROR - all connections busy - rejecting\n");
		closesocket(s);
		return;
	}

	client_array[i].sock = s;
	atomic_set(&client_array[i].refcnt, 1);
	acm_log(2, "assigned client %d\n", i);
}

static int
acm_is_path_from_port(struct acm_port *port, struct ibv_path_record *path)
{
	union ibv_gid gid;
	uint8_t i;

	if (!ib_any_gid(&path->sgid)) {
		return (acm_gid_index(port, &path->sgid) < port->gid_cnt);
	}

	if (path->slid) {
		return (port->lid == (ntohs(path->slid) & port->lid_mask));
	}

	if (ib_any_gid(&path->dgid)) {
		return 1;
	}

	if (acm_gid_index(port, &path->dgid) < port->gid_cnt) {
		return 1;
	}

	for (i = 0; i < port->gid_cnt; i++) {
		ibv_query_gid(port->dev->verbs, port->port_num, i, &gid);
		if (gid.global.subnet_prefix == path->dgid.global.subnet_prefix) {
			return 1;
		}
	}

	return 0;
}

static struct acm_ep *
acm_get_port_ep(struct acm_port *port, struct acm_ep_addr_data *data)
{
	struct acm_ep *ep;
	DLIST_ENTRY *ep_entry;

	if (port->state != IBV_PORT_ACTIVE)
		return NULL;

	if (data->type == ACM_EP_INFO_PATH &&
	    !acm_is_path_from_port(port, &data->info.path))
		return NULL;

	for (ep_entry = port->ep_list.Next; ep_entry != &port->ep_list;
		 ep_entry = ep_entry->Next) {

		ep = container_of(ep_entry, struct acm_ep, entry);
		if (ep->state != ACM_READY)
			continue;

		if ((data->type == ACM_EP_INFO_PATH) &&
		    (!data->info.path.pkey || (ntohs(data->info.path.pkey) == ep->pkey)))
			return ep;

		if (acm_addr_index(ep, data->info.addr, (uint8_t) data->type) >= 0)
			return ep;
	}

	return NULL;
}

static struct acm_ep *
acm_get_ep(struct acm_ep_addr_data *data)
{
	struct acm_device *dev;
	struct acm_ep *ep;
	DLIST_ENTRY *dev_entry;
	int i;

	acm_format_name(2, log_data, sizeof log_data,
			data->type, data->info.addr, sizeof data->info.addr);
	acm_log(2, "%s\n", log_data);
	for (dev_entry = dev_list.Next; dev_entry != &dev_list;
		 dev_entry = dev_entry->Next) {

		dev = container_of(dev_entry, struct acm_device, entry);
		for (i = 0; i < dev->port_cnt; i++) {
			lock_acquire(&dev->port[i].lock);
			ep = acm_get_port_ep(&dev->port[i], data);
			lock_release(&dev->port[i].lock);
			if (ep)
				return ep;
		}
	}

	acm_format_name(0, log_data, sizeof log_data,
			data->type, data->info.addr, sizeof data->info.addr);
	acm_log(1, "notice - could not find %s\n", log_data);
	return NULL;
}

static int
acm_svr_query_path(struct acm_client *client, struct acm_msg *msg)
{
	struct acm_request *req;
	struct acm_send_msg *sa_msg;
	struct ib_sa_mad *mad;
	struct acm_ep *ep;
	uint8_t status;

	acm_log(2, "client %d\n", client->index);
	if (msg->hdr.length != ACM_MSG_HDR_LENGTH + ACM_MSG_EP_LENGTH) {
		acm_log(0, "ERROR - invalid length: 0x%x\n", msg->hdr.length);
		status = ACM_STATUS_EINVAL;
		goto resp;
	}

	ep = acm_get_ep(&msg->resolve_data[0]);
	if (!ep) {
		acm_log(1, "notice - could not find local end point\n");
		status = ACM_STATUS_ESRCADDR;
		goto resp;
	}

	req = acm_alloc_req(client, msg);
	if (!req) {
		status = ACM_STATUS_ENOMEM;
		goto resp;
	}

	if (!acm_acquire_sa_dest(ep->port)) {
		acm_log(1, "cannot acquire SA destination\n");
		status = ACM_STATUS_EINVAL;
		goto free;
	}

	sa_msg = acm_alloc_send(ep, &ep->port->sa_dest, sizeof(*mad));
	acm_release_sa_dest(&ep->port->sa_dest);
	if (!sa_msg) {
		acm_log(0, "ERROR - cannot allocate send msg\n");
		status = ACM_STATUS_ENOMEM;
		goto free;
	}

	acm_init_send_req(sa_msg, (void *) req, acm_client_sa_resp);
	mad = (struct ib_sa_mad *) sa_msg->data;
	acm_init_path_query(mad);

	memcpy(mad->data, &msg->resolve_data[0].info.path,
		sizeof(struct ibv_path_record));
	mad->comp_mask = acm_path_comp_mask(&msg->resolve_data[0].info.path);

	atomic_inc(&counter[ACM_CNTR_ROUTE_QUERY]);
	acm_post_send(&ep->sa_queue, sa_msg);
	return ACM_STATUS_SUCCESS;

free:
	acm_free_req(req);
resp:
	return acm_client_query_resp(client, msg, status);
}

static uint8_t
acm_send_resolve(struct acm_ep *ep, struct acm_dest *dest,
	struct acm_ep_addr_data *saddr)
{
	struct acm_send_msg *msg;
	struct acm_mad *mad;
	struct acm_resolve_rec *rec;
	int i;

	acm_log(2, "\n");
	msg = acm_alloc_send(ep, &ep->mc_dest[0], sizeof(*mad));
	if (!msg) {
		acm_log(0, "ERROR - cannot allocate send msg\n");
		return ACM_STATUS_ENOMEM;
	}

	acm_init_send_req(msg, (void *) dest, acm_process_addr_resp);
	(void) atomic_inc(&dest->refcnt);

	mad = (struct acm_mad *) msg->data;
	mad->base_version = 1;
	mad->mgmt_class = ACM_MGMT_CLASS;
	mad->class_version = 1;
	mad->method = IB_METHOD_GET;
	mad->control = ACM_CTRL_RESOLVE;
	mad->tid = htonll((uint64_t) atomic_inc(&tid));

	rec = (struct acm_resolve_rec *) mad->data;
	rec->src_type = (uint8_t) saddr->type;
	rec->src_length = ACM_MAX_ADDRESS;
	memcpy(rec->src, saddr->info.addr, ACM_MAX_ADDRESS);
	rec->dest_type = dest->addr_type;
	rec->dest_length = ACM_MAX_ADDRESS;
	memcpy(rec->dest, dest->address, ACM_MAX_ADDRESS);

	rec->gid_cnt = (uint8_t) ep->mc_cnt;
	for (i = 0; i < ep->mc_cnt; i++)
		memcpy(&rec->gid[i], ep->mc_dest[i].address, 16);
	
	atomic_inc(&counter[ACM_CNTR_ADDR_QUERY]);
	acm_post_send(&ep->resolve_queue, msg);
	return 0;
}

static int acm_svr_select_src(struct acm_ep_addr_data *src, struct acm_ep_addr_data *dst)
{
	union socket_addr addr;
	socklen_t len;
	int ret;
	SOCKET s;

	acm_log(2, "selecting source address\n");
	memset(&addr, 0, sizeof addr);
	switch (dst->type) {
	case ACM_EP_INFO_ADDRESS_IP:
		addr.sin.sin_family = AF_INET;
		memcpy(&addr.sin.sin_addr, dst->info.addr, 4);
		len = sizeof(struct sockaddr_in);
		break;
	case ACM_EP_INFO_ADDRESS_IP6:
		addr.sin6.sin6_family = AF_INET6;
		memcpy(&addr.sin6.sin6_addr, dst->info.addr, 16);
		len = sizeof(struct sockaddr_in6);
		break;
	default:
		acm_log(1, "notice - bad destination type, cannot lookup source\n");
		return ACM_STATUS_EDESTTYPE;
	}

	s = socket(addr.sa.sa_family, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET) {
		acm_log(0, "ERROR - unable to allocate socket\n");
		return socket_errno();
	}

	ret = connect(s, &addr.sa, len);
	if (ret) {
		acm_log(0, "ERROR - unable to connect socket\n");
		ret = socket_errno();
		goto out;
	}

	ret = getsockname(s, &addr.sa, &len);
	if (ret) {
		acm_log(0, "ERROR - failed to get socket address\n");
		ret = socket_errno();
		goto out;
	}

	src->type = dst->type;
	src->flags = ACM_EP_FLAG_SOURCE;
	if (dst->type == ACM_EP_INFO_ADDRESS_IP) {
		memcpy(&src->info.addr, &addr.sin.sin_addr, 4);
	} else {
		memcpy(&src->info.addr, &addr.sin6.sin6_addr, 16);
	}
out:
	close(s);
	return ret;
}

/*
 * Verify the resolve message from the client and return
 * references to the source and destination addresses.
 * The message buffer contains extra address data buffers.  If a
 * source address is not given, reference an empty address buffer,
 * and we'll resolve a source address later.  Record the location of
 * the source and destination addresses in the message header data
 * to avoid further searches.
 */
static uint8_t acm_svr_verify_resolve(struct acm_msg *msg)
{
	int i, cnt, have_dst = 0;

	if (msg->hdr.length < ACM_MSG_HDR_LENGTH) {
		acm_log(0, "ERROR - invalid msg hdr length %d\n", msg->hdr.length);
		return ACM_STATUS_EINVAL;
	}

	msg->hdr.src_out = 1;
	cnt = (msg->hdr.length - ACM_MSG_HDR_LENGTH) / ACM_MSG_EP_LENGTH;
	for (i = 0; i < cnt; i++) {
		if (msg->resolve_data[i].flags & ACM_EP_FLAG_SOURCE) {
			if (!msg->hdr.src_out) {
				acm_log(0, "ERROR - multiple sources specified\n");
				return ACM_STATUS_ESRCADDR;
			}
			if (!msg->resolve_data[i].type ||
			    (msg->resolve_data[i].type >= ACM_ADDRESS_RESERVED)) {
				acm_log(0, "ERROR - unsupported source address type\n");
				return ACM_STATUS_ESRCTYPE;
			}
			msg->hdr.src_out = 0;
			msg->hdr.src_index = i;
		}
		if (msg->resolve_data[i].flags & ACM_EP_FLAG_DEST) {
			if (have_dst) {
				acm_log(0, "ERROR - multiple destinations specified\n");
				return ACM_STATUS_EDESTADDR;
			}
			if (!msg->resolve_data[i].type ||
			    (msg->resolve_data[i].type >= ACM_ADDRESS_RESERVED)) {
				acm_log(0, "ERROR - unsupported destination address type\n");
				return ACM_STATUS_EDESTTYPE;
			}
			have_dst = 1;
			msg->hdr.dst_index = i;
		}
	}

	if (!have_dst) {
		acm_log(0, "ERROR - destination address required\n");
		return ACM_STATUS_EDESTTYPE;
	}

	if (msg->hdr.src_out) {
		msg->hdr.src_index = i;
		memset(&msg->resolve_data[i], 0, sizeof(struct acm_ep_addr_data));
	}
	return ACM_STATUS_SUCCESS;
}

/* Caller must hold dest lock */
static uint8_t
acm_svr_queue_req(struct acm_dest *dest, struct acm_client *client,
	struct acm_msg *msg)
{
	struct acm_request *req;

	acm_log(2, "client %d\n", client->index);
	req = acm_alloc_req(client, msg);
	if (!req) {
		return ACM_STATUS_ENOMEM;
	}

	DListInsertTail(&req->entry, &dest->req_queue);
	return ACM_STATUS_SUCCESS;
}

static int acm_dest_timeout(struct acm_dest *dest)
{
	uint64_t timestamp = time_stamp_min();

	if (timestamp > dest->addr_timeout) {
		acm_log(2, "%s address timed out\n", dest->name);
		dest->state = ACM_INIT;
		return 1;
	} else if (timestamp > dest->route_timeout) {
		acm_log(2, "%s route timed out\n", dest->name);
		dest->state = ACM_ADDR_RESOLVED;
		return 1;
	}
	return 0;
}

static int
acm_svr_resolve_dest(struct acm_client *client, struct acm_msg *msg)
{
	struct acm_ep *ep;
	struct acm_dest *dest;
	struct acm_ep_addr_data *saddr, *daddr;
	uint8_t status;
	int ret;

	acm_log(2, "client %d\n", client->index);
	status = acm_svr_verify_resolve(msg);
	if (status) {
		acm_log(0, "notice - misformatted or unsupported request\n");
		return acm_client_resolve_resp(client, msg, NULL, status);
	}

	saddr = &msg->resolve_data[msg->hdr.src_index];
	daddr = &msg->resolve_data[msg->hdr.dst_index];
	if (msg->hdr.src_out) {
		status = acm_svr_select_src(saddr, daddr);
		if (status) {
			acm_log(0, "notice - unable to select suitable source address\n");
			return acm_client_resolve_resp(client, msg, NULL, status);
		}
	}

	acm_format_name(2, log_data, sizeof log_data,
			saddr->type, saddr->info.addr, sizeof saddr->info.addr);
	acm_log(2, "src  %s\n", log_data);
	ep = acm_get_ep(saddr);
	if (!ep) {
		acm_log(0, "notice - unknown local end point\n");
		return acm_client_resolve_resp(client, msg, NULL, ACM_STATUS_ESRCADDR);
	}

	acm_format_name(2, log_data, sizeof log_data,
			daddr->type, daddr->info.addr, sizeof daddr->info.addr);
	acm_log(2, "dest %s\n", log_data);

	dest = acm_acquire_dest(ep, daddr->type, daddr->info.addr);
	if (!dest) {
		acm_log(0, "ERROR - unable to allocate destination in client request\n");
		return acm_client_resolve_resp(client, msg, NULL, ACM_STATUS_ENOMEM);
	}

	lock_acquire(&dest->lock);
test:
	switch (dest->state) {
	case ACM_READY:
		if (acm_dest_timeout(dest))
			goto test;
		acm_log(2, "request satisfied from local cache\n");
		atomic_inc(&counter[ACM_CNTR_ROUTE_CACHE]);
		status = ACM_STATUS_SUCCESS;
		break;
	case ACM_ADDR_RESOLVED:
		acm_log(2, "have address, resolving route\n");
		atomic_inc(&counter[ACM_CNTR_ADDR_CACHE]);
		status = acm_resolve_path(ep, dest, acm_dest_sa_resp);
		if (status) {
			break;
		}
		goto queue;
	case ACM_INIT:
		acm_log(2, "sending resolve msg to dest\n");
		status = acm_send_resolve(ep, dest, saddr);
		if (status) {
			break;
		}
		dest->state = ACM_QUERY_ADDR;
		/* fall through */
	default:
queue:
		if (daddr->flags & ACM_FLAGS_NODELAY) {
			acm_log(2, "lookup initiated, but client wants no delay\n");
			status = ACM_STATUS_ENODATA;
			break;
		}
		status = acm_svr_queue_req(dest, client, msg);
		if (status) {
			break;
		}
		ret = 0;
		lock_release(&dest->lock);
		goto put;
	}
	lock_release(&dest->lock);
	ret = acm_client_resolve_resp(client, msg, dest, status);
put:
	acm_put_dest(dest);
	return ret;
}

/*
 * The message buffer contains extra address data buffers.  We extract the
 * destination address from the path record into an extra buffer, so we can
 * lookup the destination by either LID or GID.
 */
static int
acm_svr_resolve_path(struct acm_client *client, struct acm_msg *msg)
{
	struct acm_ep *ep;
	struct acm_dest *dest;
	struct ibv_path_record *path;
	uint8_t *addr;
	uint8_t status;
	int ret;

	acm_log(2, "client %d\n", client->index);
	if (msg->hdr.length < (ACM_MSG_HDR_LENGTH + ACM_MSG_EP_LENGTH)) {
		acm_log(0, "notice - invalid msg hdr length %d\n", msg->hdr.length);
		return acm_client_resolve_resp(client, msg, NULL, ACM_STATUS_EINVAL);
	}

	path = &msg->resolve_data[0].info.path;
	if (!path->dlid && ib_any_gid(&path->dgid)) {
		acm_log(0, "notice - no destination specified\n");
		return acm_client_resolve_resp(client, msg, NULL, ACM_STATUS_EDESTADDR);
	}

	acm_format_name(2, log_data, sizeof log_data, ACM_EP_INFO_PATH,
		msg->resolve_data[0].info.addr, sizeof *path);
	acm_log(2, "path %s\n", log_data);
	ep = acm_get_ep(&msg->resolve_data[0]);
	if (!ep) {
		acm_log(0, "notice - unknown local end point\n");
		return acm_client_resolve_resp(client, msg, NULL, ACM_STATUS_ESRCADDR);
	}

	addr = msg->resolve_data[1].info.addr;
	memset(addr, 0, ACM_MAX_ADDRESS);
	if (path->dlid) {
		* ((uint16_t *) addr) = path->dlid;
		dest = acm_acquire_dest(ep, ACM_ADDRESS_LID, addr);
	} else {
		memcpy(addr, &path->dgid, sizeof path->dgid);
		dest = acm_acquire_dest(ep, ACM_ADDRESS_GID, addr);
	}
	if (!dest) {
		acm_log(0, "ERROR - unable to allocate destination in client request\n");
		return acm_client_resolve_resp(client, msg, NULL, ACM_STATUS_ENOMEM);
	}

	lock_acquire(&dest->lock);
test:
	switch (dest->state) {
	case ACM_READY:
		if (acm_dest_timeout(dest))
			goto test;
		acm_log(2, "request satisfied from local cache\n");
		atomic_inc(&counter[ACM_CNTR_ROUTE_CACHE]);
		status = ACM_STATUS_SUCCESS;
		break;
	case ACM_INIT:
		acm_log(2, "have path, bypassing address resolution\n");
		acm_record_path_addr(ep, dest, path);
		/* fall through */
	case ACM_ADDR_RESOLVED:
		acm_log(2, "have address, resolving route\n");
		status = acm_resolve_path(ep, dest, acm_dest_sa_resp);
		if (status) {
			break;
		}
		/* fall through */
	default:
		if (msg->resolve_data[0].flags & ACM_FLAGS_NODELAY) {
			acm_log(2, "lookup initiated, but client wants no delay\n");
			status = ACM_STATUS_ENODATA;
			break;
		}
		status = acm_svr_queue_req(dest, client, msg);
		if (status) {
			break;
		}
		ret = 0;
		lock_release(&dest->lock);
		goto put;
	}
	lock_release(&dest->lock);
	ret = acm_client_resolve_resp(client, msg, dest, status);
put:
	acm_put_dest(dest);
	return ret;
}

static int acm_svr_resolve(struct acm_client *client, struct acm_msg *msg)
{
	(void) atomic_inc(&client->refcnt);

	if (msg->resolve_data[0].type == ACM_EP_INFO_PATH) {
		if (msg->resolve_data[0].flags & ACM_FLAGS_QUERY_SA) {
			return acm_svr_query_path(client, msg);
		} else {
			return acm_svr_resolve_path(client, msg);
		}
	} else {
		return acm_svr_resolve_dest(client, msg);
	}
}

static int acm_svr_perf_query(struct acm_client *client, struct acm_msg *msg)
{
	int ret, i;
	uint16_t len;

	acm_log(2, "client %d\n", client->index);
	msg->hdr.opcode |= ACM_OP_ACK;
	msg->hdr.status = ACM_STATUS_SUCCESS;
	msg->hdr.data[0] = ACM_MAX_COUNTER;
	msg->hdr.data[1] = 0;
	msg->hdr.data[2] = 0;
	len = ACM_MSG_HDR_LENGTH + (ACM_MAX_COUNTER * sizeof(uint64_t));
	msg->hdr.length = htons(len);

	for (i = 0; i < ACM_MAX_COUNTER; i++)
		msg->perf_data[i] = htonll((uint64_t) atomic_get(&counter[i]));

	ret = send(client->sock, (char *) msg, len, 0);
	if (ret != len)
		acm_log(0, "ERROR - failed to send response\n");
	else
		ret = 0;

	return ret;
}

static int acm_msg_length(struct acm_msg *msg)
{
	return (msg->hdr.opcode == ACM_OP_RESOLVE) ?
		msg->hdr.length : ntohs(msg->hdr.length);
}

static void acm_svr_receive(struct acm_client *client)
{
	struct acm_msg msg;
	int ret;

	acm_log(2, "client %d\n", client->index);
	ret = recv(client->sock, (char *) &msg, sizeof msg, 0);
	if (ret <= 0 || ret != acm_msg_length(&msg)) {
		acm_log(2, "client disconnected\n");
		ret = ACM_STATUS_ENOTCONN;
		goto out;
	}

	if (msg.hdr.version != ACM_VERSION) {
		acm_log(0, "ERROR - unsupported version %d\n", msg.hdr.version);
		goto out;
	}

	switch (msg.hdr.opcode & ACM_OP_MASK) {
	case ACM_OP_RESOLVE:
		atomic_inc(&counter[ACM_CNTR_RESOLVE]);
		ret = acm_svr_resolve(client, &msg);
		break;
	case ACM_OP_PERF_QUERY:
		ret = acm_svr_perf_query(client, &msg);
		break;
	default:
		acm_log(0, "ERROR - unknown opcode 0x%x\n", msg.hdr.opcode);
		break;
	}

out:
	if (ret)
		acm_disconnect_client(client);
}

static struct acm_device *
acm_get_device_from_gid(union ibv_gid *sgid, uint8_t *port);
static struct acm_ep *acm_find_ep(struct acm_port *port, uint16_t pkey);
static int
acm_ep_insert_addr(struct acm_ep *ep, uint8_t *addr, size_t addr_len, uint8_t addr_type);

static int acm_nl_to_addr_data(struct acm_ep_addr_data *ad,
				  int af_family, uint8_t *addr, size_t addr_len)
{
	if (addr_len > ACM_MAX_ADDRESS)
		return EINVAL;

	/* find the ep associated with this address "if any" */
	switch (af_family) {
	case AF_INET:
		ad->type = ACM_ADDRESS_IP;
		break;
	case AF_INET6:
		ad->type = ACM_ADDRESS_IP6;
		break;
	default:
		return EINVAL;
	}
	memcpy(&ad->info.addr, addr, addr_len);
	return 0;
}

static void acm_add_ep_ip(struct acm_ep_addr_data *data, char *ifname)
{
	struct acm_ep *ep;
	struct acm_device *dev;
	uint8_t port_num;
	uint16_t pkey;
	union ibv_gid sgid;

	ep = acm_get_ep(data);
	if (ep) {
		acm_format_name(1, log_data, sizeof log_data,
				data->type, data->info.addr, sizeof data->info.addr);
		acm_log(1, "Address '%s' already available\n", log_data);
		return;
	}

	if (acm_if_get_sgid(ifname, &sgid))
		return;

	dev = acm_get_device_from_gid(&sgid, &port_num);
	if (!dev)
		return;

	if (acm_if_get_pkey(ifname, &pkey))
		return;

	acm_format_name(0, log_data, sizeof log_data,
			data->type, data->info.addr, sizeof data->info.addr);
	acm_log(0, " %s\n", log_data);

	ep = acm_find_ep(&dev->port[port_num-1], pkey);
	if (ep) {
		if (acm_ep_insert_addr(ep, data->info.addr, sizeof data->info.addr, data->type))
			acm_log(0, "Failed to add '%s' to EP\n", log_data);
	} else {
		acm_log(0, "Failed to add '%s' no EP for pkey\n", log_data);
	}
}

static void acm_rm_ep_ip(struct acm_ep_addr_data *data)
{
	struct acm_ep *ep;

	ep = acm_get_ep(data);
	if (ep) {
		acm_format_name(0, log_data, sizeof log_data,
				data->type, data->info.addr, sizeof data->info.addr);
		acm_log(0, " %s\n", log_data);
		acm_mark_addr_invalid(ep, data);
	}
}

static int acm_ipnl_create(void)
{
	struct sockaddr_nl addr;

	if ((ip_mon_socket = socket(PF_NETLINK, SOCK_RAW | SOCK_NONBLOCK, NETLINK_ROUTE)) == -1) {
		acm_log(0, "Failed to open NETLINK_ROUTE socket");
		return EIO;
	}

	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR;

	if (bind(ip_mon_socket, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		acm_log(0, "Failed to bind NETLINK_ROUTE socket");
		return EIO;
	}

	return 0;
}

static void acm_ip_iter_cb(char *ifname, union ibv_gid *gid, uint16_t pkey,
		uint8_t addr_type, uint8_t *addr, size_t addr_len,
		char *addr_name, void *ctx)
{
	int ret = EINVAL;
	struct acm_device *dev = NULL;
	struct acm_ep *ep = NULL;
	uint8_t port_num;
	char gid_str[INET6_ADDRSTRLEN];

	dev = acm_get_device_from_gid(gid, &port_num);
	if (dev)
		ep = acm_find_ep(&dev->port[port_num-1], pkey);

	if (ep)
		ret = acm_ep_insert_addr(ep, addr, addr_len, addr_type);

	if (ret) {
		acm_format_name(2, log_data, sizeof log_data,
			addr_type, addr, addr_len);
		inet_ntop(AF_INET6, gid->raw, gid_str, sizeof(gid_str));
		acm_log(0, "Failed to add '%s' (gid %s; pkey 0x%x)\n",
			log_data, gid_str, pkey);
	}
}

/* Netlink updates have indicated a failure which means we are no longer in
 * sync.  This should be a rare condition so we handle this with a "big
 * hammer" by clearing and re-reading all the system IP's.
 */
static int resync_system_ips(void)
{
	DLIST_ENTRY *dev_entry;
	struct acm_device *dev;
	int cnt;

	acm_log(0, "Resyncing all IP's\n");

	/* mark all IP's invalid */
	for (dev_entry = dev_list.Next; dev_entry != &dev_list;
	     dev_entry = dev_entry->Next) {
		struct acm_ep *ep = NULL;
		DLIST_ENTRY *entry;

		dev = container_of(dev_entry, struct acm_device, entry);

		for (cnt = 0; cnt < dev->port_cnt; cnt++) {
			struct acm_port *port = &dev->port[cnt];

			for (entry = port->ep_list.Next; entry != &port->ep_list;
			     entry = entry->Next) {
				int i;

				ep = container_of(entry, struct acm_ep, entry);
				for (i = 0; i < MAX_EP_ADDR; i++) {
					if (ep->addr_type[i] == ACM_ADDRESS_IP
					    || ep->addr_type[i] == ACM_ADDRESS_IP6)
						ep->addr_type[i] = ACM_ADDRESS_INVALID;
				}
			}
		}
	}

	return acm_if_iter_sys(acm_ip_iter_cb, NULL);
}

#define NL_MSG_BUF_SIZE 4096
static void acm_ipnl_handler(void)
{
	int len;
	char buffer[NL_MSG_BUF_SIZE];
	struct nlmsghdr *nlh;
	char name[IFNAMSIZ];
	char ip_str[INET6_ADDRSTRLEN];
	struct acm_ep_addr_data ad;

	while ((len = recv(ip_mon_socket, buffer, NL_MSG_BUF_SIZE, 0)) > 0) {
		nlh = (struct nlmsghdr *)buffer;
		while ((NLMSG_OK(nlh, len)) && (nlh->nlmsg_type != NLMSG_DONE)) {
			struct ifaddrmsg *ifa = (struct ifaddrmsg *) NLMSG_DATA(nlh);
			struct ifinfomsg *ifi = (struct ifinfomsg *) NLMSG_DATA(nlh);
			struct rtattr *rth = IFA_RTA(ifa);
			int rtl = IFA_PAYLOAD(nlh);

			switch (nlh->nlmsg_type) {
			case RTM_NEWADDR:
			{
				if_indextoname(ifa->ifa_index, name);
				while (rtl && RTA_OK(rth, rtl)) {
					if (rth->rta_type == IFA_LOCAL) {
						acm_log(1, "New system address available %s : %s\n",
						        name, inet_ntop(ifa->ifa_family, RTA_DATA(rth),
							ip_str, sizeof(ip_str)));
						if (!acm_nl_to_addr_data(&ad, ifa->ifa_family,
								      RTA_DATA(rth),
								      RTA_PAYLOAD(rth))) {
							acm_add_ep_ip(&ad, name);
						}
					}
					rth = RTA_NEXT(rth, rtl);
				}
				break;
			}
			case RTM_DELADDR:
			{
				if_indextoname(ifa->ifa_index, name);
				while (rtl && RTA_OK(rth, rtl)) {
					if (rth->rta_type == IFA_LOCAL) {
						acm_log(1, "System address removed %s : %s\n",
						        name, inet_ntop(ifa->ifa_family, RTA_DATA(rth),
							ip_str, sizeof(ip_str)));
						if (!acm_nl_to_addr_data(&ad, ifa->ifa_family,
								      RTA_DATA(rth),
								      RTA_PAYLOAD(rth))) {
							acm_rm_ep_ip(&ad);
						}
					}
					rth = RTA_NEXT(rth, rtl);
				}
				break;
			}
			case RTM_NEWLINK:
			{
				acm_log(2, "Link added : %s\n", if_indextoname(ifi->ifi_index, name));
				break;
			}
			case RTM_DELLINK:
			{
				acm_log(2, "Link removed : %s\n", if_indextoname(ifi->ifi_index, name));
				break;
			}
			default:
				acm_log(2, "unknown netlink message\n");
				break;
			}
			nlh = NLMSG_NEXT(nlh, len);
		}
	}

	if (len < 0 && errno == ENOBUFS) {
		acm_log(0, "ENOBUFS returned from netlink...\n");
		resync_system_ips();
	}
}

static void acm_server(void)
{
	fd_set readfds;
	int i, n, ret;

	acm_log(0, "started\n");
	acm_init_server();
	ret = acm_listen();
	if (ret) {
		acm_log(0, "ERROR - server listen failed\n");
		return;
	}

	while (1) {
		n = (int) listen_socket;
		FD_ZERO(&readfds);
		FD_SET(listen_socket, &readfds);

		n = max(n, (int) ip_mon_socket);
		FD_SET(ip_mon_socket, &readfds);

		for (i = 0; i < FD_SETSIZE - 1; i++) {
			if (client_array[i].sock != INVALID_SOCKET) {
				FD_SET(client_array[i].sock, &readfds);
				n = max(n, (int) client_array[i].sock);
			}
		}

		ret = select(n + 1, &readfds, NULL, NULL, NULL);
		if (ret == SOCKET_ERROR) {
			acm_log(0, "ERROR - server select error\n");
			continue;
		}

		if (FD_ISSET(listen_socket, &readfds))
			acm_svr_accept();

		if (FD_ISSET(ip_mon_socket, &readfds))
			acm_ipnl_handler();

		for (i = 0; i < FD_SETSIZE - 1; i++) {
			if (client_array[i].sock != INVALID_SOCKET &&
				FD_ISSET(client_array[i].sock, &readfds)) {
				acm_log(2, "receiving from client %d\n", i);
				acm_svr_receive(&client_array[i]);
			}
		}
	}
}

static enum acm_addr_prot acm_convert_addr_prot(char *param)
{
	if (!stricmp("acm", param))
		return ACM_ADDR_PROT_ACM;

	return addr_prot;
}

static enum acm_route_prot acm_convert_route_prot(char *param)
{
	if (!stricmp("acm", param))
		return ACM_ROUTE_PROT_ACM;
	else if (!stricmp("sa", param))
		return ACM_ROUTE_PROT_SA;

	return route_prot;
}

static enum acm_loopback_prot acm_convert_loopback_prot(char *param)
{
	if (!stricmp("none", param))
		return ACM_LOOPBACK_PROT_NONE;
	else if (!stricmp("local", param))
		return ACM_LOOPBACK_PROT_LOCAL;

	return loopback_prot;
}

static enum acm_route_preload acm_convert_route_preload(char *param)
{
	if (!stricmp("none", param) || !stricmp("no", param))
		return ACM_ROUTE_PRELOAD_NONE;
	else if (!stricmp("opensm_full_v1", param))
		return ACM_ROUTE_PRELOAD_OSM_FULL_V1;

	return route_preload;
}

static enum acm_route_preload acm_convert_addr_preload(char *param)
{
	if (!stricmp("none", param) || !stricmp("no", param))
		return ACM_ADDR_PRELOAD_NONE;
	else if (!stricmp("acm_hosts", param))
		return ACM_ADDR_PRELOAD_HOSTS;

	return addr_preload;
}

static enum ibv_rate acm_get_rate(uint8_t width, uint8_t speed)
{
	switch (width) {
	case 1:
		switch (speed) {
		case 1: return IBV_RATE_2_5_GBPS;
		case 2: return IBV_RATE_5_GBPS;
		case 4: return IBV_RATE_10_GBPS;
		default: return IBV_RATE_MAX;
		}
	case 2:
		switch (speed) {
		case 1: return IBV_RATE_10_GBPS;
		case 2: return IBV_RATE_20_GBPS;
		case 4: return IBV_RATE_40_GBPS;
		default: return IBV_RATE_MAX;
		}
	case 4:
		switch (speed) {
		case 1: return IBV_RATE_20_GBPS;
		case 2: return IBV_RATE_40_GBPS;
		case 4: return IBV_RATE_80_GBPS;
		default: return IBV_RATE_MAX;
		}
	case 8:
		switch (speed) {
		case 1: return IBV_RATE_30_GBPS;
		case 2: return IBV_RATE_60_GBPS;
		case 4: return IBV_RATE_120_GBPS;
		default: return IBV_RATE_MAX;
		}
	default:
		acm_log(0, "ERROR - unknown link width 0x%x\n", width);
		return IBV_RATE_MAX;
	}
}

static enum ibv_mtu acm_convert_mtu(int mtu)
{
	switch (mtu) {
	case 256:  return IBV_MTU_256;
	case 512:  return IBV_MTU_512;
	case 1024: return IBV_MTU_1024;
	case 2048: return IBV_MTU_2048;
	case 4096: return IBV_MTU_4096;
	default:   return IBV_MTU_2048;
	}
}

static enum ibv_rate acm_convert_rate(int rate)
{
	switch (rate) {
	case 2:   return IBV_RATE_2_5_GBPS;
	case 5:   return IBV_RATE_5_GBPS;
	case 10:  return IBV_RATE_10_GBPS;
	case 20:  return IBV_RATE_20_GBPS;
	case 30:  return IBV_RATE_30_GBPS;
	case 40:  return IBV_RATE_40_GBPS;
	case 60:  return IBV_RATE_60_GBPS;
	case 80:  return IBV_RATE_80_GBPS;
	case 120: return IBV_RATE_120_GBPS;
	default:  return IBV_RATE_10_GBPS;
	}
}

static int acm_post_recvs(struct acm_ep *ep)
{
	int i, size;

	size = recv_depth * ACM_RECV_SIZE;
	ep->recv_bufs = malloc(size);
	if (!ep->recv_bufs) {
		acm_log(0, "ERROR - unable to allocate receive buffer\n");
		return ACM_STATUS_ENOMEM;
	}

	ep->mr = ibv_reg_mr(ep->port->dev->pd, ep->recv_bufs, size,
		IBV_ACCESS_LOCAL_WRITE);
	if (!ep->mr) {
		acm_log(0, "ERROR - unable to register receive buffer\n");
		goto err;
	}

	for (i = 0; i < recv_depth; i++) {
		acm_post_recv(ep, (uintptr_t) (ep->recv_bufs + ACM_RECV_SIZE * i));
	}
	return 0;

err:
	free(ep->recv_bufs);
	return -1;
}

static FILE *acm_open_addr_file(void)
{
	FILE *f;

	if ((f = fopen(addr_file, "r")))
		return f;

	acm_log(0, "notice - generating %s file\n", addr_file);
	if (!(f = popen(acme, "r"))) {
		acm_log(0, "ERROR - cannot generate %s\n", addr_file);
		return NULL;
	}
	pclose(f);
	return fopen(addr_file, "r");
}

/* Parse "opensm full v1" file to build LID to GUID table */
static void acm_parse_osm_fullv1_lid2guid(FILE *f, uint64_t *lid2guid)
{
	char s[128];
	char *p, *ptr, *p_guid, *p_lid;
	uint64_t guid;
	uint16_t lid;

	while (fgets(s, sizeof s, f)) {
		if (s[0] == '#')
			continue;
		if (!(p = strtok_r(s, " \n", &ptr)))
			continue;	/* ignore blank lines */

		if (strncmp(p, "Switch", sizeof("Switch") - 1) &&
		    strncmp(p, "Channel", sizeof("Channel") - 1) &&
		    strncmp(p, "Router", sizeof("Router") - 1))
			continue;

		if (!strncmp(p, "Channel", sizeof("Channel") - 1)) {
			p = strtok_r(NULL, " ", &ptr); /* skip 'Adapter' */
			if (!p)
				continue;
		}

		p_guid = strtok_r(NULL, ",", &ptr);
		if (!p_guid)
			continue;

		guid = (uint64_t) strtoull(p_guid, NULL, 16);

		ptr = strstr(ptr, "base LID");
		if (!ptr)
			continue;
		ptr += sizeof("base LID");
		p_lid = strtok_r(NULL, ",", &ptr);
		if (!p_lid)
			continue;

		lid = (uint16_t) strtoul(p_lid, NULL, 0);
		if (lid >= IB_LID_MCAST_START)
			continue;
		if (lid2guid[lid])
			acm_log(0, "ERROR - duplicate lid %u\n", lid);
		else
			lid2guid[lid] = htonll(guid);
	}
}

/* Parse 'opensm full v1' file to populate PR cache */
static int acm_parse_osm_fullv1_paths(FILE *f, uint64_t *lid2guid, struct acm_ep *ep)
{
	union ibv_gid sgid, dgid;
	struct ibv_port_attr attr = { 0 };
	struct acm_dest *dest;
	char s[128];
	char *p, *ptr, *p_guid, *p_lid;
	uint64_t guid;
	uint16_t lid, dlid, net_dlid;
	int sl, mtu, rate;
	int ret = 1, i;
	uint8_t addr[ACM_MAX_ADDRESS];
	uint8_t addr_type;

	ibv_query_gid(ep->port->dev->verbs, ep->port->port_num, 0, &sgid);

	/* Search for endpoint's SLID */
	while (fgets(s, sizeof s, f)) {
		if (s[0] == '#')
			continue;
		if (!(p = strtok_r(s, " \n", &ptr)))
			continue;	/* ignore blank lines */

		if (strncmp(p, "Switch", sizeof("Switch") - 1) &&
		    strncmp(p, "Channel", sizeof("Channel") - 1) &&
		    strncmp(p, "Router", sizeof("Router") - 1))
			continue;

		if (!strncmp(p, "Channel", sizeof("Channel") - 1)) {
			p = strtok_r(NULL, " ", &ptr); /* skip 'Adapter' */
			if (!p)
				continue;
		}

		p_guid = strtok_r(NULL, ",", &ptr);
		if (!p_guid)
			continue;

		guid = (uint64_t) strtoull(p_guid, NULL, 16);
		if (guid != ntohll(sgid.global.interface_id))
			continue;

		ptr = strstr(ptr, "base LID");
		if (!ptr)
			continue;
		ptr += sizeof("base LID");
		p_lid = strtok_r(NULL, ",", &ptr);
		if (!p_lid)
			continue;

		lid = (uint16_t) strtoul(p_lid, NULL, 0);
		if (lid != ep->port->lid)
			continue;

		ibv_query_port(ep->port->dev->verbs, ep->port->port_num, &attr);
		ret = 0;
		break;
	}

	while (fgets(s, sizeof s, f)) {
		if (s[0] == '#')
			continue;
		if (!(p = strtok_r(s, " \n", &ptr)))
			continue;	/* ignore blank lines */

		if (!strncmp(p, "Switch", sizeof("Switch") - 1) ||
		    !strncmp(p, "Channel", sizeof("Channel") - 1) ||
		    !strncmp(p, "Router", sizeof("Router") - 1))
			break;

		dlid = strtoul(p, NULL, 0);
		net_dlid = htons(dlid);

		p = strtok_r(NULL, ":", &ptr);
		if (!p)
			continue;
		if (strcmp(p, "UNREACHABLE") == 0)
			continue;
		sl = atoi(p);

		p = strtok_r(NULL, ":", &ptr);
		if (!p)
			continue;
		mtu = atoi(p);

		p = strtok_r(NULL, ":", &ptr);
		if (!p)
			continue;
		rate = atoi(p);

		if (!lid2guid[dlid]) {
			acm_log(0, "ERROR - dlid %u not found in lid2guid table\n", dlid);
			continue;
		}

		dgid.global.subnet_prefix = sgid.global.subnet_prefix;
		dgid.global.interface_id = lid2guid[dlid];

		for (i = 0; i < 2; i++) {
			memset(addr, 0, ACM_MAX_ADDRESS);
			if (i == 0) {
				addr_type = ACM_ADDRESS_LID;
				memcpy(addr, &net_dlid, sizeof net_dlid);
			} else {
				addr_type = ACM_ADDRESS_GID;
				memcpy(addr, &dgid, sizeof(dgid));
			}
			dest = acm_acquire_dest(ep, addr_type, addr);
			if (!dest) {
				acm_log(0, "ERROR - unable to create dest\n");
				break;
			}

			dest->path.sgid = sgid;
			dest->path.slid = htons(ep->port->lid);
			dest->path.dgid = dgid;
			dest->path.dlid = net_dlid;
			dest->path.reversible_numpath = IBV_PATH_RECORD_REVERSIBLE;
			dest->path.pkey = htons(ep->pkey);
			dest->path.mtu = (uint8_t) mtu;
			dest->path.rate = (uint8_t) rate;
			dest->path.qosclass_sl = htons((uint16_t) sl & 0xF);
			if (dlid == ep->port->lid) {
				dest->path.packetlifetime = 0;
				dest->addr_timeout = (uint64_t)~0ULL;
				dest->route_timeout = (uint64_t)~0ULL;
			} else {
				dest->path.packetlifetime = attr.subnet_timeout;
				dest->addr_timeout = time_stamp_min() + (unsigned) addr_timeout;
				dest->route_timeout = time_stamp_min() + (unsigned) route_timeout;
			}
			dest->remote_qpn = 1;
			dest->state = ACM_READY;
			acm_put_dest(dest);
			acm_log(1, "added cached dest %s\n", dest->name);
		}
	}
	return ret;
}

static int acm_parse_osm_fullv1(struct acm_ep *ep)
{
	FILE *f;
	uint64_t *lid2guid;
	int ret = 1;

	if (!(f = fopen(route_data_file, "r"))) {
		acm_log(0, "ERROR - couldn't open %s\n", route_data_file);
		return ret;
	}

	lid2guid = calloc(IB_LID_MCAST_START, sizeof(*lid2guid));
	if (!lid2guid) {
		acm_log(0, "ERROR - no memory for path record parsing\n");
		goto err;
	}

	acm_parse_osm_fullv1_lid2guid(f, lid2guid);
	rewind(f);
	ret = acm_parse_osm_fullv1_paths(f, lid2guid, ep);
	free(lid2guid);
err:
	fclose(f);
	return ret;
}

static void acm_parse_hosts_file(struct acm_ep *ep)
{
	FILE *f;
	char s[120];
	char addr[INET6_ADDRSTRLEN], gid[INET6_ADDRSTRLEN];
	uint8_t name[ACM_MAX_ADDRESS];
	struct in6_addr ip_addr, ib_addr;
	struct acm_dest *dest, *gid_dest;
	uint8_t addr_type;

	if (!(f = fopen(addr_data_file, "r"))) {
		acm_log(0, "ERROR - couldn't open %s\n", addr_data_file);
		return;
        }

	while (fgets(s, sizeof s, f)) {
		if (s[0] == '#')
			continue;

		if (sscanf(s, "%46s%46s", addr, gid) != 2)
			continue;

		acm_log(2, "%s", s);
		if (inet_pton(AF_INET6, gid, &ib_addr) <= 0) {
			acm_log(0, "ERROR - %s is not IB GID\n", gid);
			continue;
		}
		memset(name, 0, ACM_MAX_ADDRESS);
		if (inet_pton(AF_INET, addr, &ip_addr) > 0) {
			addr_type = ACM_ADDRESS_IP;
			memcpy(name, &ip_addr, 4);
		} else if (inet_pton(AF_INET6, addr, &ip_addr) > 0) {
			addr_type = ACM_ADDRESS_IP6;
			memcpy(name, &ip_addr, sizeof(ip_addr));
		} else {
			addr_type = ACM_ADDRESS_NAME;
			strncpy((char *)name, addr, ACM_MAX_ADDRESS);
		}

		dest = acm_acquire_dest(ep, addr_type, name);
		if (!dest) {
			acm_log(0, "ERROR - unable to create dest %s\n", addr);
			continue;
		}

		memset(name, 0, ACM_MAX_ADDRESS);
		memcpy(name, &ib_addr, sizeof(ib_addr));
		gid_dest = acm_get_dest(ep, ACM_ADDRESS_GID, name);
		if (gid_dest) {
			dest->path = gid_dest->path;
			dest->state = ACM_READY;
			acm_put_dest(gid_dest);
		} else {
			memcpy(&dest->path.dgid, &ib_addr, 16);
			//ibv_query_gid(ep->port->dev->verbs, ep->port->port_num,
			//		0, &dest->path.sgid);
			dest->path.slid = htons(ep->port->lid);
			dest->path.reversible_numpath = IBV_PATH_RECORD_REVERSIBLE;
			dest->path.pkey = htons(ep->pkey);
			dest->state = ACM_ADDR_RESOLVED;
		}

		dest->remote_qpn = 1;
		dest->addr_timeout = time_stamp_min() + (unsigned) addr_timeout;
		dest->route_timeout = time_stamp_min() + (unsigned) route_timeout;
		acm_put_dest(dest);
		acm_log(1, "added host %s address type %d IB GID %s\n",
			addr, addr_type, gid);
	}

	fclose(f);
}

static int
acm_ep_insert_addr(struct acm_ep *ep, uint8_t *addr, size_t addr_len, uint8_t addr_type)
{
	int i;
	int ret = ENOMEM;
	uint8_t tmp[ACM_MAX_ADDRESS];
	char name_str[INET6_ADDRSTRLEN];

	if (addr_len > ACM_MAX_ADDRESS)
		return EINVAL;

	memset(tmp, 0, sizeof tmp);
	memcpy(tmp, addr, addr_len);

	lock_acquire(&ep->lock);
	if (acm_addr_index(ep, tmp, addr_type) < 0) {
		for (i = 0; i < MAX_EP_ADDR; i++) {
			if (ep->addr_type[i] == ACM_ADDRESS_INVALID) {

				ep->addr_type[i] = addr_type;
				memcpy(ep->addr[i].addr, tmp, ACM_MAX_ADDRESS);

				switch (addr_type) {
				case ACM_ADDRESS_IP:
					inet_ntop(AF_INET, addr, name_str, sizeof name_str);
					strncpy(ep->name[i], name_str, ACM_MAX_ADDRESS);
					break;
				case ACM_ADDRESS_IP6:
					inet_ntop(AF_INET6, addr, name_str, sizeof name_str);
					strncpy(ep->name[i], name_str, ACM_MAX_ADDRESS);
					break;
				case ACM_ADDRESS_NAME:
					strncpy(ep->name[i], (const char *)addr, ACM_MAX_ADDRESS);
					break;
				}

				ret = 0;
				break;
			}
		}
	} else {
		ret = 0;
	}
	lock_release(&ep->lock);
	return ret;
}

static struct acm_device *
acm_get_device_from_gid(union ibv_gid *sgid, uint8_t *port)
{
	DLIST_ENTRY *dev_entry;
	struct acm_device *dev;
	struct ibv_device_attr dev_attr;
	struct ibv_port_attr port_attr;
	union ibv_gid gid;
	int ret, i;

	for (dev_entry = dev_list.Next; dev_entry != &dev_list;
		 dev_entry = dev_entry->Next) {

		dev = container_of(dev_entry, struct acm_device, entry);

		ret = ibv_query_device(dev->verbs, &dev_attr);
		if (ret)
			continue;

		for (*port = 1; *port <= dev_attr.phys_port_cnt; (*port)++) {
			ret = ibv_query_port(dev->verbs, *port, &port_attr);
			if (ret)
				continue;

			for (i = 0; i < port_attr.gid_tbl_len; i++) {
				ret = ibv_query_gid(dev->verbs, *port, i, &gid);
				if (ret || !gid.global.interface_id)
					break;

				if (!memcmp(sgid->raw, gid.raw, sizeof gid))
					return dev;
			}
		}
	}
	return NULL;
}

static void acm_ep_ip_iter_cb(char *ifname, union ibv_gid *gid, uint16_t pkey,
		uint8_t addr_type, uint8_t *addr, size_t addr_len,
		char *addr_name, void *ctx)
{
	uint8_t port_num;
	struct acm_device *dev;
	struct acm_ep *ep = (struct acm_ep *)ctx;

	dev = acm_get_device_from_gid(gid, &port_num);
	if (dev && ep->port->dev == dev
	    && ep->port->port_num == port_num && ep->pkey == pkey) {
		if (!acm_ep_insert_addr(ep, addr, addr_len, addr_type)) {
			acm_log(0, "Added %s %s %d 0x%x from %s\n", addr_name,
				dev->verbs->device->name, port_num, pkey,
				ifname);
		}
	}
}

static int acm_get_system_ips(struct acm_ep *ep)
{
	return acm_if_iter_sys(acm_ep_ip_iter_cb, (void *)ep);
}

static int acm_assign_ep_names(struct acm_ep *ep)
{
	FILE *faddr;
	char *dev_name;
	char s[120];
	char dev[32], addr[INET6_ADDRSTRLEN], pkey_str[8];
	uint16_t pkey;
	uint8_t type;
	int port, ret = 0;
	struct in6_addr ip_addr;
	size_t addr_len;

	dev_name = ep->port->dev->verbs->device->name;
	acm_log(1, "device %s, port %d, pkey 0x%x\n",
		dev_name, ep->port->port_num, ep->pkey);

	acm_get_system_ips(ep);

	if (!(faddr = acm_open_addr_file())) {
		acm_log(0, "ERROR - address file not found\n");
		return -1;
	}

	while (fgets(s, sizeof s, faddr)) {
		if (s[0] == '#')
			continue;

		if (sscanf(s, "%46s%32s%d%8s", addr, dev, &port, pkey_str) != 4)
			continue;

		acm_log(2, "%s", s);
		if (inet_pton(AF_INET, addr, &ip_addr) > 0) {
			if (!support_ips_in_addr_cfg) {
				acm_log(0, "ERROR - IP's are not configured to be read from ibacm_addr.cfg\n");
				continue;
			}
			type = ACM_ADDRESS_IP;
			addr_len = 4;
		} else if (inet_pton(AF_INET6, addr, &ip_addr) > 0) {
			if (!support_ips_in_addr_cfg) {
				acm_log(0, "ERROR - IP's are not configured to be read from ibacm_addr.cfg\n");
				continue;
			}
			type = ACM_ADDRESS_IP6;
			addr_len = ACM_MAX_ADDRESS;
		} else {
			type = ACM_ADDRESS_NAME;
			addr_len = strlen(addr);
		}

		if (stricmp(pkey_str, "default")) {
			if (sscanf(pkey_str, "%hx", &pkey) != 1) {
				acm_log(0, "ERROR - bad pkey format %s\n", pkey_str);
				continue;
			}
		} else {
			pkey = 0xFFFF;
		}

		if (!stricmp(dev_name, dev) && (ep->port->port_num == (uint8_t) port) &&
			(ep->pkey == pkey)) {

			acm_log(1, "assigning %s\n", addr);
			if ((ret = acm_ep_insert_addr(ep, (uint8_t *)&ip_addr, addr_len, type)) != 0) {
				acm_log(1, "maximum number of names assigned to EP\n");
				break;
			}
		}
	}
	fclose(faddr);

	return ret;
}

/*
 * We currently require that the routing data be preloaded in order to
 * load the address data.  This is backwards from normal operation, which
 * usually resolves the address before the route.
 */
static void acm_ep_preload(struct acm_ep *ep)
{
	switch (route_preload) {
	case ACM_ROUTE_PRELOAD_OSM_FULL_V1:
		if (acm_parse_osm_fullv1(ep))
			acm_log(0, "ERROR - failed to preload EP\n");
		break;
	default:
		break;
	}

	switch (addr_preload) {
	case ACM_ADDR_PRELOAD_HOSTS:
		acm_parse_hosts_file(ep);
		break;
	default:
		break;
	}
}

static int acm_init_ep_loopback(struct acm_ep *ep)
{
	struct acm_dest *dest;
	int i;

	acm_log(2, "\n");
	if (loopback_prot != ACM_LOOPBACK_PROT_LOCAL)
		return 0;

	for (i = 0; i < MAX_EP_ADDR && ep->addr_type[i]; i++) {
		dest = acm_acquire_dest(ep, ep->addr_type[i], ep->addr[i].addr);
		if (!dest) {
			acm_format_name(0, log_data, sizeof log_data,
					ep->addr_type[i], ep->addr[i].addr,
					sizeof ep->addr[i].addr);
			acm_log(0, "ERROR - unable to create loopback dest %s\n", log_data);
			return -1;
		}

		ibv_query_gid(ep->port->dev->verbs, ep->port->port_num,
			      0, &dest->path.sgid);

		dest->path.dgid = dest->path.sgid;
		dest->path.dlid = dest->path.slid = htons(ep->port->lid);
		dest->path.reversible_numpath = IBV_PATH_RECORD_REVERSIBLE;
		dest->path.pkey = htons(ep->pkey);
		dest->path.mtu = (uint8_t) ep->port->mtu;
		dest->path.rate = (uint8_t) ep->port->rate;

		dest->remote_qpn = ep->qp->qp_num;
		dest->addr_timeout = (uint64_t) ~0ULL;
		dest->route_timeout = (uint64_t) ~0ULL;
		dest->state = ACM_READY;
		acm_put_dest(dest);
		acm_log(1, "added loopback dest %s\n", dest->name);
	}
	return 0;
}

static struct acm_ep *acm_find_ep(struct acm_port *port, uint16_t pkey)
{
	struct acm_ep *ep, *res = NULL;
	DLIST_ENTRY *entry;

	acm_log(2, "pkey 0x%x\n", pkey);

	lock_acquire(&port->lock);
	for (entry = port->ep_list.Next; entry != &port->ep_list; entry = entry->Next) {
		ep = container_of(entry, struct acm_ep, entry);
		if (ep->pkey == pkey) {
			res = ep;
			break;
		}
	}
	lock_release(&port->lock);
	return res;
}

static struct acm_ep *
acm_alloc_ep(struct acm_port *port, uint16_t pkey, uint16_t pkey_index)
{
	struct acm_ep *ep;

	acm_log(1, "\n");
	ep = calloc(1, sizeof *ep);
	if (!ep)
		return NULL;

	ep->port = port;
	ep->pkey = pkey;
	ep->pkey_index = pkey_index;
	ep->resolve_queue.credits = resolve_depth;
	ep->sa_queue.credits = sa_depth;
	ep->resp_queue.credits = send_depth;
	DListInit(&ep->resolve_queue.pending);
	DListInit(&ep->sa_queue.pending);
	DListInit(&ep->resp_queue.pending);
	DListInit(&ep->active_queue);
	DListInit(&ep->wait_queue);
	lock_init(&ep->lock);
	sprintf(ep->id_string, "%s-%d-0x%x", port->dev->verbs->device->name,
		port->port_num, pkey);

	return ep;
}

static void acm_ep_up(struct acm_port *port, uint16_t pkey_index)
{
	struct acm_ep *ep;
	struct ibv_qp_init_attr init_attr;
	struct ibv_qp_attr attr;
	int ret, sq_size;
	uint16_t pkey;

	acm_log(1, "\n");
	ret = ibv_query_pkey(port->dev->verbs, port->port_num, pkey_index, &pkey);
	if (ret)
		return;

	pkey = ntohs(pkey);	/* ibv_query_pkey returns pkey in network order */
	if (acm_find_ep(port, pkey)) {
		acm_log(2, "endpoint for pkey 0x%x already exists\n", pkey);
		return;
	}

	acm_log(2, "creating endpoint for pkey 0x%x\n", pkey);
	ep = acm_alloc_ep(port, pkey, pkey_index);
	if (!ep)
		return;

	ret = acm_assign_ep_names(ep);
	if (ret) {
		acm_log(0, "ERROR - unable to assign EP name for pkey 0x%x\n", pkey);
		goto err0;
	}

	sq_size = resolve_depth + sa_depth + send_depth;
	ep->cq = ibv_create_cq(port->dev->verbs, sq_size + recv_depth,
		ep, port->dev->channel, 0);
	if (!ep->cq) {
		acm_log(0, "ERROR - failed to create CQ\n");
		goto err0;
	}

	ret = ibv_req_notify_cq(ep->cq, 0);
	if (ret) {
		acm_log(0, "ERROR - failed to arm CQ\n");
		goto err1;
	}

	memset(&init_attr, 0, sizeof init_attr);
	init_attr.cap.max_send_wr = sq_size;
	init_attr.cap.max_recv_wr = recv_depth;
	init_attr.cap.max_send_sge = 1;
	init_attr.cap.max_recv_sge = 1;
	init_attr.qp_context = ep;
	init_attr.sq_sig_all = 1;
	init_attr.qp_type = IBV_QPT_UD;
	init_attr.send_cq = ep->cq;
	init_attr.recv_cq = ep->cq;
	ep->qp = ibv_create_qp(ep->port->dev->pd, &init_attr);
	if (!ep->qp) {
		acm_log(0, "ERROR - failed to create QP\n");
		goto err1;
	}

	attr.qp_state = IBV_QPS_INIT;
	attr.port_num = port->port_num;
	attr.pkey_index = pkey_index;
	attr.qkey = ACM_QKEY;
	ret = ibv_modify_qp(ep->qp, &attr, IBV_QP_STATE | IBV_QP_PKEY_INDEX |
		IBV_QP_PORT | IBV_QP_QKEY);
	if (ret) {
		acm_log(0, "ERROR - failed to modify QP to init\n");
		goto err2;
	}

	attr.qp_state = IBV_QPS_RTR;
	ret = ibv_modify_qp(ep->qp, &attr, IBV_QP_STATE);
	if (ret) {
		acm_log(0, "ERROR - failed to modify QP to rtr\n");
		goto err2;
	}

	attr.qp_state = IBV_QPS_RTS;
	attr.sq_psn = 0;
	ret = ibv_modify_qp(ep->qp, &attr, IBV_QP_STATE | IBV_QP_SQ_PSN);
	if (ret) {
		acm_log(0, "ERROR - failed to modify QP to rts\n");
		goto err2;
	}

	ret = acm_post_recvs(ep);
	if (ret)
		goto err2;

	ret = acm_init_ep_loopback(ep);
	if (ret) {
		acm_log(0, "ERROR - unable to init loopback\n");
		goto err2;
	}
	lock_acquire(&port->lock);
	DListInsertHead(&ep->entry, &port->ep_list);
	lock_release(&port->lock);
	acm_ep_preload(ep);
	return;

err2:
	ibv_destroy_qp(ep->qp);
err1:
	ibv_destroy_cq(ep->cq);
err0:
	free(ep);
}

static void acm_port_up(struct acm_port *port)
{
	struct ibv_port_attr attr;
	union ibv_gid gid;
	uint16_t pkey;
	int i, ret;
	int is_full_default_pkey_set = 0;

	acm_log(1, "%s %d\n", port->dev->verbs->device->name, port->port_num);
	ret = ibv_query_port(port->dev->verbs, port->port_num, &attr);
	if (ret) {
		acm_log(0, "ERROR - unable to get port state\n");
		return;
	}
	if (attr.state != IBV_PORT_ACTIVE) {
		acm_log(1, "port not active\n");
		return;
	}

	port->mtu = attr.active_mtu;
	port->rate = acm_get_rate(attr.active_width, attr.active_speed);
	if (attr.subnet_timeout >= 8)
		port->subnet_timeout = 1 << (attr.subnet_timeout - 8);
	for (port->gid_cnt = 0;; port->gid_cnt++) {
		ret = ibv_query_gid(port->dev->verbs, port->port_num, port->gid_cnt, &gid);
		if (ret || !gid.global.interface_id)
			break;

		if (port->gid_cnt == 0)
			port->base_gid = gid;
	}

	port->lid = attr.lid;
	port->lid_mask = 0xffff - ((1 << attr.lmc) - 1);

	port->sa_dest.av.src_path_bits = 0;
	port->sa_dest.av.dlid = attr.sm_lid;
	port->sa_dest.av.sl = attr.sm_sl;
	port->sa_dest.av.port_num = port->port_num;
	port->sa_dest.remote_qpn = 1;
	attr.sm_lid = htons(attr.sm_lid);
	acm_set_dest_addr(&port->sa_dest, ACM_ADDRESS_LID,
		(uint8_t *) &attr.sm_lid, sizeof(attr.sm_lid));

	port->sa_dest.ah = ibv_create_ah(port->dev->pd, &port->sa_dest.av);
	if (!port->sa_dest.ah)
		return;

	atomic_set(&port->sa_dest.refcnt, 1);
	for (i = 0; i < attr.pkey_tbl_len; i++) {
		ret = ibv_query_pkey(port->dev->verbs, port->port_num, i, &pkey);
		if (ret)
			continue;
		pkey = ntohs(pkey);
		if (!(pkey & 0x7fff))
			continue;

		/* Determine pkey index for default partition with preference for full membership */
		if (!is_full_default_pkey_set && (pkey & 0x7fff) == 0x7fff) {
			port->default_pkey_ix = i;
			if (pkey & 0x8000)
				is_full_default_pkey_set = 1;
		}
		acm_ep_up(port, (uint16_t) i);
	}

	acm_port_join(port);
	port->state = IBV_PORT_ACTIVE;
	acm_log(1, "%s %d is up\n", port->dev->verbs->device->name, port->port_num);
}

static void acm_port_down(struct acm_port *port)
{
	struct ibv_port_attr attr;
	int ret;

	acm_log(1, "%s %d\n", port->dev->verbs->device->name, port->port_num);
	ret = ibv_query_port(port->dev->verbs, port->port_num, &attr);
	if (!ret && attr.state == IBV_PORT_ACTIVE) {
		acm_log(1, "port active\n");
		return;
	}

	port->state = attr.state;

	/*
	 * We wait for the SA destination to be released.  We could use an
	 * event instead of a sleep loop, but it's not worth it given how
	 * infrequently we should be processing a port down event in practice.
	 */
	atomic_dec(&port->sa_dest.refcnt);
	while (atomic_get(&port->sa_dest.refcnt))
		sleep(0);
	ibv_destroy_ah(port->sa_dest.ah);
	acm_log(1, "%s %d is down\n", port->dev->verbs->device->name, port->port_num);
}

/*
 * There is one event handler thread per device.  This is the only thread that
 * modifies the port state or a port endpoint list.  Other threads which access
 * those must synchronize against changes accordingly, but this thread only
 * needs to lock when making modifications.
 */
static void CDECL_FUNC acm_event_handler(void *context)
{
	struct acm_device *dev = (struct acm_device *) context;
	struct ibv_async_event event;
	int i, ret;

	acm_log(1, "started\n");
	for (i = 0; i < dev->port_cnt; i++) {
		acm_port_up(&dev->port[i]);
	}

	for (;;) {
		ret = ibv_get_async_event(dev->verbs, &event);
		if (ret)
			continue;

		acm_log(2, "processing async event %s\n",
			ibv_event_type_str(event.event_type));
		i = event.element.port_num - 1;
		switch (event.event_type) {
		case IBV_EVENT_PORT_ACTIVE:
			if (dev->port[i].state != IBV_PORT_ACTIVE)
				acm_port_up(&dev->port[i]);
			break;
		case IBV_EVENT_PORT_ERR:
			if (dev->port[i].state == IBV_PORT_ACTIVE)
				acm_port_down(&dev->port[i]);
			break;
		case IBV_EVENT_CLIENT_REREGISTER:
			if (dev->port[i].state == IBV_PORT_ACTIVE) {
				acm_port_join(&dev->port[i]);
				acm_log(1, "%s %d has reregistered\n",
					dev->verbs->device->name, i + 1);
			}
			break;
		default:
			break;
		}

		ibv_ack_async_event(&event);
	}
}

static void acm_activate_devices()
{
	struct acm_device *dev;
	DLIST_ENTRY *dev_entry;

	acm_log(1, "\n");
	for (dev_entry = dev_list.Next; dev_entry != &dev_list;
		dev_entry = dev_entry->Next) {

		dev = container_of(dev_entry, struct acm_device, entry);
		beginthread(acm_event_handler, dev);
		beginthread(acm_comp_handler, dev);
	}
}

static void acm_open_port(struct acm_port *port, struct acm_device *dev, uint8_t port_num)
{
	acm_log(1, "%s %d\n", dev->verbs->device->name, port_num);
	port->dev = dev;
	port->port_num = port_num;
	lock_init(&port->lock);
	DListInit(&port->ep_list);
	acm_init_dest(&port->sa_dest, ACM_ADDRESS_LID, NULL, 0);

	port->mad_portid = umad_open_port(dev->verbs->device->name, port->port_num);
	if (port->mad_portid < 0) {
		acm_log(0, "ERROR - unable to open MAD port\n");
		return;
	}

	port->mad_agentid = umad_register(port->mad_portid,
		IB_MGMT_CLASS_SA, 1, 1, NULL);
	if (port->mad_agentid < 0) {
		acm_log(0, "ERROR - unable to register MAD client\n");
		goto err;
	}

	port->state = IBV_PORT_DOWN;
	return;
err:
	umad_close_port(port->mad_portid);
}

static void acm_open_dev(struct ibv_device *ibdev)
{
	struct acm_device *dev;
	struct ibv_device_attr attr;
	struct ibv_context *verbs;
	size_t size;
	int i, ret;

	acm_log(1, "%s\n", ibdev->name);
	verbs = ibv_open_device(ibdev);
	if (verbs == NULL) {
		acm_log(0, "ERROR - opening device %s\n", ibdev->name);
		return;
	}

	ret = ibv_query_device(verbs, &attr);
	if (ret) {
		acm_log(0, "ERROR - ibv_query_device (%s) %d\n", ret, ibdev->name);
		goto err1;
	}

	size = sizeof(*dev) + sizeof(struct acm_port) * attr.phys_port_cnt;
	dev = (struct acm_device *) calloc(1, size);
	if (!dev)
		goto err1;

	dev->verbs = verbs;
	dev->guid = ibv_get_device_guid(ibdev);
	dev->port_cnt = attr.phys_port_cnt;

	dev->pd = ibv_alloc_pd(dev->verbs);
	if (!dev->pd) {
		acm_log(0, "ERROR - unable to allocate PD\n");
		goto err2;
	}

	dev->channel = ibv_create_comp_channel(dev->verbs);
	if (!dev->channel) {
		acm_log(0, "ERROR - unable to create comp channel\n");
		goto err3;
	}

	for (i = 0; i < dev->port_cnt; i++)
		acm_open_port(&dev->port[i], dev, i + 1);

	DListInsertHead(&dev->entry, &dev_list);

	acm_log(1, "%s opened\n", ibdev->name);
	return;

err3:
	ibv_dealloc_pd(dev->pd);
err2:
	free(dev);
err1:
	ibv_close_device(verbs);
}

static int acm_open_devices(void)
{
	struct ibv_device **ibdev;
	int dev_cnt;
	int i;

	acm_log(1, "\n");
	ibdev = ibv_get_device_list(&dev_cnt);
	if (!ibdev) {
		acm_log(0, "ERROR - unable to get device list\n");
		return -1;
	}

	for (i = 0; i < dev_cnt; i++)
		acm_open_dev(ibdev[i]);

	ibv_free_device_list(ibdev);
	if (DListEmpty(&dev_list)) {
		acm_log(0, "ERROR - no devices\n");
		return -1;
	}

	return 0;
}

static void acm_set_options(void)
{
	FILE *f;
	char s[120];
	char opt[32], value[256];

	if (!(f = fopen(opts_file, "r")))
		return;

	while (fgets(s, sizeof s, f)) {
		if (s[0] == '#')
			continue;

		if (sscanf(s, "%32s%256s", opt, value) != 2)
			continue;

		if (!stricmp("log_file", opt))
			strcpy(log_file, value);
		else if (!stricmp("log_level", opt))
			log_level = atoi(value);
		else if (!stricmp("lock_file", opt))
			strcpy(lock_file, value);
		else if (!stricmp("addr_prot", opt))
			addr_prot = acm_convert_addr_prot(value);
		else if (!stricmp("addr_timeout", opt))
			addr_timeout = atoi(value);
		else if (!stricmp("route_prot", opt))
			route_prot = acm_convert_route_prot(value);
		else if (!strcmp("route_timeout", opt))
			route_timeout = atoi(value);
		else if (!stricmp("loopback_prot", opt))
			loopback_prot = acm_convert_loopback_prot(value);
		else if (!stricmp("server_port", opt))
			server_port = (short) atoi(value);
		else if (!stricmp("timeout", opt))
			timeout = atoi(value);
		else if (!stricmp("retries", opt))
			retries = atoi(value);
		else if (!stricmp("resolve_depth", opt))
			resolve_depth = atoi(value);
		else if (!stricmp("sa_depth", opt))
			sa_depth = atoi(value);
		else if (!stricmp("send_depth", opt))
			send_depth = atoi(value);
		else if (!stricmp("recv_depth", opt))
			recv_depth = atoi(value);
		else if (!stricmp("min_mtu", opt))
			min_mtu = acm_convert_mtu(atoi(value));
		else if (!stricmp("min_rate", opt))
			min_rate = acm_convert_rate(atoi(value));
		else if (!stricmp("route_preload", opt))
			route_preload = acm_convert_route_preload(value);
		else if (!stricmp("route_data_file", opt))
			strcpy(route_data_file, value);
		else if (!stricmp("addr_preload", opt))
			addr_preload = acm_convert_addr_preload(value);
		else if (!stricmp("addr_data_file", opt))
			strcpy(addr_data_file, value);
		else if (!stricmp("support_ips_in_addr_cfg", opt))
			support_ips_in_addr_cfg = atoi(value);
	}

	fclose(f);
}

static void acm_log_options(void)
{
	acm_log(0, "log level %d\n", log_level);
	acm_log(0, "lock file %s\n", lock_file);
	acm_log(0, "address resolution %d\n", addr_prot);
	acm_log(0, "address timeout %d\n", addr_timeout);
	acm_log(0, "route resolution %d\n", route_prot);
	acm_log(0, "route timeout %d\n", route_timeout);
	acm_log(0, "loopback resolution %d\n", loopback_prot);
	acm_log(0, "server_port %d\n", server_port);
	acm_log(0, "timeout %d ms\n", timeout);
	acm_log(0, "retries %d\n", retries);
	acm_log(0, "resolve depth %d\n", resolve_depth);
	acm_log(0, "sa depth %d\n", sa_depth);
	acm_log(0, "send depth %d\n", send_depth);
	acm_log(0, "receive depth %d\n", recv_depth);
	acm_log(0, "minimum mtu %d\n", min_mtu);
	acm_log(0, "minimum rate %d\n", min_rate);
	acm_log(0, "route preload %d\n", route_preload);
	acm_log(0, "route data file %s\n", route_data_file);
	acm_log(0, "address preload %d\n", addr_preload);
	acm_log(0, "address data file %s\n", addr_data_file);
	acm_log(0, "support IP's in ibacm_addr.cfg %d\n", support_ips_in_addr_cfg);
}

static FILE *acm_open_log(void)
{
	FILE *f;

	if (!stricmp(log_file, "stdout"))
		return stdout;

	if (!stricmp(log_file, "stderr"))
		return stderr;

	if (!(f = fopen(log_file, "w")))
		f = stdout;

	return f;
}

static int acm_open_lock_file(void)
{
	int lock_fd;
	char pid[16];

	lock_fd = open(lock_file, O_RDWR | O_CREAT, 0640);
	if (lock_fd < 0)
		return lock_fd;

	if (lockf(lock_fd, F_TLOCK, 0)) {
		close(lock_fd);
		return -1;
	}

	snprintf(pid, sizeof pid, "%d\n", getpid());
	write(lock_fd, pid, strlen(pid));
	return 0;
}

static void daemonize(void)
{
	pid_t pid, sid;

	pid = fork();
	if (pid)
		exit(pid < 0);

	sid = setsid();
	if (sid < 0)
		exit(1);

	if (chdir("/"))
		exit(1);

	freopen("/dev/null", "r", stdin);
	freopen("/dev/null", "w", stdout);
	freopen("/dev/null", "w", stderr);
}

static void show_usage(char *program)
{
	printf("usage: %s\n", program);
	printf("   [-D]             - run as a daemon (default)\n");
	printf("   [-P]             - run as a standard process\n");
	printf("   [-A addr_file]   - address configuration file\n");
	printf("                      (default %s/%s)\n", ACM_CONF_DIR, ACM_ADDR_FILE);
	printf("   [-O option_file] - option configuration file\n");
	printf("                      (default %s/%s)\n", ACM_CONF_DIR, ACM_OPTS_FILE);
}

int CDECL_FUNC main(int argc, char **argv)
{
	int i, op, daemon = 1;

	while ((op = getopt(argc, argv, "DPA:O:")) != -1) {
		switch (op) {
		case 'D':
			/* option no longer required */
			break;
		case 'P':
			daemon = 0;
			break;
		case 'A':
			addr_file = optarg;
			break;
		case 'O':
			opts_file = optarg;
			break;
		default:
			show_usage(argv[0]);
			exit(1);
		}
	}

	if (daemon)
		daemonize();

	if (osd_init())
		return -1;

	acm_set_options();
	if (acm_open_lock_file())
		return -1;

	lock_init(&log_lock);
	flog = acm_open_log();

	acm_log(0, "Assistant to the InfiniBand Communication Manager\n");
	acm_log_options();

	atomic_init(&tid);
	atomic_init(&wait_cnt);
	DListInit(&dev_list);
	DListInit(&timeout_list);
	event_init(&timeout_event);
	for (i = 0; i < ACM_MAX_COUNTER; i++)
		atomic_init(&counter[i]);

	umad_init();
	if (acm_open_devices()) {
		acm_log(0, "ERROR - unable to open any devices\n");
		return -1;
	}

	acm_log(1, "creating IP Netlink socket\n");
	acm_ipnl_create();

	acm_activate_devices();
	acm_log(1, "starting timeout/retry thread\n");
	beginthread(acm_retry_handler, NULL);
	acm_log(1, "starting server\n");
	acm_server();

	acm_log(0, "shutting down\n");
	fclose(flog);
	return 0;
}
