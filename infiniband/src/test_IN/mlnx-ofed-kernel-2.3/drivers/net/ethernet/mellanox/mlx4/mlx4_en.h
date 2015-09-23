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

#ifndef _MLX4_EN_H_
#define _MLX4_EN_H_

#include <linux/bitops.h>
#include <linux/compiler.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kobject.h>
#include <linux/netdevice.h>
#include <linux/if_vlan.h>
#include <linux/net_tstamp.h>
#include <linux/pm_qos.h>

#if defined (CONFIG_COMPAT_TIMECOMPARE) && !(defined (CONFIG_PTP_1588_CLOCK) || defined (CONFIG_PTP_1588_CLOCK_MODULE))
#include <linux/timecompare.h>
#endif
#ifdef CONFIG_MLX4_EN_DCB
#include <linux/dcbnl.h>
#endif
#include <linux/cpu_rmap.h>
#if defined (CONFIG_COMPAT_PTP_CLOCK) && (defined (CONFIG_PTP_1588_CLOCK) || defined (CONFIG_PTP_1588_CLOCK_MODULE))
#include <linux/ptp_clock_kernel.h>
#endif
#ifdef CONFIG_COMPAT_LRO_ENABLED
#include <linux/inet_lro.h>
#else
#include <net/ip.h>
#endif

#include <linux/mlx4/device.h>
#include <linux/mlx4/qp.h>
#include <linux/mlx4/cq.h>
#include <linux/mlx4/srq.h>
#include <linux/mlx4/doorbell.h>
#include <linux/mlx4/cmd.h>
#include <linux/bitmap.h>

#include "en_port.h"
#include "mlx4_stats.h"

#define DRV_NAME	"mlx4_en"
#define DRV_VERSION	"2.3-2.0.0"
#define DRV_RELDATE	__DATE__

#ifndef CONFIG_COMPAT_DISABLE_DCB
#ifdef CONFIG_MLX4_EN_DCB

#ifndef CONFIG_COMPAT_IS_MAXRATE
#define CONFIG_COMPAT_MAXRATE
#endif

/* make sure to define QCN only when DCB is not disabled
 * and EN_DCB is defined
 */
#ifndef CONFIG_COMPAT_IS_QCN
#define CONFIG_COMPAT_QCN
#endif

#ifndef CONFIG_COMPAT_MQPRIO
#ifndef CONFIG_NET_SCH_MULTIQ
#define CONFIG_COMPAT_MQPRIO
#endif
#endif

#endif
#endif

#ifndef CONFIG_COMPAT_INDIR_SETTING
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0))
#define CONFIG_COMPAT_INDIR_SETTING
#endif
#endif

#ifndef CONFIG_COMPAT_NUM_CHANNELS
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0) && !defined(CONFIG_COMPAT_HAS_NUM_CHANNELS)
#define CONFIG_COMPAT_NUM_CHANNELS
#endif
#endif

#define MLX4_EN_MSG_LEVEL	(NETIF_MSG_LINK | NETIF_MSG_IFDOWN)

/*
 * Device constants
 */


#define MLX4_EN_PAGE_SHIFT	12
#define MLX4_EN_PAGE_SIZE	(1 << MLX4_EN_PAGE_SHIFT)
#define DEF_RX_RINGS		16
#define MAX_RX_RINGS		16
#define MIN_RX_RINGS		4
#define TXBB_SIZE		64
#define HEADROOM		(2048 / TXBB_SIZE + 1)
#define STAMP_STRIDE		64
#define STAMP_DWORDS		(STAMP_STRIDE / 4)
#define STAMP_SHIFT		31
#define STAMP_VAL		0x7fffffff
#define STATS_DELAY		(HZ / 4)
#define SERVICE_TASK_DELAY	(HZ / 4)
#define MAX_NUM_OF_FS_RULES	256

#define MLX4_EN_FILTER_HASH_SHIFT 4
#define MLX4_EN_FILTER_EXPIRY_QUOTA 60

#ifdef CONFIG_NET_RX_BUSY_POLL
#define LL_EXTENDED_STATS
#endif

/* vlan valid range */
#define VLAN_MIN_VALUE		1
#define VLAN_MAX_VALUE		4094

/* Typical TSO descriptor with 16 gather entries is 352 bytes... */
#define MAX_DESC_SIZE		512
#define MAX_DESC_TXBBS		(MAX_DESC_SIZE / TXBB_SIZE)

/*
 * OS related constants and tunables
 */

#define MLX4_EN_WATCHDOG_TIMEOUT	(15 * HZ)

#define MLX4_EN_ALLOC_SIZE  PAGE_ALIGN(PAGE_SIZE)

#if PAGE_SIZE > 16384 || defined (CONFIG_COMPAT_ALLOC_PAGES_ORDER_0)
#define MLX4_EN_ALLOC_PREFER_ORDER 0
#else
#define MLX4_EN_ALLOC_PREFER_ORDER PAGE_ALLOC_COSTLY_ORDER
#endif

/* Receive fragment sizes; we use at most 3 fragments (for 9600 byte MTU
 * and 4K allocations)
 */
enum {
	FRAG_SZ0 = 1536,
	FRAG_SZ1 = 4096,
	FRAG_SZ2 = 4096,
	FRAG_SZ3 = MLX4_EN_ALLOC_SIZE
};
#define MLX4_EN_MAX_RX_FRAGS    4

/* Minimum packet number till arming the CQ */
#define MLX4_EN_MIN_RX_ARM	128000
#define MLX4_EN_MIN_TX_ARM	128000

/* Maximum ring sizes */
#define MLX4_EN_MAX_TX_SIZE	8192
#define MLX4_EN_MAX_RX_SIZE	8192

/* Minimum ring sizes */
#define MLX4_EN_MIN_RX_SIZE	(4096 / TXBB_SIZE)
#define MLX4_EN_MIN_TX_SIZE	(4096 / TXBB_SIZE)

#define MLX4_EN_SMALL_PKT_SIZE		64

