/* Copyright (c) 2014 Freescale Semiconductor, Inc.
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

#include <ppac.h>
#include "ppam_if.h"
#include <ppac_interface.h>
#include <usdpaa/fsl_bman.h>
#include <usdpaa/fsl_qman.h>

#include <error.h>
#include <unistd.h>
#include "fmc.h"
#include "app_config.h"
#include "usdpaa/fsl_dpa_ipsec.h"
#include "app_common.h"

const char ppam_pcd_path[] = __stringify(DEF_PCD_PATH);
const char ppam_cfg_path[] = __stringify(DEF_CFG_PATH);
const char ppam_pdl_path[] = __stringify(DEF_PDL_PATH);
const char ppam_swp_path[] = __stringify(DEF_SWP_PATH);

/* cc nodes required by DPA IPsec offloading */
t_Handle cc_in_rx[DPA_IPSEC_MAX_SA_TYPE], cc_flow_id, cc_post_flow_id,
	 cc_out_pre_enc[DPA_IPSEC_MAX_SUPPORTED_PROTOS];

/* cc nodes used for outbound/inbound post ipsec forwarding */
t_Handle cc_out_post_enc[MAX_ETHER_TYPES];
t_Handle cc_in_post_dec[MAX_ETHER_TYPES];

/* header manipulation handles for ethernet header replacing */
t_Handle ob_fwd_hm, ib_fwd_hm;

/* Inbound reassembly stats */
t_Handle ib_reass;

/* cc nodes for local traffic */
t_Handle cc_out_local[MAX_ETHER_TYPES];
t_Handle cc_in_local[MAX_ETHER_TYPES];

t_Handle pcd_dev;

static struct fmc_model_t cmodel;

static inline void *get_dev_id(void *node)
{
	if (!node)
		return NULL;
	else
		return (void *)((struct t_Device *)node)->id;
}

int set_dist_base_fqid(struct fmc_model_t *_cmodel, char *fmc_path,
		       uint32_t fqid)
{
	int i = 0;
	for (i = 0; i < _cmodel->scheme_count; i++) {
		if (!strcmp(_cmodel->scheme_name[i], fmc_path)) {
			_cmodel->scheme[i].baseFqid = fqid;
			return 0;
		}
	}
	return -1;
}

int set_cc_miss_fqid(struct fmc_model_t *_cmodel, char *fmc_path,
		     uint32_t fqid)
{
	int i = 0;
	for (i = 0; i < _cmodel->ccnode_count; i++) {
		if (!strcmp(_cmodel->ccnode_name[i], fmc_path)) {
			_cmodel->ccnode[i].keysParams.
			ccNextEngineParamsForMiss.
			params.enqueueParams.newFqid = fqid;
			return 0;
		}
	}
	return -1;
}

struct fmc_model_t *fmc_compile_model(void)
{
	t_Error err = E_OK;
	const char *pcd_path = ppam_pcd_path;
	const char *cfg_path = ppam_cfg_path;
	const char *pdl_path = ppam_pdl_path;
	const char *swp_path = ppam_swp_path;
	const char *envp;

	envp = getenv(ppam_pcd_path);
	if (envp != NULL)
		pcd_path = envp;
	envp = getenv(ppam_cfg_path);
	if (envp != NULL)
		cfg_path = envp;
	envp = getenv(ppam_pdl_path);
	if (envp != NULL)
		pdl_path = envp;
	envp = getenv(ppam_swp_path);
	if (envp != NULL)
		swp_path = envp;

	err = fmc_compile(&cmodel,
			  cfg_path,
			  pcd_path,
			  pdl_path,
			  swp_path,
			  0x20,
			  0,
			  NULL);

	if (err != E_OK) {
		fprintf(stderr,
			"error compiling fmc configuration (%d) : %s\n", err,
			fmc_get_error());
		return NULL;
	}

