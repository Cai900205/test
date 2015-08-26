/* Copyright (c) 2011-2013 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *	 notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *	 notice, this list of conditions and the following disclaimer in the
 *	 documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *	 names of its contributors may be used to endorse or promote products
 *	 derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ppac.h>
#include <usdpaa/fman.h>
#include "ppam_if.h"
#include <ppac_interface.h>
#include <error.h>

#include <usdpaa/fsl_dpa_stats.h>
#include <inttypes.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>

#include <linux/if_vlan.h>

#include "fmc.h"

/* Override the default command prompt */
const char ppam_prompt[] = "fragmentation_demo > ";

struct fmc_model_t cmodel;

/*
 * PPAM-overridable paths to FMan configuration files.
 */
const char ppam_pcd_path[] = __stringify(DEF_PCD_PATH);
const char ppam_cfg_path[] = __stringify(DEF_CFG_PATH);
const char ppam_pdl_path[] = __stringify(DEF_PDL_PATH);

/* Ports and fman used */
struct ppam_arguments {
	int	fm;
	int	eth;
	int	oh;
};

struct ppam_arguments ppam_args = {
	.fm	= 1,
	.eth	= 0,
	.oh	= 2
};

/* VLAN header definition */
struct vlan_hdr {
	__u16 tci;
	__u16 type;
};

int dpa_stats_id;
int cnt_ids[3];
void *storage;

enum dpa_stats_op {
	dpa_stats_get_async = 0,
	dpa_stats_get_sync,
	dpa_stats_reset
};

static int create_dpa_stats_counters(void);

struct ppam_arguments ppam_args;

#define SCRATCH_BPID		4

#define DMA_MEM_BP4_BPID	4
#define DMA_MEM_BP4_SIZE	1728
#define DMA_MEM_BP4_NUM		0

#define DMA_MAP_SIZE	0x4000000 /*64M*/

#define DMA_MEM_BPOOL \
	(DMA_MEM_BP4_SIZE * DMA_MEM_BP4_NUM)

#define ENABLE_PROMISC

static const struct bpool {
	int bpid;
	unsigned int num;
	unsigned int size;
} bpool[] = {
		{ DMA_MEM_BP4_BPID, DMA_MEM_BP4_NUM, DMA_MEM_BP4_SIZE},
		{ -1, 0, 0 }
};

static int init_buffer_pools(void)
{
	const struct bpool *bp = bpool;

	/* - map DMA mem */
	dma_mem_generic = dma_mem_create(DMA_MAP_FLAG_ALLOC, NULL,
		DMA_MAP_SIZE);
	if (!dma_mem_generic) {
		fprintf(stderr, "error: dma_mem init, continuing\n");
		return -EINVAL;
	}

	while (bp->bpid != -1) {
		int err = ppac_prepare_bpid(bp->bpid, bp->num, bp->size, 0,
					bp->bpid == SCRATCH_BPID ? 0 : 1,
					NULL, NULL);
		if (err) {
			fprintf(stderr, "error: bpool (%d) init failure\n",
				bp->bpid);
			return err;
		}
		bp++;
	}

	return 0;
}

int ppam_init(void)
{
	const char *pcd_path = ppam_pcd_path;
	const char *cfg_path = ppam_cfg_path;
	const char *pdl_path = ppam_pdl_path;
	const char *envp;
	int err = 0;

	envp = getenv("DEF_PCD_PATH");
	if (envp != NULL)
		pcd_path = envp;

	envp = getenv("DEF_CFG_PATH");
	if (envp != NULL)
		cfg_path = envp;

	envp = getenv("DEF_PDL_PATH");
	if (envp != NULL)
		pdl_path = envp;

	/* Parse the input XML files and create the FMC Model */
	err = fmc_compile(&cmodel,
			cfg_path,
			pcd_path,
			pdl_path,
			NULL,
			0,
			0,
			NULL);
	if (err != 0) {
		error(0, err, "Failed to create the FMC Model\n");
		return -err;
	}

	/* Execute the obtained FMC Model */
	err = fmc_execute(&cmodel);
	if (err != 0) {
		error(0, err, "Failed to execute the FMC Model\n");
		return -err;
	}

	TRACE("PCD configuration successfully applied.\n");

	err = init_buffer_pools();
	if (unlikely(err < 0)) {
		error(0, err, "Fragmentation_demo buffer pools init\n");
		return err;
	}

	TRACE("Initialized fragmentation_demo buffer pools\n");

	/* Create DPA Stats counters */
	err = create_dpa_stats_counters();
	if (err != 0) {
		error(0, err, "Failed to create the DPA Stats counters\n");
		return -err;
	}

	return 0;
}

