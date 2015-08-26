/**
 \file ip_accept.c
 \brief If packet is valid then IP packet is sent here for PREROUTING
	stage. Hooks before routing execute at this stage
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

#include "ip_accept.h"

#include "ip_hooks.h"
#include "ip_route.h"

enum IP_STATUS ip_accept_preparsed(const struct ppam_rx_hash *ctxt,
				   struct annotations_t *notes,
				   struct iphdr *ip_hdr,
				   enum state source)
{
	return exec_hook(ctxt, IP_HOOK_PREROUTING, notes, ip_hdr,
		&ip_accept_finish, source);
}

enum IP_STATUS ip_accept_finish(const struct ppam_rx_hash *ctxt,
				struct annotations_t *notes,
				struct iphdr *ip_hdr,
				enum state source)
{
	if (unlikely(has_options(ip_hdr))) {
		/* TODO:
		 Handle Preroute options */
	}
	return ip_route_input(ctxt, notes, ip_hdr, source);
}