	return &cmodel;
}
int fmc_apply_model(void)
{
	t_Error err = E_OK;
	const char *port_type;
	char fmc_path[64];

	err = fmc_execute(&cmodel);
	if (err != E_OK) {
		fprintf(stderr,
			"error executing fmc model (%d)\n", err);
		return -1;
	}

	memset(fmc_path, 0, sizeof(fmc_path));
	sprintf(fmc_path, "fm%d/pcd", app_conf.fm);
	pcd_dev = fmc_get_handle(&cmodel, fmc_path);
	if (!pcd_dev)
		goto err;

	memset(fmc_path, 0, sizeof(fmc_path));

	sprintf(fmc_path, "fm%d/port/OFFLINE/%d/ccnode/"
		"esp_cc", app_conf.fm, app_conf.ib_oh_pre->mac_idx);
	cc_in_rx[DPA_IPSEC_SA_IPV4] = fmc_get_handle(&cmodel, fmc_path);
	if (!cc_in_rx[DPA_IPSEC_SA_IPV4])
		goto err;

	memset(fmc_path, 0, sizeof(fmc_path));
	sprintf(fmc_path, "fm%d/port/OFFLINE/%d/ccnode/"
		"esp_udp_cc", app_conf.fm, app_conf.ib_oh_pre->mac_idx);  
	cc_in_rx[DPA_IPSEC_SA_IPV4_NATT] = fmc_get_handle(&cmodel, fmc_path);
	if (!cc_in_rx[DPA_IPSEC_SA_IPV4_NATT])
		goto err;

	memset(fmc_path, 0, sizeof(fmc_path));
	sprintf(fmc_path, "fm%d/port/OFFLINE/%d/ccnode/"
		"esp6_cc", app_conf.fm, app_conf.ib_oh_pre->mac_idx);
	cc_in_rx[DPA_IPSEC_SA_IPV6] = fmc_get_handle(&cmodel, fmc_path);
	if (!cc_in_rx[DPA_IPSEC_SA_IPV6])
		goto err;

	memset(fmc_path, 0, sizeof(fmc_path));
	sprintf(fmc_path,
		"fm%d/port/OFFLINE/%d/ccnode/flow_id_cc",
		app_conf.fm, app_conf.ib_oh->mac_idx);
	cc_flow_id = fmc_get_handle(&cmodel, fmc_path);
	if (!cc_flow_id)
		goto err;

	memset(fmc_path, 0, sizeof(fmc_path));
	sprintf(fmc_path,
		"fm%d/port/OFFLINE/%d/ccnode/post_flow_id_cc",
		app_conf.fm, app_conf.ib_oh->mac_idx);
	cc_post_flow_id = fmc_get_handle(&cmodel, fmc_path);
	if (!cc_post_flow_id)
		goto err;

	memset(fmc_path, 0, sizeof(fmc_path));
	sprintf(fmc_path,
		"fm%d/port/OFFLINE/%d/ccnode/tcpudp_cc",
		app_conf.fm, app_conf.ob_oh_pre->mac_idx);
	cc_out_pre_enc[DPA_IPSEC_PROTO_ANY_IPV4] = fmc_get_handle(&cmodel,
						   fmc_path);
	if (!cc_out_pre_enc[DPA_IPSEC_PROTO_ANY_IPV4])
		goto err;

	sprintf(fmc_path,
		"fm%d/port/OFFLINE/%d/ccnode/tcpudp6_cc",
		app_conf.fm, app_conf.ob_oh_pre->mac_idx);
	cc_out_pre_enc[DPA_IPSEC_PROTO_ANY_IPV6] = fmc_get_handle(&cmodel,
								  fmc_path);
	if (!cc_out_pre_enc[DPA_IPSEC_PROTO_ANY_IPV6])
		goto err;

	sprintf(fmc_path,
		"fm%d/port/OFFLINE/%d/ccnode/icmp_cc",
		app_conf.fm, app_conf.ob_oh_pre->mac_idx);
	cc_out_pre_enc[DPA_IPSEC_PROTO_ICMP_IPV4] = fmc_get_handle(&cmodel,
								   fmc_path);
	if (!cc_out_pre_enc[DPA_IPSEC_PROTO_ICMP_IPV4])
		goto err;

	sprintf(fmc_path,
		"fm%d/port/OFFLINE/%d/ccnode/icmp6_cc",
		app_conf.fm, app_conf.ob_oh_pre->mac_idx);
	cc_out_pre_enc[DPA_IPSEC_PROTO_ICMP_IPV6] = fmc_get_handle(&cmodel,
								   fmc_path);
	if (!cc_out_pre_enc[DPA_IPSEC_PROTO_ICMP_IPV6])
		goto err;

	sprintf(fmc_path,
		"fm%d/port/OFFLINE/%d/ccnode/ob_post_ip_cc",
		app_conf.fm, app_conf.ob_oh_post->mac_idx);
	cc_out_post_enc[ETHER_TYPE_IPv4] = fmc_get_handle(&cmodel,
							     fmc_path);
	if (!cc_out_post_enc[ETHER_TYPE_IPv4])
		goto err;

	sprintf(fmc_path,
		"fm%d/port/OFFLINE/%d/ccnode/ob_post_ip6_cc",
		app_conf.fm, app_conf.ob_oh_post->mac_idx);
	cc_out_post_enc[ETHER_TYPE_IPv6] = fmc_get_handle(&cmodel,
							fmc_path);
	if (!cc_out_post_enc[ETHER_TYPE_IPv6])
		goto err;
	sprintf(fmc_path,
		"fm%d/port/OFFLINE/%d/ccnode/ib_post_ip_cc",
		app_conf.fm, app_conf.ib_oh->mac_idx);
	cc_in_post_dec[ETHER_TYPE_IPv4] = fmc_get_handle(&cmodel,
							fmc_path);

	if (!cc_in_post_dec[ETHER_TYPE_IPv4])
		goto err;

	sprintf(fmc_path,
		"fm%d/port/OFFLINE/%d/ccnode/ib_post_ip6_cc",
		app_conf.fm, app_conf.ib_oh->mac_idx);
	cc_in_post_dec[ETHER_TYPE_IPv6] = fmc_get_handle(&cmodel,
							fmc_path);

	if (!cc_in_post_dec[ETHER_TYPE_IPv6])
		goto err;

	port_type = get_port_type(app_conf.ob_eth);
	sprintf(fmc_path,
		"fm%d/port/%s/%d/ccnode/ob_ip4_local_cc",
		app_conf.fm, port_type, app_conf.ob_eth->mac_idx);
	cc_out_local[ETHER_TYPE_IPv4] = fmc_get_handle(&cmodel,
						      fmc_path);
	if (!cc_out_local[ETHER_TYPE_IPv4])
		goto err;
	sprintf(fmc_path,
		"fm%d/port/%s/%d/ccnode/ob_ip6_local_cc",
		app_conf.fm, port_type, app_conf.ob_eth->mac_idx);
	cc_out_local[ETHER_TYPE_IPv6] = fmc_get_handle(&cmodel,
							fmc_path);
	if (!cc_out_local[ETHER_TYPE_IPv6])
		goto err;

	sprintf(fmc_path,
		"fm%d/port/OFFLINE/%d/ccnode/ib_ip4_local_cc",
		app_conf.fm, app_conf.ib_oh_pre->mac_idx);
	cc_in_local[ETHER_TYPE_IPv4] = fmc_get_handle(&cmodel,
							fmc_path);
	if (!cc_in_local[ETHER_TYPE_IPv4])
		goto err;

	sprintf(fmc_path, "fm%d/port/OFFLINE/%d/ccnode/ib_ip6_local_cc",
		app_conf.fm, app_conf.ib_oh_pre->mac_idx);
	cc_in_local[ETHER_TYPE_IPv6] = fmc_get_handle(&cmodel,
							fmc_path);

	sprintf(fmc_path, "fm%d/hdr/ob_replace", app_conf.fm);
	ob_fwd_hm = fmc_get_handle(&cmodel, fmc_path);
	if (!ob_fwd_hm)
		goto err;

	sprintf(fmc_path, "fm%d/hdr/ib_replace", app_conf.fm);
	ib_fwd_hm = fmc_get_handle(&cmodel, fmc_path);
	if (!ib_fwd_hm)
		goto err;

	memset(fmc_path, 0, sizeof(fmc_path));
	sprintf(fmc_path, "fm%d/reasm/ib_post_reass", app_conf.fm);
	ib_reass = fmc_get_handle(&cmodel, fmc_path);
	if (!ib_reass)
		goto err;

	return 0;
err:
	fprintf(stderr, "error getting %s handle\n", fmc_path);
	fmc_clean(&cmodel);
	return -1;
}

void fmc_cleanup(void)
{
	fmc_clean(&cmodel);
}