void ppam_finish()
{
	int err = 0;
	uint32_t i = 0;

	for (i = 0; i < sizeof(cnt_ids)/sizeof(cnt_ids[0]); i++) {
		err = dpa_stats_remove_counter(cnt_ids[i]);
		if (err != 0)
			error(0, err, "Failed to remove counter");
		 else
			TRACE("DPA Stats counter successfully removed.\n");
	}

	err = dpa_stats_free(dpa_stats_id);
	if (err < 0)
		error(0, -err, "Failed to release DPA Stats instance\n");

	TRACE("DPA Stats instance successfully released\n");

	/* Release DPA Stats instance */
	 dpa_stats_lib_exit();
	TRACE("DPA Stats library successfully released\n");

	/* Clean-up the FMC model */
	err = fmc_clean(&cmodel);
	if (err != 0)
		error(0, err, "Failed to clean-up PCD configuration");
	 else
		TRACE("PCD configuration successfully restored\n");
}

static int ppam_interface_init(struct ppam_interface *p,
			       const struct fm_eth_port_cfg *cfg,
			       unsigned int num_tx_fqs,
			       uint32_t *flags)
{
	p->num_tx_fqids = num_tx_fqs;
	p->tx_fqids = malloc(p->num_tx_fqids * sizeof(*p->tx_fqids));
	if (!p->tx_fqids)
		return -ENOMEM;

	if (cfg->fman_if->mac_type == fman_offline)
		*flags |= PPAM_TX_FQ_NO_BUF_DEALLOC;

#ifdef ENABLE_PROMISC
	if ((cfg->fman_if->mac_type != fman_offline) &&
			(cfg->fman_if->mac_type != fman_mac_less))
		/* Enable promiscuous mode: */
		fman_if_promiscuous_enable(cfg->fman_if);
#endif /* ENABLE_PROMISC */

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
	const struct qm_fd *fd = &dqrr->fd;
	ppac_drop_frame(fd);
}

static int ppam_rx_default_init(struct ppam_rx_default *p,
				struct ppam_interface *_if,
				unsigned int idx,
				struct qm_fqd_stashing *stash_opts)
{
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
	const struct qm_fd *fd = &dqrr->fd;

	ppac_drop_frame(fd);
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
	const struct qm_fd *fd = &dqrr->fd;
	ppac_drop_frame(fd);
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
	const struct qm_fd *fd = &dqrr->fd;
	ppac_drop_frame(fd);
}

