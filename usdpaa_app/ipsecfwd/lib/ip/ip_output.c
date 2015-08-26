/**
 \file ip_output.c
 */
/*
 * Copyright (C) 2010 - 2012 Freescale Semiconductor, Inc.
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

#include "ip_output.h"

#include <ppac_interface.h>
#include <ppac.h>

#include "net/neigh.h"
#include "ip_hooks.h"

#include <assert.h>

#ifdef NOT_USDPAA
void arp_retransmit_cb(uint32_t timer_id, void *p_data)
{
	in_addr_t gw_ip;
	struct neigh_t *n;
	struct ppac_interface *dev;

	pr_debug("%s: ARP retransmit timer ID 0x%x expired\n", __func__,
			timer_id);

	gw_ip = *(typeof(&gw_ip))p_data;
	n = neigh_lookup(stack.arp_table, gw_ip, sizeof(gw_ip));
	if (unlikely(NULL == n)) {
		pr_err("%s: neighbour entry not found for IP 0x%x\n",
			__func__, gw_ip);
		return;
	}

	if (n->retransmit_count < 3) {
		dev = n->dev;
		arp_send_request(dev, n->proto_addr[0]);
		n->retransmit_count++;

	} else {
		pr_info("%s: MAX no. of %d ARP retransmission attempted\n",
				__func__, n->retransmit_count);
		if (0 != stop_timer(timer_id)) {
			pr_err("%s Stopping ARP retransmit timer failed\n",
					 __func__, timer_id);
			return;
		} else
			pr_info("%s: ARP retransmit timer 0x%x stopped...\n",
					__func__, timer_id);

		n->retransmit_count = 0;
		n->neigh_state = NEIGH_STATE_FAILED;
	}
}
#endif

/*
 * If packet length > next_hop mtu, call ip_fragment
 */
enum IP_STATUS ip_send(const struct ppam_rx_hash *ctxt,
		       struct annotations_t *notes, struct iphdr *ip_hdr)
{
	assert(notes->dest != NULL);

	return ip_output(ctxt, notes, ip_hdr);
}

/*
 * Call intervening POSTROUTING hooks for each frame
 */
enum IP_STATUS ip_output(const struct ppam_rx_hash *ctxt,
			 struct annotations_t *notes,
			 struct iphdr *ip_hdr)
{
	return exec_hook(ctxt, IP_HOOK_POSTROUTING, notes, ip_hdr,
			&ip_output_finish, SOURCE_POST_FMAN);
}

/*
 * Find the correct neighbor for this frame, using ARP tables
 */
enum IP_STATUS ip_output_finish(const struct ppam_rx_hash *ctxt,
				struct annotations_t *notes,
				struct iphdr *ip_hdr,
				enum state source)
{
	struct ll_cache_t *ll_cache;
	struct neigh_t *neighbor;
	const struct ppam_interface *p;
	enum IP_STATUS retval;
	struct ether_header *ll_hdr;
#ifdef NOT_USDPAA
	uint32_t timer_id;
#endif
#ifdef IPSECFWD_HYBRID_GENERATOR
	uint32_t temp;
#endif

	retval = IP_STATUS_ACCEPT;

	neighbor = notes->dest->neighbor;
	ll_cache = neighbor->ll_cache;

	if (unlikely(ll_cache == NULL)) {
		if (NEIGH_STATE_PENDING == neighbor->neigh_state) {
			pr_debug("Discarding packet destined for IP 0x%x\n",
						neighbor->proto_addr[0]);
			pr_debug("ARP entry state is pending\n");
			/* Discard successive packet (on the assumption the
			 * packet will be retransmitted by a higher network
			 * layer)
			 */
			ppac_drop_frame(&notes->dqrr->fd);
			return IP_STATUS_DROP;
		}

		pr_info("Could not found ARP cache entries for IP 0x%x\n",
				neighbor->proto_addr[0]);

		/* Save first packet and forward it upon ARP reply */
		neighbor->fd = notes->dqrr->fd;

		/* Create and send ARP request */
#ifdef NOT_USDPAA
		arp_send_request(i, neighbor->proto_addr);
		timer_id = start_timer(ARP_RETRANSMIT_INTERVAL, true, NULL,
				SWI_PRI_HIGH,
				arp_retransmit_cb,
				neighbor->proto_addr);

		if (INV_TIMER_ID == timer_id)
			pr_err("%s: ARP retransmit timer failed\n", __func__);
		else {
			pr_info("%s: ARP retransmit timer 0x%x started...\n",
				__func__, timer_id);
			neighbor->retransmit_timer = timer_id;
		}
#endif
		neighbor->neigh_state = NEIGH_STATE_PENDING;
		neighbor->retransmit_count = 0;
	} else {
		ll_hdr = (void *)ip_hdr - ll_cache->ll_hdr_len;
		p = &neighbor->dev->ppam_data;
		p->output_header(ll_hdr, ll_cache);
#ifdef IPSECFWD_HYBRID_GENERATOR
		eth_header_swap(ll_hdr);
		temp = ip_hdr->src_addr;
		ip_hdr->src_addr = ip_hdr->dst_addr;
		ip_hdr->dst_addr = temp;
#endif
		ppac_send_frame(p->tx_fqids[notes->dqrr->fqid %
			p->num_tx_fqids], &notes->dqrr->fd);
	}

	return retval;
}