#define MLX4_EN_NUM_TX_RING_PER_VLAN	8
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)) || defined (CONFIG_COMPAT_IS_NUM_TX_QUEUES)
#define MLX4_EN_MAX_TX_RING_P_UP	32
#define MLX4_EN_NUM_UP			8
#define MAX_TX_RINGS			(MLX4_EN_MAX_TX_RING_P_UP * \
					 (MLX4_EN_NUM_UP + 1))
#else
#define MLX4_EN_NUM_TX_RINGS		8
#define MLX4_EN_NUM_PPP_RINGS		8
#define MAX_TX_RINGS			(MLX4_EN_NUM_TX_RINGS * 2 + \
					MLX4_EN_NUM_PPP_RINGS)
#endif

#define MAX_VLANS			(MAX_TX_RINGS / \
					 MLX4_EN_NUM_TX_RING_PER_VLAN)
#define MLX4_EN_NO_VLAN			0xffff

#define MLX4_EN_DEF_TX_RING_SIZE	1024

#if PAGE_SIZE > 4096
#define MLX4_EN_DEF_RX_RING_SIZE	8192
#else
#define MLX4_EN_DEF_RX_RING_SIZE	1024
#endif

/* Target number of bytes to coalesce with interrupt moderation */
#define MLX4_EN_RX_COAL_TARGET	0x2e842
#define MLX4_EN_RX_COAL_TIME	0x10

#define MLX4_EN_TX_COAL_PKTS	64
#define MLX4_EN_TX_COAL_TIME	8

#define MLX4_EN_RX_RATE_LOW		100000
#define MLX4_EN_RX_COAL_TIME_LOW	0
#define MLX4_EN_RX_RATE_HIGH		400000
#define MLX4_EN_RX_COAL_TIME_HIGH	64
#define MLX4_EN_RX_SIZE_THRESH		1024
#define MLX4_EN_RX_RATE_THRESH		(1000000 / MLX4_EN_RX_COAL_TIME_HIGH)
#define MLX4_EN_SAMPLE_INTERVAL		0
#define MLX4_EN_AVG_PKT_SMALL		128

#define MLX4_EN_AUTO_CONF	0xffff

#define MLX4_EN_DEF_RX_PAUSE	1
#define MLX4_EN_DEF_TX_PAUSE	1

/* Interval between successive polls in the Tx routine when polling is used
   instead of interrupts (in per-core Tx rings) - should be power of 2 */
#define MLX4_EN_TX_POLL_MODER	16
#define MLX4_EN_TX_POLL_TIMEOUT	(HZ / 4)

#define MLX4_EN_64_ALIGN	(64 - NET_SKB_PAD)
#define SMALL_PACKET_SIZE      (256 - NET_IP_ALIGN)
#define HEADER_COPY_SIZE       (128 - NET_IP_ALIGN)
#define MLX4_LOOPBACK_TEST_PAYLOAD (HEADER_COPY_SIZE - ETH_HLEN)

#define MLX4_EN_MIN_MTU		46
#define ETH_BCAST		0xffffffffffffULL

#define MLX4_EN_LOOPBACK_RETRIES	5
#define MLX4_EN_LOOPBACK_TIMEOUT	100

#ifdef CONFIG_MLX4_EN_PERF_STAT
/* Number of samples to 'average' */
#define AVG_SIZE			128
#define AVG_FACTOR			1024

#define INC_PERF_COUNTER(cnt)		(++(cnt))
#define ADD_PERF_COUNTER(cnt, add)	((cnt) += (add))
#define AVG_PERF_COUNTER(cnt, sample) \
	((cnt) = ((cnt) * (AVG_SIZE - 1) + (sample) * AVG_FACTOR) / AVG_SIZE)
#define GET_PERF_COUNTER(cnt)		(cnt)
#define GET_AVG_PERF_COUNTER(cnt)	((cnt) / AVG_FACTOR)

#else

#define INC_PERF_COUNTER(cnt)		do {} while (0)
#define ADD_PERF_COUNTER(cnt, add)	do {} while (0)
#define AVG_PERF_COUNTER(cnt, sample)	do {} while (0)
#define GET_PERF_COUNTER(cnt)		(0)
#define GET_AVG_PERF_COUNTER(cnt)	(0)
#endif /* MLX4_EN_PERF_STAT */

/*
 * Configurables
 */

enum cq_type {
	RX = 0,
	TX = 1,
};

#define IP_HDR_LEN(iph) (iph->ihl << 2)
#define TCP_HDR_LEN(tcph) (tcph->doff << 2)

union mlx4_en_phdr {
	/* per byte iterator */
	unsigned char *network;
	/* l2 headers */
	struct ethhdr *eth;
	struct vlan_hdr *vlan;
	/* l3 headers */
	struct iphdr *ipv4;
	struct ipv6hdr *ipv6;
	/* l4 headers */
	struct tcphdr *tcph;
	struct udphdr *udph;
};

/*
 * Useful macros
 */
#define ROUNDUP_LOG2(x)		ilog2(roundup_pow_of_two(x))
#define XNOR(x, y)		(!(x) == !(y))
#define ILLEGAL_MAC(addr)	(addr == 0xffffffffffffULL || addr == 0x0)


struct mlx4_en_tx_info {
	struct sk_buff *skb;
	u32 nr_txbb;
	u32 nr_bytes;
	u8 linear;
	u8 data_offset;
	u8 inl;
	u8 ts_requested;
};


#define MLX4_EN_BIT_DESC_OWN	0x80000000
#define CTRL_SIZE	sizeof(struct mlx4_wqe_ctrl_seg)
#define MLX4_EN_MEMTYPE_PAD	0x100
#define DS_SIZE		sizeof(struct mlx4_wqe_data_seg)


struct mlx4_en_tx_desc {
	struct mlx4_wqe_ctrl_seg ctrl;
	union {
		struct mlx4_wqe_data_seg data; /* at least one data segment */
		struct mlx4_wqe_lso_seg lso;
		struct mlx4_wqe_inline_seg inl;
	};
};

