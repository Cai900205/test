/**
 \file ip_output.c
 */
/*
 * Copyright (C) 2012 Freescale Semiconductor, Inc.
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

/*
 * If packet length > next_hop mtu, call ip_fragment
 */
enum IP_STATUS ip_send(const struct ppam_rx_hash *ctxt,
		       struct annotations_t *notes, struct iphdr *ip_hdr)
{
	BUG_ON(notes->neighbor == NULL);

	return ip_output(ctxt, notes, ip_hdr);
}

enum IP_STATUS ip_output(const struct ppam_rx_hash *ctxt,
			 struct annotations_t *notes,
			 struct iphdr *ip_hdr)
{
	return ip_output_finish(ctxt, notes, ip_hdr, SOURCE_POST_FMAN);
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

	retval = IP_STATUS_ACCEPT;

	neighbor = notes->neighbor;
	ll_cache = neighbor->ll_cache;

	if (unlikely(ll_cache == NULL)) {
		pr_info("Could not found ARP cache entries for IP 0x%x\n",
				neighbor->proto_addr[0]);
		pr_info("Discarding packet destined for IP 0x%x\n",
					neighbor->proto_addr[0]);
		ppac_drop_frame(&notes->dqrr->fd);
		return IP_STATUS_DROP;
	} else {
		ll_hdr = (void *)ip_hdr - ll_cache->ll_hdr_len;
		p = &neighbor->dev->ppam_data;
		p->output_header(ll_hdr, ll_cache);
		ppac_send_frame(p->tx_fqids[notes->dqrr->fqid %
				 p->num_tx_fqids], &notes->dqrr->fd);
	}

	return retval;
}