static int ppam_rx_hash_init(struct ppam_rx_hash *p, struct ppam_interface *_if,
			     unsigned idx, struct qm_fqd_stashing *stash_opts)
{
	int fman_idx, mac_idx;
	struct ppac_interface *ppac_if;
	static struct ppac_interface *ib_oh_if, *eth_if;
	struct ppam_interface *__if;

	/* loop over ppac interfaces and get the ports */
	if (!ib_oh_if && !eth_if) {
		list_for_each_entry(ppac_if, &ifs, node) {
			fman_idx = ppac_if->port_cfg->fman_if->fman_idx;
			mac_idx = ppac_if->port_cfg->fman_if->mac_idx;

			/* offline port */
			if (ppac_if->port_cfg->fman_if->mac_type == fman_offline) {
				if (fman_idx == ppam_args.fm && mac_idx == ppam_args.oh)
					ib_oh_if = ppac_if;
			}

			/* ethernet port */
			if (ppac_if->port_cfg->fman_if->mac_type != fman_offline &&
				ppac_if->port_cfg->fman_if->mac_idx == ppam_args.eth){
				eth_if = ppac_if;
			}
		}
	}

	if (!ib_oh_if) {
		error(0, ENODEV, "Offline port fm%d#%d not found\n",
			ppam_args.fm, ppam_args.oh);
		return -ENODEV;
	}

	if (!eth_if) {
		error(0, ENODEV, "Ethernet port fm%d#%d not found\n",
			ppam_args.fm, ppam_args.eth);
		return -ENODEV;
	}

	if (&ib_oh_if->ppam_data == _if) {
		__if = &eth_if->ppam_data;
		p->tx_fqid = __if->tx_fqids[idx % __if->num_tx_fqids];
		TRACE("Mapping Rx FQ %p:%d --> Tx FQID %d\n",
			p, idx, p->tx_fqid);
	}

	if (&eth_if->ppam_data == _if) {
		__if = &ib_oh_if->ppam_data;
		p->tx_fqid = __if->tx_fqids[idx % __if->num_tx_fqids];
		TRACE("Mapping Rx FQ %p:%d --> Tx FQID %d\n",
			p, idx, p->tx_fqid);
	}

	TRACE("Mapping Rx FQ %p:%d --> Tx FQID %d\n", p, idx, p->tx_fqid);

	return 0;
}

static void ppam_rx_hash_finish(struct ppam_rx_hash *p,
				struct ppam_interface *_if,
				unsigned idx)
{
}

/* Swap 6-byte MAC headers "efficiently" (hopefully) */
static inline void ether_header_swap(struct ether_header *prot_eth)
{
	register u32 a, b, c;
	u32 *overlay = (u32 *)prot_eth;
	a = overlay[0];
	b = overlay[1];
	c = overlay[2];
	overlay[0] = (b << 16) | (c >> 16);
	overlay[1] = (c << 16) | (a >> 16);
	overlay[2] = (a << 16) | (b >> 16);
}

