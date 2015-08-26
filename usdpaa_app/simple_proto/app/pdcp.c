/* Copyright 2013 Freescale Semiconductor, Inc.
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "pdcp.h"
#include "pdcp_test_vector.h"

/* Forward declarations */
static error_t parse_opts(int, char *, struct argp_state *);
static void unregister_pdcp(struct protocol_info *);

struct argp_option pdcp_options[] = {
	{"type", 'y', "TYPE",  0,
	 "Select PDCP PDU type:"
	 "\n\t 0 = Control Plane"
	 "\n\t 1 = User Plane"
	 "\n\t 2 = Short MAC"
	 "\n"},
	{"cipher", 'r', "CIPHER",  0,
	 "Ciphering algorithm:"
	 "\n0 = NULL     (EEA0)"
	 "\n1 = SNOW f8  (EEA1)"
	 "\n2 = AES-CTR  (EEA2)"
	 "\n3 = ZUC-E    (EEA3) (ERA >= 5)"
	 "\n"},
	{"integrity", 'i', "INTEGRITY",  0,
	"For PDCP Control Plane & Short MAC only"
	"\n\nSelect PDCP integrity algorithm:"
	 "\n0 = NULL     (EIA0)"
	 "\n1 = SNOW f9  (EIA1)"
	 "\n2 = AES-CMAC (EIA2)"
	 "\n3 = ZUC-I    (EIA3) (ERA >= 5)"
	 "\n"},
	{"direction", 'd', 0, 0,
	 "OPTIONAL PARAMETER"
	 "\n\nInput PDU is for downlink direction"
	 "\n"},
	{"snlen", 'x', "SNLEN", 0,
	 "For PDCP User Plane only"
	 "\n\nSelect PDCP PDU Sequence Number length:"
	 "\n0 = 12 bit Sequence Number PDU"
	 "\n1 = 7 bit Sequence Number PDU"
	 "\n2 = 15 bit Sequence Number PDU"
	 "\n"},
	{"hfn_ov", 'v', "HFN_OV_VAL", 0,
	 "OPTIONAL PARAMETER"
	 "\n\nEnable HFN override mechanism (only for Control & Data Plane)"
	 "\n"},
	{0}
};

/* Parser for PDCP command line options */
static struct argp pdcp_argp = {
	pdcp_options, parse_opts
};

static struct argp_child argp_children = {
		&pdcp_argp , 0, "PDCP protocol options", 2};

/*
 * NOTE: this function will be called iff HFN override is enabled; thus
 * no need to check if hfn_ov_en is true.
 */
static void set_enc_buf_cb(struct qm_fd *fd, uint8_t *buf,
		struct test_param *crypto_info)
{
	struct protocol_info *proto = crypto_info->proto;
	struct pdcp_params *pdcp_params = proto->proto_params;
	struct pdcp_ref_vector_s *ref_test_vector = proto->proto_vector;
	int i;
	/*
	 * NOTE: this is done to avoid checkpatch error "Do not initialise
	 * statics to 0 or NULL". So the start value is set to -1 (255) and
	 * preincrement is done instead of postincrement below.
	 */
	static uint8_t plain_data = -1;
	uint8_t offset = 0;
	uint32_t fd_cmd;

	if (CIPHER == crypto_info->mode) {
		fd_cmd = PDCP_DPOVRD_HFN_OV_EN | ref_test_vector->hfn;
		if (rta_sec_era > RTA_SEC_ERA_2) {
			fd->status = fd_cmd;
		} else {
			*(uint32_t *)buf = fd_cmd;
			offset = PDCP_P4080REV2_HFN_OV_BUFLEN;
		}
		memcpy(buf + offset, ref_test_vector->plaintext,
		       crypto_info->buf_size);
	} else {
		fd_cmd = PDCP_DPOVRD_HFN_OV_EN | pdcp_params->hfn_ov_val;
		if (rta_sec_era > RTA_SEC_ERA_2) {
			fd->status = fd_cmd;
		} else {
			*(uint32_t *)buf = fd_cmd;
			offset = PDCP_P4080REV2_HFN_OV_BUFLEN;
		}
		for (i = offset; i < crypto_info->buf_size; i++)
			buf[i] = ++plain_data;
	}
}

