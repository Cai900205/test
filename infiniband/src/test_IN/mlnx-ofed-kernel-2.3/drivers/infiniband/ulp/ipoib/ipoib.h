/*
 * Copyright (c) 2004, 2005 Topspin Communications.  All rights reserved.
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

#ifndef _IPOIB_H
#define _IPOIB_H

#include <linux/string.h>
#include <linux/list.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/workqueue.h>
#include <linux/kref.h>
#include <linux/if_infiniband.h>
#include <linux/mutex.h>

#include <linux/inet_lro.h>

#include <net/neighbour.h>
#include <net/sch_generic.h>

#include <linux/atomic.h>

#include <rdma/ib_verbs.h>
#include <rdma/ib_pack.h>
#include <rdma/ib_sa.h>
#include <linux/sched.h>
#include <rdma/e_ipoib.h>
#include "ipoib_allmulti.h"

#define DRV_VERSION	"2.3-2.0.0"
#define DRV_RELDATE __DATE__
extern const char ipoib_driver_version[];

/* constants */

enum ipoib_flush_level {
	IPOIB_FLUSH_LIGHT,
	IPOIB_FLUSH_NORMAL,
	IPOIB_FLUSH_HEAVY
};

enum {
	IPOIB_ENCAP_LEN		  = 4,

	IPOIB_UD_HEAD_SIZE	  = IB_GRH_BYTES + IPOIB_ENCAP_LEN,
	IPOIB_UD_HEAD_BUFF_SIZE   = IPOIB_UD_HEAD_SIZE + 128, /* reserve some tailroom for IP/TCP headers */
	IPOIB_UD_RX_SG		  = 2, /* max buffer needed for 4K mtu */
	IPOIB_NUM_RX_SKB	  = 2,
	IPOIB_CM_MTU		  = 0x10000 - 0x10, /* padding to align header to 16 */
	IPOIB_CM_BUF_SIZE	  = IPOIB_CM_MTU  + IPOIB_ENCAP_LEN,
	IPOIB_CM_HEAD_SIZE	  = IPOIB_CM_BUF_SIZE % PAGE_SIZE,
	IPOIB_CM_RX_SG		  = ALIGN(IPOIB_CM_BUF_SIZE, PAGE_SIZE) / PAGE_SIZE,
	IPOIB_RX_RING_SIZE	  = 512,
	IPOIB_TX_RING_SIZE	  = 512,
	IPOIB_MAX_QUEUE_SIZE	  = 8192,
	IPOIB_MIN_QUEUE_SIZE	  = 2,
	IPOIB_CM_MAX_CONN_QP	  = 4096,

	IPOIB_NUM_WC		  = 64,

	IPOIB_MAX_PATH_REC_QUEUE  = 3,
	IPOIB_MAX_MCAST_QUEUE	  = 3,

	IPOIB_FLAG_OPER_UP	  = 0,
	IPOIB_FLAG_INITIALIZED	  = 1,
	IPOIB_FLAG_ADMIN_UP	  = 2,
	IPOIB_PKEY_ASSIGNED	  = 3,
	IPOIB_FLAG_SUBINTERFACE	  = 5,
	IPOIB_MCAST_RUN		  = 6,
	IPOIB_STOP_REAPER	  = 7,
	IPOIB_ALL_MULTI           = 8,
	IPOIB_FLAG_ADMIN_CM	  = 9,
	IPOIB_FLAG_UMCAST	  = 10,
	IPOIB_STOP_NEIGH_GC	  = 11,
	IPOIB_NEIGH_TBL_FLUSH	  = 12,
	IPOIB_FLAG_AUTO_MODER     = 13, /*indicates moderation is running*/
	/*indicates if event handler was registered*/
	IPOIB_FLAG_EVENTS_REGISTERED    = 14,
	/*indicates interface in the middle of destruction, like delete_child*/
	IPOIB_FLAG_INTF_ON_DESTROY	= 15,

	IPOIB_MAX_BACKOFF_SECONDS = 16,

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39))
	IPOIB_FLAG_CSUM = 17,
#endif
	IPOIB_MCAST_FLAG_FOUND	  = 0,	/* used in set_multicast_list */
	IPOIB_MCAST_FLAG_SENDONLY = 1,
	IPOIB_MCAST_FLAG_BUSY	  = 2,	/* joining or already joined */
	IPOIB_MCAST_FLAG_ATTACHED = 3,
	IPOIB_MCAST_JOIN_STARTED  = 4,

	IPOIB_USR_MC_MEMBER		= 7,	/* used for user-related mcg */

