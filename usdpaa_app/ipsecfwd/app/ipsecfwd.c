/*
 * Copyright (C) 2011 - 2012 Freescale Semiconductor, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ipsecfwd.h"

#include "ppam_if.h"
#include <ppac_interface.h>

#include "net/annotations.h"
#include "ethernet/eth.h"
#include "ipsecfwd_stack.h"
#include "ipsecfwd_splitkey.h"
#include "ip/ip_handler.h"
#include "ip/ip_appconf.h"
#include "ipsec/ipsec_sec.h"

#include <mqueue.h>

static struct bman_pool *sec_bpool;
u32 sec_bpid;
bool simple_fd_mode;

int32_t g_tunnel_id;
struct ipsec_tunnel_config_t *g_ipsec_tunnel_config;

struct ipsec_stack_t ipsec_stack;
static mqd_t mq_fd_rcv = -1, mq_fd_snd = -1;
static struct sigevent notification;

int is_iface_ip(in_addr_t addr)
{
	const struct ppac_interface *i;

	list_for_each_entry(i, &ifs, node)
		if (i->ppam_data.addr == addr)
			return 0;

	return -ENXIO;
}

struct ppac_interface *ipfwd_get_iface_for_ip(in_addr_t addr)
{
	struct ppac_interface *i;

	list_for_each_entry(i, &ifs, node)
		if ((i->ppam_data.addr & i->ppam_data.mask) ==
		    (addr & i->ppam_data.mask))
			return i;

	return NULL;
}

/**
 \brief Adds a new Security association entry
 \param[out] lwe_ctrl_op_info contains SA parameters
 \return Integer status
 */
int32_t ipsecfwd_add_sa(struct app_ctrl_op_info *sa_info)
{
	return ipsecfwd_create_sa(&sa_info->ipsec_info, &ipsec_stack);
}

/**
 \brief Deletes a Security Association
 \param[in] lwe_ctrl_op_info contains SA parameters
 \return Integer status
*/
int32_t ipsecfwd_del_sa(struct app_ctrl_op_info *sa_info)
{
	pr_err("Not implemented yet\n");
	return -1;
}

/**
 \brief Adds a new Route Cache entry
 \param[out] app_ctrl_route_info contains Route parameters
 \return Integer status
 */
static int ipfwd_add_route(const struct app_ctrl_op_info *route_info)
{
	struct rc_entry_t *entry;
	struct rt_dest_t *dest;
	struct ppac_interface *dev = NULL;
	in_addr_t gw_ipaddr = route_info->ip_info.gw_ipaddr;
	int _errno;

	pr_debug("ipfwd_add_route: Enter\n");

	dest = rt_dest_alloc(&ipsec_stack.ip_stack.rt);
	if (dest == NULL) {
		pr_err("Could not allocate route cache related data structure\n");
		return -1;
	}

	dest->next = NULL;
	dest->neighbor = neigh_lookup(&ipsec_stack.ip_stack.arp_table,
				gw_ipaddr,
				ipsec_stack.ip_stack.arp_table.proto_len);
	if (dest->neighbor == NULL) {
		pr_debug
		    ("%s: Could not find neighbor entry for link-local addr\n",
		     __func__);

		dev = ipfwd_get_iface_for_ip(gw_ipaddr);
		if (dev == NULL) {
			pr_err("%s: not a valid gateway for any subnet\n",
				  __func__);
			return -1;
		}

		dest->neighbor = neigh_create(&ipsec_stack.ip_stack.arp_table);
		if (unlikely(!dest->neighbor)) {
			pr_err("%s: Unable to create Neigh Entry\n", __func__);
			return -1;
		}

		if (NULL == neigh_init(&ipsec_stack.ip_stack.arp_table,
				dest->neighbor, dev, &gw_ipaddr)) {
			pr_err("%s: Unable to init Neigh Entry\n", __func__);
			return -1;
		}

		if (false == neigh_add(&ipsec_stack.ip_stack.arp_table,
				dest->neighbor)) {
			pr_err("%s: Unable to add Neigh Entry\n", __func__);
			return -1;
		}
		/* MAC addr would be updated later through ARP request */

		pr_debug("%s: Created neighbor entry, IP addr = %x\n",
			  __func__, gw_ipaddr);
	}

	dest->dev = dest->neighbor->dev;
	dest->scope = ROUTE_SCOPE_GLOBAL;

	entry = rc_create_entry(ipsec_stack.ip_stack.rc);
	if (entry == NULL) {
		pr_err("Could not allocate route cache entry\n");
		rt_dest_free(&ipsec_stack.ip_stack.rt, dest);
		return -1;
	}

	entry->saddr = route_info->ip_info.src_ipaddr;
	entry->daddr = route_info->ip_info.dst_ipaddr;
#ifdef STATS_TBD
	_errno = posix_memalign((void **)&entry->stats, L1_CACHE_BYTES,
			   sizeof(struct rc_entry_statistics_t));
	if (unlikely(_errno < 0)) {
		pr_err("Unable to allocate route entry stats\n");
		return _errno;
	}
	memset(entry->stats, 0, sizeof(struct rc_entry_statistics_t));
#endif
	refcount_acquire(dest->neighbor->refcnt);

	entry->dest = dest;

	if (rc_add_update_entry(ipsec_stack.ip_stack.rc, entry) == false) {
		pr_err("Route cache entry updated\n");
		rc_free_entry(ipsec_stack.ip_stack.rc, entry);
	}

	pr_debug("ipfwd_add_route: Exit\n");
	return 0;
}