#define MLX4_EN_USE_SRQ		0x01000000

#define MLX4_EN_TX_BUDGET 64
#define MLX4_EN_RX_BUDGET 64

#ifdef CONFIG_COMPAT_LRO_ENABLED
/* LRO defines for MLX4_EN */
#define MLX4_EN_LRO_MAX_DESC	32
#define MLX4_EN_LRO_MAX_AGGR	MAX_SKB_FRAGS

struct mlx4_en_lro {
	struct net_lro_mgr	lro_mgr;
	struct net_lro_desc	lro_desc[MLX4_EN_LRO_MAX_DESC];
};

#endif
#define MLX4_EN_CX3_LOW_ID	0x1000
#define MLX4_EN_CX3_HIGH_ID	0x1005

struct mlx4_en_tx_ring {
	struct mlx4_hwq_resources wqres;
	u32 size ; /* number of TXBBs */
	u32 size_mask;
	u16 stride;
	u16 cqn;	/* index of port CQ associated with this ring */
	u32 prod;
	u32 cons;
	u32 buf_size;
	u32 doorbell_qpn;
	void *buf;
	u16 poll_cnt;
	struct mlx4_en_tx_info *tx_info;
	u8 *bounce_buf;
	u32 last_nr_txbb;
	struct mlx4_qp qp;
	struct mlx4_qp_context context;
	int qpn;
	enum mlx4_qp_state qp_state;
	struct mlx4_srq dummy;
	unsigned long bytes;
	unsigned long packets;
	unsigned long tso_packets;
	unsigned long tx_csum;
	unsigned long queue_stopped;
	unsigned long wake_queue;
	struct mlx4_bf bf;
	bool bf_enabled;
	bool bf_alloced;
	struct netdev_queue *tx_queue;
	int hwtstamp_tx_type;
	int full_size;
	int inline_thold;
};

struct mlx4_en_rx_desc {
	/* actual number of entries depends on rx ring stride */
	struct mlx4_wqe_data_seg data[0];
};

struct mlx4_en_frag_info {
	u16 frag_size;
	u16 frag_prefix_size;
	u16 frag_stride;
};

struct mlx4_en_rx_alloc {
	struct page     *page;
	dma_addr_t      dma;
	u32             page_offset;
	u32             page_size;
};

enum {
	MLX4_EN_RX_VLAN_OFFLOAD = 1 << 0,
	MLX4_EN_TX_VLAN_OFFLOAD = 1 << 1,
};

struct mlx4_en_config {
	u32 flags;
	struct hwtstamp_config hwtstamp;
};

struct mlx4_en_rx_ring {
	struct mlx4_hwq_resources wqres;
	struct mlx4_en_rx_alloc page_alloc[MLX4_EN_MAX_RX_FRAGS];
	u32 size ;	/* number of Rx descs*/
	u32 actual_size;
	u32 size_mask;
	u16 stride;
	u16 log_stride;
	u16 cqn;	/* index of port CQ associated with this ring */
	u32 prod;
	u32 cons;
	u32 buf_size;
	u8  fcs_del;
	int qpn;
	void *buf;
	void *rx_info;
	unsigned long bytes;
	unsigned long packets;
#ifdef LL_EXTENDED_STATS
	unsigned long yields;
	unsigned long misses;
	unsigned long cleaned;
#endif
	unsigned long csum_ok;
	unsigned long csum_none;
	unsigned long no_reuse_cnt;
	struct mlx4_en_config config;
	int numa_node;
#ifdef CONFIG_COMPAT_LRO_ENABLED
	struct mlx4_en_lro lro;
#endif
};

struct mlx4_en_cq {
	struct mlx4_cq          mcq;
	struct mlx4_hwq_resources wqres;
	int                     ring;
	spinlock_t              lock;
	struct net_device      *dev;
	struct napi_struct	napi;
	int size;
	int buf_size;
	unsigned vector;
	enum cq_type is_tx;
	u16 moder_time;
	u16 moder_cnt;
	struct mlx4_cqe *buf;
	cpumask_t affinity_mask;
#define MLX4_EN_OPCODE_ERROR	0x1e
	u32 tot_rx;
	u32 tot_tx;

#ifdef CONFIG_NET_RX_BUSY_POLL
	unsigned int state;
#define MLX4_EN_CQ_STATEIDLE        0
#define MLX4_EN_CQ_STATENAPI     1    /* NAPI owns this CQ */
#define MLX4_EN_CQ_STATEPOLL     2    /* poll owns this CQ */
#define MLX4_CQ_LOCKED (MLX4_EN_CQ_STATENAPI | MLX4_EN_CQ_STATEPOLL)
#define MLX4_EN_CQ_STATENAPI_YIELD  4    /* NAPI yielded this CQ */
#define MLX4_EN_CQ_STATEPOLL_YIELD  8    /* poll yielded this CQ */
#define CQ_YIELD (MLX4_EN_CQ_STATENAPI_YIELD | MLX4_EN_CQ_STATEPOLL_YIELD)
#define CQ_USER_PEND (MLX4_EN_CQ_STATEPOLL | MLX4_EN_CQ_STATEPOLL_YIELD)
	spinlock_t poll_lock; /* protects from LLS/napi conflicts */
#endif  /* CONFIG_NET_RX_BUSY_POLL */
};

struct mlx4_en_port_profile {
	u32 flags;
	u32 tx_ring_num;
	u32 rx_ring_num;
	u32 tx_ring_size;
	u32 rx_ring_size;
	u8 rx_pause;
	u8 rx_ppp;
	u8 tx_pause;
	u8 tx_ppp;
	u8 num_tx_rings_p_up;
	int rss_rings;
};

struct mlx4_en_profile {
	int rss_xor;
	int udp_rss;
	u8 rss_mask;
	u32 active_ports;
	u32 small_pkt_int;
	u8 no_reset;
	struct mlx4_en_port_profile prof[MLX4_MAX_PORTS + 1];
};

