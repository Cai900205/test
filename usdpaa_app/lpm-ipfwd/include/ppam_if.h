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

#ifndef __PPAM_IF_H
#define __PPAM_IF_H

#include "net/ll_cache.h"
#include "ip/ip.h"		/* node_t */
#include "ip/ip_common.h"	/* ip_statistics_t */

struct ppac_interface;

/* structs required by ppac.c */
struct ppam_interface {
	size_t mtu;
	size_t header_len;
	in_addr_t addr, mask;
	int ifnum;
	char *ifname;

	size_t num_tx_fqids;
	uint32_t *tx_fqids;

	void (*set_header)(const struct ppac_interface *i, void *payload,
			   const void *src, const void *dst);
	void (*cache_header)(struct ll_cache_t *llc, const void *hdr);
	void (*output_header)(void *hdr, const struct ll_cache_t *llc);

	struct node_t local_nodes[23];
};

struct ppam_rx_error {
	struct ip_statistics_t *stats;
	struct ip_protos_t *protos;
};

struct ppam_rx_default {
	struct ip_statistics_t *stats;
	struct ip_protos_t *protos;
	uint32_t tx_fqid;
	int is_macless;
};

struct ppam_tx_error { };
struct ppam_tx_confirm { };

struct ppam_rx_hash {
	struct ip_statistics_t *stats;
	struct ip_protos_t *protos;
};

#endif	/* __PPAM_IF_H */
