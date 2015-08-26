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
#include "fmc.h"
#include "ceetm_api.h"
#include "hash_table.h"
#include "fman_crc64_hash_func.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <error.h>
#include <assert.h>
#include <usdpaa/compat.h>
#include <usdpaa/fsl_dpa_classifier.h>
#include <usdpaa/fsl_dpa_stats.h>
#include <usdpaa/fsl_qman.h>
#include <usdpaa/dma_mem.h>
#include <ppac.h>
#include <fsl_fman.h>

#include <inttypes.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <arpa/inet.h>

#include <linux/if_vlan.h>

#include "ppam_if.h"
#include <ppac_interface.h>
#include "dpa_classif_demo.h"

static int		ppam_cli_parse(int		key,
				char			*arg,
				struct argp_state	*state);

static const uint8_t dscp_key[MAX_NUM_OF_DSCP_KEYS][APP_DSCP_TABLE_KEY_SIZE] = {
	{ 0x0A }, /* AF11 */
	{ 0x0C }, /* AF12 */     /* Channel 1 */
	{ 0x0E }, /* AF13 */

	{ 0x12 }, /* AF21 */
	{ 0x14 }, /* AF22 */     /* Channel 2 */
	{ 0x16 }, /* AF23 */

	{ 0x1A }, /* AF31 */
	{ 0x1C }, /* AF32 */     /* Channel 3 */
	{ 0x1E }, /* AF33 */

	{ 0x22 }, /* AF41 */
	{ 0x24 }, /* AF42 */     /* Channel 4 */
	{ 0x26 }  /* AF43 */
};

struct ppam_arguments ppam_args = {
	.fm		= 1,
	.port		= 0
};

const char ppam_doc[] = "DPA Classifier use case";

static const struct argp_option argp_opts[] = {
	{"fm",		'f', "INT", 0, "FMan index"},
	{"port",	't', "INT", 0, "FMan port index"},
	{}
};

const struct argp ppam_argp = {argp_opts, ppam_cli_parse, 0, ppam_doc};

struct fmc_model_t		cmodel;

t_Handle			ccnodes[3];

static struct hash_table	*hash_ipv4, *hash_ipv6;

LIST_HEAD(avail_ipv4_counters);
LIST_HEAD(avail_ipv6_counters);

struct counter_data		ipv4_counter[MAX_NUM_OF_IPv4_KEYS];
struct counter_data		ipv6_counter[MAX_NUM_OF_IPv6_KEYS];

static int			ipv4_td = DPA_OFFLD_DESC_NONE;
static int			ipv6_td = DPA_OFFLD_DESC_NONE;
static int			dscp_td = DPA_OFFLD_DESC_NONE;

static int			fwd_hmd_ipv4[MAX_NUM_OF_IPv4_KEYS];
static int			fwd_hmd_ipv6[MAX_NUM_OF_IPv6_KEYS];

static bool			populate_dscp;

static int			dpa_stats_id;
static int			em4_cnt_id = DPA_OFFLD_INVALID_OBJECT_ID;
static int			em6_cnt_id = DPA_OFFLD_INVALID_OBJECT_ID;
static int			emd_cnt_id = DPA_OFFLD_INVALID_OBJECT_ID;
static int			eth_cnt_id = DPA_OFFLD_INVALID_OBJECT_ID;
extern int			tmg_cnt_id;
extern int			cng_cnt_id;

static uint32_t			txfq = -1;

static int			inserted_ipv4_keys;
static int			inserted_ipv6_keys;

static void			*storage;

#if defined(B4860)
extern struct interface		intf;
#endif


void clean_up(void)
{
	int err, i;

	/* Remove the DPA Stats counters */
	if (em4_cnt_id != DPA_OFFLD_INVALID_OBJECT_ID)
		dpa_stats_remove_counter(em4_cnt_id);
	if (em6_cnt_id != DPA_OFFLD_INVALID_OBJECT_ID)
		dpa_stats_remove_counter(em6_cnt_id);
	if (emd_cnt_id != DPA_OFFLD_INVALID_OBJECT_ID)
		dpa_stats_remove_counter(emd_cnt_id);
	if (eth_cnt_id != DPA_OFFLD_INVALID_OBJECT_ID)
		dpa_stats_remove_counter(eth_cnt_id);
	if (tmg_cnt_id != DPA_OFFLD_INVALID_OBJECT_ID)
		dpa_stats_remove_counter(tmg_cnt_id);
	if (cng_cnt_id != DPA_OFFLD_INVALID_OBJECT_ID)
		dpa_stats_remove_counter(cng_cnt_id);

	err = dpa_stats_free(dpa_stats_id);
	if (err < 0)
		error(0, -err, "Failed to release DPA Stats instance\n");
	else
		printf("DPA Stats instance successfully released\n");

	/* Release DPA Stats instance */
	dpa_stats_lib_exit();

	printf("DPA Stats library successfully released\n");

	if (dscp_td != DPA_OFFLD_DESC_NONE) {
		/* Free DSCP DPA Classifier table */
		err = dpa_classif_table_free(dscp_td);
		if (err < 0)
			error(0, -err, "Failed to free DPA Classifier DSCP table (td=%d)",
					dscp_td);
		else
			printf("INFO: DPA Classifier DSCP table resources released\n");
	}

	if (ipv4_td != DPA_OFFLD_DESC_NONE) {
		/* Free IPv4 DPA Classifier table */
		err = dpa_classif_table_free(ipv4_td);
		if (err < 0)
			error(0, -err, "Failed to free DPA Classifier IPv4 table (td=%d)",
					ipv4_td);
		else
			printf("INFO: DPA Classifier IPv4 table resources released.\n");
	}

	if (ipv6_td != DPA_OFFLD_DESC_NONE) {
		/* Free IPv6 DPA Classifier table */
		err = dpa_classif_table_free(ipv6_td);
		if (err < 0)
			error(0, -err, "Failed to free DPA Classifier IPv6 table (td=%d)",
					ipv6_td);
		else
			printf("INFO: DPA Classifier IPv6 table resources released.\n");
	}

	/* Free header manipulation operations */
	for (i = 0; i < MAX_NUM_OF_IPv4_KEYS; i++)
		if (fwd_hmd_ipv4[i] != DPA_OFFLD_DESC_NONE)
			dpa_classif_free_hm(fwd_hmd_ipv4[i]);

	for (i = 0; i < MAX_NUM_OF_IPv6_KEYS; i++)
		if (fwd_hmd_ipv6[i] != DPA_OFFLD_DESC_NONE)
			dpa_classif_free_hm(fwd_hmd_ipv6[i]);

	/* Release DPA Classifier library */
	dpa_classif_lib_exit();

	/* Clean-up the FMC model */
	err = fmc_clean(&cmodel);
	if (err != 0)
		error(0, EBUSY, "Failed to clean-up PCD configuration");
	else
		printf("INFO: PCD configuration successfully restored.\n");

	/* Release the resources of hash tables used */
	hash_table_destroy(hash_ipv4);
	hash_ipv4 = NULL;
	hash_table_destroy(hash_ipv6);
	hash_ipv6 = NULL;
}

int create_exact_match_table(int *td, t_Handle ccnode, uint8_t key_size,
		unsigned int num_of_keys)
{
	struct dpa_cls_tbl_params	table_params;
	struct dpa_cls_tbl_action	miss_action;
	int				err = 0;

	/* Create an Exact Match table */
	memset(&table_params, 0, sizeof(table_params));
	table_params.type	= DPA_CLS_TBL_EXACT_MATCH;
	table_params.cc_node	= ccnode;
	table_params.entry_mgmt	= DPA_CLS_TBL_MANAGE_BY_KEY;
	table_params.prefilled_entries = 0;

	table_params.exact_match_params.key_size = key_size;
	table_params.exact_match_params.entries_cnt = num_of_keys;

	err = dpa_classif_table_create(&table_params, td);
	if (err < 0) {
		error(0, -err, "Failed to create DPA Classifier table");
		return err;
	}
	printf("Successfully CREATED DPA Classifier Exact Match table (td=%d).\n",
			*td);

	memset(&miss_action, 0, sizeof(struct dpa_cls_tbl_action));
	miss_action.enable_statistics = TRUE;
	miss_action.type = DPA_CLS_TBL_ACTION_DROP;

	err = dpa_classif_table_modify_miss_action(*td, &miss_action);
	if (err < 0) {
		error(0, -err, "Failed to modify DPA Classifier table");
		return err;
	}
	printf("Successfully Modified Miss Action for DPA Classifier Exact Match table (td=%d).\n",
			*td);

	return err;
}

