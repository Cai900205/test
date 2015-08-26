/**
   \file arp.c
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

#include "arp.h"

#include <netinet/if_ether.h>

static void arp_handle_request(const struct ppam_interface *p,
			       const struct annotations_t *notes, void *data)
{
	struct ether_header *eth;
	struct ether_arp *arp;
	const struct ppac_interface *dev;

	eth = data;
	arp = (typeof(arp))(eth + 1);

	if (memcmp(&p->addr, arp->arp_tpa, arp->arp_pln) != 0) {
		ppac_drop_frame(&notes->dqrr->fd);
		return;
	}

	dev = container_of(p, struct ppac_interface, ppam_data);

	arp->arp_op = ARPOP_REPLY;
	memcpy(eth->ether_dhost, eth->ether_shost, sizeof(eth->ether_dhost));
	memcpy(arp->arp_tha, eth->ether_shost, arp->arp_hln);
	memcpy(eth->ether_shost, &dev->port_cfg->fman_if->mac_addr,
			sizeof(eth->ether_shost));
	memcpy(arp->arp_tpa, arp->arp_spa, arp->arp_pln);
	memcpy(arp->arp_sha, eth->ether_shost, arp->arp_hln);
	memcpy(arp->arp_spa, &p->addr, arp->arp_pln);
	ppac_send_frame(p->tx_fqids[notes->dqrr->fqid % p->num_tx_fqids],
				&notes->dqrr->fd);
}

void arp_handler(const struct ppam_interface *p,
		 const struct annotations_t *notes, void *data)
{
	const struct ether_arp *arp;

	arp = data + ETHER_HDR_LEN;

	if (arp->arp_op == ARPOP_REQUEST)
		arp_handle_request(p, notes, data);
	else
		ppac_drop_frame(&notes->dqrr->fd);
}

static void arp_solicit(struct neigh_t *n,
		const void *annotations, void *ll_payload)
{
	const struct annotations_t *notes = annotations;
#ifdef STATS_TBD
	decorated_notify_inc_64(&n->nt->stats->solicit_errors);
#endif
	ppac_drop_frame(&notes->dqrr->fd);
}

static void arp_error_handler(struct neigh_t *n,
		const void *annotations, void *ll_payload)
{
	const struct annotations_t *notes = annotations;
#ifdef STATS_TBD
	decorated_notify_inc_64(&n->nt->stats->protocol_errors);
#endif
	ppac_drop_frame(&notes->dqrr->fd);
}

static void arp_constructor(struct neigh_t *n)
{
	n->funcs->solicit = &arp_solicit;
	n->funcs->error_handler = &arp_error_handler;
}

int arp_table_init(struct neigh_table_t *nt)
{
	nt->proto_len = sizeof(in_addr_t);
	nt->constructor = arp_constructor;
	nt->config.base_reachable_timeout = 30;
	nt->config.reachable_timeout = 30;
	nt->config.retrans_timeout = 1;
	nt->config.quiesce_timeout = 5;
	nt->config.solicit_queue_len = 1;

	return 0;
}