static inline void ppam_rx_hash_cb(struct ppam_rx_hash *p,
				   const struct qm_dqrr_entry *dqrr)
{
	void *addr;
	void *annotations;
	struct ether_header *prot_eth;
	const struct qm_fd *fd = &dqrr->fd;
	struct qm_fd _fd;
	uint32_t tx_fqid;
	void *next_header;
	bool continue_parsing;
	uint16_t proto, len = 0;

	annotations = __dma_mem_ptov(qm_fd_addr(fd));
	TRACE("Rx: 2fwd	 fqid=%d\n", dqrr->fqid);
	switch (fd->format)	{
	case qm_fd_contig:
		TRACE("FD format = qm_fd_contig\n");
		addr = annotations + fd->offset;
		prot_eth = addr;
		break;

	case qm_fd_sg:
		TRACE("FD format = qm_fd_sg\n");
		addr = annotations + fd->offset;
		prot_eth = __dma_mem_ptov(qm_sg_entry_get64(addr)) +
				((struct qm_sg_entry *)addr)->offset;
		break;

	default:
		TRACE("FD format not supported!\n");
		BUG();
	}

	next_header = (prot_eth + 1);
	proto = prot_eth->ether_type;
	len = sizeof(struct ether_header);

	TRACE("	     phys=0x%"PRIx64", virt=%p, offset=%d, len=%d, bpid=%d\n",
	      qm_fd_addr(fd), addr, fd->offset, fd->length20, fd->bpid);
	TRACE("	     dhost="ETH_MAC_PRINTF_FMT"\n",
	      prot_eth->ether_dhost[0], prot_eth->ether_dhost[1],
	      prot_eth->ether_dhost[2], prot_eth->ether_dhost[3],
	      prot_eth->ether_dhost[4], prot_eth->ether_dhost[5]);
	TRACE("	     shost="ETH_MAC_PRINTF_FMT"\n",
	      prot_eth->ether_shost[0], prot_eth->ether_shost[1],
	      prot_eth->ether_shost[2], prot_eth->ether_shost[3],
	      prot_eth->ether_shost[4], prot_eth->ether_shost[5]);
	TRACE("	     ether_type=%04x\n", prot_eth->ether_type);
	/* Eliminate ethernet broadcasts. */
	if (prot_eth->ether_dhost[0] & 0x01)
		TRACE("	     -> dropping broadcast packet\n");
	else {
	continue_parsing = TRUE;
	while (continue_parsing) {
		switch (proto) {
		case ETHERTYPE_IP:
			TRACE("	       -> it's ETHERTYPE_IP!\n");
			{
			struct iphdr *iphdr = (typeof(iphdr))(next_header);
#ifdef ENABLE_TRACE
			u8 *src = (void *)&iphdr->saddr;
			u8 *dst = (void *)&iphdr->daddr;
			TRACE("		  ver=%d,ihl=%d,tos=%d,len=%d,id=%d\n",
				iphdr->version, iphdr->ihl, iphdr->tos,
				iphdr->tot_len, iphdr->id);
			TRACE("		  frag_off=%d,ttl=%d,prot=%d,"
				"csum=0x%04x\n", iphdr->frag_off, iphdr->ttl,
				iphdr->protocol, iphdr->check);
			TRACE("		  src=%d.%d.%d.%d\n",
				src[0], src[1], src[2], src[3]);
			TRACE("		  dst=%d.%d.%d.%d\n",
				dst[0], dst[1], dst[2], dst[3]);
#endif
			/* switch ethernet src/dest MAC addresses */
			ether_header_swap(prot_eth);
			TRACE("Tx: 2fwd	 fqid=%d\n", p->tx_fqid);
			TRACE("	     phys=0x%"PRIx64", offset=%d, len=%d,"
				" bpid=%d\n", qm_fd_addr(fd), fd->offset,
				fd->length20, fd->bpid);

			/* Frame received on Offline port PCD FQ range */
			if (!p->tx_fqid) {
				BUG_ON(fd->offset < sizeof(tx_fqid));
				tx_fqid = *(uint32_t *)annotations;
			} else
				tx_fqid = p->tx_fqid;
			/* IPv4 frame may contain ESP padding */
			_fd = *fd;
			_fd.length20 = len + iphdr->tot_len;

			ppac_send_frame(tx_fqid, &_fd);
			continue_parsing = FALSE;
			}
			return;
		case ETHERTYPE_IPV6:
			TRACE("	       -> it's ETHERTYPE_IPV6!\n");
			{
			struct ip6_hdr *ipv6hdr = (typeof(ipv6hdr))
							(next_header);

			TRACE("	ver=%d, priority=%d, payload_len=%d,"
				" nexthdr=%d, hop_limit=%d\n",
				((ipv6hdr->ip6_vfc & 0xf0) >> 4),
				((ipv6hdr->ip6_flow & 0x0ff00000) >> 20),
				ipv6hdr->ip6_plen,
				ipv6hdr->ip6_nxt, ipv6hdr->ip6_hops);
			TRACE(" flow_lbl=%d.%d.%d\n",
				((ipv6hdr->ip6_flow & 0x000f0000) >> 16),
				((ipv6hdr->ip6_flow & 0x0000ff00) >> 8),
				(ipv6hdr->ip6_flow & 0x000000ff));
			TRACE(" src=%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x\n",
				ipv6hdr->ip6_src.__in6_u.__u6_addr16[0],
				ipv6hdr->ip6_src.__in6_u.__u6_addr16[1],
				ipv6hdr->ip6_src.__in6_u.__u6_addr16[2],
				ipv6hdr->ip6_src.__in6_u.__u6_addr16[3],
				ipv6hdr->ip6_src.__in6_u.__u6_addr16[4],
				ipv6hdr->ip6_src.__in6_u.__u6_addr16[5],
				ipv6hdr->ip6_src.__in6_u.__u6_addr16[6],
				ipv6hdr->ip6_src.__in6_u.__u6_addr16[7]);
			TRACE("	dst=%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x\n",
				ipv6hdr->ip6_dst.__in6_u.__u6_addr16[0],
				ipv6hdr->ip6_dst.__in6_u.__u6_addr16[1],
				ipv6hdr->ip6_dst.__in6_u.__u6_addr16[2],
				ipv6hdr->ip6_dst.__in6_u.__u6_addr16[3],
				ipv6hdr->ip6_dst.__in6_u.__u6_addr16[4],
				ipv6hdr->ip6_dst.__in6_u.__u6_addr16[5],
				ipv6hdr->ip6_dst.__in6_u.__u6_addr16[6],
				ipv6hdr->ip6_dst.__in6_u.__u6_addr16[7]);

			/* switch ethernet src/dest MAC addresses */
			ether_header_swap(prot_eth);
			TRACE("Tx: 2fwd	 fqid=%d\n", p->tx_fqid);
			TRACE("	     phys=0x%"PRIx64", offset=%d, len=%d,"
				" bpid=%d\n", qm_fd_addr(fd), fd->offset,
				fd->length20, fd->bpid);
			/* Frame received on Offline port PCD FQ range */
			if (!p->tx_fqid) {
				BUG_ON(fd->offset < sizeof(tx_fqid));
				tx_fqid = *(uint32_t *)annotations;
			} else
				tx_fqid = p->tx_fqid;
			/* IPv6 frame may contain ESP padding */
			_fd = *fd;
			_fd.length20 = len + sizeof(struct ip6_hdr) +
				ipv6hdr->ip6_ctlun.ip6_un1.ip6_un1_plen;

			ppac_send_frame(tx_fqid, &_fd);
			continue_parsing = FALSE;
			}
			break;
		case ETHERTYPE_VLAN:
			TRACE("	       -> it's ETHERTYPE_VLAN!\n");
			{
			struct vlan_hdr *vlanhdr = (struct vlan_hdr *)
							(next_header);

			proto = vlanhdr->type;
			next_header = (void *)vlanhdr + sizeof(struct vlan_hdr);
			len = len + sizeof(struct vlan_hdr);
			}
			break;
		case ETHERTYPE_ARP:
			TRACE("	       -> it's ETHERTYPE_ARP!\n");

			{
			struct ether_arp *arp = (typeof(arp))(next_header);
			TRACE("		  hrd=%d, pro=%d, hln=%d, pln=%d,"
				" op=%d\n",
				arp->arp_hrd, arp->arp_pro, arp->arp_hln,
				arp->arp_pln, arp->arp_op);
			}

			TRACE("		  -> dropping ARP packet\n");
			continue_parsing = FALSE;
			break;
		default:
			TRACE("	       -> it's UNKNOWN (!!) type 0x%04x\n",
				prot_eth->ether_type);
			TRACE("		  -> dropping unknown packet\n");
			ppac_drop_frame(fd);
			continue_parsing = FALSE;
		}
	}
	}
}