struct mlx4_en_dev {
	struct mlx4_dev		*dev;
	struct pci_dev		*pdev;
	struct mutex		state_lock;
	struct net_device	*pndev[MLX4_MAX_PORTS + 1];
	u32			port_cnt;
	bool			device_up;
	struct mlx4_en_profile	profile;
	u32			LSO_support;
	struct workqueue_struct *workqueue;
	struct device		*dma_device;
	void __iomem		*uar_map;
	struct mlx4_uar		priv_uar;
	struct mlx4_mr		mr;
	u32			priv_pdn;
	spinlock_t		uar_lock;
	u8			mac_removed[MLX4_MAX_PORTS + 1];
	rwlock_t		clock_lock;
	u32			nominal_c_mult;
	struct cyclecounter	cycles;
	struct timecounter	clock;
	unsigned long		last_overflow_check;
	unsigned long		overflow_period;
#if defined (CONFIG_COMPAT_PTP_CLOCK) && (defined (CONFIG_PTP_1588_CLOCK) || defined (CONFIG_PTP_1588_CLOCK_MODULE))
	struct ptp_clock	*ptp_clock;
	struct ptp_clock_info	ptp_clock_info;
#endif
#if defined (CONFIG_COMPAT_TIMECOMPARE) && !(defined (CONFIG_PTP_1588_CLOCK) || defined (CONFIG_PTP_1588_CLOCK_MODULE))
        struct timecompare      compare;
#endif
};


struct mlx4_en_rss_map {
	int base_qpn;
	struct mlx4_qp qps[MAX_RX_RINGS];
	enum mlx4_qp_state state[MAX_RX_RINGS];
	struct mlx4_qp indir_qp;
	enum mlx4_qp_state indir_state;
};
enum mlx4_en_port_flag {
	MLX4_EN_PORT_ANC = 1<<0, /* Auto-negotiation complete */
};

struct mlx4_en_port_state {
	int link_state;
	int link_speed;
	int transceiver;
	int autoneg;
	u32 flags;
};

enum mlx4_en_mclist_act {
	MCLIST_NONE,
	MCLIST_REM,
	MCLIST_ADD,
};

struct mlx4_en_mc_list {
	struct list_head	list;
	enum mlx4_en_mclist_act	action;
	u8			addr[ETH_ALEN];
	u64			reg_id;
#ifdef CONFIG_COMPAT_VXLAN_ENABLED
	u64			tunnel_reg_id;
#endif
};

#ifndef CONFIG_COMPAT_DISABLE_DCB
#ifdef CONFIG_MLX4_EN_DCB
/* Minimal TC BW - setting to 0 will block traffic */
#define MLX4_EN_BW_MIN 1
#define MLX4_EN_BW_MAX 100 /* Utilize 100% of the line */

#define MLX4_EN_TC_ETS 7

enum dcb_pfc_type {
	pfc_disabled = 0,
	pfc_enabled_full,
	pfc_enabled_tx,
	pfc_enabled_rx
};

struct tc_configuration {
	enum dcb_pfc_type  dcb_pfc;
};

struct mlx4_en_dcb_config {
	bool	pfc_mode_enable;
	struct	tc_configuration tc_config[MLX4_EN_NUM_UP];
};

#endif
#endif

struct ethtool_flow_id {
	struct list_head list;
	struct ethtool_rx_flow_spec flow_spec;
	u64 id;
};

enum {
	MLX4_EN_FLAG_PROMISC		= (1 << 0),
	MLX4_EN_FLAG_MC_PROMISC		= (1 << 1),
	/* whether we need to enable hardware loopback by putting dmac
	 * in Tx WQE
	 */
	MLX4_EN_FLAG_ENABLE_HW_LOOPBACK	= (1 << 2),
	/* whether we need to drop packets that hardware loopback-ed */
	MLX4_EN_FLAG_RX_FILTER_NEEDED	= (1 << 3),
	MLX4_EN_FLAG_FORCE_PROMISC	= (1 << 4),
#ifdef CONFIG_MLX4_EN_DCB
	MLX4_EN_FLAG_DCB_ENABLED	= (1 << 5)
#endif
};



enum {
	MLX4_EN_PRIV_FLAGS_PM_QOS = (1 << 0),
	MLX4_EN_PRIV_FLAGS_RSS_HASH_XOR = (1 << 1),
	MLX4_EN_PRIV_FLAGS_FS_EN_L2 = (1 << 2),
	MLX4_EN_PRIV_FLAGS_FS_EN_IPV4 = (1 << 3),
	MLX4_EN_PRIV_FLAGS_FS_EN_TCP = (1 << 4),
	MLX4_EN_PRIV_FLAGS_FS_EN_UDP = (1 << 5),
#ifndef CONFIG_COMPAT_DISABLE_DCB
#ifdef CONFIG_MLX4_EN_DCB
	MLX4_EN_PRIV_FLAGS_DISABLE_32_14_4_E = (1 << 6),
#endif
#endif
	MLX4_EN_PRIV_FLAGS_BLUEFLAME = (1 << 7),
};
#if !defined(CONFIG_COMPAT_DISABLE_DCB) && defined(CONFIG_MLX4_EN_DCB)
#define MLX4_EN_PRIV_NUM_FLAGS 8
#else
#define MLX4_EN_PRIV_NUM_FLAGS 7
#endif

#define MLX4_EN_MAC_HASH_SIZE (1 << BITS_PER_BYTE)
#define MLX4_EN_MAC_HASH_IDX 5

struct en_port {
	struct kobject		kobj_vf;
	struct kobject		kobj_stats;
	struct mlx4_dev		*dev;
	u8			port_num;
	u8			vport_num;
};

struct vlan_start_index {
	u16 vlan_id;
	u8 start_tx_index;
};

struct mlx4_en_priv {
	struct mlx4_en_dev *mdev;
	struct mlx4_en_port_profile *prof;
	struct net_device *dev;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0))
	struct vlan_group *vlgrp;