/*
 * NOTE: this function will be called iff HFN override is enabled; thus
 * no need to check if hfn_ov_en is true.
 */
static void set_dec_buf_cb(struct qm_fd *fd, uint8_t *buf,
			   struct test_param *crypto_info)
{
	struct protocol_info *proto = crypto_info->proto;
	struct pdcp_params *pdcp_params = proto->proto_params;
	struct pdcp_ref_vector_s *ref_test_vector = proto->proto_vector;
	uint32_t fd_cmd;
	if (CIPHER == crypto_info->mode) {
		fd_cmd = PDCP_DPOVRD_HFN_OV_EN | ref_test_vector->hfn;
		if (rta_sec_era > RTA_SEC_ERA_2)
			fd->status = fd_cmd;
		else
			*(uint32_t *)buf = fd_cmd;
	} else {
		fd_cmd = PDCP_DPOVRD_HFN_OV_EN |
			 pdcp_params->hfn_ov_val;
		if (rta_sec_era > RTA_SEC_ERA_2)
			fd->status = fd_cmd;
		else
			*(uint32_t *)buf = fd_cmd;
	}
}

/*
 * NOTE: This function is called iff SEC ERA is 2 AND HFN override
 * is enabled.
 */
static int test_enc_match_cb(int fd_ind, uint8_t *enc_buf,
			     struct test_param *crypto_info)
{
	struct protocol_info *proto = crypto_info->proto;
	struct pdcp_ref_vector_s *ref_test_vector = proto->proto_vector;

	return test_vector_match((uint32_t *)(enc_buf +
					PDCP_P4080REV2_HFN_OV_BUFLEN),
				 (uint32_t *)ref_test_vector->ciphertext,
				 (crypto_info->rt.output_buf_size -
					PDCP_P4080REV2_HFN_OV_BUFLEN) *
				 BITS_PER_BYTE);
}

/*
 * NOTE: This function is called iff HFN override is enabled.
 */
static int test_dec_match_cb(int fd_ind, uint8_t *dec_buf,
			     struct test_param *crypto_info)
{
	struct protocol_info *proto = crypto_info->proto;
	struct pdcp_ref_vector_s *ref_test_vector = proto->proto_vector;
	static uint8_t plain_data = 0;
	int i = rta_sec_era > RTA_SEC_ERA_2 ? 0 : PDCP_P4080REV2_HFN_OV_BUFLEN;

	if (CIPHER == crypto_info->mode)
		return  test_vector_match(
				  (uint32_t *)(dec_buf + i),
				  (uint32_t *)ref_test_vector->plaintext,
				  ref_test_vector->length);
	else
		for (;
		     i < crypto_info->buf_size;
		     i++)
			if (dec_buf[i] != plain_data++)
				return -1;

	return 0;
}