int ppam_init(void)
{
	int					err, i;
	char					object_name[100];
	const char				*pcd_path;
	const char				*cfg_path;
	const char				*pdl_path;

	pcd_path = getenv("DEF_PCD_PATH");
	if (pcd_path == NULL) {
		error(0, EINVAL, "$DEF_PCD_PATH environment variable not defined");
		return -EINVAL;
	}

	cfg_path = getenv("DEF_CFG_PATH");
	if (cfg_path == NULL) {
		error(0, EINVAL, "$DEF_CFG_PATH environment variable not defined");
		return -EINVAL;
	}

	pdl_path = getenv("DEF_PDL_PATH");
	if (pdl_path == NULL) {
		error(0, EINVAL, "$DEF_PDL_PATH environment variable not defined");
		return -EINVAL;
	}

	printf("dpa_classifier_demo: using the following config file: %s\n",
		cfg_path);
	printf("dpa_classifier_demo: using the following PCD file: %s\n",
		pcd_path);
	printf("dpa_classifier_demo: using the following PDL file: %s\n",
		pdl_path);
	/* Parse the input XML files and create the FMC Model */
	err = fmc_compile(&cmodel,
			cfg_path,
			pcd_path,
			pdl_path,
			NULL,
			0,
			0,
			NULL);
	if (err != 0) {
		error(0, -err, "Failed to create the FMC Model");
		return err;
	}

	/* Execute the obtained FMC Model */
	err = fmc_execute(&cmodel);
	if (err != 0) {
		error(0, -err, "Failed to execute the FMC Model");
		return err;
	}

	printf("dpa_classifier_demo is assuming FMan:%d and port:%d\n",
		ppam_args.fm, ppam_args.port);

	/* Get the CC Node Handle */
	sprintf(object_name, "fm%d/port/1G/%d/ccnode/3_tuple_ipv4_classif",
		ppam_args.fm, ppam_args.port);
	ccnodes[0] = fmc_get_handle(&cmodel, object_name);
	if (!ccnodes[0]) {
		error(0, EINVAL, "Failed to acquire the IPv4 CC node handle. Are you using the correct parameters for this test and platform?");
		return -EINVAL;
	}

	sprintf(object_name, "fm%d/port/1G/%d/ccnode/3_tuple_ipv6_classif",
			ppam_args.fm, ppam_args.port);
	ccnodes[1] = fmc_get_handle(&cmodel, object_name);
	if (!ccnodes[1]) {
		error(0, EINVAL, "Failed to acquire the IPv6 CC node handle. Are you using the correct parameters for this test and platform?");
		return -EINVAL;
	}

	sprintf(object_name, "fm%d/port/1G/%d/ccnode/dscp_classif",
			ppam_args.fm, ppam_args.port);
	ccnodes[2] = fmc_get_handle(&cmodel, object_name);
	if (!ccnodes[2]) {
		error(0, EINVAL, "Failed to acquire the DSCP CC node handle. Are you using the correct parameters for this test and platform?");
		return -EINVAL;
	}

	/* Attempt to initialize the DPA Classifier user space library */
	err = dpa_classif_lib_init();
	if (err < 0) {
		error(0, -err, "Failed to initialize the DPA Classifier user space library");
		return err;
	}

	for (i = 0; i < MAX_NUM_OF_IPv4_KEYS; i++) {
		fwd_hmd_ipv4[i] = DPA_OFFLD_DESC_NONE;
		/*
		 * Add the counters control blocks to the available IPv4
		 * counters list.
		 */
		list_add(&ipv4_counter[i].node, &avail_ipv4_counters);
	}

	for (i = 0; i < MAX_NUM_OF_IPv6_KEYS; i++) {
		fwd_hmd_ipv6[i] = DPA_OFFLD_DESC_NONE;
		/*
		 * Add the counters control blocks to the available IPv6
		 * counters list.
		 */
		list_add(&ipv6_counter[i].node, &avail_ipv6_counters);
	}

	hash_ipv4 = hash_table_create(APP_TABLE_KEY_SIZE_IPv4, NUM_HASH_BUCKETS,
				     MAX_NUM_OF_IPv4_KEYS, crc64_hash_function);
	if (!hash_ipv4) {
		error(0, EINVAL, "Cannot create IPv4 application internal hash table");
		return -EINVAL;
	}

	hash_ipv6 = hash_table_create(APP_TABLE_KEY_SIZE_IPv6, NUM_HASH_BUCKETS,
				     MAX_NUM_OF_IPv6_KEYS, crc64_hash_function);
	if (!hash_ipv6) {
		error(0, EINVAL, "Cannot create IPv6 application internal hash table");
		return -EINVAL;
	}

	return 0;
}

void ppam_finish(void)
{
	clean_up();
}

static int create_dscp_table(void)
{
	struct dpa_cls_tbl_params	table_params;
	int				err = 0;

	/* Create an Exact Match table */
	memset(&table_params, 0, sizeof(table_params));
	table_params.type	= DPA_CLS_TBL_EXACT_MATCH;
	table_params.cc_node	= ccnodes[2];
	table_params.entry_mgmt	= DPA_CLS_TBL_MANAGE_BY_KEY;

	table_params.prefilled_entries = MAX_NUM_OF_DSCP_KEYS;
	table_params.exact_match_params.entries_cnt = MAX_NUM_OF_DSCP_KEYS;
	table_params.exact_match_params.key_size = APP_DSCP_TABLE_KEY_SIZE;

	err = dpa_classif_table_create(&table_params, &dscp_td);
	if (err < 0) {
		error(0, -err, "Failed to create DPA Classifier DSCP table");
		return err;
	}
	printf("\nSuccessfully CREATED DPA Classifier DSCP table (td=%d)\n",
			dscp_td);
	return err;
}