/**
 \brief Deletes a Route Cache entry
 \param[out] app_ctrl_route_info contains Route parameters
 \return Integer status
 */
static int ipfwd_del_route(const struct app_ctrl_op_info *route_info)
{
	struct rt_dest_t *dest;
	pr_debug("ipfwd_del_route: Enter\n");

	dest = rc_lookup(ipsec_stack.ip_stack.rc,
			 route_info->ip_info.src_ipaddr,
			 route_info->ip_info.dst_ipaddr);
	if (dest == NULL) {
		pr_err("Could not find route cache entry to be deleted\n");
		return -1;
	}

	refcount_release(dest->neighbor->refcnt);

	if (rc_remove_entry(ipsec_stack.ip_stack.rc,
			    route_info->ip_info.src_ipaddr,
			    route_info->ip_info.dst_ipaddr) == false) {
		pr_err("Could not delete route cache entry\n");
		return -1;
	}

	rt_dest_free(&ipsec_stack.ip_stack.rt, dest);
	pr_debug("ipfwd_del_route: Exit\n");
	return 0;
}

/**
 \brief Adds a new Arp Cache entry
 \param[out] app_ctrl_route_info contains ARP parameters
 \return Integer status
 */
static int ipfwd_add_arp(const struct app_ctrl_op_info *route_info)
{
	in_addr_t ip_addr = route_info->ip_info.src_ipaddr;
	struct ppac_interface *dev = NULL;
	struct neigh_t *n;

#if (LOG_LEVEL > 3)
	uint8_t *ip = (typeof(ip))&ip_addr;
	pr_debug("ipfwd_add_arp: Enter\n");

	pr_debug("IP = %d.%d.%d.%d ; MAC ="ETH_MAC_PRINTF_FMT"\n",
		 ip[0], ip[1], ip[2], ip[3],
		 ETH_MAC_PRINTF_ARGS(&route_info->ip_info.mac_addr));
#endif

	n = neigh_lookup(&ipsec_stack.ip_stack.arp_table, ip_addr,
			 ipsec_stack.ip_stack.arp_table.proto_len);

	if (n == NULL) {
		pr_debug
		    ("%s: Could not find neighbor entry for link-local addr\n",
		     __func__);

		dev = ipfwd_get_iface_for_ip(ip_addr);
		if (dev == NULL) {
			pr_debug("ipfwd_add_arp: Exit: Failed\n");
			return -1;
		}

		n = neigh_create(&ipsec_stack.ip_stack.arp_table);
		if (unlikely(!n)) {
			pr_debug("ipfwd_add_arp: Exit: Failed\n");
			return -1;
		}
		if (NULL == neigh_init(&ipsec_stack.ip_stack.arp_table,
				n, dev, &ip_addr)) {
			pr_err("ipfwd_add_arp: Exit: Failed\n");
			return -1;
		}

		if (false == neigh_add(&ipsec_stack.ip_stack.arp_table, n)) {
			pr_err("ipfwd_add_arp: Exit: Failed\n");
			return -1;
		}
	} else {
		n->neigh_state = NEIGH_STATE_UNKNOWN;
		if (route_info->ip_info.replace_entry) {
			if (false == neigh_replace(
					&ipsec_stack.ip_stack.arp_table, n)) {
				pr_err("ipfwd_add_arp: Exit: Failed\n");
				return -1;
			}
		}
	}
	/* Update ARP cache entry */
	if (NULL == neigh_update(n,
			route_info->ip_info.mac_addr.ether_addr_octet,
			NEIGH_STATE_PERMANENT)) {
		pr_err("ipfwd_add_arp: Exit: Failed\n");
		return -1;
	}

	pr_debug("ipfwd_add_arp: Exit\n");
	return 0;
}