#endif
	unsigned long active_vlans[BITS_TO_LONGS(VLAN_N_VID)];
	struct net_device_stats stats;
	struct net_device_stats ret_stats;
	struct mlx4_en_port_state port_state;
	spinlock_t stats_lock;
	struct ethtool_flow_id ethtool_rules[MAX_NUM_OF_FS_RULES];
	/* To allow rules removal while port is going down */
	struct list_head ethtool_list;

	unsigned long last_moder_packets[MAX_RX_RINGS];
	unsigned long last_moder_tx_packets;
	unsigned long last_moder_bytes[MAX_RX_RINGS];
	unsigned long last_moder_jiffies;
	int last_moder_time[MAX_RX_RINGS];
	u16 rx_usecs;
	u16 rx_frames;
	u16 tx_usecs;
	u16 tx_frames;
	u32 pkt_rate_low;
	u16 rx_usecs_low;
	u32 pkt_rate_high;
	u16 rx_usecs_high;
	u16 sample_interval;
	u16 adaptive_rx_coal;
	u32 msg_enable;
	u32 loopback_ok;
	u32 validate_loopback;

	/* pm_qos related variables */
	unsigned long last_cstate_jiffies;
	unsigned long last_packets;
	int last_cpu_dma_latency;
	struct pm_qos_request pm_qos_req;

	struct mlx4_hwq_resources res;
	int link_state;
	int last_link_state;
	bool port_up;
	int port;
	int registered;
	int allocated;
	int stride;
	unsigned char current_mac[ETH_ALEN + 2];
	int mac_index;
	unsigned max_mtu;
	int base_qpn;
	int cqe_factor;
	int cqe_size;

	struct mlx4_en_rss_map rss_map;
	__be32 ctrl_flags;
	u32 flags;
	u32 pflags;
	u8 num_tx_rings_p_up;
	u32 tx_ring_num;
	u32 rx_ring_num;
	u32 rx_skb_size;
	struct mlx4_en_frag_info frag_info[MLX4_EN_MAX_RX_FRAGS];
	u16 num_frags;
	u16 log_rx_info;

	struct mlx4_en_tx_ring **tx_ring;
	struct mlx4_en_rx_ring *rx_ring[MAX_RX_RINGS];
	struct mlx4_en_cq **tx_cq;
	struct mlx4_en_cq *rx_cq[MAX_RX_RINGS];
	struct mlx4_qp drop_qp;
	struct work_struct rx_mode_task;
	struct work_struct watchdog_task;
	struct work_struct linkstate_task;
	struct delayed_work stats_task;
	struct delayed_work service_task;
	struct work_struct vxlan_add_task;
	struct work_struct vxlan_del_task;
	struct mlx4_en_perf_stats pstats;
	struct mlx4_en_pkt_stats pkstats;
	struct mlx4_en_flow_stats flowstats[MLX4_NUM_PRIORITIES];
	struct mlx4_en_port_stats port_stats;
	struct mlx4_en_vport_stats vport_stats;
	struct mlx4_en_vf_stats vf_stats;
	DECLARE_BITMAP(stats_bitmap, NUM_ALL_STATS);
	struct list_head mc_list;
	struct list_head curr_list;
	u64 broadcast_id;
	struct mlx4_en_stat_out_mbox hw_stats;
	int vids[128];
	bool wol;
	struct device *ddev;
	struct dentry *dev_root;
	struct mlx4_en_config config;
	u32 counter_index;
#define MLX4_EN_MAC_HASH_IDX 5
	struct hlist_head mac_hash[MLX4_EN_MAC_HASH_SIZE];

#ifndef CONFIG_COMPAT_DISABLE_DCB
#ifdef CONFIG_MLX4_EN_DCB
	struct ieee_ets ets;
	u16 maxrate[IEEE_8021QAZ_MAX_TCS];
	u8 dcbx_cap;
	u8 dcb_set_bitmap;
	struct mlx4_en_dcb_config dcb_cfg;
	struct mlx4_en_dcb_config temp_dcb_cfg;
	enum dcbnl_cndd_states cndd_state[IEEE_8021QAZ_MAX_TCS];
#endif
#endif
#ifdef CONFIG_RFS_ACCEL
	spinlock_t filters_lock;
	int last_filter_id;
	struct list_head filters;
	struct hlist_head filter_hash[1 << MLX4_EN_FILTER_HASH_SHIFT];
#endif
	struct en_port *vf_ports[MLX4_MAX_NUM_VF];
	unsigned long last_ifq_jiffies;
	u64 if_counters_rx_errors;
	u64 if_counters_rx_no_buffer;
#ifdef CONFIG_COMPAT_VXLAN_ENABLED
	u64 tunnel_reg_id;
	__be16 vxlan_port;
#endif
#ifdef CONFIG_COMPAT_EN_SYSFS
	int sysfs_group_initialized;
#endif
	/* NUMA based affinity variables */
	cpumask_t numa_mask;
	cpumask_t non_numa_mask;
	const struct cpumask *current_mask;
	int current_cpu;
	struct vlan_start_index vsi[MAX_VLANS];
	u8 rx_csum_mode_port;
};

enum mlx4_en_wol {
	MLX4_EN_WOL_MAGIC = (1ULL << 61),
	MLX4_EN_WOL_ENABLED = (1ULL << 62),
};

struct mlx4_mac_entry {
	struct hlist_node hlist;
	unsigned char mac[ETH_ALEN + 2];
	u64 reg_id;
	struct rcu_head rcu;
};

static inline struct mlx4_cqe *mlx4_en_get_cqe(void *buf, int idx, int cqe_sz)
{
	return buf + idx * cqe_sz;
}

#ifdef CONFIG_NET_RX_BUSY_POLL
static inline void mlx4_en_cq_init_lock(struct mlx4_en_cq *cq)
{
	spin_lock_init(&cq->poll_lock);
	cq->state = MLX4_EN_CQ_STATEIDLE;
}