static int create_dpa_stats_counters(void)
{
	struct dpa_stats_params		stats_params;
	struct dpa_stats_cnt_params	cnt_params;
	struct dpa_stats_cls_cnt_params	cls_params;
	struct dpa_offload_lookup_key	**cls_keys;
	int err = 0, i = 0, j = 0;

	/* Attempt to initialize the DPA Stats user space library */
	err = dpa_stats_lib_init();
	if (err < 0) {
		error(0, -err, "Failed to initialize the DPA Stats user space library");
		return err;
	}

	printf("DPA Stats library successfully initialized\n");

	stats_params.max_counters = 6;
	stats_params.storage_area_len = 6000;

	stats_params.storage_area = malloc(stats_params.storage_area_len);
	if (!stats_params.storage_area) {
		error(0, ENOMEM, "Cannot allocate memory for DPA Stats storage area\n");
		return -ENOMEM;
	}

	/* Save storage area pointer */
	storage = stats_params.storage_area;

	err = dpa_stats_init(&stats_params, &dpa_stats_id);
	if (err < 0) {
		error(0, -err, "Failed to initialize DPA Stats instance\n");
		return err;
	}
	printf("\nSuccessfully Initialized DPA Stats instance: %d\n",
			dpa_stats_id);

	/* Allocate memory for keys array */
	cls_params.classif_tbl_params.keys = calloc((MAX_NUM_OF_IPv4_KEYS + 1),
			sizeof(**cls_keys));
	if (!cls_params.classif_tbl_params.keys) {
		error(0, ENOMEM, "Failed to allocate memory for keys");
		return -ENOMEM;
	}

	for (i = 0; i < MAX_NUM_OF_IPv4_KEYS; i++) {
		cls_params.classif_tbl_params.keys[i] = malloc(
				sizeof(struct dpa_offload_lookup_key));
		if (!cls_params.classif_tbl_params.keys[i]) {
			error(0, ENOMEM, "Failed to allocate memory for key");
			for (j = 0; j < i; j++)
				free(cls_params.classif_tbl_params.keys[j]);
			free(cls_params.classif_tbl_params.keys);
			return -ENOMEM;
		}

		/* No lookup key is configured in the class */
		memset(cls_params.classif_tbl_params.keys[i], 0,
				sizeof(struct dpa_offload_lookup_key));
	}

	/* Set it to NULL in order to retrieve statistics for miss */
	cls_params.classif_tbl_params.keys[MAX_NUM_OF_IPv4_KEYS] = NULL;


	/* Create Classifier IPv4 Exact Match Table class counter */
	cls_params.type = DPA_STATS_CNT_CLASSIF_TBL;
	/* The last member of the class will provide statistics for miss */
	cls_params.class_members = MAX_NUM_OF_IPv4_KEYS + 1;
	cls_params.classif_tbl_params.td = ipv4_td;
	cls_params.classif_tbl_params.cnt_sel = DPA_STATS_CNT_CLASSIF_BYTES |
						DPA_STATS_CNT_CLASSIF_PACKETS;
	cls_params.classif_tbl_params.key_type = DPA_STATS_CLASSIF_SINGLE_KEY;

	err = dpa_stats_create_class_counter(dpa_stats_id, &cls_params,
			&em4_cnt_id);
	if (err < 0) {
		error(0, -err, "Failed to create DPA Stats counter\n");
		return err;
	}
	printf("Successfully created DPA Stats counter: %d\n", em4_cnt_id);

	/* Table counters were created, release the allocated memory for keys*/
	for (i = 0; i <= MAX_NUM_OF_IPv4_KEYS; i++)
		free(cls_params.classif_tbl_params.keys[i]);
	free(cls_params.classif_tbl_params.keys);

	/* Allocate memory for keys array */
	cls_params.classif_tbl_params.keys = calloc((MAX_NUM_OF_IPv6_KEYS + 1),
							sizeof(**cls_keys));
	if (!cls_params.classif_tbl_params.keys) {
		error(0, ENOMEM, "Failed to allocate memory for keys");
		return -ENOMEM;
	}

	for (i = 0; i < MAX_NUM_OF_IPv6_KEYS; i++) {
		cls_params.classif_tbl_params.keys[i] = malloc(
				sizeof(struct dpa_offload_lookup_key));
		if (!cls_params.classif_tbl_params.keys[i]) {
			error(0, ENOMEM, "Failed to allocate memory for key");
			for (j = 0; j < i; j++)
				free(cls_params.classif_tbl_params.keys[j]);
			free(cls_params.classif_tbl_params.keys);
			return -ENOMEM;
		}

		/* No lookup key is configured in the class */
		memset(cls_params.classif_tbl_params.keys[i], 0,
				sizeof(struct dpa_offload_lookup_key));
	}

	/* Set it to NULL in order to retrieve statistics for miss */
	cls_params.classif_tbl_params.keys[MAX_NUM_OF_IPv6_KEYS] = NULL;

	/* Create Classifier IPv6 Exact Match Table class counter */
	cls_params.type = DPA_STATS_CNT_CLASSIF_TBL;
	/* The last member of the class will provide statistics for miss */
	cls_params.class_members = MAX_NUM_OF_IPv6_KEYS + 1;
	cls_params.classif_tbl_params.td = ipv6_td;
	cls_params.classif_tbl_params.cnt_sel = DPA_STATS_CNT_CLASSIF_BYTES |
						DPA_STATS_CNT_CLASSIF_PACKETS;
	cls_params.classif_tbl_params.key_type = DPA_STATS_CLASSIF_SINGLE_KEY;

	err = dpa_stats_create_class_counter(dpa_stats_id, &cls_params,
			&em6_cnt_id);
	if (err < 0) {
		error(0, -err, "Failed to create DPA Stats counter\n");
		return err;
	}
	printf("Successfully created DPA Stats counter: %d\n", em6_cnt_id);

	/* Table counters were created, release the allocated memory for keys*/
	for (i = 0; i <= MAX_NUM_OF_IPv6_KEYS; i++)
		free(cls_params.classif_tbl_params.keys[i]);
	free(cls_params.classif_tbl_params.keys);

	/* Allocate memory for DSCP keys array */
	cls_params.classif_tbl_params.keys = calloc((MAX_NUM_OF_DSCP_KEYS),
			sizeof(**cls_keys));
	if (!cls_params.classif_tbl_params.keys) {
		error(0, ENOMEM, "Failed to reallocate memory for keys");
		return -ENOMEM;
	}

	for (i = 0; i < MAX_NUM_OF_DSCP_KEYS; i++) {
		cls_params.classif_tbl_params.keys[i] = malloc(
				sizeof(struct dpa_offload_lookup_key));
		if (!cls_params.classif_tbl_params.keys[i]) {
			error(0, ENOMEM, "Failed to reallocate memory for key");
			for (j = 0; j < i; j++)
				free(cls_params.classif_tbl_params.keys[j]);
			free(cls_params.classif_tbl_params.keys);
			return -ENOMEM;
		}

		/* No lookup key is configured in the class */
		memset(cls_params.classif_tbl_params.keys[i], 0,
				sizeof(struct dpa_offload_lookup_key));
	}

	/* Create Classifier Indexed Table class counter */
	cls_params.type = DPA_STATS_CNT_CLASSIF_TBL;
	/* The last member of the class will provide statistics for miss */
	cls_params.class_members = MAX_NUM_OF_DSCP_KEYS;
	cls_params.classif_tbl_params.td = dscp_td;
	cls_params.classif_tbl_params.cnt_sel = DPA_STATS_CNT_CLASSIF_BYTES |
						DPA_STATS_CNT_CLASSIF_PACKETS;
	cls_params.classif_tbl_params.key_type = DPA_STATS_CLASSIF_SINGLE_KEY;

	err = dpa_stats_create_class_counter(dpa_stats_id, &cls_params,
			&emd_cnt_id);
	if (err < 0) {
		error(0, -err, "Failed to create DPA Stats counter\n");
		return err;
	}
	printf("Successfully created DPA Stats counter: %d\n", emd_cnt_id);

	/* Table counters were created, release the allocated memory for keys*/
	for (i = 0; i < MAX_NUM_OF_DSCP_KEYS; i++)
		free(cls_params.classif_tbl_params.keys[i]);
	free(cls_params.classif_tbl_params.keys);

	/* Create Ethernet single counter to retrieve all statistics */
	cnt_params.type = DPA_STATS_CNT_ETH;
	cnt_params.eth_params.cnt_sel = DPA_STATS_CNT_ETH_ALL;
	cnt_params.eth_params.src.engine_id = ppam_args.fm;
	cnt_params.eth_params.src.eth_id = DPA_STATS_ETH_1G_PORT0 +
			ppam_args.port;

	err = dpa_stats_create_counter(dpa_stats_id, &cnt_params, &eth_cnt_id);
	if (err < 0) {
		error(0, -err, "Failed to create DPA Stats single counter\n");
		return err;
	}
	printf("Successfully created DPA Stats counter: %d\n", eth_cnt_id);

	err = create_ceetm_counters(dpa_stats_id);
	if (err < 0)
		return err;

	return 0;
}

static int populate_dscp_table(void)
{
	int					i = 0, j = 0, err = 0;

	struct dpa_offload_lookup_key		key;
	struct dpa_cls_tbl_action		action;
	struct dpa_cls_tbl_entry_mod_params	modify;
	struct dpa_stats_cls_member_params	params;

	uint8_t				key_data[DPA_OFFLD_MAXENTRYKEYSIZE];
	uint8_t				mask_data[DPA_OFFLD_MAXENTRYKEYSIZE];

	/* Prepare action */
	memset(&action, 0, sizeof(action));
	action.type = DPA_CLS_TBL_ACTION_ENQ;
	action.enable_statistics = true;
	action.enq_params.hmd = DPA_OFFLD_DESC_NONE;

	/* Prepare lookup key */
	key.byte = key_data;
	key.mask = mask_data;
	key.size = APP_DSCP_TABLE_KEY_SIZE;
	memset(key.mask, 0xff, APP_DSCP_TABLE_KEY_SIZE);

	/* Prepare modify params */
	memset(&modify, 0, sizeof(modify));

	for (i = 0; i < NUM_OF_CHANNELS; i++) {
		for (j = 0; j < NUM_OF_QUEUES; j++) {
			/* Update lookup key */
			memcpy(key.byte, dscp_key[i * NUM_OF_QUEUES + j],
					APP_DSCP_TABLE_KEY_SIZE);

			/* Update action */
#if defined(B4860)
			action.enq_params.new_fqid = intf.lfq[i][j]->idx;
#else
			action.enq_params.new_fqid = txfq;
#endif
			action.enq_params.override_fqid = TRUE;

			modify.action = &action;
			modify.type = DPA_CLS_TBL_MODIFY_ACTION;


			err = dpa_classif_table_modify_entry_by_key(dscp_td, &key,
							&modify);
			if (err < 0) {
				error(0, -err, "Failed to modify entry #%d into DPA Classifier table (td=%d)",
						i * NUM_OF_QUEUES + j, dscp_td);
				return err;
			}
			params.type = DPA_STATS_CLS_MEMBER_SINGLE_KEY;
			params.key = &key;

			/* Modify DPA Stats class counter */
			err = dpa_stats_modify_class_counter(emd_cnt_id,
					&params, i * NUM_OF_QUEUES + j);
			if (err < 0) {
				error(0, -err, "Failed to modify DPA Stats counter\n");
				return err;
			}
		}
	}

	TRACE("Successfully populated DPA Classifier table (%d entries)\n", i);

	return err;
}