/**
 \brief Deletes an Arp Cache entry
 \param[out] app_ctrl_route_info contains ARP parameters
 \return Integer status
 */
static int ipfwd_del_arp(const struct app_ctrl_op_info *route_info)
{
	struct neigh_t *neighbor = NULL;
	pr_debug("ipfwd_del_arp: Enter\n");

	/*
	 ** Do a Neighbour LookUp for the entry to be deleted
	 */
	neighbor = neigh_lookup(&ipsec_stack.ip_stack.arp_table,
				route_info->ip_info.src_ipaddr,
				ipsec_stack.ip_stack.arp_table.proto_len);
	if (neighbor == NULL) {
		pr_err
		    ("Could not find neighbor entry for link-local address\n");
		return -1;
	}

	/*
	 ** Find out if anyone is using this entry
	 */
	if (*(neighbor->refcnt) != 0) {
		pr_err
		    ("Could not delete neighbor entry as it is being used\n");
		return -1;
	}

	/*
	 ** Delete the ARP Entry
	 */
	if (false == neigh_remove(&ipsec_stack.ip_stack.arp_table,
				  route_info->ip_info.src_ipaddr,
				  ipsec_stack.ip_stack.arp_table.proto_len)) {
		pr_err("Could not delete neighbor entry\n");
		return -1;
	}

	pr_debug("ipfwd_del_arp: Exit\n");
	return 0;
}

/**
 \brief Show Interfaces
 \param[out] app_ctrl_route_info contains intf parameters
 \return Integer status
 */
static int ipfwd_show_intf(const struct app_ctrl_op_info *route_info)
{
	const struct fman_if *fif;
	const struct fm_eth_port_cfg *port;
	int i, iface, loop;

	for (loop = 0; loop < netcfg->num_ethports; loop++) {
		port = &netcfg->port_cfg[loop];
		fif = port->fman_if;
		iface = (fif->mac_type == fman_mac_1g ? 0 : 8) + fif->mac_idx;
		i = fif->fman_idx * 10 + iface;
		pr_info("Interface number: %d\n"
			"PortID=%d:%d is FMan interface node\n"
			"with MAC Address\n"ETH_MAC_PRINTF_FMT"\n",
			i, fif->fman_idx, iface,
			ETH_MAC_PRINTF_ARGS(&fif->mac_addr));
	}
	return 0;
}

/**
 \brief Change Interface Configuration
 \param[out] app_ctrl_route_info contains intf config parameters
 \return Integer status
 */
static int ipfwd_conf_intf(const struct app_ctrl_op_info *route_info)
{
	struct ppac_interface *i;
	struct ppam_interface *p;
	uint16_t addr_hi;
	int _errno = 1, node, ifnum;

	pr_debug("%s: Enter\n", __func__);

	addr_hi = ETHERNET_ADDR_MAGIC;
	ifnum = route_info->ip_info.intf_conf.ifnum;
	list_for_each_entry(i, &ifs, node) {
		p = &i->ppam_data;
		if (p->ifnum == ifnum) {
			p->addr = route_info->ip_info.intf_conf.ip_addr;
			pr_debug("IPADDR assigned = 0x%x to interface num %d\n",
				p->addr, p->ifnum);
			for (node = 0; node < ARRAY_SIZE(p->local_nodes);
				 node++) {
				p->local_nodes[node].ip = p->addr + 1 + node;
				memcpy(&p->local_nodes[node].mac, &addr_hi,
					sizeof(addr_hi));
				memcpy(p->local_nodes[node].mac.ether_addr_octet
					+ sizeof(addr_hi),
					&p->local_nodes[node].ip,
					sizeof(p->local_nodes[node].ip));
			}
			_errno = 0;
		}
	}
	if (_errno)
		pr_info("Interface number %d is not an enabled interface\n",
			 ifnum);

	pr_debug("%s: Exit\n", __func__);
	return _errno;
}