/* called from the device poll rutine to get ownership of a cq */
static inline bool mlx4_en_cq_lock_napi(struct mlx4_en_cq *cq)
{
	int rc = true;
	spin_lock(&cq->poll_lock);
	if (cq->state & MLX4_CQ_LOCKED) {
		WARN_ON(cq->state & MLX4_EN_CQ_STATENAPI);
		cq->state |= MLX4_EN_CQ_STATENAPI_YIELD;
		rc = false;
	} else
		/* we don't care if someone yielded */
		cq->state = MLX4_EN_CQ_STATENAPI;
	spin_unlock(&cq->poll_lock);
	return rc;
}

/* returns true is someone tried to get the cq while napi had it */
static inline bool mlx4_en_cq_unlock_napi(struct mlx4_en_cq *cq)
{
	int rc = false;
	spin_lock(&cq->poll_lock);
	WARN_ON(cq->state & (MLX4_EN_CQ_STATEPOLL |
			     MLX4_EN_CQ_STATENAPI_YIELD));

	if (cq->state & MLX4_EN_CQ_STATEPOLL_YIELD)
		rc = true;
	cq->state = MLX4_EN_CQ_STATEIDLE;
	spin_unlock(&cq->poll_lock);
	return rc;
}

/* called from mlx4_en_low_latency_poll() */
static inline bool mlx4_en_cq_lock_poll(struct mlx4_en_cq *cq)
{
	int rc = true;
	spin_lock_bh(&cq->poll_lock);
	if ((cq->state & MLX4_CQ_LOCKED)) {
		struct net_device *dev = cq->dev;
		struct mlx4_en_priv *priv = netdev_priv(dev);
		struct mlx4_en_rx_ring *rx_ring = priv->rx_ring[cq->ring];

		cq->state |= MLX4_EN_CQ_STATEPOLL_YIELD;
		rc = false;
#ifdef LL_EXTENDED_STATS
		rx_ring->yields++;
#endif
	} else
		/* preserve yield marks */
		cq->state |= MLX4_EN_CQ_STATEPOLL;
	spin_unlock_bh(&cq->poll_lock);
	return rc;
}

/* returns true if someone tried to get the cq while it was locked */
static inline bool mlx4_en_cq_unlock_poll(struct mlx4_en_cq *cq)
{
	int rc = false;
	spin_lock_bh(&cq->poll_lock);
	WARN_ON(cq->state & (MLX4_EN_CQ_STATENAPI));

	if (cq->state & MLX4_EN_CQ_STATEPOLL_YIELD)
		rc = true;
	cq->state = MLX4_EN_CQ_STATEIDLE;
	spin_unlock_bh(&cq->poll_lock);
	return rc;
}

/* true if a socket is polling, even if it did not get the lock */
static inline bool mlx4_en_cq_ll_polling(struct mlx4_en_cq *cq)
{
	WARN_ON(!(cq->state & MLX4_CQ_LOCKED));
	return cq->state & CQ_USER_PEND;
}
#else
static inline void mlx4_en_cq_init_lock(struct mlx4_en_cq *cq)
{
}

static inline bool mlx4_en_cq_lock_napi(struct mlx4_en_cq *cq)
{
	return true;
}

static inline bool mlx4_en_cq_unlock_napi(struct mlx4_en_cq *cq)
{
	return false;
}

static inline bool mlx4_en_cq_lock_poll(struct mlx4_en_cq *cq)
{
	return false;
}

static inline bool mlx4_en_cq_unlock_poll(struct mlx4_en_cq *cq)
{
	return false;
}

static inline bool mlx4_en_cq_ll_polling(struct mlx4_en_cq *cq)
{
	return false;
}
#endif /* CONFIG_NET_RX_BUSY_POLL */

#define MLX4_EN_WOL_DO_MODIFY (1ULL << 63)

void mlx4_en_update_loopback_state(struct net_device *dev,
				netdev_features_t features);

void mlx4_en_destroy_netdev(struct net_device *dev);
int mlx4_en_init_netdev(struct mlx4_en_dev *mdev, int port,
			struct mlx4_en_port_profile *prof);

int mlx4_en_start_port(struct net_device *dev);
void mlx4_en_stop_port(struct net_device *dev);

void mlx4_en_free_resources(struct mlx4_en_priv *priv);
int mlx4_en_alloc_resources(struct mlx4_en_priv *priv);

int mlx4_en_pre_config(struct mlx4_en_priv *priv);
int mlx4_en_create_cq(struct mlx4_en_priv *priv, struct mlx4_en_cq **pcq,
		      int entries, int ring, enum cq_type mode, int node);
void mlx4_en_set_cq_affinity(struct mlx4_en_priv *priv,
				    struct mlx4_en_cq *cq);
void mlx4_en_destroy_cq(struct mlx4_en_priv *priv, struct mlx4_en_cq **pcq);
int mlx4_en_activate_cq(struct mlx4_en_priv *priv, struct mlx4_en_cq *cq,
			int cq_idx);
void mlx4_en_deactivate_cq(struct mlx4_en_priv *priv, struct mlx4_en_cq *cq);
int mlx4_en_set_cq_moder(struct mlx4_en_priv *priv, struct mlx4_en_cq *cq);
int mlx4_en_arm_cq(struct mlx4_en_priv *priv, struct mlx4_en_cq *cq);

void mlx4_en_tx_irq(struct mlx4_cq *mcq);
#ifdef CONFIG_COMPAT_SELECT_QUEUE_ACCEL
u16 mlx4_en_select_queue(struct net_device *dev, struct sk_buff *skb,
#ifdef CONFIG_COMPAT_SELECT_QUEUE_FALLBACK
 			 void *accel_priv, select_queue_fallback_t fallback);
#else
			 void *accel_priv);
#endif
#else /* CONFIG_COMPAT_SELECT_QUEUE_ACCEL */
u16 mlx4_en_select_queue(struct net_device *dev, struct sk_buff *skb);
#endif
netdev_tx_t mlx4_en_xmit(struct sk_buff *skb, struct net_device *dev);