static int create_dpa_stats_counters(void)
{
	struct dpa_stats_params stats_params;
	struct dpa_stats_cnt_params cnt_params;
	struct dpa_stats_cls_cnt_params cls_cnt_params;
	void *oh_frag1 = NULL, *oh_frag2 = NULL; void **frags = NULL;
	char object_name[100];
	uint32_t cntId = 0;
	int err = 0;

	/* Attempt to initialize the DPA Stats user space library */
	err = dpa_stats_lib_init();
	if (err < 0) {
		error(0, -err, "Failed to initialize the"
				" DPA Stats user space library");
		return -err;
	}

	TRACE("DPA Stats library successfully initialized\n");

	stats_params.max_counters = sizeof(cnt_ids)/sizeof(cnt_ids[0]);
	stats_params.storage_area_len = 1000;
	stats_params.storage_area = malloc(stats_params.storage_area_len);
	if (!stats_params.storage_area) {
		error(0, -err, "Failed to allocate storage area\n");
		return -err;
	}

	err = dpa_stats_init(&stats_params, &dpa_stats_id);
	if (err < 0) {
		error(0, -err, "Failed to initialize DPA Stats instance\n");
		return err;
	}
	TRACE("Successfully Initialized DPA Stats instance: %d\n",
			dpa_stats_id);

	/* Save storage area pointer */
	storage = stats_params.storage_area;

	printf("fragmentation_demo is assuming FMan:%d and eth:%d and "
		"offline port:%d\n", ppam_args.fm, ppam_args.eth, ppam_args.oh);

	/* Get Fragmentation handle using FMC API */
	sprintf(object_name, "fm%d/frag/oh_frag1", ppam_args.fm);
	oh_frag1 = fmc_get_handle(&cmodel, object_name);

	/* Get Fragmentation handle using FMC API */
	sprintf(object_name, "fm%d/frag/oh_frag2", ppam_args.fm);
	oh_frag2 = fmc_get_handle(&cmodel, object_name);

	/* Create IP Fragmentation single counter to retrieve all statistics */
	cnt_params.type = DPA_STATS_CNT_FRAG;
	cnt_params.frag_params.frag = oh_frag1;
	cnt_params.frag_params.cnt_sel = DPA_STATS_CNT_FRAG_ALL;

	err = dpa_stats_create_counter(dpa_stats_id,
			&cnt_params, &cnt_ids[cntId++]);
	if (err < 0) {
		error(0, -err, "Failed to create DPA Stats counter\n");
		return err;
	}
	TRACE("Successfully created DPA Stats counter: %d\n", cnt_ids[cntId-1]);

	/* Create IP Fragmentation single counter to retrieve all statistics */
	cnt_params.type = DPA_STATS_CNT_FRAG;
	cnt_params.frag_params.frag = oh_frag2;
	cnt_params.frag_params.cnt_sel = DPA_STATS_CNT_FRAG_ALL;

	err = dpa_stats_create_counter(dpa_stats_id,
			&cnt_params, &cnt_ids[cntId++]);
	if (err < 0) {
		error(0, -err, "Failed to create DPA Stats counter\n");
		return err;
	}
	TRACE("Successfully created DPA Stats counter: %d\n", cnt_ids[cntId-1]);

	frags = malloc(2 * sizeof(void *));
	if (!frags) {
		error(0, 0, "Failed to allocate memory for array of frags\n");
		return -1;
	}

	frags[0] = oh_frag1;
	frags[1] = oh_frag2;

	cls_cnt_params.type = DPA_STATS_CNT_FRAG;
	cls_cnt_params.class_members = 2;
	cls_cnt_params.frag_params.frag = frags;
	cls_cnt_params.frag_params.cnt_sel = DPA_STATS_CNT_FRAG_ALL;

	err = dpa_stats_create_class_counter(dpa_stats_id,
			&cls_cnt_params, &cnt_ids[cntId++]);
	if (err < 0) {
		error(0, -err, "Failed to create DPA Stats counter\n");
		return err;
	}
	TRACE("Successfully created DPA Stats counter: %d\n", cnt_ids[cntId-1]);

	return 0;
}