static int ppam_interface_init(struct ppam_interface	*p,
			const struct fm_eth_port_cfg	*cfg,
			unsigned int			num_tx_fqs,
			uint32_t			*flags __maybe_unused)
{
	int err = 0;
	unsigned int deq_sp = 0;

	p->num_tx_fqids = num_tx_fqs;
	p->tx_fqids = malloc(p->num_tx_fqids * sizeof(*p->tx_fqids));
	if (!p->tx_fqids)
		return -ENOMEM;

#ifdef ENABLE_PROMISC
	if ((cfg->fman_if->mac_type != fman_offline) &&
			(cfg->fman_if->mac_type != fman_mac_less))
		/* Enable promiscuous mode for testing purposes */
		fman_if_promiscuous_enable(cfg->fman_if);
#endif /* ENABLE_PROMISC */

	if (cfg->fman_if->mac_idx == ppam_args.port &&
			cfg->fman_if->fman_idx == ppam_args.fm) {
		deq_sp = cfg->fman_if->tx_channel_id & 0xF;

		err = ceetm_init(ppam_args.fm, deq_sp);
		if (err < 0) {
			error(0, -err, "Failed to initialize CEETM resources\n");
			clean_up();
			return err;
		} else {
			printf("Successfully initialized CEETM resources for FMan %d Port %d\n",
			ppam_args.fm, ppam_args.port);
		}

		/* Create the DPA Classifier DSCP table */
		err = create_dscp_table();
		if (err < 0) {
			error(0, -err, "Failed to created DPA Classifier DSCP Table\n");
			clean_up();
			ceetm_free(clean_tx_fqs);
		}

		/* Create the DPA Classifier IPv4 table */
		err = create_exact_match_table(&ipv4_td, ccnodes[0],
				APP_TABLE_KEY_SIZE_IPv4,
				MAX_NUM_OF_IPv4_KEYS);
		if (err < 0) {
			error(0, -err, "Failed to create DPA Classifier Exact Match IPv4 Table\n");
			clean_up();
			ceetm_free(clean_tx_fqs);
		}

		/* Create the DPA Classifier IPv6 table */
		err = create_exact_match_table(&ipv6_td, ccnodes[1],
				APP_TABLE_KEY_SIZE_IPv6,
				MAX_NUM_OF_IPv6_KEYS);
		if (err < 0) {
			error(0, -err, "Failed to create DPA Classifier Exact Match IPv6 Table\n");
			clean_up();
			ceetm_free(clean_tx_fqs);
		}

		/* Create the DPA Stats counters */
		err = create_dpa_stats_counters();
		if (err < 0) {
			error(0, -err, "Failed to create DPA Stats counters");
			clean_up();
			ceetm_free(clean_tx_fqs);
		}
	}

	return 0;
}

static void ppam_interface_finish(struct ppam_interface *p)
{

	free(p->tx_fqids);
	ceetm_free(clean_tx_fqs);
}

static void ppam_interface_tx_fqid(struct ppam_interface	*p,
				unsigned			idx,
				uint32_t			fqid)
{
	p->tx_fqids[idx] = fqid;
	if (txfq == -1)
		txfq = fqid;
}

static int ppam_rx_error_init(struct ppam_rx_error	*p,
			struct ppam_interface		*_if,
			struct qm_fqd_stashing		*stash_opts)
{
	return 0;
}

static void ppam_rx_error_finish(struct ppam_rx_error	*p,
				struct ppam_interface	*_if)
{
}

static inline void ppam_rx_error_cb(struct ppam_rx_error	*p,
				struct ppam_interface		*_if,
				const struct qm_dqrr_entry	*dqrr)
{
	const struct qm_fd	*fd = &dqrr->fd;

	if ((fd->format == qm_fd_contig) || (fd->format == qm_fd_sg))
		TRACE("RX ERROR: FQID=%d, frame_format=%d, size=%d bytes, RX status=%#08x\n",
			dqrr->fqid, fd->format, fd->length20, fd->status);
	else if ((fd->format == qm_fd_contig_big) ||
						(fd->format == qm_fd_sg_big))
		TRACE("RX ERROR: FQID=%d, frame_format=%d, size=%d bytes, RX status=%#08x\n",
			dqrr->fqid, fd->format, fd->length29, fd->status);
	else
		TRACE("RX ERROR: FQID=%d, frame_format=%d, RX status=%#08x\n",
			dqrr->fqid, fd->format, fd->status);

	ppac_drop_frame(fd);
}

static int ppam_rx_default_init(struct ppam_rx_default	*p,
				struct ppam_interface	*_if,
				unsigned int idx,
				struct qm_fqd_stashing	*stash_opts)
{
	return 0;
}

static void ppam_rx_default_finish(struct ppam_rx_default	*p,
				struct ppam_interface		*_if)
{
}

static inline void ppam_rx_default_cb(struct ppam_rx_default	*p,
				struct ppam_interface		*_if,
				const struct qm_dqrr_entry	*dqrr)
{
	const struct qm_fd	*fd = &dqrr->fd;

	if ((fd->format == qm_fd_contig) || (fd->format == qm_fd_sg))
		TRACE("RX DEFAULT: FQID=%d, frame_format=%d, size=%d bytes\n",
			dqrr->fqid, fd->format, fd->length20);
	else if ((fd->format == qm_fd_contig_big) ||
						(fd->format == qm_fd_sg_big))
		TRACE("RX DEFAULT: FQID=%d, frame_format=%d, size=%d bytes\n",
			dqrr->fqid, fd->format, fd->length29);
	else
		TRACE("RX DEFAULT: FQID=%d, frame_format=%d\n",
			dqrr->fqid, fd->format);

	ppac_drop_frame(fd);
}

static int ppam_tx_error_init(struct ppam_tx_error	*p,
			struct ppam_interface		*_if,
			struct qm_fqd_stashing		*stash_opts)
{
	return 0;
}

static void ppam_tx_error_finish(struct ppam_tx_error	*p,
				struct ppam_interface	*_if)
{
}

static inline void ppam_tx_error_cb(struct ppam_tx_error	*p,
				struct ppam_interface		*_if,
				const struct qm_dqrr_entry	*dqrr)
{
	const struct qm_fd *fd = &dqrr->fd;

	if ((fd->format == qm_fd_contig) || (fd->format == qm_fd_sg))
		TRACE("TX ERROR: FQID=%d, frame_format=%d, size=%d bytes, TX status=%#08x\n",
			dqrr->fqid, fd->format, fd->length20, fd->status);
	else if ((fd->format == qm_fd_contig_big) ||
						(fd->format == qm_fd_sg_big))
		TRACE("TX ERROR: FQID=%d, frame_format=%d, size=%d bytes, TX status=%#08x\n",
			dqrr->fqid, fd->format, fd->length29, fd->status);
	else
		TRACE("TX ERROR: FQID=%d, frame_format=%d, TX status=%#08x\n",
			dqrr->fqid, fd->format, fd->status);

	ppac_drop_frame(fd);
}

static int ppam_tx_confirm_init(struct ppam_tx_confirm	*p,
				struct ppam_interface	*_if,
				struct qm_fqd_stashing	*stash_opts)
{
	return 0;
}

static void ppam_tx_confirm_finish(struct ppam_tx_confirm	*p,
				struct ppam_interface		*_if)
{
}

static inline void ppam_tx_confirm_cb(struct ppam_tx_confirm	*p,
				struct ppam_interface		*_if,
				const struct qm_dqrr_entry	*dqrr)
{
	const struct qm_fd *fd = &dqrr->fd;
	ppac_drop_frame(fd);
}

static int ppam_rx_hash_init(struct ppam_rx_hash	*p,
			struct ppam_interface		*_if,
			unsigned			idx,
			struct qm_fqd_stashing		*stash_opts)
{
	p->tx_fqid = _if->tx_fqids[idx % _if->num_tx_fqids];
	TRACE("Mapping Rx FQ %p:%d --> Tx FQID %d\n", p, idx, p->tx_fqid);
	return 0;
}

static void ppam_rx_hash_finish(struct ppam_rx_hash	*p,
				struct ppam_interface	*_if,
				unsigned		idx)
{
}

