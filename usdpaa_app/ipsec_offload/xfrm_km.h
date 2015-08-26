/* Copyright (c) 2011-2013 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _XFRM_KM_H
#define _XFRM_KM_H
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/netlink.h>
#include <linux/xfrm.h>
#include <usdpaa/compat.h>
#include "usdpaa/fsl_dpa_ipsec.h"

struct dpa_pol {
	 /* link in in/out policies list */
	struct list_head list;
	/* xfrm policy information */
	struct xfrm_userpolicy_info xfrm_pol_info;
	/* dpa_policy params - required when removed */
	struct dpa_ipsec_policy_params pol_params;
	/* matching SA src address */
	xfrm_address_t sa_saddr;
	 /* matching SA dest address*/
	xfrm_address_t sa_daddr;
	/* matching SA family */
	int sa_family;
	/* dpa sa id */
	int sa_id;
	/* optional fragmentation manip descriptor */
	int manip_desc;
};

struct dpa_sa {
	/* link in SADB */
	struct list_head list;
	/* xfrm sa information */
	struct xfrm_usersa_info xfrm_sa_info;
	/* NAT-T info */
	struct xfrm_encap_tmpl encap;

	struct dpa_ipsec_sa_params sa_params;
	 /* sa_id for inbound dpa_ipsec sa*/
	int in_sa_id;
	 /* sa_id for outbound dpa_ipsec sa*/
	int out_sa_id;
	/* policies list for inbound dpa_ipsec sa */
	struct list_head in_pols;
	/* policies list for outbound dpa_ipsec sa */
	struct list_head out_pols;
	/* parent sa id used in rekeying process */
	int parent_sa_id;
};

struct sadb_msg *do_sadbget(
			uint32_t spi, int af,
			xfrm_address_t saddr, xfrm_address_t daddr,
			struct dpa_ipsec_sa_params *sa_params,
			struct xfrm_encap_tmpl *encap);
struct sadb_msg *do_spdget(
		int spid, xfrm_address_t *saddr,
		xfrm_address_t *daddr, int *sa_af);
int get_algs_by_name(const char *cipher_alg_name, const char *auth_alg_name);

#endif