/**
 \brief Initialize IPSec Statistics
 \param[in] void
 \param[out] struct ip_statistics_t *
 */
static struct ip_statistics_t *ipfwd_stats_init(void)
{
	int _errno;
	void *ip_stats;

	_errno = posix_memalign(&ip_stats,
				__alignof__(struct ip_statistics_t),
				sizeof(struct ip_statistics_t));
	return unlikely(_errno < 0) ? NULL : ip_stats;
}

/**
 \brief Initialize IP Stack
 \param[in] struct ipsec_stack_t * IPSecfwd Stack pointer
 \param[out] Return Status
 */
static int initialize_ip_stack(struct ip_stack_t *ip_stack)
{
	int _errno;

	_errno = arp_table_init(&ip_stack->arp_table);
	if (unlikely(_errno < 0)) {
		pr_err("Failed to create ARP Table\n");
		return _errno;
	}
	_errno = neigh_table_init(&ip_stack->arp_table);
	if (unlikely(_errno < 0)) {
		pr_err("Failed to init ARP Table\n");
		return _errno;
	}
	_errno = rt_init(&ip_stack->rt);
	if (unlikely(_errno < 0)) {
		pr_err("Failed in Route table initialized\n");
		return _errno;
	}
	ip_stack->rc = rc_init(IP_RC_EXPIRE_JIFFIES, sizeof(in_addr_t));
	if (unlikely(ip_stack->rc == NULL)) {
		pr_err("Unable to allocate rc structure for stack\n");
		return -ENOMEM;
	}
	_errno = ip_hooks_init(&ip_stack->hooks);
	if (unlikely(_errno < 0)) {
		pr_err("Failed in IP Stack hooks initialized\n");
		return _errno;
	}
	_errno = ip_protos_init(&ip_stack->protos);
	if (unlikely(_errno < 0)) {
		pr_err("IP Stack L4 Protocols initialized\n");
		return _errno;
	}

	ip_stack->ip_stats = ipfwd_stats_init();
	if (unlikely(ip_stack->ip_stats == NULL)) {
		pr_err("Unable to allocate ip stats structure for stack\n");
		return -ENOMEM;
	}
	memset(ip_stack->ip_stats, 0, sizeof(struct ip_statistics_t));

	pr_debug("IP Statistics initialized\n");
	return 0;
}

/*
 \brief Peforms all global initialization required by IPsec
 \param[in] NULL
 \param[out] NULL
 */
int initialize_ipsec_stack(void)
{
	int _errno;

	/* Initializes IP stack*/
	_errno = initialize_ip_stack(&ipsec_stack.ip_stack);
	if (unlikely(_errno < 0)) {
		pr_err("Error Initializing IP Stack\n");
		return _errno;
	}

	_errno = ipsec_tunnel_table_create(&(ipsec_stack.itt));
	if (unlikely(_errno < 0)) {
		pr_err("Unable to create ipsec tunnel table\n");
		return _errno;
	}


#ifdef STATS_TBD
	/* initialization virtual netdev table */
	ipsec_stack.stats =
	    stats_memalign(CACHE_LINE_SIZE, sizeof(*ipsec_stack.stats));
	if (ipsec_stack.stats == NULL) {
		APP_ERROR("Unable to allocate ipsec stats");
		return;
	}
	memset(ipsec_stack.stats, 0, sizeof(*ipsec_stack.stats));
#endif
	return 0;
}