int mlx4_en_create_tx_ring(struct mlx4_en_priv *priv,
			   struct mlx4_en_tx_ring **pring,
			   u32 size, u16 stride, int node);
void mlx4_en_destroy_tx_ring(struct mlx4_en_priv *priv,
			     struct mlx4_en_tx_ring **pring);
int mlx4_en_activate_tx_ring(struct mlx4_en_priv *priv,
			     struct mlx4_en_tx_ring *ring,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)) || defined (CONFIG_COMPAT_NEW_TX_RING_SCHEME)
			     int cq, int user_prio,
#else
			     int cq,
#endif
			     int idx);
void mlx4_en_deactivate_tx_ring(struct mlx4_en_priv *priv,
				struct mlx4_en_tx_ring *ring);
void mlx4_en_set_num_rx_rings(struct mlx4_en_dev *mdev);
void mlx4_en_calc_rx_buf(struct net_device *dev);
int mlx4_en_create_rx_ring(struct mlx4_en_priv *priv,
			   struct mlx4_en_rx_ring **pring,
			   u32 size, int node);
void mlx4_en_destroy_rx_ring(struct mlx4_en_priv *priv,
			     struct mlx4_en_rx_ring **pring,
			     u32 size, u16 stride);
int mlx4_en_activate_rx_rings(struct mlx4_en_priv *priv);
void mlx4_en_deactivate_rx_ring(struct mlx4_en_priv *priv,
				struct mlx4_en_rx_ring *ring);
int mlx4_en_process_rx_cq(struct net_device *dev,
			  struct mlx4_en_cq *cq,
			  int budget);
int mlx4_en_poll_rx_cq(struct napi_struct *napi, int budget);
int mlx4_en_poll_tx_cq(struct napi_struct *napi, int budget);
void mlx4_en_fill_qp_context(struct mlx4_en_priv *priv, int size, int stride,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)) || defined (CONFIG_COMPAT_NEW_TX_RING_SCHEME)
		int is_tx, int rss, int qpn, int cqn, int user_prio,
#else
		int is_tx, int rss, int qpn, int cqn,
#endif
		struct mlx4_qp_context *context, int idx);
int mlx4_en_change_mcast_loopback(struct mlx4_en_priv *priv, struct mlx4_qp *qp,
				  int loopback);
void mlx4_en_sqp_event(struct mlx4_qp *qp, enum mlx4_event event);
int mlx4_en_map_buffer(struct mlx4_buf *buf, int numa_id);
void mlx4_en_unmap_buffer(struct mlx4_buf *buf);

int mlx4_en_config_rss_steer(struct mlx4_en_priv *priv);
void mlx4_en_release_rss_steer(struct mlx4_en_priv *priv);
void mlx4_en_remove_ethtool_rules(struct mlx4_en_priv *priv);
int mlx4_en_create_drop_qp(struct mlx4_en_priv *priv);
void mlx4_en_destroy_drop_qp(struct mlx4_en_priv *priv);
int mlx4_en_free_tx_buf(struct net_device *dev, struct mlx4_en_tx_ring *ring);
void mlx4_en_rx_irq(struct mlx4_cq *mcq);

int mlx4_SET_MCAST_FLTR(struct mlx4_dev *dev, u8 port, u64 mac, u64 clear, u8 mode);
int mlx4_SET_VLAN_FLTR(struct mlx4_dev *dev, struct mlx4_en_priv *priv);

int mlx4_en_DUMP_ETH_STATS(struct mlx4_en_dev *mdev, u8 port, u8 reset);
int mlx4_en_QUERY_PORT(struct mlx4_en_dev *mdev, u8 port);
int mlx4_en_get_vport_stats(struct mlx4_en_dev *mdev, u8 port);
int mlx4_get_vport_ethtool_stats(struct mlx4_dev *dev, int port,
			 struct mlx4_en_vport_stats *vport_stats,
			 int reset);
void mlx4_en_create_debug_files(struct mlx4_en_priv *priv);
void mlx4_en_delete_debug_files(struct mlx4_en_priv *priv);
int mlx4_en_register_debugfs(void);
void mlx4_en_unregister_debugfs(void);

#ifndef CONFIG_COMPAT_DISABLE_DCB
#ifdef CONFIG_MLX4_EN_DCB
extern const struct dcbnl_rtnl_ops mlx4_en_dcbnl_ops;
extern const struct dcbnl_rtnl_ops mlx4_en_dcbnl_pfc_ops;
int mlx4_disable_32_14_4_e_write(struct mlx4_dev *dev, u8 config, int port);
int mlx4_disable_32_14_4_e_read(struct mlx4_dev *dev, u8 *config, int port);
#endif
#endif

#ifdef CONFIG_COMPAT_QCN

int mlx4_en_dcbnl_ieee_getqcn(struct net_device *dev,
				struct ieee_qcn *qcn);
int mlx4_en_dcbnl_ieee_setqcn(struct net_device *dev,
				struct ieee_qcn *qcn);
int mlx4_en_dcbnl_ieee_getqcnstats(struct net_device *dev,
					struct ieee_qcn_stats *qcn_stats);
#endif

#ifdef CONFIG_COMPAT_EN_SYSFS
int mlx4_en_sysfs_create(struct net_device *dev);
void mlx4_en_sysfs_remove(struct net_device *dev);
#endif

#ifdef CONFIG_COMPAT_MAXRATE

int mlx4_en_dcbnl_ieee_setmaxrate(struct net_device *dev,
				  struct ieee_maxrate *maxrate);
int mlx4_en_dcbnl_ieee_getmaxrate(struct net_device *dev,
				  struct ieee_maxrate *maxrate);
#endif

#ifdef CONFIG_COMPAT_NUM_CHANNELS
struct ethtool_channels {
	__u32   cmd;
	__u32   max_rx;
	__u32   max_tx;
	__u32   max_other;
	__u32   max_combined;
	__u32   rx_count;
	__u32   tx_count;
	__u32   other_count;
	__u32   combined_count;
};