#ifdef CONFIG_COMPAT_LRO_ENABLED
	IPOIB_MAX_LRO_DESCRIPTORS = 8,
	IPOIB_LRO_MAX_AGGR      = 64,
#endif

	MAX_SEND_CQE		  = 16,
	IPOIB_CM_COPYBREAK	  = 256,
	IPOIB_MAX_INLINE_SIZE     = 800,
	IPOIB_NON_CHILD		  = 0,
	IPOIB_LEGACY_CHILD	  = 1,
	IPOIB_RTNL_CHILD	  = 2,

	IPOIB_MAX_RX_QUEUES   = 16,
	IPOIB_MAX_TX_QUEUES   = 16,
	IPOIB_MIN_CONN_QP     = 2,
	IPOIB_DEFAULT_CONN_QP = 128,
};

enum ipoib_alloc_type {
	IPOIB_ALLOC_NEW = 0,
	IPOIB_ALLOC_REPLACEMENT = 1,
};

#ifdef CONFIG_COMPAT_LRO_ENABLED
struct ipoib_lro {
	struct net_lro_mgr lro_mgr;
	struct net_lro_desc lro_desc[IPOIB_MAX_LRO_DESCRIPTORS];
};
#endif

#define	IPOIB_OP_RECV   (1ul << 31)
#ifdef CONFIG_INFINIBAND_IPOIB_CM
#define	IPOIB_OP_CM     (1ul << 30)
#else
#define	IPOIB_OP_CM     (0)
#endif

/* structs */

struct ipoib_header {
	__be16	proto;
	__be16	tss_qpn_mask_sz;
};

struct ipoib_cb {
	struct qdisc_skb_cb	qdisc_cb;
	u8			hwaddr[INFINIBAND_ALEN];
};

/* Used for all multicast joins (broadcast, IPv4 mcast and IPv6 mcast) */
struct ipoib_mcast {
	struct ib_sa_mcmember_rec mcmember;
	struct ib_sa_multicast	 *mc;
	struct ipoib_ah		 *ah;

	struct rb_node    rb_node;
	struct list_head  list;

	unsigned long created;
	unsigned long backoff;
	/* indicates the last time was accessed*/
	unsigned long alive;

	unsigned long flags;
	unsigned char logcount;

	struct list_head  neigh_list;

	struct sk_buff_head pkt_queue;

	struct net_device *dev;
        struct completion done;
};

struct ipoib_skb {
	struct sk_buff *skb;
	u64 mapping[IPOIB_UD_RX_SG];
};

struct ipoib_rx_buf {
	struct ipoib_skb rx_skb[IPOIB_NUM_RX_SKB];
	int skb_index;
};

struct ipoib_tx_buf {
	struct sk_buff *skb;
	struct ipoib_ah *ah;
	u64		mapping[MAX_SKB_FRAGS + 1];
	int		is_inline;
};

struct ipoib_cm_tx_buf {
	struct sk_buff *skb;
	u64		mapping;
	int		is_inline;
};

/* in order to call dst->ops->update_pmtu out of spin-lock*/
struct ipoib_pmtu_update {
	struct work_struct work;
	struct sk_buff *skb;
	unsigned int mtu;
};

struct ib_cm_id;

struct ipoib_cm_data {
	__be32 qpn; /* High byte MUST be ignored on receive */
	__be32 mtu;
};

/*
 * Quoting 10.3.1 Queue Pair and EE Context States:
 *
 * Note, for QPs that are associated with an SRQ, the Consumer should take the
 * QP through the Error State before invoking a Destroy QP or a Modify QP to the
 * Reset State.  The Consumer may invoke the Destroy QP without first performing
 * a Modify QP to the Error State and waiting for the Affiliated Asynchronous
 * Last WQE Reached Event. However, if the Consumer does not wait for the
 * Affiliated Asynchronous Last WQE Reached Event, then WQE and Data Segment
 * leakage may occur. Therefore, it is good programming practice to tear down a
 * QP that is associated with an SRQ by using the following process:
 *
 * - Put the QP in the Error State
 * - Wait for the Affiliated Asynchronous Last WQE Reached Event;
 * - either:
 *       drain the CQ by invoking the Poll CQ verb and either wait for CQ
 *       to be empty or the number of Poll CQ operations has exceeded
 *       CQ capacity size;
 * - or
 *       post another WR that completes on the same CQ and wait for this
 *       WR to return as a WC;
 * - and then invoke a Destroy QP or Reset QP.
 *
 * We use the second option and wait for a completion on the
 * same CQ before destroying QPs attached to our SRQ.
 */