/**
 \brief Message handler for message coming from Control plane
 \param[in] app_ctrl_op_info contains SA parameters
 \return NULL
*/
static void process_req_from_mq(struct app_ctrl_op_info *sa_info)
{
	int32_t s32Result = 0;
	sa_info->result = IPC_CTRL_RSLT_FAILURE;

	pr_debug("process_req_from_mq: Enter\n");
	switch (sa_info->msg_type) {
	case IPC_CTRL_CMD_TYPE_SA_ADD:
		s32Result = ipsecfwd_add_sa(sa_info);
		break;

	case IPC_CTRL_CMD_TYPE_SA_DEL:
		s32Result = ipsecfwd_del_sa(sa_info);
		break;

	case IPC_CTRL_CMD_TYPE_ROUTE_ADD:
		s32Result = ipfwd_add_route(sa_info);
		break;

	case IPC_CTRL_CMD_TYPE_ROUTE_DEL:
		s32Result = ipfwd_del_route(sa_info);
		break;

	case IPC_CTRL_CMD_TYPE_ARP_ADD:
		s32Result = ipfwd_add_arp(sa_info);
		break;

	case IPC_CTRL_CMD_TYPE_ARP_DEL:
		s32Result = ipfwd_del_arp(sa_info);
		break;

	case IPC_CTRL_CMD_TYPE_INTF_CONF_CHNG:
		s32Result = ipfwd_conf_intf(sa_info);
		break;

	case IPC_CTRL_CMD_TYPE_SHOW_INTF:
		s32Result = ipfwd_show_intf(sa_info);
		break;

	default:
		break;
	}

	if (s32Result == 0)
		sa_info->result = IPC_CTRL_RSLT_SUCCESSFULL;
	else
		pr_err("%s: CP Request can't be handled\n", __func__);

	pr_debug("process_req_from_mq: Exit\n");
	return;
}

int receive_data(mqd_t mqdes)
{
	ssize_t size;
	struct app_ctrl_op_info *ip_info = NULL;
	struct mq_attr attr;
	int _err = 0;

	ip_info = (struct app_ctrl_op_info *)malloc
			(sizeof(struct app_ctrl_op_info));
	memset(ip_info, 0, sizeof(struct app_ctrl_op_info));

	_err = mq_getattr(mqdes, &attr);
	if (unlikely(_err)) {
		pr_err("%s: %dError getting MQ attributes\n",
			 __FILE__, __LINE__);
		goto error;
	}
	size = mq_receive(mqdes, (char *)ip_info, attr.mq_msgsize, 0);
	if (unlikely(size == -1)) {
		pr_err("%s: %dRcv msgque error\n", __FILE__, __LINE__);
		goto error;
	}
	process_req_from_mq(ip_info);
	/* Sending result to application configurator tool */
	_err = mq_send(mq_fd_snd, (const char *)ip_info,
			sizeof(struct app_ctrl_op_info), 10);
	if (unlikely(_err != 0)) {
		pr_err("%s: %d Error in sending msg on MQ\n",
			__FILE__, __LINE__);
		goto error;
	}

	return 0;
error:
	free(ip_info);
	return _err;
}

static void mq_handler(union sigval sval)
{
	pr_debug("mq_handler called %d\n", sval.sival_int);

	receive_data(mq_fd_rcv);
	mq_notify(mq_fd_rcv, &notification);
}

static int create_mq(void)
{
	struct mq_attr attr_snd, attr_rcv;
	int _err = 0, ret;
	char name[10];

	pr_debug("Create mq: Enter\n");
	memset(&attr_snd, 0, sizeof(attr_snd));

	/* Create message queue to send the response */
	attr_snd.mq_maxmsg = 10;
	attr_snd.mq_msgsize = 8192;
	sprintf(name, "/mq_snd_%d", getpid());
	printf("Message queue to send: %s\n", name);
	mq_fd_snd = mq_open(name, O_CREAT | O_WRONLY,
				(S_IRWXU | S_IRWXG | S_IRWXO), &attr_snd);
	if (mq_fd_snd == -1) {
		pr_err("%s: %dError opening SND MQ\n",
				__FILE__, __LINE__);
		_err = -errno;
		goto error;
	}

	memset(&attr_rcv, 0, sizeof(attr_rcv));

	/* Create message queue to read the message */
	attr_rcv.mq_maxmsg = 10;
	attr_rcv.mq_msgsize = 8192;
	sprintf(name, "/mq_rcv_%d", getpid());
	printf("Message queue to receive: %s\n", name);
	mq_fd_rcv = mq_open(name, O_CREAT | O_RDONLY,
			(S_IRWXU | S_IRWXG | S_IRWXO), &attr_rcv);
	if (mq_fd_rcv == -1) {
		pr_err("%s: %dError opening RCV MQ\n",
				 __FILE__, __LINE__);
		_err = -errno;
		goto error;
	}

	notification.sigev_notify = SIGEV_THREAD;
	notification.sigev_notify_function = mq_handler;
	notification.sigev_value.sival_ptr = &mq_fd_rcv;
	notification.sigev_notify_attributes = NULL;
	ret =  mq_notify(mq_fd_rcv, &notification);
	if (ret) {
		pr_err("%s: %dError in mq_notify call\n",
				 __FILE__, __LINE__);
		_err = -errno;
		goto error;
	}
	pr_debug("Create mq: Exit\n");
	return 0;
error:
	if (mq_fd_snd)
		mq_close(mq_fd_snd);

	if (mq_fd_rcv)
		mq_close(mq_fd_rcv);

	return _err;
}

