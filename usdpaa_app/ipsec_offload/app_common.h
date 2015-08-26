/* Copyright (c) 2011-2013 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *	 notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *	 notice, this list of conditions and the following disclaimer in the
 *	 documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *	 names of its contributors may be used to endorse or promote products
 *	 derived from this software without specific prior written permission.
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
#ifndef __APP_COMMON_H
#define __APP_COMMON_H

#include <ppac.h>
#include <flib/rta.h>
#include "std_ext.h"
#include "fmc.h"

static inline struct fman_if *get_fif(int fm,
				      int port_idx,
				      enum fman_mac_type type)
{
	int idx;
	struct fm_eth_port_cfg *port_cfg;
	for (idx = 0; idx < netcfg->num_ethports; idx++) {
		port_cfg = &netcfg->port_cfg[idx];
		if ((fm == port_cfg->fman_if->fman_idx) &&
		    (type == port_cfg->fman_if->mac_type) &&
		    (port_idx == port_cfg->fman_if->mac_idx))
			return port_cfg->fman_if;
	}
	return NULL;
}

static inline struct ppac_interface *get_ppac_if(struct fman_if *__if)
{
	struct ppac_interface *ppac_if;
	list_for_each_entry(ppac_if, &ifs, node) {
		if (ppac_if->port_cfg->fman_if == __if)
			return ppac_if;
	}
	return NULL;
}

static inline char * get_macless_name (struct fman_if * __if)
{
	if (!__if)
		return NULL;

	return __if->mac_type == fman_mac_less? __if->macless_info.macless_name :
		   __if->mac_type == fman_onic ? __if->onic_info.macless_name :
		   NULL;
}

static inline void set_macless_name (struct fman_if * __if, const char * macless_name)
{
	char * p = NULL;
	if (!__if || !macless_name)
		return;

	p = __if->mac_type == fman_mac_less? __if->macless_info.macless_name :
	    __if->mac_type == fman_onic ? __if->onic_info.macless_name :
	   NULL;

	if (!p)
		return;

	strncpy(p, macless_name, IFNAMSIZ);
	p[IFNAMSIZ-1] = 0;
}


static inline struct ether_addr * get_macless_peer_mac (struct fman_if * __if)
{
	if (!__if)
		return NULL;

	return __if->mac_type == fman_mac_less? &__if->macless_info.peer_mac :
		   __if->mac_type == fman_onic ?  &__if->onic_info.peer_mac :
		   NULL;
}

static inline struct fman_if * get_fman_if_by_name (const char * p_macless_name)
{
	char * port_name = NULL;
	struct fman_if * __if = NULL;

	if (!p_macless_name)
		return NULL;

	list_for_each_entry(__if, fman_if_list, node) {
		port_name = get_macless_name(__if);
		if (port_name && !strcmp(p_macless_name, port_name))
			return __if;
	}

	return NULL;
}

static inline struct fman_if * get_fman_if_by_mac (struct ether_addr * p_macless_mac)
{
	struct ether_addr * port_mac = NULL;
	struct fman_if * __if = NULL;

	if (!p_macless_mac)
		return NULL;

	list_for_each_entry(__if, fman_if_list, node) {
		port_mac = get_macless_peer_mac(__if);
		if (port_mac &&	!memcmp(&p_macless_mac->ether_addr_octet,
				                &port_mac->ether_addr_octet, ETH_ALEN))
			return __if;
	}

	return NULL;
}


/* VLAN header definition */
struct vlan_hdr {
	__u16 tci;
	__u16 type;
};

enum ether_types {
	ETHER_TYPE_IPv4 = 0,
	ETHER_TYPE_IPv6,
	MAX_ETHER_TYPES
};

static const char *fmc_1g = "1G";
static const char *fmc_10g = "10G";
static const char *fmc_offline = "OFFLINE";

static inline const char *get_port_type(struct fman_if *__if)
{
	if (__if->mac_type == fman_mac_1g)
		return fmc_1g;
	else if (__if->mac_type == fman_mac_10g)
		return fmc_10g;
	else if (__if->mac_type == fman_offline)
		return fmc_offline;
	else
		return NULL;
}

extern t_Handle pcd_dev;
/* inbound SA lookup */
extern t_Handle cc_in_rx[DPA_IPSEC_MAX_SA_TYPE];
/* VIPSEC Rx flow classification */
extern t_Handle cc_vipsec_rx[MAX_ETHER_TYPES];
/* outbound SP lookup */
extern t_Handle cc_out_pre_enc[DPA_IPSEC_MAX_SUPPORTED_PROTOS];
/* outbound post ipsec forwarding - ether header manip */
extern t_Handle cc_out_post_enc[MAX_ETHER_TYPES];
/* inbound post ipsec forwarding - ether header manip */
extern t_Handle cc_in_post_dec[MAX_ETHER_TYPES];
/* local deliver tables */
extern t_Handle cc_out_local[MAX_ETHER_TYPES];
extern t_Handle cc_in_local[MAX_ETHER_TYPES];
/* forwarding header manip resources */
extern t_Handle ob_fwd_hm, ib_fwd_hm;
/* inbound reassembly */
extern t_Handle ib_reass;

int fmc_config(void);
void fmc_cleanup(void);
void stats_cleanup(void);
int ipsec_offload_init(int *dpa_ipsec_id);
int ipsec_offload_cleanup(int dpa_ipsec_id);
int setup_xfrm_msgloop(int dpa_ipsec_id, pthread_t *tid);
int setup_neigh_loop(pthread_t *tid);
int create_nl_socket(int protocol, int groups);
int get_dst_addrs(struct in_addr *dst_addr, unsigned char *dst_len,
		  struct in_addr *gw_addr, unsigned int max_len);
int set_dist_base_fqid(struct fmc_model_t *cmodel, char *fmc_path,
		       uint32_t fqid);
int set_cc_miss_fqid(struct fmc_model_t *cmodel, char *fmc_path,
		     uint32_t fqid);
#if defined(B4860) || defined(T4240) || defined(B4420)
int set_cc_miss_fqid_with_vsp(struct fmc_model_t *cmodel, char *fmc_path,
		     uint32_t fqid);
#endif
struct fmc_model_t *fmc_compile_model(void);
int fmc_apply_model(void);
int stats_init(void);
int show_sa_stats(int argc, char *argv[]);
int show_ipsec_stats(int argc, char *argv[]);
int list_dpa_sa(int argc, char *argv[]);
int show_eth_stats(int argc, char *argv[]);
int show_ib_reass_stats(int argc, char *argv[]);
#endif