enum ipoib_cm_state {
	IPOIB_CM_RX_LIVE,
	IPOIB_CM_RX_ERROR, /* Ignored by stale task */
	IPOIB_CM_RX_FLUSH  /* Last WQE Reached event observed */
};

struct ipoib_cm_rx {
	struct ib_cm_id	       *id;
	struct ib_qp	       *qp;
	struct ipoib_cm_rx_buf *rx_ring;
	struct list_head	list;
	struct net_device      *dev;
	unsigned long		jiffies;
	enum ipoib_cm_state	state;
	int			recv_count;
	u32			qpn;
	int index; /* For ring counters */
};

struct ipoib_cm_tx {
	struct ib_cm_id	    *id;
	struct ib_qp	    *qp;
	struct list_head     list;
	struct net_device   *dev;
	struct ipoib_neigh  *neigh;
	struct ipoib_path   *path;
	struct ipoib_cm_tx_buf *tx_ring;
	unsigned	     tx_head;
	unsigned	     tx_tail;
	unsigned long	     flags;
	u32		     mtu;
};

struct ipoib_cm_rx_buf {
	struct sk_buff *skb;
	u64 mapping[IPOIB_CM_RX_SG];
};

struct ipoib_cm_dev_priv {
	struct ib_srq	       *srq;
	struct ipoib_cm_rx_buf *srq_ring;
	struct ib_cm_id	       *id;
	struct list_head	passive_ids;   /* state: LIVE */
	struct list_head	rx_error_list; /* state: ERROR */
	struct list_head	rx_flush_list; /* state: FLUSH, drain not started */
	struct list_head	rx_drain_list; /* state: FLUSH, drain started */
	struct list_head	rx_reap_list;  /* state: FLUSH, drain done */
	struct work_struct      start_task;
	struct work_struct      reap_task;
	struct work_struct      skb_task;
	struct work_struct      rx_reap_task;
	struct delayed_work     stale_task;
	struct sk_buff_head     skb_queue;
	struct list_head	start_list;
	struct list_head	reap_list;
	struct ib_wc		ibwc[IPOIB_NUM_WC];
	int			nonsrq_conn_qp;
	int			max_cm_mtu;
	int			num_frags;
	u32			rx_cq_ind;
};

/* adaptive moderation parameters: */
enum {
	/* Target number of packets to coalesce with interrupt moderation */
	IPOIB_RX_COAL_TARGET	= 88,
	IPOIB_RX_COAL_TIME	= 16,
	IPOIB_TX_COAL_PKTS	= 5,
	IPOIB_TX_COAL_TIME	= 0x80,
	IPOIB_RX_RATE_LOW	= 400000,
	IPOIB_RX_COAL_TIME_LOW	= 0,
	IPOIB_RX_RATE_HIGH	= 450000,
	IPOIB_RX_COAL_TIME_HIGH	= 128,
	IPOIB_RX_SIZE_THRESH	= 1024,
	IPOIB_RX_RATE_THRESH	= 1000000 / IPOIB_RX_COAL_TIME_HIGH,
	IPOIB_SAMPLE_INTERVAL	= 0,
	IPOIB_AVG_PKT_SMALL	= 256,
	IPOIB_AUTO_CONF		= 0xffff,
	ADAPT_MODERATION_DELAY	= HZ / 4,
};

struct ipoib_ethtool_st {
	__u32 rx_max_coalesced_frames;
	__u32 rx_coalesce_usecs;
	__u32	pkt_rate_low;
	__u32	pkt_rate_high;
	__u32	pkt_rate_low_per_ring;
	__u32	pkt_rate_high_per_ring;
	__u32	rx_coalesce_usecs_low;
	__u32	rx_coalesce_usecs_high;
	__u32	rate_sample_interval;
	__u32	use_adaptive_rx_coalesce;
	u16	sample_interval;
};

#define IPOIB_NEIGH_GC_TIME (30 * HZ)

struct ipoib_neigh_table;

struct ipoib_neigh_hash {
	struct ipoib_neigh_table       *ntbl;
	struct ipoib_neigh __rcu      **buckets;
	struct rcu_head			rcu;
	u32				mask;
	u32				size;
};

struct ipoib_neigh_table {
	struct ipoib_neigh_hash __rcu  *htbl;
	atomic_t			entries;
	struct completion		flushed;
	struct completion		deleted;
};

/*
 * Per QP stats
 */

struct ipoib_tx_ring_stats {
	unsigned long tx_packets;
	unsigned long tx_bytes;
	unsigned long tx_errors;
	unsigned long tx_dropped;
};