static void print_dpa_stats_cnts(void)
{
	uint32_t *stg = (uint32_t *)storage;
	uint32_t i = 0;

	printf("\nSTATISTICS:\t  FRAG_TOTAL_FRAMES FRAG_FRAMES FRAG_GEN_FRAGS\n");

	printf("OH_FRAG1	:%12d %11d %12d\n",
			*(stg + i), *(stg + i+1), *(stg + i+2));
	printf("OH_FRAG2	:%12d %11d %12d\n",
			*(stg + i+3), *(stg + i+4), *(stg + i+5));
	printf("CLS_MBR_OH_FRAG1:%12d %11d %12d\n",
			*(stg + i+6), *(stg + i+7), *(stg + i+8));
	printf("CLS_MBR_OH_FRAG1:%12d %11d %12d\n",
			*(stg + i+9), *(stg + i+10), *(stg + i+11));
}

void request_done_cb(int dpa_id,
		unsigned int storage_area_offset, unsigned int cnts_written,
		int bytes_written)
{
	print_dpa_stats_cnts();
}

static int get_cnts_statistics(enum dpa_stats_op op)
{
	struct dpa_stats_cnt_request_params req_params;
	int cnts_len = 0, err = 0;

	req_params.cnts_ids = cnt_ids;
	req_params.cnts_ids_len = sizeof(cnt_ids)/sizeof(cnt_ids[0]);
	req_params.reset_cnts = FALSE;
	req_params.storage_area_offset = 0;

	cnts_len = 0;

	switch (op) {
	case dpa_stats_get_sync:
		err = dpa_stats_get_counters(req_params, &cnts_len, NULL);
		if (err < 0) {
			error(0, -err, "Failed to create DPA Stats request\n");
			return err;
		}
		printf("\nSuccessfully created DPA Stats request\n");
		print_dpa_stats_cnts();
		break;
	case dpa_stats_get_async:
		err = dpa_stats_get_counters(req_params,
				&cnts_len, &request_done_cb);
		if (err < 0) {
			error(0, -err, "Failed to create DPA Stats request\n");
			return err;
		}
		printf("\nSuccessfully created DPA Stats request\n");
		break;
	case dpa_stats_reset:
		err = dpa_stats_reset_counters(cnt_ids,
				sizeof(cnt_ids)/sizeof(cnt_ids[0]));
		if (err < 0) {
			error(0, -err, "Failed to reset DPA Stats counters\n");
			return err;
		}
		printf("\nSuccessfully reset DPA Stats counters\n");
		break;
	default:
		printf("Invalid operation\n");
		break;
	}

	return 0;
}