void init_rtv_pdcp_c_plane(struct test_param *crypto_info)
{
	struct protocol_info *proto = crypto_info->proto;
	struct pdcp_params *pdcp_params = proto->proto_params;
	struct pdcp_ref_vector_s *ref_test_vector = proto->proto_vector;

	const int proto_offset = PDCP_CPLANE_TEST_ARRAY_OFFSET(pdcp_params);
	uint8_t *cipherkey, *authkey;

	SAFE_STRNCPY(proto->name, pdcp_test_params[proto_offset].name,
		     sizeof(proto->name));

	cipherkey = __dma_mem_memalign(L1_CACHE_BYTES, PDCP_MAX_KEY_LEN);
	memcpy(cipherkey, pdcp_test_crypto_key[proto_offset], PDCP_MAX_KEY_LEN);

	authkey = __dma_mem_memalign(L1_CACHE_BYTES, PDCP_MAX_KEY_LEN);
	memcpy(authkey, pdcp_test_auth_key[proto_offset], PDCP_MAX_KEY_LEN);

	ref_test_vector->cipher_alg =
			pdcp_test_params[proto_offset].cipher_algorithm;
	ref_test_vector->dma_addr_key = __dma_mem_vtop(cipherkey);
	ref_test_vector->cipher_keylen = PDCP_MAX_KEY_LEN;

	ref_test_vector->auth_alg =
			pdcp_test_params[proto_offset].integrity_algorithm;
	ref_test_vector->dma_addr_auth_key = __dma_mem_vtop(authkey);
	ref_test_vector->auth_keylen = PDCP_MAX_KEY_LEN;

	ref_test_vector->bearer = pdcp_test_bearer[proto_offset];
	ref_test_vector->dir = pdcp_test_packet_direction[proto_offset];

	ref_test_vector->hfn = pdcp_test_hfn[proto_offset];
	ref_test_vector->hfn_thr = pdcp_test_hfn_threshold[proto_offset];

	if (CIPHER == crypto_info->mode) {
		ref_test_vector->length =
				NO_OF_BITS(pdcp_test_data_in_len[proto_offset]);
		ref_test_vector->plaintext = pdcp_test_data_in[proto_offset];
		ref_test_vector->ciphertext = pdcp_test_data_out[proto_offset];
	}
}

void init_rtv_pdcp_u_plane(struct test_param *crypto_info)
{
	struct protocol_info *proto = crypto_info->proto;
	struct pdcp_params *pdcp_params = proto->proto_params;
	struct pdcp_ref_vector_s *ref_test_vector = proto->proto_vector;
	const int proto_offset = PDCP_UPLANE_TEST_ARRAY_OFFSET(pdcp_params);
	uint8_t *cipherkey;

	SAFE_STRNCPY(proto->name, pdcp_test_params[proto_offset].name,
		     sizeof(proto->name));

	cipherkey = __dma_mem_memalign(L1_CACHE_BYTES, PDCP_MAX_KEY_LEN);
	memcpy(cipherkey, pdcp_test_crypto_key[proto_offset], PDCP_MAX_KEY_LEN);

	ref_test_vector->cipher_alg =
			pdcp_test_params[proto_offset].cipher_algorithm;
	ref_test_vector->dma_addr_key = __dma_mem_vtop(cipherkey);
	ref_test_vector->cipher_keylen = PDCP_MAX_KEY_LEN;

	ref_test_vector->bearer = pdcp_test_bearer[proto_offset];
	ref_test_vector->dir = pdcp_test_packet_direction[proto_offset];
	ref_test_vector->hfn = pdcp_test_hfn[proto_offset];
	ref_test_vector->hfn_thr = pdcp_test_hfn_threshold[proto_offset];
	ref_test_vector->sns = pdcp_test_data_sn_size[proto_offset];

	if (CIPHER == crypto_info->mode) {
		ref_test_vector->length =
				NO_OF_BITS(pdcp_test_data_in_len[proto_offset]);
		ref_test_vector->plaintext = pdcp_test_data_in[proto_offset];
		ref_test_vector->ciphertext = pdcp_test_data_out[proto_offset];
	}
}