struct ipoib_rx_ring_stats {
	unsigned long rx_packets;
	unsigned long rx_bytes;
	unsigned long rx_errors;
	unsigned long rx_dropped;
};

/*
 * Encapsulates the per send QP information
 */
struct ipoib_send_ring {
	struct net_device	*dev;
	struct ib_cq		*send_cq;
	struct ib_qp		*send_qp;
	struct ipoib_tx_buf	*tx_ring;
	unsigned		tx_head;
	unsigned		tx_tail;
	struct ib_sge		tx_sge[MAX_SKB_FRAGS + 1];
	struct ib_send_wr	tx_wr;
	unsigned		tx_outstanding;
	struct ib_wc		tx_wc[MAX_SEND_CQE];
	struct timer_list	poll_timer;
	struct ipoib_tx_ring_stats stats;
	unsigned		index;
};

struct ipoib_rx_cm_info {
	struct ib_sge		rx_sge[IPOIB_CM_RX_SG];
	struct ib_recv_wr       rx_wr;
};

struct ipoib_ethtool_last_st {
	unsigned long last_moder_jiffies;
	unsigned long last_moder_packets;
	unsigned long last_moder_tx_packets;
	unsigned long last_moder_bytes;
	int	last_moder_time;
};

/*
 * Encapsulates the per recv QP information
 */
struct ipoib_recv_ring {
	struct net_device	*dev;
	struct ib_qp		*recv_qp;
	struct ib_cq		*recv_cq;
	struct ib_wc		ibwc[IPOIB_NUM_WC];
	struct napi_struct	napi;
	struct ipoib_rx_buf	*rx_ring;
	struct ib_recv_wr	rx_wr;
	struct ib_sge		rx_sge[IPOIB_UD_RX_SG];
	struct ipoib_rx_cm_info	cm;
	struct ipoib_rx_ring_stats stats;
	unsigned		index;
	struct ipoib_ethtool_last_st ethtool;
#ifdef CONFIG_COMPAT_LRO_ENABLED
	struct ipoib_lro lro;
#endif
};

struct ipoib_arp_repath {
	struct work_struct work;
	u16 lid;
	union ib_gid	    sgid;
	struct net_device   *dev;
};

struct ipoib_qp_state_validate {
	struct work_struct work;
	struct ipoib_dev_priv   *priv;
	struct ipoib_send_ring *send_ring;
};

/*
 * Device private locking: network stack tx_lock protects members used
 * in TX fast path, lock protects everything else.  lock nests inside
 * of tx_lock (ie tx_lock must be acquired first if needed).
 */
struct ipoib_dev_priv {
	spinlock_t lock;

	struct net_device *dev;

	unsigned long flags;

	struct rw_semaphore vlan_rwsem;

	struct rb_root  path_tree;
	struct list_head path_list;

	struct ipoib_neigh_table ntbl;

	struct ipoib_mcast *broadcast;
	struct list_head multicast_list;
	struct rb_root multicast_tree;

	struct delayed_work mcast_task;
	struct work_struct carrier_on_task;
	struct work_struct flush_light;
	struct work_struct flush_normal;
	struct work_struct flush_heavy;
	struct work_struct restart_task;
	struct delayed_work ah_reap_task;
	struct delayed_work adaptive_moder_task;
	struct delayed_work neigh_reap_task;
	struct ib_device *ca;
	u8		  port;
	u16		  pkey;
	u16		  pkey_index;
	struct ib_pd	 *pd;
	struct ib_mr	 *mr;
	struct ib_qp	 *qp; /* also parent QP for TSS & RSS */
	u32		  qkey;

	union ib_gid local_gid;
	u16	     local_lid;

	unsigned int admin_mtu;
	unsigned int mcast_mtu;
	unsigned int max_ib_mtu;

	struct list_head dead_ahs;

	struct ib_event_handler event_handler;

