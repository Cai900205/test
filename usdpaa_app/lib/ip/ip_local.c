/**
 \file ip_local.c
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

#include "ip_local.h"
#include "ip_protos.h"

enum IP_STATUS ip_local_deliver(const struct ppam_rx_hash *ctxt,
				struct annotations_t *notes,
				struct iphdr *ip_hdr)
{
	enum IP_STATUS retval;

	if (unlikely(is_fragment(ip_hdr))) {
		ip_defragment(ctxt, notes, ip_hdr);
		retval = IP_STATUS_STOLEN;
	} else {
		ip_protos_exec(ctxt, ip_hdr->protocol, notes, ip_hdr);
	}

	return retval;
}

void ip_defragment(const struct ppam_rx_hash *ctxt,
		   const struct annotations_t *notes,
		   struct iphdr *ip_hdr __always_unused)
{
#ifdef STATS_TBD
	decorated_notify_inc_32(&ctxt->stats->ip_local_frag_reassem_started);
#endif
	/* For now, do not reassemble fragments - discard them */
	ppac_drop_frame(&notes->dqrr->fd);
}