void init_rtv_pdcp_short_mac(struct test_param *crypto_info)
{
	struct protocol_info *proto = crypto_info->proto;
	struct pdcp_params *pdcp_params = proto->proto_params;
	struct pdcp_ref_vector_s *ref_test_vector = proto->proto_vector;
	const int proto_offset = PDCP_SHORT_MAC_TEST_ARRAY_OFFSET(pdcp_params);
	uint8_t *authkey;

	SAFE_STRNCPY(proto->name, pdcp_test_params[proto_offset].name,
		     sizeof(proto->name));

	authkey = __dma_mem_memalign(L1_CACHE_BYTES, PDCP_MAX_KEY_LEN);

	memcpy(authkey, pdcp_test_auth_key[proto_offset], PDCP_MAX_KEY_LEN);

	ref_test_vector->auth_alg =
			pdcp_test_params[proto_offset].integrity_algorithm;

	ref_test_vector->dma_addr_auth_key = __dma_mem_vtop(authkey);
	ref_test_vector->auth_keylen = PDCP_MAX_KEY_LEN;

	if (CIPHER == crypto_info->mode) {
		ref_test_vector->length =
				NO_OF_BITS(pdcp_test_data_in_len[proto_offset]);
		ref_test_vector->plaintext = pdcp_test_data_in[proto_offset];
		ref_test_vector->ciphertext = pdcp_test_data_out[proto_offset];
	}

	crypto_info->authnct = 1;
}

int init_rtv_pdcp(struct test_param *crypto_info)
{
	struct protocol_info *proto = crypto_info->proto;
	struct pdcp_params *pdcp_params = proto->proto_params;

	switch (pdcp_params->type) {
	case PDCP_CONTROL_PLANE:
		init_rtv_pdcp_c_plane(crypto_info);
		break;

	case PDCP_DATA_PLANE:
		init_rtv_pdcp_u_plane(crypto_info);
		break;

	case PDCP_SHORT_MAC:
		init_rtv_pdcp_short_mac(crypto_info);
		break;

	default:
		fprintf(stderr, "Unknown PDCP PDU type %d (should never reach here)\n",
			pdcp_params->type);
		return -EINVAL;
	}

	return 0;
}