static inline void ppam_rx_hash_cb(struct ppam_rx_hash		*p,
				   const struct qm_dqrr_entry	*dqrr)
{
	void *addr;
	void *annotations;
	struct ether_header *prot_eth;
	const struct qm_fd *fd = &dqrr->fd;
	void *next_header;
	bool continue_parsing;
	uint16_t proto, len = 0;

	annotations = __dma_mem_ptov(qm_fd_addr(fd));
	TRACE("Rx: 2fwd	 fqid=%d\n", dqrr->fqid);
	switch (fd->format)	{
	case qm_fd_contig:
		TRACE("FD format = qm_fd_contig\n");
		addr = annotations + fd->offset;
		prot_eth = addr;
		break;

	case qm_fd_sg:
		TRACE("FD format = qm_fd_sg\n");
		addr = annotations + fd->offset;
		prot_eth = __dma_mem_ptov(qm_sg_entry_get64(addr)) +
				((struct qm_sg_entry *)addr)->offset;
		break;

	default:
		TRACE("FD format not supported!\n");
		BUG();
		break;
	}

	next_header = (prot_eth + 1);
	proto = prot_eth->ether_type;
	len = sizeof(struct ether_header);

	TRACE("	     phys=0x%"PRIx64", virt=%p, offset=%d, len=%d, bpid=%d\n",
	      qm_fd_addr(fd), addr, fd->offset, fd->length20, fd->bpid);
	TRACE("	     dhost="ETH_MAC_PRINTF_FMT"\n",
	      prot_eth->ether_dhost[0], prot_eth->ether_dhost[1],
	      prot_eth->ether_dhost[2], prot_eth->ether_dhost[3],
	      prot_eth->ether_dhost[4], prot_eth->ether_dhost[5]);
	TRACE("	     shost="ETH_MAC_PRINTF_FMT"\n",
	      prot_eth->ether_shost[0], prot_eth->ether_shost[1],
	      prot_eth->ether_shost[2], prot_eth->ether_shost[3],
	      prot_eth->ether_shost[4], prot_eth->ether_shost[5]);
	TRACE("	     ether_type=%04x\n", prot_eth->ether_type);
	/* Eliminate ethernet broadcasts. */
	if (prot_eth->ether_dhost[0] & 0x01)
		TRACE("	     -> dropping broadcast packet\n");
	else {
	continue_parsing = TRUE;
	while (continue_parsing) {
		switch (proto) {
		case ETHERTYPE_VLAN:
			TRACE("	       -> it's ETHERTYPE_VLAN!\n");
			{
			struct vlan_hdr *vlanhdr = (struct vlan_hdr *)
							(next_header);

			proto = vlanhdr->type;
			next_header = (void *)vlanhdr + sizeof(struct vlan_hdr);
			len = len + sizeof(struct vlan_hdr);
			}
			break;
		case ETHERTYPE_IP:
			TRACE("	       -> it's ETHERTYPE_IP!\n");
			{
			struct iphdr *iphdr = (typeof(iphdr))(next_header);
			u8 *src = (void *)&iphdr->saddr;
			u8 *dst = (void *)&iphdr->daddr;
			TRACE("		  ver=%d,ihl=%d,tos=%d,len=%d,id=%d\n",
				iphdr->version, iphdr->ihl, iphdr->tos,
				iphdr->tot_len, iphdr->id);
			TRACE("		  frag_off=%d,ttl=%d,prot=%d, csum=0x%04x\n",
					iphdr->frag_off, iphdr->ttl,
				iphdr->protocol, iphdr->check);
			TRACE("		  src=%d.%d.%d.%d\n",
				src[0], src[1], src[2], src[3]);
			TRACE("		  dst=%d.%d.%d.%d\n",
				dst[0], dst[1], dst[2], dst[3]);
			TRACE("Tx: 2fwd	 fqid=%d\n", p->tx_fqid);
			TRACE("	     phys=0x%"PRIx64", offset=%d, len=%d, bpid=%d\n",
					qm_fd_addr(fd), fd->offset,
					fd->length20, fd->bpid);
			}
			continue_parsing = FALSE;
			return;

		case ETHERTYPE_IPV6:
			TRACE("	       -> it's ETHERTYPE_IPV6!\n");
			{
			struct ip6_hdr *ipv6hdr = (typeof(ipv6hdr))
							(next_header);
			TRACE("	ver=%d, priority=%d, payload_len=%d, nexthdr=%d, hop_limit=%d\n",
				((ipv6hdr->ip6_vfc & 0xf0) >> 4),
				((ipv6hdr->ip6_flow & 0x0ff00000) >> 20),
				ipv6hdr->ip6_plen,
				ipv6hdr->ip6_nxt, ipv6hdr->ip6_hops);
			TRACE(" flow_lbl=%d.%d.%d\n",
				((ipv6hdr->ip6_flow & 0x000f0000) >> 16),
				((ipv6hdr->ip6_flow & 0x0000ff00) >> 8),
				(ipv6hdr->ip6_flow & 0x000000ff));
			TRACE(" src=%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x\n",
				ipv6hdr->ip6_src.__in6_u.__u6_addr16[0],
				ipv6hdr->ip6_src.__in6_u.__u6_addr16[1],
				ipv6hdr->ip6_src.__in6_u.__u6_addr16[2],
				ipv6hdr->ip6_src.__in6_u.__u6_addr16[3],
				ipv6hdr->ip6_src.__in6_u.__u6_addr16[4],
				ipv6hdr->ip6_src.__in6_u.__u6_addr16[5],
				ipv6hdr->ip6_src.__in6_u.__u6_addr16[6],
				ipv6hdr->ip6_src.__in6_u.__u6_addr16[7]);
			TRACE("	dst=%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x\n",
				ipv6hdr->ip6_dst.__in6_u.__u6_addr16[0],
				ipv6hdr->ip6_dst.__in6_u.__u6_addr16[1],
				ipv6hdr->ip6_dst.__in6_u.__u6_addr16[2],
				ipv6hdr->ip6_dst.__in6_u.__u6_addr16[3],
				ipv6hdr->ip6_dst.__in6_u.__u6_addr16[4],
				ipv6hdr->ip6_dst.__in6_u.__u6_addr16[5],
				ipv6hdr->ip6_dst.__in6_u.__u6_addr16[6],
				ipv6hdr->ip6_dst.__in6_u.__u6_addr16[7]);
			TRACE("Tx: 2fwd	 fqid=%d\n", p->tx_fqid);
			TRACE("	     phys=0x%"PRIx64", offset=%d, len=%d, bpid=%d\n",
					qm_fd_addr(fd), fd->offset,
					fd->length20, fd->bpid);
			}
			continue_parsing = FALSE;
			return;

		case ETHERTYPE_ARP:
			TRACE("	       -> it's ETHERTYPE_ARP!\n");

			{
			struct ether_arp *arp = (typeof(arp))(next_header);
			TRACE("		  hrd=%d, pro=%d, hln=%d, pln=%d, op=%d\n",
				arp->arp_hrd, arp->arp_pro, arp->arp_hln,
				arp->arp_pln, arp->arp_op);
			}

			TRACE("		  -> dropping ARP packet\n");
			continue_parsing = FALSE;
			break;
		default:
			TRACE("	       -> it's UNKNOWN (!!) type 0x%04x\n",
				prot_eth->ether_type);
			TRACE("		  -> dropping unknown packet\n");
			ppac_drop_frame(fd);
			continue_parsing = FALSE;
			break;
		}
	}
	}
	ppac_drop_frame(fd);
}

int populate_exact_match_table(int tbl_desc, int proto, uint8_t *key_byte)
{
	int					err, entry_id;
	struct dpa_offload_lookup_key		key;
	struct dpa_cls_tbl_action		action;
	struct dpa_stats_cls_member_params	params;
	struct counter_data			*counter_data;

	uint8_t				key_data[DPA_OFFLD_MAXENTRYKEYSIZE];
	uint8_t				mask_data[DPA_OFFLD_MAXENTRYKEYSIZE];

	/* Prepare action */
	memset(&action, 0, sizeof(action));
	action.type = DPA_CLS_TBL_ACTION_NEXT_TABLE;
	action.enable_statistics = true;
	action.next_table_params.next_td = dscp_td;
	action.next_table_params.hmd = DPA_OFFLD_DESC_NONE;

	/* Prepare lookup key */
	key.byte = key_data;
	key.mask = mask_data;

	switch (proto) {
	case IPv4:
		action.next_table_params.hmd = fwd_hmd_ipv4[inserted_ipv4_keys];
		/* Update lookup key */
		key.size = APP_TABLE_KEY_SIZE_IPv4;
		memset(key.mask, 0xff, APP_TABLE_KEY_SIZE_IPv4);
		memcpy(key.byte, key_byte, APP_TABLE_KEY_SIZE_IPv4);
		err = dpa_classif_table_insert_entry(tbl_desc, &key, &action,
								0, &entry_id);
		if (err < 0) {
			error(0, -err, "Failed to insert entry #%d in the DPA Classifier table (td=%d)",
					inserted_ipv4_keys, tbl_desc);
			return err;
		}

		params.type = DPA_STATS_CLS_MEMBER_SINGLE_KEY;
		params.key = &key;
		err = dpa_stats_modify_class_counter(em4_cnt_id, &params,
							inserted_ipv4_keys);
		if (err < 0) {
			error(0, -err, "Failed to modify DPA Stats counter\n");
			return err;
		}

		if (list_empty(&avail_ipv4_counters)) {
			error(0, ENOSPC, "No more counters available");
			return -ENOSPC;
		}
		counter_data = (struct counter_data*)
				list_entry(avail_ipv4_counters.next,
						struct counter_data, node);
		list_del(avail_ipv4_counters.next);

		counter_data->cnt_idx = inserted_ipv4_keys;
		counter_data->key_ref = entry_id;
		err = hash_table_insert(hash_ipv4, key_byte, counter_data);
		if (err < 0)
			return err;

		inserted_ipv4_keys++;
		/* Populate the DPA Classifier DSCP table */
		if (!populate_dscp) {
			err = populate_dscp_table();
			if (err < 0) {
				error(0, -err, "Failed to populate DPA Classifier DSCP Table\n");
				clean_up();
			}
			populate_dscp = true;
		}
		break;
	case IPv6:
		action.next_table_params.hmd = fwd_hmd_ipv6[inserted_ipv6_keys];
		/* Update lookup key */
		key.size = APP_TABLE_KEY_SIZE_IPv6;
		memset(key.mask, 0xff, APP_TABLE_KEY_SIZE_IPv6);
		memcpy(key.byte, key_byte, APP_TABLE_KEY_SIZE_IPv6);
		err = dpa_classif_table_insert_entry(tbl_desc, &key, &action,
								0, NULL);
		if (err < 0) {
			error(0, -err, "Failed to insert entry #%d in the DPA Classifier table (td=%d)",
					inserted_ipv6_keys, tbl_desc);
			return err;
		}

		params.type = DPA_STATS_CLS_MEMBER_SINGLE_KEY;
		params.key = &key;
		err = dpa_stats_modify_class_counter(em6_cnt_id, &params,
							inserted_ipv6_keys);
		if (err < 0) {
			error(0, -err, "Failed to modify DPA Stats counter\n");
			return err;
		}

		if (list_empty(&avail_ipv6_counters)) {
			error(0, ENOSPC, "No more counters available");
			return -ENOSPC;
		}
		counter_data = (struct counter_data*)
				list_entry(avail_ipv4_counters.next,
						struct counter_data, node);
		list_del(avail_ipv6_counters.next);

		counter_data->cnt_idx = inserted_ipv6_keys;
		counter_data->key_ref = entry_id;
		err = hash_table_insert(hash_ipv6, key_byte, counter_data);
		if (err < 0)
			return err;

		inserted_ipv6_keys++;
		/* Populate the DPA Classifier DSCP table */
		if (!populate_dscp) {
			err = populate_dscp_table();
			if (err) {
				error(0, -err, "Failed to populate DPA Classifier DSCP Table\n");
				return err;
			}
			populate_dscp = true;
		}
		break;
	}

	return 0;
}