	struct net_device *parent;
	struct list_head child_intfs;
	struct list_head list;
	int child_index;
	int    child_type;

#ifdef CONFIG_INFINIBAND_IPOIB_CM
	struct ipoib_cm_dev_priv cm;
#endif

#ifdef CONFIG_INFINIBAND_IPOIB_DEBUG
	struct list_head fs_list;
	struct dentry *mcg_dentry;
	struct dentry *path_dentry;
	struct dentry *neigh_dentry;
#endif
	int	hca_caps;
	struct ipoib_ethtool_st ethtool;
	struct timer_list poll_timer;
	struct ipoib_recv_ring *recv_ring;
	struct ipoib_send_ring *send_ring;
	unsigned int sendq_size;
	unsigned int recvq_size;
	unsigned int rss_qp_num; /* No RSS HW support 0 */
	unsigned int tss_qp_num; /* No TSS (HW or SW) used 0 */
	unsigned int max_rx_queues; /* No RSS HW support 1 */
	unsigned int max_tx_queues; /* No TSS HW support tss_qp_num + 1 */
	unsigned int num_rx_queues; /* Actual */
	unsigned int num_tx_queues; /* Actual */
	struct rw_semaphore rings_rwsem;
	__be16 tss_qpn_mask_sz; /* Put in ipoib header reserved */
	atomic_t tx_ring_ind;
	struct mutex		state_lock;

	/* all state change operation over the ring's QP should be under lock*/
	struct mutex		ring_qp_lock;
	/* netlink */
	struct genl_mdata	netlink;
	/* promiscuous mc */
	struct promisc_mc promisc;
};

struct ipoib_ah {
	struct net_device *dev;
	struct ib_ah	  *ah;
	struct list_head   list;
	struct kref	   ref;
	atomic_t	   refcnt;
};

struct ipoib_path {
	struct net_device    *dev;
	struct ib_sa_path_rec pathrec;
	struct ipoib_ah      *ah;
	struct sk_buff_head   queue;

	struct list_head      neigh_list;

	int		      query_id;
	struct ib_sa_query   *query;
	struct completion     done;

	struct rb_node	      rb_node;
	struct list_head      list;
	int  		      valid;
};

struct ipoib_neigh {
	struct ipoib_ah    *ah;
#ifdef CONFIG_INFINIBAND_IPOIB_CM
	struct ipoib_cm_tx *cm;
#endif
	u8     daddr[INFINIBAND_ALEN];
	struct sk_buff_head queue;

	struct net_device *dev;

	struct list_head    list;
	struct ipoib_neigh __rcu *hnext;
	struct rcu_head     rcu;
	atomic_t	    refcnt;
	unsigned long       alive;
	int index; /* For ndo_select_queue and ring counters */
};

struct ipoib_neigh_iter {
	struct net_device *dev;
	struct ipoib_neigh *neigh;
	int htbl_index;
};

#define IPOIB_UD_MTU(ib_mtu)		(ib_mtu - IPOIB_ENCAP_LEN)
#define IPOIB_UD_BUF_SIZE(ib_mtu)	(ib_mtu + IB_GRH_BYTES)

void ipoib_neigh_dtor(struct ipoib_neigh *neigh);
static inline void ipoib_neigh_put(struct ipoib_neigh *neigh)
{
	if (atomic_dec_and_test(&neigh->refcnt))
		ipoib_neigh_dtor(neigh);
}
struct ipoib_neigh *ipoib_neigh_get(struct net_device *dev, u8 *daddr);
struct ipoib_neigh *ipoib_neigh_alloc(u8 *daddr,
				      struct net_device *dev);
void ipoib_neigh_free(struct ipoib_neigh *neigh);
void ipoib_del_neighs_by_gid(struct net_device *dev, u8 *gid);

extern struct workqueue_struct *ipoib_workqueue;
extern struct workqueue_struct *ipoib_auto_moder_workqueue;

/* functions */

int ipoib_poll(struct napi_struct *napi, int budget);
void ipoib_ib_completion(struct ib_cq *cq, void *recv_ring_ptr);
void ipoib_send_comp_handler(struct ib_cq *cq, void *send_ring_ptr);

struct ipoib_ah *ipoib_create_ah(struct net_device *dev,
				 struct ib_pd *pd, struct ib_ah_attr *attr);
void ipoib_free_ah(struct kref *kref);
static inline void ipoib_put_ah(struct ipoib_ah *ah)
{
	kref_put(&ah->ref, ipoib_free_ah);
}
int ipoib_open(struct net_device *dev);
int ipoib_add_pkey_attr(struct net_device *dev);
int ipoib_add_umcast_attr(struct net_device *dev);
int ipoib_add_channels_attr(struct net_device *dev);
void ipoib_dev_uninit(struct net_device *dev);

void ipoib_send(struct net_device *dev, struct sk_buff *skb,
		struct ipoib_ah *address, u32 qpn);
void ipoib_reap_ah(struct work_struct *work);
void ipoib_repath_ah(struct work_struct *work);

void ipoib_mark_paths_invalid(struct net_device *dev);
void ipoib_flush_paths(struct net_device *dev);
struct ipoib_dev_priv *ipoib_intf_alloc(const char *format,
					struct ipoib_dev_priv *temp_priv);