static int init_sec_bpool(void)
{
#define SEC_BPOOL_BUF_COUNT	256
	struct bm_buffer bufs[8];

	struct bman_pool_params params = {
		.flags	= BMAN_POOL_FLAG_DYNAMIC_BPID,
	};

	unsigned int num_bufs = 0;
	int ret = 0, count = SEC_BPOOL_BUF_COUNT;

	sec_bpool = bman_new_pool(&params);
	if (!sec_bpool) {
		pr_err("Unable to allocate bpool for SEC use\n");
		return -ENOMEM;
	}

	sec_bpid = bman_get_params(sec_bpool)->bpid;

	/* Drain the pool of anything already in it. */
	do {
		/* Acquire is all-or-nothing, so we drain in 8s, then in 1s for
		 * the remainder. */
		if (ret != 1)
			ret = bman_acquire(sec_bpool, bufs, 8, 0);
		if (ret < 8)
			ret = bman_acquire(sec_bpool, bufs, 1, 0);
		if (ret > 0)
			num_bufs += ret;
	} while (ret > 0);

	if (num_bufs)
		fprintf(stderr, "Warn: drained %u bufs from SEC BPID %u\n",
			num_bufs, sec_bpid);


	/* Fill the SEC bpool */
	count = SEC_BPOOL_BUF_COUNT;
	for (num_bufs = 0; num_bufs < count; ) {
		unsigned int loop, rel = (count - num_bufs) > 8 ? 8 :
					(count - num_bufs);

		for (loop = 0; loop < rel; loop++) {
			void *ptr = __dma_mem_memalign(64, DMA_MEM_BP3_SIZE);

			if (!ptr) {
				fprintf(stderr, "error: no buffer space\n");
				abort();
			}

			bm_buffer_set64(&bufs[loop], __dma_mem_vtop(ptr));
		}

		do {
			ret = bman_release(sec_bpool, bufs, rel, 0);
		} while (ret == -EBUSY);

		if (ret)
			fprintf(stderr, "Fail: %s\n", "bman_release()");

		num_bufs += rel;
	}

	printf("Released %u bufs to SEC BPID %u\n", num_bufs, sec_bpid);
	return 0;
}

int ppam_init(void)
{
	int _errno;

	/* Allocate a temporary bpool for SEC usage if simple FD*/
	if (simple_fd_mode) {
		_errno = init_sec_bpool();
		if (_errno < 0) {
			pr_err("SEC bool initialization error\n");
			return -errno;
		}
	}

	/* Initializes a soft cache of buffers */
	if (unlikely(NULL == mem_cache_init())) {
		pr_err("Cache Creation error\n");
		return -ENOMEM;
	}

	_errno = ipsec_config_create();
	if (unlikely(_errno < 0)) {
		pr_err("Error in creating Ipsec configuration\n");
		return _errno;
	}

	_errno = initialize_ipsec_stack();
	if (unlikely(_errno < 0)) {
		pr_err("Error Initializing IP Stack\n");
		return _errno;
	}

	/* Create Message queues to send and receive */
	_errno = create_mq();
	if (unlikely(_errno < 0)) {
		pr_err("Error in creating message queues\n");
		return _errno;
	}

	_errno = init_split_key_fqs();
	if (unlikely(_errno < 0)) {
		pr_err("Unable to initialize Split key FQs\n");
		return _errno;
	}

	_errno = generate_splitkey();
	if (unlikely(_errno < 0)) {
		pr_err("Unable to genetare Split Key\n");
		return _errno;
	}

	return 0;
}

