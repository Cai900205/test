/* Copyright (c) 2010,2011 Freescale Semiconductor, Inc.
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
#include "ppam_if.h"
#include <ppac_interface.h>

#include <inttypes.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>

/* All of the following things could be placed into vtables, declared as
 * "extern", implemented elsewhere, and generally made less hacky if you want.
 *
 * For this example however we want to show how ppac.c can be used without
 * any loss of execution speed due to the code separation between PPAC and an
 * application module. Ie. we want an equivalent compilation and the same
 * execution speed as a standalone implementation that has no "ppac"-style
 * modularity. This is the most awkward way to work but yields the least
 * performance to jumps, dereferences, and other cycle-gobblers. More modular
 * and elegant things can be done, by assuming the risks of some associated
 * indirection, dereferencing, [etc].
 *
 * To achieve this, we declare the FQ handling hooks as inlines, all prior to
 * including ppac.c. The code in ppac.c implements its own FQ handlng
 * callbacks and simply assumes that it can "call" these hooks at the
 * appropriate places within those implementations (so our hooks don't need to
 * be real function entities with well-defined addresses that can be stored). If
 * these "hooks" are in fact macros or inlines, they will be expanded in-place
 * by the pre-compiler or compiler, respectively. Ie. the resulting compilation
 * requires no excess jumping back and forth between generic (ppac.c)
 * packet-handling logic and application-specific (reflector.c) code when
 * processing packets.
 */

/* Override the default command prompt */
const char ppam_prompt[] = "reflector> ";

/* Global switch to enable/disable forwarding of rx-default traffic to an
 * offline port. Default to on? TBD */
static int fwd_offline = 1;

/* There is no configuration that specifies how many Tx FQs to use
 * per-interface, it's an internal choice for ppac.c and may depend on
 * optimisations, link-speeds, command-line options, etc. Also the Tx FQIDs are
 * dynamically allocated, so they're not known until ppac.c has already
 * initialised them. So firstly, the # of Tx FQs is passed in as a parameter
 * here because there's no other place where it could be meaningfully captured.
 * (Note, an interesting alternative would be to have this hook *choose* how
 * many Tx FQs to use!) Secondly, the Tx FQIDs are "notified" to us
 * post-allocation but prior to Rx initialisation. */