int ipoib_ib_dev_init(struct net_device *dev, struct ib_device *ca, int port);
void ipoib_ib_dev_flush_light(struct work_struct *work);
void ipoib_ib_dev_flush_normal(struct work_struct *work);
void ipoib_ib_dev_flush_heavy(struct work_struct *work);
void ipoib_pkey_event(struct work_struct *work);
void ipoib_ib_dev_cleanup(struct net_device *dev);

int ipoib_ib_dev_open(struct net_device *dev, int flush);
int ipoib_ib_dev_up(struct net_device *dev);
int ipoib_ib_dev_down(struct net_device *dev, int flush);
int ipoib_ib_dev_stop(struct net_device *dev, int flush);
void ipoib_pkey_dev_check_presence(struct net_device *dev);

int ipoib_dev_init(struct net_device *dev, struct ib_device *ca, int port);
void ipoib_dev_cleanup(struct net_device *dev);

int ipoib_reinit(struct net_device *dev, int num_rx, int num_tx);

void ipoib_mcast_join_task(struct work_struct *work);
void ipoib_mcast_carrier_on_task(struct work_struct *work);
void ipoib_mcast_send(struct net_device *dev, u8 *daddr, struct sk_buff *skb);

void ipoib_mcast_restart_task(struct work_struct *work);
int ipoib_mcast_start_thread(struct net_device *dev);
int ipoib_mcast_stop_thread(struct net_device *dev, int flush);

void ipoib_mcast_dev_down(struct net_device *dev);
void ipoib_mcast_dev_flush(struct net_device *dev);

#ifdef CONFIG_INFINIBAND_IPOIB_DEBUG
struct ipoib_mcast_iter *ipoib_mcast_iter_init(struct net_device *dev);
int ipoib_mcast_iter_next(struct ipoib_mcast_iter *iter);
void ipoib_mcast_iter_read(struct ipoib_mcast_iter *iter,
				  union ib_gid *gid,
				  unsigned long *created,
				  unsigned int *queuelen,
				  unsigned int *complete,
				  unsigned int *send_only);

struct ipoib_path_iter *ipoib_path_iter_init(struct net_device *dev);
int ipoib_path_iter_next(struct ipoib_path_iter *iter);
void ipoib_path_iter_read(struct ipoib_path_iter *iter,
			  struct ipoib_path *path);
struct ipoib_neigh_iter *ipoib_neigh_iter_init(struct net_device *dev);
int ipoib_neigh_iter_next(struct ipoib_neigh_iter *iter);
void ipoib_neigh_iter_read(struct ipoib_neigh_iter *iter,
			  struct ipoib_neigh *neigh);

#endif

int ipoib_mcast_attach(struct net_device *dev, u16 mlid,
		       union ib_gid *mgid, int set_qkey);

int ipoib_init_qp(struct net_device *dev);
int ipoib_transport_dev_init(struct net_device *dev, struct ib_device *ca);
void ipoib_transport_dev_cleanup(struct net_device *dev);

void ipoib_event(struct ib_event_handler *handler,
		 struct ib_event *record);

int ipoib_vlan_add(struct net_device *pdev, unsigned short pkey,
						unsigned char clone_index);
int ipoib_vlan_delete(struct net_device *pdev, unsigned short pkey,
						unsigned char clone_index);

int __ipoib_vlan_add(struct ipoib_dev_priv *ppriv, struct ipoib_dev_priv *priv,
		     u16 pkey, int child_type);

void ipoib_set_umcast(struct net_device *ndev, int umcast_val);
int  ipoib_set_mode(struct net_device *dev, const char *buf);

void ipoib_setup(struct net_device *dev);

void ipoib_pkey_open(struct ipoib_dev_priv *priv);
void ipoib_drain_cq(struct net_device *dev);
void ipoib_arm_cq(struct net_device *dev);

void ipoib_set_ethtool_ops(struct net_device *dev);
int ipoib_set_dev_features(struct ipoib_dev_priv *priv, struct ib_device *hca);

#ifdef CONFIG_IPOIB_NO_OPTIONS
#define IPOIB_FLAGS_RC		0x0
#define IPOIB_FLAGS_UC		0x0
#define IPOIB_FLAGS_TSS		0x0
#else
#define IPOIB_FLAGS_RC		0x80
#define IPOIB_FLAGS_UC		0x40
#define IPOIB_FLAGS_TSS		0x20
#endif