void ppam_finish(void)
{
	char name[10];
	int i;

	TRACE("closing snd and rcv message queues\n");

	if (mq_fd_snd >= 0) {
		if (mq_close(mq_fd_snd) == -1)
			error(0, errno, "%s():mq_close send", __func__);
		mq_fd_snd = -1;
		sprintf(name, "/mq_snd_%d", getpid());
		if (mq_unlink(name) == -1)
			error(0, errno, "%s():mq_unlink send", __func__);
	}
	if (mq_fd_rcv >= 0) {
		if (mq_close(mq_fd_rcv) == -1)
			error(0, errno, "%s():mq_close rcv", __func__);
		mq_fd_rcv = -1;
		sprintf(name, "/mq_rcv_%d", getpid());
		if (mq_unlink(name) == -1)
			error(0, errno, "%s():mq_unlink rcv", __func__);
	}

	/* Closing splitkey FQ's */
	teardown_fq(g_splitkey_fq_to_sec);
	teardown_fq(g_splitkey_fq_from_sec);

	/* Closing SEC Rx and Tx FQ's */
	for (i = 0; i < IPSEC_TUNNEL_ENTRIES; i++) {
		if (g_ipsec_ctxt[i] != NULL) {
			int j = 0;
			do {
				teardown_fq(&(g_ipsec_ctxt[i]->fq_to_sec[j]));
				j++;
			} while (j < g_ipsec_ctxt[i]->num_fq_to_sec);
			teardown_fq(&(g_ipsec_ctxt[i]->fq_from_sec));
		}
	}

	if (simple_fd_mode)
		bman_free_pool(sec_bpool);
}

static int ppam_interface_init(struct ppam_interface *p,
			       const struct fm_eth_port_cfg *cfg,
			       unsigned int num_tx_fqs,
			       uint32_t *flags __maybe_unused)
{
	int iface;
	const struct fman_if *fif;

	fif = cfg->fman_if;
	iface = (fif->mac_type == fman_mac_1g ? 0 : 8) + fif->mac_idx;
	p->ifnum = fif->fman_idx * 10 + iface;
	p->mtu = ETHERMTU;
	p->header_len = ETHER_HDR_LEN;
	p->mask = IN_CLASSC_NET;

	p->num_tx_fqids = num_tx_fqs;
	p->tx_fqids = malloc(p->num_tx_fqids * sizeof(*p->tx_fqids));
	if (unlikely(p->tx_fqids == 0))
		return -ENOMEM;

	eth_setup(p);

	if (fif->mac_type == fman_mac_1g)
		printf("Configured 1G port @ FMAN:%d, MAC:%d as IF_IDX:%d\n",
			fif->fman_idx, fif->mac_idx, p->ifnum);
	if (fif->mac_type == fman_mac_10g)
		printf("Configured 10G port @ FMAN:%d, MAC:%d as IF_IDX:%d\n",
			fif->fman_idx, fif->mac_idx, p->ifnum);

	return 0;
}
static void ppam_interface_finish(struct ppam_interface *p)
{
	free(p->tx_fqids);
}
static void ppam_interface_tx_fqid(struct ppam_interface *p, unsigned idx,
				   uint32_t fqid)
{
	p->tx_fqids[idx] = fqid;
}
static int ppam_rx_error_init(struct ppam_rx_error *p,
			      struct ppam_interface *_if,
			      struct qm_fqd_stashing *stash_opts)
{
	p->stats = ipsec_stack.ip_stack.ip_stats;
	p->hooks = &ipsec_stack.ip_stack.hooks;
	p->protos = &ipsec_stack.ip_stack.protos;
	p->rc = ipsec_stack.ip_stack.rc;

