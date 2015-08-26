/**
 \file ip_forward.c
 \brief Implements forwarding function if routelookup is successful
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

#include "ip_forward.h"

#include <ppac_interface.h>

#include "ip_hooks.h"
#include "ip_output.h"

enum IP_STATUS ip_forward(const struct ppam_rx_hash *ctxt,
			  struct annotations_t *notes,
			  struct iphdr *ip_hdr)
{
	struct ppac_interface *dev;

	dev = notes->dest->dev;
	if (likely(ip_hdr->ttl > 1)) {
		ip_hdr->ttl -= 1;
		ip_hdr->check += 0x100;
		if (unlikely((ip_hdr->check & 0xff00) == 0))
			ip_hdr->check += 0x1;

	} else {
#ifdef STATS_TBD
		decorated_notify_inc_64(&ctxt->stats->ip_ttl_time_exceeded);
#endif
		ppac_drop_frame(&notes->dqrr->fd);
		return IP_STATUS_DROP;
	}

	/* If we have not dropped yet, and if the interface MTU is smaller than
	   the frame, and we can't fragment, drop the frame.  This calculation
	   is uneccessarily in the "DROP" path if the above tests fail - should
	   not do it if status is not ACCEPT.
	 */
	if (unlikely(notes->dqrr->fd.length20 - dev->ppam_data.header_len >
			dev->ppam_data.mtu)) {
		pr_err("%s: Dropping pkt, mtu exceeded\n",
			  __func__);
#ifdef STATS_TBD
		decorated_notify_inc_64(
			&ctxt->stats->ip_xmit_icmp_unreach_need_frag);
#endif
		ppac_drop_frame(&notes->dqrr->fd);
		return IP_STATUS_DROP;
	}

	/* If we have not dropped it yet, and source == dest, send redirect */
	if (unlikely(dev->port_cfg->fman_if->mac_idx == notes->parse.port_id)) {
		/* send_icmp(ip_hdr, ICMP_MSG_REDIRECT); */
#ifdef STATS_TBD
		decorated_notify_inc_64(
			&ctxt->stats->ip_xmit_icmp_redir_in_eq_out);
#endif
	}

	/* If we have not dropped yet, execute all of the forwarding hooks */
	return exec_hook(ctxt, IP_HOOK_FORWARD, notes, ip_hdr,
		&ip_forward_finish, SOURCE_POST_FMAN);
}

enum IP_STATUS ip_forward_finish(const struct ppam_rx_hash *ctxt,
				 struct annotations_t *notes,
				 struct iphdr *ip_hdr,
				 enum state source)
{
#ifdef STATS_TBD
	decorated_notify_inc_32(&ctxt->stats->ip_out_forward);
#endif
	return ip_send(ctxt, notes, ip_hdr);
}
