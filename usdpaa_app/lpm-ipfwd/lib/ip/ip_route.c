/**
 \file ip_route.c
 \brief IPv4 Route lookup is done for forwarding decision.
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

#include "ip_route.h"

#include "ip/ip_forward.h"
#include "ip/ip_local.h"
#include "fib.h"

enum IP_STATUS ip_route_input(const struct ppam_rx_hash *ctxt,
			      struct annotations_t *notes,
			      struct iphdr *ip_hdr, enum state source)
{
	enum IP_STATUS retval = IP_STATUS_DROP;
	uint32_t gwaddr;
	int ret;

	switch (source) {
	case SOURCE_POST_FMAN:
	{
		ret = ip_route_lookup(ip_hdr->daddr, &gwaddr, notes);
		if (unlikely(ret != 0)) {
			pr_info("error in lookup for IP%x\n", ip_hdr->daddr);
			ppac_drop_frame(&notes->dqrr->fd);
			return ret;
		}
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
	if (likely(notes->neighbor->dev))
		return ip_forward(ctxt, notes, ip_hdr);
	ppac_drop_frame(&notes->dqrr->fd);
	return IP_STATUS_DROP;
}