int mlx4_en_set_channels(struct net_device *dev,
			 struct ethtool_channels *channel);
void mlx4_en_get_channels(struct net_device *dev,
			  struct ethtool_channels *channel);
#endif

int mlx4_en_setup_tc(struct net_device *dev, u8 up);

#ifdef CONFIG_RFS_ACCEL
#ifdef CONFIG_COMPAT_IS_NETDEV_EXTENDED
#define mlx4_en_rx_cpu_rmap(__priv) netdev_extended(__priv->dev)->rfs_data.rx_cpu_rmap
#endif
void mlx4_en_cleanup_filters(struct mlx4_en_priv *priv);
#endif

#define MLX4_EN_NUM_SELF_TEST	5
void mlx4_en_ex_selftest(struct net_device *dev, u32 *flags, u64 *buf);
void mlx4_en_ptp_overflow_check(struct mlx4_en_dev *mdev);

/*
 * Functions for time stamping
 */
#define SKBTX_HW_TSTAMP (1 << 0)
#define SKBTX_IN_PROGRESS (1 << 2)

u64 mlx4_en_get_cqe_ts(struct mlx4_cqe *cqe);
void mlx4_en_fill_hwtstamps(struct mlx4_en_dev *mdev,
			    struct skb_shared_hwtstamps *hwts,
			    u64 timestamp);
void mlx4_en_init_timestamp(struct mlx4_en_dev *mdev);
#if defined (CONFIG_COMPAT_PTP_CLOCK) && (defined (CONFIG_PTP_1588_CLOCK) || defined (CONFIG_PTP_1588_CLOCK_MODULE))
void mlx4_en_remove_timestamp(struct mlx4_en_dev *mdev);
#endif
int mlx4_en_reset_config(struct net_device *dev);

/* Functions for caching and restoring statistics */
int mlx4_en_get_sset_count(struct net_device *dev, int sset);
void mlx4_en_get_ethtool_stats(struct net_device *dev,
			       struct ethtool_stats *stats,
			       u64 *data);
void mlx4_en_restore_ethtool_stats(struct mlx4_en_priv *priv,
				    u64 *data);

/*
 * Functions for dcbnl
 */
#ifndef CONFIG_COMPAT_DISABLE_DCB
#ifdef CONFIG_MLX4_EN_DCB
int mlx4_en_dcbnl_ieee_getets(struct net_device *dev, struct ieee_ets *ets);
int mlx4_en_dcbnl_ieee_setets(struct net_device *dev, struct ieee_ets *ets);
int mlx4_en_restorepfc(struct net_device *dev);
int mlx4_en_dcbnl_ieee_getmaxrate(struct net_device *dev, struct ieee_maxrate *maxrate);
int mlx4_en_dcbnl_ieee_setmaxrate(struct net_device *dev, struct ieee_maxrate *maxrate);
#endif
#endif

/*
 * Globals
 */
extern const struct ethtool_ops mlx4_en_ethtool_ops;
#ifdef CONFIG_COMPAT_ETHTOOL_OPS_EXT
extern const struct ethtool_ops_ext mlx4_en_ethtool_ops_ext;
#endif

/*
 * Defines for link speed - needed by selftest
 */
#define MLX4_EN_LINK_SPEED_100M 100
#define MLX4_EN_LINK_SPEED_1G	1000
#define MLX4_EN_LINK_SPEED_10G	10000
#define MLX4_EN_LINK_SPEED_20G	20000
#define MLX4_EN_LINK_SPEED_40G	40000
#define MLX4_EN_LINK_SPEED_56G	56000

/*
 * printk / logging functions
 */
#if (defined CONFIG_COMPAT_DISABLE_VA_FORMAT_PRINT || defined CONFIG_X86_XEN)
#define en_print(level, priv, format, arg...)                   \
        {                                                       \
        if ((priv)->registered)                                 \
                printk(level "%s: %s: " format, DRV_NAME,       \
                        (priv->dev)->name, ## arg);             \
        else                                                    \
                printk(level "%s: %s: Port %d: " format,        \
                        DRV_NAME, dev_name(&priv->mdev->pdev->dev), \
                        (priv)->port, ## arg);                  \
        }
#else
__printf(3, 4)
int en_print(const char *level, const struct mlx4_en_priv *priv,
	     const char *format, ...);
#endif

#define en_dbg(mlevel, priv, format, arg...)			\
do {								\
	if (NETIF_MSG_##mlevel & priv->msg_enable)		\
		en_print(KERN_DEBUG, priv, format, ##arg);	\
} while (0)
#define en_warn(priv, format, arg...)			\
	en_print(KERN_WARNING, priv, format, ##arg)
#define en_err(priv, format, arg...)			\
	en_print(KERN_ERR, priv, format, ##arg)
#define en_info(priv, format, arg...)			\
	en_print(KERN_INFO, priv, format, ## arg)

#define mlx4_err(mdev, format, arg...)			\
	pr_err("%s %s: " format, DRV_NAME,		\
	       dev_name(&mdev->pdev->dev), ##arg)
#define mlx4_info(mdev, format, arg...)			\
	pr_info("%s %s: " format, DRV_NAME,		\
		dev_name(&mdev->pdev->dev), ##arg)
#define mlx4_warn(mdev, format, arg...)			\
	pr_warning("%s %s: " format, DRV_NAME,		\
		   dev_name(&mdev->pdev->dev), ##arg)

#ifdef CONFIG_COMPAT_INDIR_SETTING
u32 mlx4_en_get_rxfh_indir_size(struct net_device *dev);
int mlx4_en_get_rxfh_indir(struct net_device *dev, u32 *ring_index);
int mlx4_en_set_rxfh_indir(struct net_device *dev, const u32 *ring_index);
#endif
#ifdef CONFIG_COMPAT_LOOPBACK
int mlx4_en_set_features(struct net_device *netdev,
		netdev_features_t features);
#endif
#endif