/* We don't support UC connections at the moment */
#define IPOIB_CM_SUPPORTED(ha)   (ha[0] & (IPOIB_FLAGS_RC))
#define IPOIB_TSS_SUPPORTED(ha)   (ha[0] & (IPOIB_FLAGS_TSS))

#ifdef CONFIG_INFINIBAND_IPOIB_CM

extern int ipoib_max_conn_qp;

static inline int ipoib_cm_admin_enabled(struct net_device *dev)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	return IPOIB_CM_SUPPORTED(dev->dev_addr) &&
		test_bit(IPOIB_FLAG_ADMIN_CM, &priv->flags);
}

static inline int ipoib_cm_enabled(struct net_device *dev, u8 *hwaddr)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	return IPOIB_CM_SUPPORTED(hwaddr) &&
		test_bit(IPOIB_FLAG_ADMIN_CM, &priv->flags);
}

static inline int ipoib_cm_up(struct ipoib_neigh *neigh)

{
	return test_bit(IPOIB_FLAG_OPER_UP, &neigh->cm->flags);
}

static inline struct ipoib_cm_tx *ipoib_cm_get(struct ipoib_neigh *neigh)
{
	return neigh->cm;
}

static inline void ipoib_cm_set(struct ipoib_neigh *neigh, struct ipoib_cm_tx *tx)
{
	neigh->cm = tx;
}

static inline int ipoib_cm_has_srq(struct net_device *dev)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	return !!priv->cm.srq;
}

static inline unsigned int ipoib_cm_max_mtu(struct net_device *dev)
{
	struct ipoib_dev_priv *priv = netdev_priv(dev);
	return priv->cm.max_cm_mtu;
}

void ipoib_cm_send(struct net_device *dev, struct sk_buff *skb, struct ipoib_cm_tx *tx);
int ipoib_cm_dev_open(struct net_device *dev);
void ipoib_cm_dev_stop(struct net_device *dev);
int ipoib_cm_dev_init(struct net_device *dev);
int ipoib_cm_add_mode_attr(struct net_device *dev);
void ipoib_cm_dev_cleanup(struct net_device *dev);
struct ipoib_cm_tx *ipoib_cm_create_tx(struct net_device *dev, struct ipoib_path *path,
				    struct ipoib_neigh *neigh);
void ipoib_cm_destroy_tx(struct ipoib_cm_tx *tx);
void ipoib_cm_skb_too_long(struct net_device *dev, struct sk_buff *skb,
			   unsigned int mtu);
void ipoib_cm_handle_rx_wc(struct net_device *dev,
			   struct ipoib_recv_ring *recv_ring,
			   struct ib_wc *wc);
void ipoib_cm_handle_tx_wc(struct net_device *dev, struct ib_wc *wc);
#else

struct ipoib_cm_tx;

#define ipoib_max_conn_qp 0

static inline int ipoib_cm_admin_enabled(struct net_device *dev)
{
	return 0;
}
static inline int ipoib_cm_enabled(struct net_device *dev, u8 *hwaddr)

{
	return 0;
}

static inline int ipoib_cm_up(struct ipoib_neigh *neigh)

{
	return 0;
}

static inline struct ipoib_cm_tx *ipoib_cm_get(struct ipoib_neigh *neigh)
{
	return NULL;
}

static inline void ipoib_cm_set(struct ipoib_neigh *neigh, struct ipoib_cm_tx *tx)
{
}

static inline int ipoib_cm_has_srq(struct net_device *dev)
{
	return 0;
}

static inline unsigned int ipoib_cm_max_mtu(struct net_device *dev)
{
	return 0;
}

static inline
void ipoib_cm_send(struct net_device *dev, struct sk_buff *skb, struct ipoib_cm_tx *tx)
{
	return;
}

static inline
int ipoib_cm_dev_open(struct net_device *dev)
{
	return 0;
}

static inline
void ipoib_cm_dev_stop(struct net_device *dev)
{
	return;
}

static inline
int ipoib_cm_dev_init(struct net_device *dev)
{
	return -ENOSYS;
}

static inline
void ipoib_cm_dev_cleanup(struct net_device *dev)
{
	return;
}

static inline
struct ipoib_cm_tx *ipoib_cm_create_tx(struct net_device *dev, struct ipoib_path *path,
				    struct ipoib_neigh *neigh)
{
	return NULL;
}

static inline
void ipoib_cm_destroy_tx(struct ipoib_cm_tx *tx)
{
	return;
}

static inline
int ipoib_cm_add_mode_attr(struct net_device *dev)
{
	return 0;
}

