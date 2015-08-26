/* Copyright (c) 2011-2013 Freescale Semiconductor, Inc.
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
#include "usdpaa/fsl_dpa_classifier.h"
#include "usdpaa/fsl_dpa_ipsec.h"
#include "fm_ext.h"
#include "fm_pcd_ext.h"
#include "ncsw_ext.h"
#include <unistd.h>
#include <stdbool.h>
#include "fmc.h"

#include "app_config.h"
#include "app_common.h"
#include "ipsec_sizing.h"

static int ipsec_initialized;
static struct dpa_ipsec_params ipsec_params;

int ipsec_offload_init(int *dpa_ipsec_id)
{
	int i, cls_td;
	struct dpa_cls_tbl_params cls_tbl_params;
	int err;
	t_FmPcdParams pcd_params;

	err = dpa_classif_lib_init();
	if (err < 0) {
		fprintf(stderr, "dpa_classif_lib_init failed, err %d\n", err);
		goto out;
	}
	err = dpa_ipsec_lib_init();
	if (err < 0) {
		fprintf(stderr, "dpa_ipsec_lib_init failed, err %d\n", err);
		dpa_classif_lib_exit();
		goto out;
	}

	memset(&ipsec_params, 0, sizeof(ipsec_params));
	memset(&pcd_params, 0, sizeof(pcd_params));

	/* number of entries in flow_id_cc classifcation PCD xml node */
	ipsec_params.max_sa_pairs = app_conf.max_sa;
	ipsec_params.fm_pcd = pcd_dev;
	ipsec_params.ipf_bpid = app_conf.ipf_bpid;
	ipsec_params.qm_sec_ch = qm_channel_caam;

	for (i = 0; i < DPA_IPSEC_MAX_SA_TYPE; i++) {
		/* INB/DL pre SEC classifier */
		memset(&cls_tbl_params, 0, sizeof(cls_tbl_params));
		cls_tbl_params.cc_node = cc_in_rx[i];
		cls_tbl_params.type = DPA_CLS_TBL_HASH;
		cls_tbl_params.entry_mgmt = DPA_CLS_TBL_MANAGE_BY_REF;
		cls_tbl_params.hash_params.hash_offs = IN_SA_PCD_HASH_OFF;
		cls_tbl_params.hash_params.max_ways = get_in_sa_hash_ways(i);
		cls_tbl_params.hash_params.num_sets = get_in_sa_hash_sets(i);
		cls_tbl_params.hash_params.key_size = get_inb_key_size(i);

		err = dpa_classif_table_create(&cls_tbl_params, &cls_td);
		if (err < 0) {
			fprintf(stderr, "Error creating inbound SA classif table (%d), err %d\n",
					i, err);
			goto out_libs;
		}

		ipsec_params.pre_sec_in_params.dpa_cls_td[i] = cls_td;
	}

	/* INB/DL  post SEC params */
	if (rta_get_sec_era() < RTA_SEC_ERA_5)
		ipsec_params.post_sec_in_params.data_off =
				SEC_DATA_OFF_BURST;
	else
		ipsec_params.post_sec_in_params.data_off =
				SEC_ERA_5_DATA_OFF_BURST;
	ipsec_params.post_sec_in_params.base_flow_id = IPSEC_START_IN_FLOW_ID;
	ipsec_params.post_sec_in_params.use_ipv6_pol = false;
	/*
	 * If aggregation is enabled, decrypted
	 * traffic from multiple SAs on inbound direction, will be sent
	 * on outbound O/H port pre encryption, encrypted through one SA
	 * and sent to the TX port. This is accomplished by assigning the
	 * outbound pre SEC offline port Tx channel to the inbound post
	 * SEC queues.(traffic from those queues will be enqueued to outbound
	 * O/H pre SEC port)
	 * Simple schema of aggregation: OB_SA1 = IB_SA1 + IB_SA2 + IB_SA3
	 */
	if (app_conf.ib_aggreg == true)
		ipsec_params.post_sec_in_params.qm_tx_ch =
					      app_conf.ob_oh_pre->tx_channel_id;
	else
		ipsec_params.post_sec_in_params.qm_tx_ch =
						  app_conf.ib_oh->tx_channel_id;

	/* INB policy verification */
	ipsec_params.post_sec_in_params.dpa_cls_td = DPA_OFFLD_DESC_NONE;
	ipsec_params.post_sec_in_params.do_pol_check = false;

	/* OUTB/UL post SEC params */
	if (rta_get_sec_era() < RTA_SEC_ERA_5)
		ipsec_params.post_sec_out_params.data_off =
				SEC_DATA_OFF_BURST;
	else
		ipsec_params.post_sec_out_params.data_off =
				SEC_ERA_5_DATA_OFF_BURST;
	ipsec_params.post_sec_out_params.qm_tx_ch =
					app_conf.ob_oh_post->tx_channel_id;

	/* OUTB/UL pre SEC params */
	for (i = 0; i < DPA_IPSEC_MAX_SUPPORTED_PROTOS; i++) {
		if (cc_out_pre_enc[i] != NULL) {
			memset(&cls_tbl_params, 0, sizeof(cls_tbl_params));
			cls_tbl_params.cc_node = cc_out_pre_enc[i];
			cls_tbl_params.type = DPA_CLS_TBL_EXACT_MATCH;
			cls_tbl_params.entry_mgmt = DPA_CLS_TBL_MANAGE_BY_REF;
			cls_tbl_params.exact_match_params.entries_cnt =
						get_out_pol_num(i);
			cls_tbl_params.exact_match_params.key_size =
						get_outb_key_size(i);
			err = dpa_classif_table_create(&cls_tbl_params,
							&cls_td);
			if (err < 0) {
				fprintf(stderr, "Error creating outbound classif table (%d),err %d\n",
						i, err);
				goto out_outb_pre_sec;
			}

			ipsec_params.pre_sec_out_params.
				table[i].dpa_cls_td = cls_td;
			if (i == DPA_IPSEC_PROTO_ANY_IPV4 ||
			    i == DPA_IPSEC_PROTO_ANY_IPV6)
				ipsec_params.pre_sec_out_params.
					table[i].key_fields =
					IPSEC_OUT_POL_TCPUDP_KEY_FIELDS;
			else if (i == DPA_IPSEC_PROTO_ICMP_IPV4 ||
				 i == DPA_IPSEC_PROTO_ICMP_IPV6)
				ipsec_params.pre_sec_out_params.
					table[i].key_fields =
					IPSEC_OUT_POL_ICMP_KEY_FIELDS;
		} else
			ipsec_params.pre_sec_out_params.table[i].dpa_cls_td =
							DPA_OFFLD_DESC_NONE;
	}

	err = dpa_ipsec_init(&ipsec_params, dpa_ipsec_id);
	if (err < 0) {
		fprintf(stderr, "dpa_ipsec_init failed\n");
		goto out_outb_pre_sec;
	}

	ipsec_initialized = true;
	return 0;