int depopulate_exact_match_table(int tbl_desc, int proto, uint8_t *key_byte)
{
	int					err;
	struct dpa_offload_lookup_key		key;
	struct dpa_stats_cls_member_params	params;
	struct counter_data			*counter_data;

	uint8_t				key_data[DPA_OFFLD_MAXENTRYKEYSIZE];
	uint8_t				mask_data[DPA_OFFLD_MAXENTRYKEYSIZE];

	/* Prepare lookup key */
	key.mask = mask_data;

	switch (proto) {
	case IPv4:
		key.size = APP_TABLE_KEY_SIZE_IPv4;

		err = hash_table_remove(hash_ipv4,
					key_byte,
					(void**)&counter_data);
		if (err != 0) {
			error(0, err, "Remove key from IPv4 hash table\n");
			return err;
		}

		/* Invalidate the DPA Stats key */
		key.byte = NULL;
		params.type = DPA_STATS_CLS_MEMBER_SINGLE_KEY;
		params.key = &key;

		err = dpa_stats_modify_class_counter(em4_cnt_id, &params,
							counter_data->cnt_idx);
		if (err < 0) {
			error(0, -err, "Failed to modify DPA Stats counter\n");
			return err;
		}

		key.byte = key_data;
		memset(key.mask, 0xff, APP_TABLE_KEY_SIZE_IPv4);
		memcpy(key.byte, key_byte, APP_TABLE_KEY_SIZE_IPv4);

		err = dpa_classif_table_delete_entry_by_key(tbl_desc, &key);
		if (err < 0) {
			error(0, -err, "Failed to remove entry from DPA Classifier table (td=%d)",
					tbl_desc);
			return err;
		}

		/*
		 * Add counter control block back to the available control
		 * blocks list for IPv4
		 */
		list_add(&avail_ipv4_counters, &counter_data->node);

		inserted_ipv4_keys--;

		break;
	case IPv6:
		key.size = APP_TABLE_KEY_SIZE_IPv6;

		err = hash_table_remove(hash_ipv6,
					key_byte,
					(void**)&counter_data);
		if (err != 0) {
			error(0, err, "Remove key from IPv6 hash table\n");
			return err;
		}

		/* Invalidate the DPA Stats key */
		key.byte = NULL;
		params.type = DPA_STATS_CLS_MEMBER_SINGLE_KEY;
		params.key = &key;
		err = dpa_stats_modify_class_counter(em6_cnt_id, &params,
							counter_data->cnt_idx);
		if (err < 0) {
			error(0, -err, "Failed to modify DPA Stats counter\n");
			return err;
		}

		key.byte = key_data;
		memset(key.mask, 0xff, APP_TABLE_KEY_SIZE_IPv6);
		memcpy(key.byte, key_byte, APP_TABLE_KEY_SIZE_IPv6);

		err = dpa_classif_table_delete_entry_by_key(tbl_desc, &key);
		if (err < 0) {
			error(0, -err, "Failed to remove entry from DPA Classifier table (td=%d)",
					tbl_desc);
			return err;
		}

		/*
		 * Add counter control block back to the available control
		 * blocks list for IPv6
		 */
		list_add(&avail_ipv6_counters, &counter_data->node);

		inserted_ipv6_keys--;

		break;
	}

	return 0;
}

static void print_classif_dpa_stats_cnts(void)
{
	int i = 0;
	struct em_ipv4 {
		struct key_counter keys[MAX_NUM_OF_IPv4_KEYS + 1];
	};
	struct em_ipv6 {
		struct key_counter keys[MAX_NUM_OF_IPv6_KEYS + 1];
	};
	struct em_dscp {
		struct key_counter keys[MAX_NUM_OF_DSCP_KEYS];
	};
	struct classif_counters {
		struct em_dscp tbl_dscp;
		struct em_ipv4 tbl_ipv4;
		struct em_ipv6 tbl_ipv6;
	};

	struct classif_counters *cnts = (struct classif_counters *)storage;

	printf("\nCLASSIFIER STATISTICS\n");
	printf("\n\nExact Match IPv4 Table\n");
	printf("KEY::       BYTES   FRAMES\n");
	for (i = 0; i < inserted_ipv4_keys; i++)
		printf("%2d %14d %8d\n", i, cnts->tbl_ipv4.keys[i].bytes,
				cnts->tbl_ipv4.keys[i].frames);
	printf("MISS::      BYTES   FRAMES\n");
	printf("%17d %8d\n", cnts->tbl_ipv4.keys[MAX_NUM_OF_IPv4_KEYS].bytes,
			cnts->tbl_ipv4.keys[MAX_NUM_OF_IPv4_KEYS].frames);

	printf("\n\nExact Match IPv6 Table\n");
	printf("KEY::       BYTES   FRAMES\n");
	for (i = 0; i < inserted_ipv6_keys; i++)
		printf("%2d %14d %8d\n", i, cnts->tbl_ipv6.keys[i].bytes,
				cnts->tbl_ipv6.keys[i].frames);
	printf("MISS::      BYTES   FRAMES\n");
	printf("%17d %8d\n", cnts->tbl_ipv6.keys[MAX_NUM_OF_IPv6_KEYS].bytes,
			cnts->tbl_ipv6.keys[MAX_NUM_OF_IPv6_KEYS].frames);

	printf("\n\nExact Match DSCP Table\n");
	printf("KEY::       BYTES   FRAMES\n");
	for (i = 0; i < MAX_NUM_OF_DSCP_KEYS; i++)
		printf("%2d %14d %8d\n", i, cnts->tbl_dscp.keys[i].bytes,
				cnts->tbl_dscp.keys[i].frames);
}

void print_traffic_dpa_stats_cnts(void)
{
	struct traffic_counters *cnts = (struct traffic_counters *)storage;

	printf("\nTRAFFIC STATISTICS\n");

	printf("\n\nETHERNET\n");
	printf("IN:\n");
	printf("\tFRAMES: %d\n", cnts->eth.pkts);
	printf("\tBYTES: %d\n", cnts->eth.bytes);
	printf("\tDROPED FRAMES: %d\n", cnts->eth.dropped_pkts);
	printf("\tERRORS: %d\n", cnts->eth.in_errors);
	printf("OUT:\n");
	printf("\tFRAMES: %d\n", cnts->eth.out_pkts);
	printf("\tBYTES: %d\n", cnts->eth.out_bytes);
	printf("\tDROPED FRAMES: %d\n", cnts->eth.out_drop_pkts);
	printf("\tERRORS: %d\n", cnts->eth.out_errors);

	print_ceetm_counters(cnts);
}

void classif_request_done_cb(int dpa_id,
		unsigned int storage_area_offset,
		unsigned int cnts_written,
		int bytes_written)
{
	printf("storage_area_offset = %d\n", storage_area_offset);
	printf("cnts_written = %d\n", cnts_written);
	printf("bytes_written = %d\n", bytes_written);
	print_classif_dpa_stats_cnts();
}