static inline void ipoib_cm_skb_too_long(struct net_device *dev, struct sk_buff *skb,
					 unsigned int mtu)
{
	dev_kfree_skb_any(skb);
}

static inline void ipoib_cm_handle_rx_wc(struct net_device *dev,
					 struct ipoib_recv_ring *recv_ring,
					 struct ib_wc *wc)
{
}

static inline void ipoib_cm_handle_tx_wc(struct net_device *dev, struct ib_wc *wc)
{
}
#endif

#ifdef CONFIG_INFINIBAND_IPOIB_DEBUG
void ipoib_create_debug_files(struct net_device *dev);
void ipoib_delete_debug_files(struct net_device *dev);
int ipoib_register_debugfs(void);
void ipoib_unregister_debugfs(void);
#else
static inline void ipoib_create_debug_files(struct net_device *dev) { }
static inline void ipoib_delete_debug_files(struct net_device *dev) { }
static inline int ipoib_register_debugfs(void) { return 0; }
static inline void ipoib_unregister_debugfs(void) { }
#endif

#define ipoib_printk(level, priv, format, arg...)			\
	printk(level "%s: " format,					\
	      !priv || strchr(((struct ipoib_dev_priv *)priv)->dev->name, '%') \
	      ? "dev not registered" :					\
	      ((struct ipoib_dev_priv *)priv)->dev->name, ## arg)

#define ipoib_warn(priv, format, arg...)		\
	ipoib_printk(KERN_WARNING, priv, format , ## arg)

#define ipoib_err(priv, format, arg...)			\
	ipoib_printk(KERN_ERR, priv, format, ## arg)

extern int ipoib_sendq_size;
extern int ipoib_recvq_size;
extern int ipoib_inline_thold;

extern struct ib_sa_client ipoib_sa_client;

static inline void set_skb_oob_cb_data(struct sk_buff *skb, struct ib_wc *wc,
				struct napi_struct *napi)
{
	struct ipoib_cm_rx *p_cm_ctx = NULL;
	struct eipoib_cb_data *data = NULL;
	struct ipoib_header *header;
	unsigned int tss_mask, tss_qpn_mask_sz;

	p_cm_ctx = wc->qp->qp_context;
	data = IPOIB_HANDLER_CB(skb);

	data->rx.slid = wc->slid;
	header = (struct ipoib_header *)(skb->data - IPOIB_ENCAP_LEN);
	if (header->tss_qpn_mask_sz & cpu_to_be16(0xF000)) {
		tss_qpn_mask_sz = be16_to_cpu(header->tss_qpn_mask_sz) >> 12;
		tss_mask = 0xffff >> (16 - tss_qpn_mask_sz);
		data->rx.sqpn = wc->src_qp & tss_mask;
	} else {
		data->rx.sqpn = wc->src_qp;
	}

	data->rx.napi = napi;

	/* in CM mode, use the "base" qpn as sqpn */
	if (p_cm_ctx)
		data->rx.sqpn = p_cm_ctx->qpn;
}

#ifdef CONFIG_INFINIBAND_IPOIB_DEBUG
extern int ipoib_debug_level;

#define ipoib_dbg(priv, format, arg...)			\
	do {						\
		if (ipoib_debug_level > 0)			\
			ipoib_printk(KERN_DEBUG, priv, format , ## arg); \
	} while (0)
#define ipoib_dbg_mcast(priv, format, arg...)		\
	do {						\
		if (mcast_debug_level > 0)		\
			ipoib_printk(KERN_DEBUG, priv, format , ## arg); \
	} while (0)
#else /* CONFIG_INFINIBAND_IPOIB_DEBUG */
#define ipoib_dbg(priv, format, arg...)			\
	do { (void) (priv); } while (0)
#define ipoib_dbg_mcast(priv, format, arg...)		\
	do { (void) (priv); } while (0)
#endif /* CONFIG_INFINIBAND_IPOIB_DEBUG */

#ifdef CONFIG_INFINIBAND_IPOIB_DEBUG_DATA
#define ipoib_dbg_data(priv, format, arg...)		\
	do {						\
		if (data_debug_level > 0)		\
			ipoib_printk(KERN_DEBUG, priv, format , ## arg); \
	} while (0)
#else /* CONFIG_INFINIBAND_IPOIB_DEBUG_DATA */
#define ipoib_dbg_data(priv, format, arg...)		\
	do { (void) (priv); } while (0)
#endif /* CONFIG_INFINIBAND_IPOIB_DEBUG_DATA */

#define IPOIB_QPN(ha) (be32_to_cpup((__be32 *) ha) & 0xffffff)

#endif /* _IPOIB_H */
