/**
 \file eth.c
 */
/*
 * Copyright (C) 2010,2011 Freescale Semiconductor, Inc.
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

#include "eth.h"

#include <ppac_interface.h>

#include "net/ll_cache.h"

static void set_header(const struct ppac_interface *i, void *payload,
		       const void *src, const void *dst)
{
	struct ether_header *eth;

	eth = (typeof(eth))payload - 1;

	memcpy(eth->ether_shost,
	       src != NULL ? src : &i->port_cfg->fman_if->mac_addr,
	       sizeof(eth->ether_shost));
	memcpy(eth->ether_dhost, dst, sizeof(eth->ether_dhost));

	eth->ether_type = ETHERTYPE_IP;
}

static void cache_header(struct ll_cache_t *llc, const void *hdr)
{
	llc->ll_hdr_len = ETHER_HDR_LEN;
	memcpy(llc->ll_data, hdr, 2 * ETHER_ADDR_LEN);
}

static void output_header(void *hdr, const struct ll_cache_t *llc)
{
	memcpy(hdr, llc->ll_data, 2 * ETHER_ADDR_LEN);

	((struct ether_header *)hdr)->ether_type = ETHERTYPE_IP;
}

void eth_setup(struct ppam_interface *p)
{
	p->set_header		= set_header;
	p->cache_header		= cache_header;
	p->output_header	= output_header;
}