	return 0;
}
static void ppam_rx_error_finish(struct ppam_rx_error *p,
				 struct ppam_interface *_if)
{
}
static inline void ppam_rx_error_cb(struct ppam_rx_error *p,
				    struct ppam_interface *_if,
				    const struct qm_dqrr_entry *dqrr)
{
	ppac_drop_frame(&dqrr->fd);
}
static int ppam_rx_default_init(struct ppam_rx_default *p,
				struct ppam_interface *_if,
				unsigned idx,
				struct qm_fqd_stashing *stash_opts)
{
	p->stats = ipsec_stack.ip_stack.ip_stats;
	p->hooks = &ipsec_stack.ip_stack.hooks;
	p->protos = &ipsec_stack.ip_stack.protos;
	p->rc = ipsec_stack.ip_stack.rc;

	return 0;
}
static void ppam_rx_default_finish(struct ppam_rx_default *p,
				   struct ppam_interface *_if)
{
}
static inline void ppam_rx_default_cb(struct ppam_rx_default *p,
				      struct ppam_interface *_if,
				      const struct qm_dqrr_entry *dqrr)
{
	ppac_drop_frame(&dqrr->fd);
}
static int ppam_tx_error_init(struct ppam_tx_error *p,
			      struct ppam_interface *_if,
			      struct qm_fqd_stashing *stash_opts)
{
	return 0;
}
static void ppam_tx_error_finish(struct ppam_tx_error *p,
				 struct ppam_interface *_if)
{
}
static inline void ppam_tx_error_cb(struct ppam_tx_error *p,
				    struct ppam_interface *_if,
				    const struct qm_dqrr_entry *dqrr)
{
	ppac_drop_frame(&dqrr->fd);
}
static int ppam_tx_confirm_init(struct ppam_tx_confirm *p,
				struct ppam_interface *_if,
				struct qm_fqd_stashing *stash_opts)
{
	return 0;
}
static void ppam_tx_confirm_finish(struct ppam_tx_confirm *p,
				   struct ppam_interface *_if)
{
}
static inline void ppam_tx_confirm_cb(struct ppam_tx_confirm *p,
				      struct ppam_interface *_if,
				      const struct qm_dqrr_entry *dqrr)
{
	ppac_drop_frame(&dqrr->fd);
}

static int ppam_rx_hash_init(struct ppam_rx_hash *p, struct ppam_interface *_if,
			     unsigned idx, struct qm_fqd_stashing *stash_opts)
{
	p->stats = ipsec_stack.ip_stack.ip_stats;
	p->hooks = &ipsec_stack.ip_stack.hooks;
	p->protos = &ipsec_stack.ip_stack.protos;
	p->rc = ipsec_stack.ip_stack.rc;
	p->itt = &ipsec_stack.itt;

	/* Override defaults, enable 1 CL of annotation stashing */
	stash_opts->annotation_cl =
		(sizeof(struct annotations_t) +	L1_CACHE_BYTES - 1) /
			L1_CACHE_BYTES;

	return 0;
}
static void ppam_rx_hash_finish(struct ppam_rx_hash *p,
				struct ppam_interface *_if,
			 unsigned idx)
{
}

static inline void ppam_rx_hash_cb(struct ppam_rx_hash *p,
				   const struct qm_dqrr_entry *dqrr)
{
	struct annotations_t *notes;
	void *data;

	switch (dqrr->fd.format) {
	case qm_fd_contig:
		notes = __dma_mem_ptov(qm_fd_addr(&dqrr->fd));
		data = (void *)notes + dqrr->fd.offset;
		break;
	default:
		pr_err("Unsupported format packet came\n");
		return;
	}
	notes->dqrr = dqrr;

	ip_handler(p, notes, data);
}

int ppam_sec_needed()
{
	return 1;
}

#include <ppac.c>

struct ppam_arguments {
};

struct ppam_arguments ppam_args;

static error_t ipsecfwd_parser(int key, char *arg, struct argp_state *state)
{
	unsigned long val;
	switch (key) {
	case 'm':
		errno = 0;
		val = strtoul(arg, NULL, 0);
		if (errno)
			argp_usage(state);
		if (val)
			simple_fd_mode = true;
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

const char ppam_doc[] = "IP forwarding";

static const struct argp_option argp_opts[] = {
	{"sec-simple-fd", 'm', "0/non-zero", 0, "Use Simple FD for SEC"},
	{}
};

const struct argp ppam_argp = {argp_opts, ipsecfwd_parser, 0, ppam_doc};