void traffic_request_done_cb(int dpa_id,
		unsigned int storage_area_offset,
		unsigned int cnts_written,
		int bytes_written)
{
	printf("storage_area_offset = %d\n", storage_area_offset);
	printf("cnts_written = %d\n", cnts_written);
	printf("bytes_written = %d\n", bytes_written);
	print_traffic_dpa_stats_cnts();
}

static int get_cnts_statistics(enum dpa_stats_op op)
{
	struct dpa_stats_cnt_request_params req_params;
	int cnts_len = 0, err = 0;
	int classif_cnt_id[3] = {emd_cnt_id, em4_cnt_id, em6_cnt_id};
#if defined(B4860)
	int traffic_cnt_id[3] = {eth_cnt_id, tmg_cnt_id, cng_cnt_id};
#else
	int traffic_cnt_id[3] = {eth_cnt_id};
#endif
	int all_cnt_id[sizeof(classif_cnt_id) / sizeof(classif_cnt_id[0]) +
	sizeof(traffic_cnt_id) / sizeof(traffic_cnt_id[0])];

	memcpy(all_cnt_id, classif_cnt_id, sizeof(classif_cnt_id));
	memcpy(all_cnt_id + 3, traffic_cnt_id, sizeof(traffic_cnt_id));

	req_params.reset_cnts = FALSE;
	req_params.storage_area_offset = 0;

	switch (op) {
	case dpa_classif_stats_get_sync:
		req_params.cnts_ids = classif_cnt_id;
		req_params.cnts_ids_len =
			sizeof(classif_cnt_id) / sizeof(classif_cnt_id[0]);
		err = dpa_stats_get_counters(req_params, &cnts_len, NULL);
		if (err < 0) {
			error(0, -err, "Failed to create DPA Stats request\n");
			return err;
		}
		printf("\nSuccessfully created DPA Stats sync request\n");
		print_classif_dpa_stats_cnts();
		break;

	case dpa_classif_stats_get_async:
		req_params.cnts_ids = classif_cnt_id;
		req_params.cnts_ids_len =
			sizeof(classif_cnt_id) / sizeof(classif_cnt_id[0]);
		err = dpa_stats_get_counters(req_params,
				&cnts_len, &classif_request_done_cb);
		if (err < 0) {
			error(0, -err, "Failed to create DPA Stats request\n");
			return err;
		}
		printf("\nSuccessfully created DPA Stats async request\n");
		break;

	case dpa_traffic_stats_get_sync:
		req_params.cnts_ids = traffic_cnt_id;
		req_params.cnts_ids_len =
			sizeof(traffic_cnt_id) / sizeof(traffic_cnt_id[0]);
#if defined(B4860)
		err = ceetm_get_counters_sync(req_params, &cnts_len);
		if (err < 0)
			return err;
#else
		err = dpa_stats_get_counters(req_params, &cnts_len, NULL);
		if (err < 0) {
			error(0, -err, "Failed to create DPA Stats request\n");
			return err;
		}
		printf("\nSuccessfully created DPA Stats sync request\n");
		print_traffic_dpa_stats_cnts();
#endif
		break;

	case dpa_traffic_stats_get_async:
		req_params.cnts_ids = traffic_cnt_id;
		req_params.cnts_ids_len =
			sizeof(traffic_cnt_id)/sizeof(traffic_cnt_id[0]);
		err = dpa_stats_get_counters(req_params,
				&cnts_len, &traffic_request_done_cb);
		if (err < 0) {
			error(0, -err, "Failed to create DPA Stats request\n");
			return err;
		}
		printf("\nSuccessfully created DPA Stats async request\n");
		break;

	case dpa_stats_reset:
		err = dpa_stats_reset_counters(all_cnt_id,
				sizeof(all_cnt_id)/sizeof(all_cnt_id[0]));
		if (err < 0) {
			error(0, -err, "Failed to reset DPA Stats counters\n");
			return err;
		}
		printf("\nSuccessfully reset DPA Stats counters\n");
		break;
	default:
		printf("Invalid operation\n");
		break;
	}

	return 0;
}