static void *create_descriptor(bool mode, void *params)
{
	struct test_param *crypto_info = (struct test_param *)params;
	struct protocol_info *proto = crypto_info->proto;
	struct pdcp_params *pdcp_params = proto->proto_params;
	struct pdcp_ref_vector_s *ref_test_vector = proto->proto_vector;
	struct sec_descriptor_t *prehdr_desc;
	struct alginfo cipher_info, auth_info;
	uint32_t *shared_desc = NULL;
	unsigned shared_desc_len = 0;
	unsigned sw_hfn_ov = 0;
	int i, hfn_val;
	bool found = 0;

	prehdr_desc = __dma_mem_memalign(L1_CACHE_BYTES,
					 sizeof(struct sec_descriptor_t));
	if (unlikely(!prehdr_desc)) {
		fprintf(stderr,
			"error: %s: dma_mem_memalign failed for preheader\n",
			__func__);
		return NULL;
	}

	/* Store the pointer to the descriptor for freeing later on */
	for (i = mode ? 0 : 1; i < proto->num_cpus * FQ_PER_CORE * 2; i += 2) {
		mutex_lock(&proto->desc_wlock);
		if (proto->descr[i].descr == NULL) {
			proto->descr[i].descr = (uint32_t *)prehdr_desc;
			proto->descr[i].mode = mode;
			found = 1;
			mutex_unlock(&proto->desc_wlock);
			break;
		}
		mutex_unlock(&proto->desc_wlock);
	}

	if (!found) {
		pr_err("Could not store descriptor pointer %s\n", __func__);
		return NULL;
	}

	memset(prehdr_desc, 0, sizeof(struct sec_descriptor_t));
	shared_desc = (typeof(shared_desc))&prehdr_desc->descbuf;

	cipher_info.algtype = ref_test_vector->cipher_alg;
	cipher_info.key = ref_test_vector->dma_addr_key;
	cipher_info.keylen = ref_test_vector->cipher_keylen;
	cipher_info.key_enc_flags = 0;

	auth_info.algtype = ref_test_vector->auth_alg;
	auth_info.key = ref_test_vector->dma_addr_auth_key;
	auth_info.keylen = ref_test_vector->auth_keylen;
	auth_info.key_enc_flags = 0;

	sw_hfn_ov = ((rta_sec_era == RTA_SEC_ERA_2) &&
		(pdcp_params->hfn_ov_en));
	hfn_val = pdcp_params->hfn_ov_en ?
			pdcp_params->hfn_ov_val : ref_test_vector->hfn;

	switch (pdcp_params->type) {
	case PDCP_CONTROL_PLANE:
		if (ENCRYPT == mode)
			cnstr_shdsc_pdcp_c_plane_encap(shared_desc,
						       &shared_desc_len,
/*
* This is currently hardcoded. The application doesn't allow for
* proper retrieval of PS.
*/
						       1,
						       hfn_val,
						       ref_test_vector->bearer,
						       ref_test_vector->dir,
						       ref_test_vector->hfn_thr,
						       &cipher_info,
						       &auth_info,
						       sw_hfn_ov);
		else
			cnstr_shdsc_pdcp_c_plane_decap(shared_desc,
						       &shared_desc_len,
/*
* This is currently hardcoded. The application doesn't allow for
* proper retrieval of PS.
*/
						       1,
						       hfn_val,
						       ref_test_vector->bearer,
						       ref_test_vector->dir,
						       ref_test_vector->hfn_thr,
						       &cipher_info,
						       &auth_info,
						       sw_hfn_ov);
		break;

	case PDCP_DATA_PLANE:
		if (ENCRYPT == mode)
			cnstr_shdsc_pdcp_u_plane_encap(shared_desc,
						       &shared_desc_len,
/*
* This is currently hardcoded. The application doesn't allow for
* proper retrieval of PS.
*/
						       1,
						       ref_test_vector->sns,
						       hfn_val,
						       ref_test_vector->bearer,
						       ref_test_vector->dir,
						       ref_test_vector->hfn_thr,
						       &cipher_info,
						       sw_hfn_ov);
		else
			cnstr_shdsc_pdcp_u_plane_decap(shared_desc,
						       &shared_desc_len,
/*
* This is currently hardcoded. The application doesn't allow for
* proper retrieval of PS.
*/
						       1,
						       ref_test_vector->sns,
						       hfn_val,
						       ref_test_vector->bearer,
						       ref_test_vector->dir,
						       ref_test_vector->hfn_thr,
						       &cipher_info,
						       sw_hfn_ov);
		break;

	case PDCP_SHORT_MAC:
		if (ENCRYPT == mode)
			cnstr_shdsc_pdcp_short_mac(shared_desc,
						   &shared_desc_len,
/*
* This is currently hardcoded. The application doesn't allow for
* proper retrieval of PS.
*/
						   1,
						   &auth_info);
		break;

	default:
		fprintf(stderr, "error: %s: Invalid PDCP type\n", __func__);
		return NULL;
	}

	prehdr_desc->prehdr.hi.word = shared_desc_len & SEC_PREHDR_SDLEN_MASK;

	pr_debug("SEC %s shared descriptor:\n", proto->name);

	for (i = 0; i < shared_desc_len; i++)
		pr_debug("0x%x\n", *shared_desc++);

	return prehdr_desc;
}

/**
 * @brief	Parse PDCP related command line options
 *
 */