out_outb_pre_sec:
	for (i = 0; i < DPA_IPSEC_MAX_SUPPORTED_PROTOS; i++)
		if (ipsec_params.pre_sec_out_params.table[i].dpa_cls_td !=
							DPA_OFFLD_DESC_NONE)
			dpa_classif_table_free(ipsec_params.
					pre_sec_out_params.table[i].dpa_cls_td);

	for (i = 0; i < DPA_IPSEC_MAX_SA_TYPE; i++)
		if (ipsec_params.pre_sec_in_params.dpa_cls_td[i] !=
			DPA_OFFLD_DESC_NONE)
			dpa_classif_table_free(ipsec_params.pre_sec_in_params.
						dpa_cls_td[i]);
out_libs:
	dpa_ipsec_lib_exit();
	dpa_classif_lib_exit();
out:
	return err;
}

int ipsec_offload_cleanup(int dpa_ipsec_id)
{
	int i, ret;
	if (!ipsec_initialized)
		return 0;

	ret = dpa_ipsec_free(dpa_ipsec_id);
	if (ret < 0) {
		fprintf(stderr, "%s:%d: error freeing dpa ipsec instance %d\n",
			__func__, __LINE__, dpa_ipsec_id);
		return ret;
	}

	for (i = 0; i < DPA_IPSEC_MAX_SA_TYPE; i++)
		dpa_classif_table_free(ipsec_params.pre_sec_in_params.
				dpa_cls_td[i]);


	for (i = 0; i < DPA_IPSEC_MAX_SUPPORTED_PROTOS; i++)
		if (ipsec_params.pre_sec_out_params.table[i].dpa_cls_td !=
							DPA_OFFLD_DESC_NONE)
			dpa_classif_table_free(ipsec_params.
					pre_sec_out_params.table[i].dpa_cls_td);
	return 0;
}