static int ppam_cli_parse(int key, char *arg, struct argp_state *state)
{
	switch (key) {
	case 'f':
		ppam_args.fm = atoi(arg);
		if ((ppam_args.fm < 0) || (ppam_args.fm > 1)) {
			error(0, EINVAL, "FMan Id must be zero or 1");
			return -EINVAL;
		}
		break;
	case 't':
		ppam_args.port = atoi(arg);
		if ((ppam_args.port < 0) || (ppam_args.port > 5)) {
			error(0, EINVAL,
				"FMan port Id must be in the range 0-5");
			return -EINVAL;
		}
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

static bool check_mac_addr(char *mac, uint8_t *ret)
{
	int i = 0, j = 0;
	char *str;
	for (i = 0; i < MAC_CHARACTER_LEN; i++) {
		if (i % 3 != 2 && !isxdigit(mac[i]))
			return false;
		if (i % 3 == 2 && mac[i] != ':')
			return false;
	}
	if (mac[MAC_CHARACTER_LEN] != '\0')
		return false;

	str = strtok(mac, ":");
	while (str) {
		ret[j++] = strtoul(str, NULL, 16);
		str = strtok(NULL, ":");
	}

	return true;
}

static int ppac_cli_classif_rem_cmd(int argc, char *argv[])
{
	int i = 0, j = 0, err = 1, pos = 0, offset = 0;
	uint8_t key_ipv4[APP_TABLE_KEY_SIZE_IPv4];
	uint8_t key_ipv6[APP_TABLE_KEY_SIZE_IPv6];
	char data[100];

	struct sockaddr_in			addr4;
	struct sockaddr_in6			addr6;

	if (argc != REM_CMD_ARGC) {
		printf("Not a valid remove classification key command! Please try again!\n");
		printf("rem4/6 [IPSA][IPDA][PROT]\n");
		return -EINVAL;
	}

	if (!strcmp(argv[0], "rem4")) {
		if (inserted_ipv4_keys == 0) {
			printf("Cannot remove IPv4 key. Number of inserted keys is 0!\n");
			return -ENOSPC;
		}

		for (i = 1; i < argc - 1; i++) {
			err = inet_pton(AF_INET, argv[i], &(addr4.sin_addr));
			if (err != 1) {
				printf("Invalid IP address %s entered! Please try again!\n",
						argv[i]);
				return -EINVAL;
			}
			for (j = IPv4_LEN - 1; j >= 0; j--)
				key_ipv4[pos++] = (addr4.sin_addr.s_addr >>
						(j * 8)) & 0xFF;
		}

		if (!strcmp(argv[i], "TCP") ||
				!strcmp(argv[i], "tcp") ||
				!strcmp(argv[i], "6")) {
			key_ipv4[pos] = 0x6;
		} else if (!strcmp(argv[i], "UDP") ||
				!strcmp(argv[i], "udp") ||
				!strcmp(argv[i], "17")) {
			key_ipv4[pos] = 0x11;
		} else {
			printf("Not a valid protocol specified! Please chose between TCP and UDP and try again!\n");
			return -EINVAL;
		}

		for (j = 0; j < APP_TABLE_KEY_SIZE_IPv4; j++) {
			sprintf(&data[offset], " %02x", key_ipv4[j]);
			offset += 3;
		}

		data[offset] = 0;

		err = depopulate_exact_match_table(ipv4_td, IPv4, key_ipv4);
		if (err != 0) {
			printf("Cannot remove IPv4 Classification key %s. Please try again!\n",
					data);
			return -EINVAL;
		} else {
			printf("Successfully removed IPv4 Classification key %s\n",
					data);
		}
	}

	if (!strcmp(argv[0], "rem6")) {
		if (inserted_ipv6_keys == 0) {
			printf("Cannot remove IPv6 key. Number of inserted keys is 0!\n");
			return -ENOSPC;
		}

		for (i = 1; i < argc - 1; i++) {
			err = inet_pton(AF_INET6, argv[i], &(addr6.sin6_addr));
			if (err != 1) {
				printf("Invalid IP address %s entered! Please try again!\n",
						argv[i]);
				return -EINVAL;
			}
			for (j = 0; j < IPv6_LEN; j++)
				key_ipv6[pos++] = addr6.sin6_addr.s6_addr[j];
		}

		if (!strcmp(argv[i], "TCP") ||
				!strcmp(argv[i], "tcp") ||
				!strcmp(argv[i], "6")) {
			key_ipv6[pos] = 0x6;
		} else if (!strcmp(argv[i], "UDP") ||
				!strcmp(argv[i], "udp") ||
				!strcmp(argv[i], "17")) {
			key_ipv6[pos] = 0x11;
		} else {
			printf("Not a valid protocol specified! Please chose between TCP and UDP and try again!\n");
			return -EINVAL;
		}

		for (j = 0; j < APP_TABLE_KEY_SIZE_IPv6; j++) {
			sprintf(&data[offset], " %02x", key_ipv6[j]);
			offset += 3;
		}

		data[offset] = 0;

		err = depopulate_exact_match_table(ipv6_td, IPv6, key_ipv6);
		if (err != 0) {
			printf("Cannot remove IPv6 Classification key %s. Please try again!\n",
					data);
			return -EINVAL;
		} else {
			printf("Successfully removed IPv6 Classification key %s\n",
					data);
		}
	}

	return 0;
}

static int ppac_cli_classif_add_cmd(int argc, char *argv[])
{
	int i = 0, j = 0, err = 1, pos = 0, offset = 0;
	uint8_t key_ipv4[APP_TABLE_KEY_SIZE_IPv4];
	uint8_t key_ipv6[APP_TABLE_KEY_SIZE_IPv6];
	uint8_t mac[ETH_ALEN];
	char data[100], object_name[100];

	t_Handle				hm_fwd;

	struct sockaddr_in			addr4;
	struct sockaddr_in6			addr6;
	struct dpa_cls_hm_fwd_params		fwd_params;
	struct dpa_cls_hm_fwd_resources		fwd_hm_res;

	if (argc != ADD_CMD_ARGC) {
		printf("Not a valid add classification key command! Please try again!\n");
		printf("add4/6 [IPSA][IPDA][PROT][MACDA]\n");
		return -EINVAL;
	}

	memset(&fwd_params, 0, sizeof(fwd_params));
	memset(&fwd_hm_res, 0, sizeof(fwd_hm_res));

	fwd_params.out_if_type = DPA_CLS_HM_IF_TYPE_ETHERNET;
	fwd_params.fm_pcd = NULL;

	if (!strcmp(argv[0], "add4")) {
		if (inserted_ipv4_keys == MAX_NUM_OF_IPv4_KEYS) {
			printf("Cannot add more than %d IPv4 keys.\n",
							MAX_NUM_OF_IPv4_KEYS);
			return -ENOSPC;
		}

		for (i = 1; i < argc - 2; i++) {
			err = inet_pton(AF_INET, argv[i], &(addr4.sin_addr));
			if (err != 1) {
				printf("Invalid IP address %s entered! Please try again!\n",
						argv[i]);
				return -EINVAL;
			}
			for (j = IPv4_LEN - 1; j >= 0; j--)
				key_ipv4[pos++] = (addr4.sin_addr.s_addr >>
								(j * 8)) & 0xFF;
		}

		if (!strcmp(argv[i], "TCP") ||
				!strcmp(argv[i], "tcp") ||
				!strcmp(argv[i], "6")) {
			key_ipv4[pos] = 0x6;
		} else if (!strcmp(argv[i], "UDP") ||
				!strcmp(argv[i], "udp") ||
				!strcmp(argv[i], "17")) {
			key_ipv4[pos] = 0x11;
		} else {
			printf("Not a valid protocol specified! Please chose between TCP and UDP and try again!\n");
			return -EINVAL;
		}

		if (!check_mac_addr(argv[i + 1], mac)) {
			printf("Not a valid MAC address entered! Please try again!\n");
			return -EINVAL;
		}

		memcpy(fwd_params.eth.macda, mac, ETH_ALEN);

		sprintf(object_name, "fm%d/hdr/fwd4_%d", ppam_args.fm,
							inserted_ipv4_keys + 1);
		hm_fwd = fmc_get_handle(&cmodel, object_name);
		if (!hm_fwd) {
			error(0, -EINVAL, "Cannot obtain handle for IPv4 forwarding header manipulation %d",
					inserted_ipv4_keys + 1);
			return -EINVAL;
		}

		fwd_hm_res.fwd_node = hm_fwd;

		for (j = 0; j < APP_TABLE_KEY_SIZE_IPv4; j++) {
			sprintf(&data[offset], " %02x", key_ipv4[j]);
			offset += 3;
		}

		data[offset] = 0;

		err = dpa_classif_set_fwd_hm(&fwd_params,
				DPA_OFFLD_DESC_NONE,
				&fwd_hmd_ipv4[inserted_ipv4_keys], true,
				&fwd_hm_res);
		if (err < 0) {
			error(0, -err, "Failed to set up forwarding header manipulation.\n");
			return -err;
		}

		err = populate_exact_match_table(ipv4_td, IPv4, key_ipv4);
		if (err != 0) {
			printf("Cannot add IPv4 Classification key %s. Please try again!\n",
					data);
			return -EINVAL;
		} else {
			printf("Successfully added IPv4 Classification key %s\n",
					data);
		}
	}

	if (!strcmp(argv[0], "add6")) {
		if (inserted_ipv6_keys == MAX_NUM_OF_IPv6_KEYS) {
			printf("Cannot add more than %d IPv6 keys.\n",
							MAX_NUM_OF_IPv6_KEYS);
			return -ENOSPC;
		}

		for (i = 1; i < argc - 2; i++) {
			err = inet_pton(AF_INET6, argv[i], &(addr6.sin6_addr));
			if (err != 1) {
				printf("Invalid IP address %s entered! Please try again!\n",
						argv[i]);
				return -EINVAL;
			}
			for (j = 0; j < IPv6_LEN; j++)
				key_ipv6[pos++] = addr6.sin6_addr.s6_addr[j];
		}

		if (!strcmp(argv[i], "TCP") ||
				!strcmp(argv[i], "tcp") ||
				!strcmp(argv[i], "6")) {
			key_ipv6[pos] = 0x6;
		} else if (!strcmp(argv[i], "UDP") ||
				!strcmp(argv[i], "udp") ||
				!strcmp(argv[i], "17")) {
			key_ipv6[pos] = 0x11;
		} else {
			printf("Not a valid protocol specified! Please chose between TCP and UDP and try again!\n");
			return -EINVAL;
		}

		if (!check_mac_addr(argv[i + 1], mac)) {
			printf("Not a valid MAC address entered! Please try again!\n");
			return -EINVAL;
		}

		memcpy(fwd_params.eth.macda, mac, ETH_ALEN);

		sprintf(object_name, "fm%d/hdr/fwd6_%d", ppam_args.fm,
							inserted_ipv6_keys + 1);
		hm_fwd = fmc_get_handle(&cmodel, object_name);
		if (!hm_fwd) {
			error(0, EINVAL, "Cannot obtain handle for IPv6 forwarding header manipulation %d",
					inserted_ipv6_keys + 1);
			return -EINVAL;
		}

		fwd_hm_res.fwd_node = hm_fwd;

		for (j = 0; j < APP_TABLE_KEY_SIZE_IPv6; j++) {
			sprintf(&data[offset], " %02x", key_ipv6[j]);
			offset += 3;
		}

		data[offset] = 0;

		err = dpa_classif_set_fwd_hm(&fwd_params,
				DPA_OFFLD_DESC_NONE,
				&fwd_hmd_ipv6[inserted_ipv6_keys], true,
				&fwd_hm_res);
		if (err < 0) {
			error(0, -err, "Failed to set up forwarding header manipulation.\n");
			return err;
		}

		err = populate_exact_match_table(ipv6_td, IPv6, key_ipv6);
		if (err != 0) {
			printf("Cannot add IPv6 Classification key %s. Please try again!\n",
					data);
			return err;
		} else {
			printf("Successfully added IPv6 Classification key %s\n",
					data);
		}
	}

	return 0;
}

static int ppac_cli_dpa_stats_cmd(int argc, char *argv[])
{
	if (!strcmp(argv[0], "get_classif_stats"))
		get_cnts_statistics(dpa_classif_stats_get_async);
	else if (!strcmp(argv[0], "get_classif_stats_sync"))
		get_cnts_statistics(dpa_classif_stats_get_sync);
	else if (!strcmp(argv[0], "get_traffic_stats"))
		get_cnts_statistics(dpa_traffic_stats_get_async);
	else if (!strcmp(argv[0], "get_traffic_stats_sync"))
		get_cnts_statistics(dpa_traffic_stats_get_sync);
	else if (!strcmp(argv[0], "reset_stats"))
		get_cnts_statistics(dpa_stats_reset);

	return 0;
}

/* Register PPAC CLI commands */
cli_cmd(rem4, ppac_cli_classif_rem_cmd);
cli_cmd(rem6, ppac_cli_classif_rem_cmd);
cli_cmd(add4, ppac_cli_classif_add_cmd);
cli_cmd(add6, ppac_cli_classif_add_cmd);
cli_cmd(get_classif_stats, ppac_cli_dpa_stats_cmd);
cli_cmd(get_classif_stats_sync, ppac_cli_dpa_stats_cmd);
cli_cmd(get_traffic_stats, ppac_cli_dpa_stats_cmd);
cli_cmd(get_traffic_stats_sync, ppac_cli_dpa_stats_cmd);
cli_cmd(reset_stats, ppac_cli_dpa_stats_cmd);

/* Inline the PPAC machinery */
#include <ppac.c>