static error_t parse_opts(int key, char *arg, struct argp_state *state)
{
	struct parse_input_t *input = state->input;
	uint32_t *p_proto_params = input->proto_params;
	struct test_param *crypto_info = input->crypto_info;
	struct pdcp_params *pdcp_params;

	/*
	 * If the protocol was not selected, then it makes no sense to go
	 * further.
	 */
	if (!crypto_info->proto)
		return 0;

	pdcp_params = crypto_info->proto->proto_params;
	switch (key) {
	case 'y':
		pdcp_params->type = atoi(arg);
		*p_proto_params |= BMASK_PDCP_TYPE;
		fprintf(stdout, "PDCP type = %d\n", pdcp_params->type);
		break;

	case 'r':
		pdcp_params->cipher_alg = atoi(arg);
		*p_proto_params |= BMASK_PDCP_CIPHER;
		break;

	case 'i':
		pdcp_params->integrity_alg = atoi(arg);
		*p_proto_params |= BMASK_PDCP_INTEGRITY;
		break;

	case 'd':
		pdcp_params->downlink = 1;
		*p_proto_params |= BMASK_PDCP_DIR_DL;
		break;

	case 'x':
		pdcp_params->sn_size = atoi(arg);
		*p_proto_params |= BMASK_PDCP_SN_SIZE;
		break;

	case 'v':
		pdcp_params->hfn_ov_val = atoi(arg);
		*p_proto_params |= BMASK_PDCP_HFN_OV_EN;
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

/**
 * @brief	Check SEC parameters provided by user for PDCP are valid
 *		or not.
 * @param[in]	g_proto_params - Bit mask of the optional parameters provided
 *		by user
 * @param[in]	crypto_info - test parameters
 * @return	0 on success, otherwise -EINVAL value
 */
static int validate_opts(uint32_t g_proto_params,
				struct test_param *crypto_info)
{
	struct protocol_info *proto = crypto_info->proto;
	struct pdcp_params *pdcp_params = proto->proto_params;
	int invalid = 0;

	switch (pdcp_params->type) {
	case PDCP_CONTROL_PLANE:
		if ((BMASK_PDCP_CPLANE_VALID & g_proto_params) !=
		     BMASK_PDCP_CPLANE_VALID)
			invalid = 1;
		break;

	case PDCP_DATA_PLANE:
		if ((BMASK_PDCP_UPLANE_VALID & g_proto_params) !=
		     BMASK_PDCP_UPLANE_VALID)
			invalid = 1;
		break;

	case PDCP_SHORT_MAC:
		if ((BMASK_PDCP_SHORT_MAC_VALID & g_proto_params) !=
		     BMASK_PDCP_SHORT_MAC_VALID)
			invalid = 1;
		break;

	default:
		invalid = 1;
		break;
	}

	if (invalid) {
		fprintf(stderr,
			"error: PDCP Invalid Parameters: Invalid type\n"
			"see --help option\n");
		return -EINVAL;
	}

	if (g_proto_params & BMASK_PDCP_CIPHER) {
		switch (pdcp_params->cipher_alg) {
		case PDCP_CIPHER_TYPE_NULL:
		case PDCP_CIPHER_TYPE_SNOW:
		case PDCP_CIPHER_TYPE_AES:
			break;

		case PDCP_CIPHER_TYPE_ZUC:
			if (rta_sec_era < RTA_SEC_ERA_5) {
				fprintf(stderr,
					"error: PDCP Invalid Parameters: Invalid cipher algorithm\n"
					"see --help option\n");
				return -EINVAL;
			}
			break;

		default:
			fprintf(stderr,
				"error: PDCP Invalid Parameters: Invalid cipher algorithm\n"
				"see --help option\n");
			return -EINVAL;
		}
	}

	if (g_proto_params & BMASK_PDCP_INTEGRITY) {
		switch (pdcp_params->type) {
		case PDCP_CONTROL_PLANE:
		case PDCP_SHORT_MAC:
			break;
		case PDCP_DATA_PLANE:
			fprintf(stderr, "error: PDCP Invalid Parameters: Invalid integrity setting for type\n"
					"see --help option\n");
			return -EINVAL;
		}

		switch (pdcp_params->integrity_alg) {
		case PDCP_AUTH_TYPE_NULL:
		case PDCP_AUTH_TYPE_SNOW:
		case PDCP_AUTH_TYPE_AES:
			break;

		case PDCP_AUTH_TYPE_ZUC:
			if (rta_sec_era < RTA_SEC_ERA_5) {
				fprintf(stderr,
					"error: PDCP Invalid Parameters: Invalid integrity algorithm\n"
					"see --help option\n");
				return -EINVAL;
			}
			break;

		default:
			fprintf(stderr,
				"error: PDCP Invalid Parameters: Invalid integrity algorithm\n"
				"see --help option\n");
			return -EINVAL;
		}
	}

	if (g_proto_params & BMASK_PDCP_SN_SIZE) {
		switch (pdcp_params->type) {
		case PDCP_DATA_PLANE:
			break;

		default:
			fprintf(stderr,
				"error: PDCP Invalid Parameters: Invalid sequence number for type\n"
				"see --help option\n");
			return -EINVAL;
		}
	}

	if (g_proto_params & BMASK_PDCP_HFN_OV_EN) {
		switch (pdcp_params->type) {
		case PDCP_CONTROL_PLANE:
		case PDCP_DATA_PLANE:
			break;

		default:
			fprintf(stderr,
				"error: PDCP Invalid Parameters: Invalid HFN override for type\n"
				"see --help option\n");
			return -EINVAL;
		}
		pdcp_params->hfn_ov_en = 1;

		/* Set the PDCP encap/decap callbacks, for modifying the FD */
		proto->set_enc_buf_cb = set_enc_buf_cb;
		proto->set_dec_buf_cb = set_dec_buf_cb;

		/*
		 * For ERA2, the in/out frames are not identical with the test
		 * vector. Override the callbacks here.
		 */
		if (rta_sec_era == RTA_SEC_ERA_2)
			proto->test_enc_match_cb = test_enc_match_cb;
		proto->test_dec_match_cb = test_dec_match_cb;
	} else {
		pdcp_params->hfn_ov_en = 0;
	}

	return 0;
}
static int get_buf_size(struct test_param *crypto_info)
{
	struct protocol_info *proto = crypto_info->proto;
	struct pdcp_params *pdcp_params = proto->proto_params;
	unsigned int total_size = crypto_info->buf_size;

	switch (pdcp_params->type) {
	case PDCP_CONTROL_PLANE:
	case PDCP_SHORT_MAC:
		total_size += crypto_info->buf_size + PDCP_MAC_I_LEN;
		break;

	case PDCP_DATA_PLANE:
		total_size += crypto_info->buf_size;
		break;
	}

	if (pdcp_params->hfn_ov_en && rta_sec_era == RTA_SEC_ERA_2)
		/* The input and output buffer are 4 bytes longer */
		total_size += 2 * PDCP_P4080REV2_HFN_OV_BUFLEN;

	return total_size;
}

/**
 * @brief	Set buffer sizes for input/output frames
 * @param[in]	crypto_info - test parameters
 * @return	0 on success, otherwise -EINVAL value
 */
static int set_buf_size(struct test_param *crypto_info)
{
	struct protocol_info *proto = crypto_info->proto;
	struct pdcp_params *pdcp_params = proto->proto_params;
	struct runtime_param *p_rt = &(crypto_info->rt);

	p_rt->input_buf_capacity = crypto_info->buf_size;
	p_rt->input_buf_length = crypto_info->buf_size;

	switch (pdcp_params->type) {
	case PDCP_CONTROL_PLANE:
	case PDCP_SHORT_MAC:
		crypto_info->rt.output_buf_size =
			crypto_info->buf_size + PDCP_MAC_I_LEN;
		break;

	case PDCP_DATA_PLANE:
		crypto_info->rt.output_buf_size = crypto_info->buf_size;
		break;

	default:
		fprintf(stderr, "error: %s: PDCP protocol type %d not supported\n",
			__func__,
			pdcp_params->type);
		return -EINVAL;
	}

	if (pdcp_params->hfn_ov_en && rta_sec_era == RTA_SEC_ERA_2) {
		/* The input buffer is 4 bytes longer */
		p_rt->input_buf_capacity += PDCP_P4080REV2_HFN_OV_BUFLEN;
		p_rt->input_buf_length += PDCP_P4080REV2_HFN_OV_BUFLEN;
		crypto_info->rt.output_buf_size += PDCP_P4080REV2_HFN_OV_BUFLEN;
	}

	return 0;
}

/**
 * @brief	Verifies if user gave a correct test set
 * @param[in]	crypto_info - test parameters
 * @return	0 on success, otherwise -EINVAL value
 */
static int validate_test_set(struct test_param *crypto_info)
{
	if (crypto_info->test_set == 1)
		return 0;

	fprintf(stderr, "error: Invalid Parameters: Test set number is invalid\n");
	return -EINVAL;
}

/**
 * @brief	Allocates the necessary structures for a protocol, sets the
 *		callbacks for the protocol and returns the allocated chunk.
 * @return	NULL if an error occurred, pointer to the protocol structure
 *		otherwise.
 */
struct protocol_info *register_pdcp(void)
{
	unsigned num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	struct protocol_info *proto_info = calloc(1, sizeof(*proto_info));

	if (unlikely(!proto_info)) {
		pr_err("failed to allocate protocol structure in %s",
		       __FILE__);
		return NULL;
	}

	SAFE_STRNCPY(proto_info->name, "PDCP", sizeof(proto_info->name));
	proto_info->unregister = unregister_pdcp;
	proto_info->argp_children = &argp_children;
	proto_info->init_ref_test_vector = init_rtv_pdcp;
	proto_info->setup_sec_descriptor = create_descriptor;
	proto_info->validate_opts = validate_opts;
	proto_info->get_buf_size = get_buf_size;
	proto_info->set_buf_size = set_buf_size;
	proto_info->validate_test_set = validate_test_set;
	proto_info->proto_params = calloc(1, sizeof(struct pdcp_params));
	if (unlikely(!proto_info->proto_params)) {
		pr_err("failed to allocate protocol parameters in %s",
		       __FILE__);
		goto err;
	}

	proto_info->proto_vector =
		calloc(1, sizeof(struct pdcp_ref_vector_s));
	if (unlikely(!proto_info->proto_vector)) {
		pr_err("failed to allocate protocol test vector in %s",
		       __FILE__);
		goto err;
	}

	/*
	 * For each "to SEC" FQ, there is one descriptor
	 * There are FQ_PER_CORE descriptors per core
	 * There is one descriptor for each "direction" (enc/dec).
	 * Thus the total number of descriptors that need to be stored across
	 * the whole system is:
	 *                       num_desc = num_cpus * FQ_PER_CORE * 2
	 * Note: This assumes that all the CPUs are used; if not all CPUs are
	 *       used, some memory will be wasted (equal to the # of unused
	 *       cores multiplied by sizeof(struct desc_storage))
	 */
	proto_info->descr = calloc(num_cpus * FQ_PER_CORE * 2,
				   sizeof(struct desc_storage));
	if (unlikely(!proto_info->descr)) {
		pr_err("failed to allocate descriptor storage in %s",
		       __FILE__);
		goto err;
	}
	mutex_init(&proto_info->desc_wlock);
	proto_info->num_cpus = num_cpus;

	return proto_info;
err:
	free(proto_info->proto_params);
	free(proto_info->proto_vector);
	free(proto_info);
	return NULL;
}

/**
 * @brief	Deallocates the structures for a protocol (allocated on
 *		registration) and frees any other memory that was allocated
 *		during the protocol processing.
 * @param[in]	proto_info - protocol parameters
 * @return	None
 *
 */
void unregister_pdcp(struct protocol_info *proto_info)
{
	int i;

	if (!proto_info)
		return;

	for (i = 0; i < proto_info->num_cpus * FQ_PER_CORE * 2; i++)
		if (proto_info->descr[i].descr)
			__dma_mem_free(proto_info->descr[i].descr);

	free(proto_info->proto_vector);
	free(proto_info->proto_params);
	free(proto_info);
}