static int ppam_interface_init(struct ppam_interface *p,
			       const struct fm_eth_port_cfg *cfg,
			       unsigned int num_tx_fqs,
			       uint32_t *flags __maybe_unused)
{
	struct ppac_interface *c_if = container_of(p, struct ppac_interface,
						   ppam_data);

	p->num_tx_fqids = num_tx_fqs;
	p->tx_fqids = malloc(p->num_tx_fqids * sizeof(*p->tx_fqids));
	if (!p->tx_fqids)
		return -ENOMEM;

	if (ppac_interface_type(c_if) == fman_offline)
		*flags = PPAM_TX_FQ_NO_BUF_DEALLOC;
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

/* Note: this implementation always maps this rx-default to the first available
 * offline port. Ie. if there are multiple offline ports, only the first gets
 * used */
static int ppam_rx_default_init(struct ppam_rx_default *p,
				struct ppam_interface *_if,
				unsigned idx,
				struct qm_fqd_stashing *stash_opts)
{
	/* Store a static cache of detected offline ports. When initialising
	 * regular interfaces, we pick first offline port to map it to. */
	static struct ppac_interface *offline_ports[10];
	static int num_offline_ports, next_offline_port;

	struct ppac_interface *c_if = container_of(_if, struct ppac_interface,
						   ppam_data);
	if (ppac_interface_type(c_if) == fman_offline) {
		p->am_offline_port = 1;
		BUG_ON(num_offline_ports == 10);
		offline_ports[num_offline_ports++] = c_if;
	} else {
		p->am_offline_port = 0;
		if (num_offline_ports) {
			struct ppac_interface *i =
				offline_ports[next_offline_port];
			p->regular.offline_fqid = qman_fq_fqid(&i->tx_fqs[0]);
			if (++next_offline_port >= num_offline_ports)
				next_offline_port = 0;
		} else
			/* No offline ports available */
			p->regular.offline_fqid = 0;
	}
	return 0;
}
static void ppam_rx_default_finish(struct ppam_rx_default *p,
				   struct ppam_interface *_if)
{
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
	struct ppac_interface *c_if = container_of(_if, struct ppac_interface,
					ppam_data);

	/* Transmit FQID for Offline port is in annotation area */
	if (ppac_interface_type(c_if) == fman_offline)
		p->tx_fqid = 0;
	else
		p->tx_fqid = _if->tx_fqids[idx % _if->num_tx_fqids];

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
	uint32_t tx_fqid;

	BUG_ON(fd->format != qm_fd_contig);
	annotations = __dma_mem_ptov(qm_fd_addr(fd));
	addr = annotations + fd->offset;
	TRACE("Rx: 2fwd	 fqid=%d\n", dqrr->fqid);
	TRACE("	     phys=0x%"PRIx64", virt=%p, offset=%d, len=%d, bpid=%d\n",
		qm_fd_addr(fd), addr, fd->offset, fd->length20, fd->bpid);
	prot_eth = addr;
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
	else
	switch (prot_eth->ether_type)
	{
	case ETHERTYPE_IP:
		TRACE("	       -> it's ETHERTYPE_IP!\n");
		{
		struct iphdr *iphdr = (typeof(iphdr))(prot_eth + 1);
		__be32 tmp;
#ifdef ENABLE_TRACE
		u8 *src = (void *)&iphdr->saddr;
		u8 *dst = (void *)&iphdr->daddr;
		TRACE("		  ver=%d,ihl=%d,tos=%d,len=%d,id=%d\n",
			iphdr->version, iphdr->ihl, iphdr->tos, iphdr->tot_len,
			iphdr->id);
		TRACE("		  frag_off=%d,ttl=%d,prot=%d,csum=0x%04x\n",
			iphdr->frag_off, iphdr->ttl, iphdr->protocol,
			iphdr->check);
		TRACE("		  src=%d.%d.%d.%d\n",
			src[0], src[1], src[2], src[3]);
		TRACE("		  dst=%d.%d.%d.%d\n",
			dst[0], dst[1], dst[2], dst[3]);
#endif
		/* switch ipv4 src/dst addresses */
		tmp = iphdr->daddr;
		iphdr->daddr = iphdr->saddr;
		iphdr->saddr = tmp;
		/* switch ethernet src/dest MAC addresses */
		ether_header_swap(prot_eth);
		TRACE("Tx: 2fwd	 fqid=%d\n", p->tx_fqid);
		TRACE("	     phys=0x%"PRIx64", offset=%d, len=%d, bpid=%d\n",
			qm_fd_addr(fd), fd->offset, fd->length20, fd->bpid);

		/* Frame received on Offline port PCD FQ range */
		if (!p->tx_fqid) {
			BUG_ON(fd->offset < sizeof(tx_fqid));
			tx_fqid = *(uint32_t *)annotations;
		} else
			tx_fqid = p->tx_fqid;

		ppac_send_frame(tx_fqid, fd);
		}
		return;
	case ETHERTYPE_ARP:
		TRACE("	       -> it's ETHERTYPE_ARP!\n");
#ifdef ENABLE_TRACE
		{
		struct ether_arp *arp = (typeof(arp))(prot_eth + 1);
		TRACE("		  hrd=%d, pro=%d, hln=%d, pln=%d, op=%d\n",
		      arp->arp_hrd, arp->arp_pro, arp->arp_hln,
		      arp->arp_pln, arp->arp_op);
		}
#endif
		TRACE("		  -> dropping ARP packet\n");
		break;
	default:
		TRACE("	       -> it's UNKNOWN (!!) type 0x%04x\n",
			prot_eth->ether_type);
		TRACE("		  -> dropping unknown packet\n");
	}
	ppac_drop_frame(fd);
}

static inline void ppam_rx_default_cb(struct ppam_rx_default *p,
				      struct ppam_interface *_if,
				      const struct qm_dqrr_entry *dqrr)
{
	const struct qm_fd *fd = &dqrr->fd;
	struct ppac_interface *c_if = container_of(_if,
						   struct ppac_interface,
						   ppam_data);

	const struct fm_eth_port_cfg *pcfg = ppac_interface_pcfg(c_if);

	if (!p->am_offline_port) {
		/* Only forward to Offline port if it's turned on */
		if (fwd_offline && (p->regular.offline_fqid)) {
			uint32_t *annotations = __dma_mem_ptov(qm_fd_addr(fd));
			*annotations = _if->tx_fqids[0];
			BUG_ON(!*annotations);
			ppac_send_frame(p->regular.offline_fqid, fd);
			return;
		}

		if (fman_mac_less == pcfg->fman_if->mac_type) {
			struct ppam_rx_hash rx_hash;

			/* Forward the frame to first Tx FQ of the interface*/
			rx_hash.tx_fqid = _if->tx_fqids[0];
			ppam_rx_hash_cb(&rx_hash, dqrr);
			return;
		}
		if (fman_onic == pcfg->fman_if->mac_type) {
			struct ppam_rx_hash rx_hash;
			/* Forward the frame to first Tx FQ of the interface*/
			rx_hash.tx_fqid = _if->tx_fqids[0];
			ppam_rx_hash_cb(&rx_hash, dqrr);
			return;
		}
	}
	ppac_drop_frame(fd);
}

/* We implement no arguments, these are the minimal stubs */
struct ppam_arguments {
};
struct ppam_arguments ppam_args;
const char ppam_doc[] = "Packet reflector";
static const struct argp_option argp_opts[] = {
	{}
};
const struct argp ppam_argp = {argp_opts, 0, 0, ppam_doc};

/* Implement a CLI command to enable/disable forwarding to offline ports */
static int offline_cli(int argc, char *argv[])
{
	if (argc != 2)
		return -EINVAL;
	if (strcmp(argv[1], "enable") == 0)
		fwd_offline = 1;
	else if (strcmp(argv[1], "disable") == 0)
		fwd_offline = 0;
	else
		return -EINVAL;
	return 0;
}
cli_cmd(offline, offline_cli);

/* Inline the PPAC machinery */
#include <ppac.c>