static int ppac_cli_dpa_stats_cmd(int argc, char *argv[])
{
	if (!strcmp(argv[0], "get_stats"))
		get_cnts_statistics(dpa_stats_get_async);
	else if (!strcmp(argv[0], "get_stats_sync"))
		get_cnts_statistics(dpa_stats_get_sync);
	else if (!strcmp(argv[0], "reset_stats"))
		get_cnts_statistics(dpa_stats_reset);

	return 0;
}

cli_cmd(get_stats, ppac_cli_dpa_stats_cmd);
cli_cmd(get_stats_sync, ppac_cli_dpa_stats_cmd);
cli_cmd(reset_stats, ppac_cli_dpa_stats_cmd);

const char ppam_doc[] = "Fragmentation demo application";

static const struct argp_option argp_opts[] = {
	{"fm",	'f',	"INT", 0, "FMAN index for Offline port"},
	{"eth",	't',	"INT", 0, "Ethernet port index"},
	{"oh",  'o',	"INT", 0, "Offline port index" },
	{}
};

static error_t parse_opts(int key, char *arg, struct argp_state *state)
{
	switch (key) {
	case 'f':
		ppam_args.fm = atoi(arg);
		if ((ppam_args.fm < 0) || (ppam_args.fm > 1)) {
			error(0, EINVAL, "FMan Id must be zero or 1");
			return -EINVAL;
		}
		break;
	case 't':
		ppam_args.eth = atoi(arg);
		if ((ppam_args.eth < 0) || (ppam_args.eth > 5)) {
			error(0, EINVAL,
				"FMan port Id must be in the range 0-5");
			return -EINVAL;
		}
		break;
	case 'o':
		ppam_args.oh = atoi(arg);
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

const struct argp ppam_argp = {argp_opts, parse_opts, 0, ppam_doc};

/* Inline the PPAC machinery */
#include <ppac.c>
