
/**
 \file ip_route.c
 \brief Route lookup is done for forwarding decision.
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

#include "ip_route.h"

#include "ip_forward.h"
#include "ip_local.h"
#include "ip_protos.h"
#include "ipsec/ipsec_encap.h"
#include "ipsec/ipsec_decap.h"

enum IP_STATUS ip_route_input(const struct ppam_rx_hash *ctxt,
			      struct annotations_t *notes,
			      struct iphdr *ip_hdr, enum state source)
{
	enum IP_STATUS retval = IP_STATUS_DROP;
	struct ipsec_esp_hdr_t *esp_hdr;

	switch (source) {
	case SOURCE_POST_FMAN:
	{
		struct rc_entry_t *entry;
		if ((ip_hdr->protocol == IPPROTO_ESP)) {
			/* Pkt may be for decryption */
			retval = ipsec_decap_send(ctxt, notes, ip_hdr);

			if (retval != IP_STATUS_HOLD)
				return retval;
		}
		entry = rc_entry_fast_lookup(ctxt->rc, ip_hdr->saddr,
				ip_hdr->daddr, RC_BUCKET_INDEX(notes));
		pr_debug("Hash index= %x\n", RC_BUCKET_INDEX(notes));

		if (entry == NULL) {
			entry = rc_entry_lookup(ctxt->rc, ip_hdr->saddr,
					ip_hdr->daddr);
			if (entry == NULL) {
				pr_debug("Fast Lookup Failed, going slow \
				   for Src = 0x%x; Dest = 0x%x\n",
				   ip_hdr->saddr,
				   ip_hdr->daddr);
				retval =
				ip_route_input_slow(ctxt, notes, ip_hdr);
				return retval;
			}
		}

		notes->dest = entry->dest;
#ifdef STATS_TBD
		decorated_notify_inc_64(&(entry->stats->hits));
#endif
		retval = ip_route_finish(ctxt, notes, ip_hdr);
	}
		break;
	case SOURCE_POST_ENCAP:
	{
		struct ipsec_tunnel_t *entry;
		esp_hdr =
			(struct ipsec_esp_hdr_t *)((uintptr_t) ip_hdr +
					   sizeof(struct iphdr));
		pr_debug("%s: Pkt src = %x, dst = %x and spi = %x\n", __func__,
		ip_hdr->saddr, ip_hdr->daddr, esp_hdr->spi);
		retval = ipsec_lookup_tunnel_entry(&entry,
			(struct ipsec_tunnel_table_t *)
			ctxt->itt,
			ip_hdr->saddr,
			ip_hdr->daddr,
			esp_hdr->spi);
		if (unlikely(retval == false)) {
			pr_err("%s: Couldn't find tunnel", __func__);
#ifdef STATS_TBD
			decorated_notify_inc_32(&(ctxt->stats->dropped));
#endif
			return IP_STATUS_HOLD;
		}
		notes->dest = entry->dest;
		retval = ip_route_finish(ctxt, notes, ip_hdr);
	}
	break;
	case SOURCE_POST_DECAP:
	{
		struct rc_entry_t *entry = rc_entry_lookup(ctxt->rc,
					ip_hdr->saddr,
					ip_hdr->daddr);
		if (entry == NULL) {
			pr_debug("Fast Lookup Failed, going slow for Src = \
				0x%x; Dest = 0x%x; TOS = 0x%x",
				ip_hdr->saddr,
				ip_hdr->daddr, ip_hdr->tos);
			retval =
				ip_route_input_slow(ctxt, notes, ip_hdr);
			return retval;
		}
		notes->dest = entry->dest;
#ifdef STATS_TBD
		decorated_notify_inc_64(&(entry->stats->hits));
#endif
		retval = ip_route_finish(ctxt, notes, ip_hdr);
	}
		break;
	default:
		pr_err("Invalid Case of routing\n");
		break;
	}

	return retval;
}

enum IP_STATUS ip_route_input_slow(const struct ppam_rx_hash *ctxt,
				   const struct annotations_t *notes,
				   struct iphdr *ip_hdr __always_unused)
{
#ifdef STATS_TBD
	decorated_notify_inc_64(&ctxt->stats->ip_route_input_slow);
#endif
	ppac_drop_frame(&notes->dqrr->fd);
	return IP_STATUS_DROP;
}

enum IP_STATUS ip_route_finish(const struct ppam_rx_hash *ctxt,
			       struct annotations_t *notes,
			       struct iphdr *ip_hdr)
{
	struct rt_dest_t *dest;


	dest = notes->dest;
	switch (dest->scope) {
	case ROUTE_SCOPE_GLOBAL:
	case ROUTE_SCOPE_DECAP:
		if (likely(dest->dev)) {
			return ip_forward(ctxt, notes, ip_hdr);
		} else {
#ifdef STATS_TBD
			decorated_notify_inc_64(
				&ctxt->stats->ip_xmit_icmp_unreach_no_egress);
#endif
		}
		break;
	case ROUTE_SCOPE_LOCAL:
		return ip_local_deliver(ctxt, notes, ip_hdr);
	case ROUTE_SCOPE_ENCAP:
		return ipsec_encap_send(ctxt, notes, ip_hdr);
	}
	ppac_drop_frame(&notes->dqrr->fd);
	return IP_STATUS_DROP;
}
