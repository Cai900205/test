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

#include "wimax.h"
#include "wimax_test_vector.h"

/* Forward declarations */
void unregister_wimax(struct protocol_info *);
static error_t parse_opts(int, char *, struct argp_state *);

struct argp_option wimax_options[] = {
	{"ofdma", 'a', 0, 0,
	 "OPTIONAL PARAMETER"
	 "\n\nEnable OFDMa processing (default: OFDM)\n"},
	{"fcs", 'f', 0, 0,
	 "OPTIONAL PARAMETER"
	 "\n\nEnable FCS calculation (default: off)\n"},
	{"ar_len", 'w', "ARWIN", 0,
	 "OPTIONAL PARAMETER"
	 "\nSet anti-replay window length\n"},
	{0}
};

/* Parser for WiMAX command line options */
static struct argp wimax_argp = {
	wimax_options, parse_opts
};

static struct argp_child argp_children = {
	 &wimax_argp, 0,
	 "WiMAX protocol options"
	 "\n\nNOTE: The WiMAX frame size, including the FCS if present,"
	 "\nmust be shorter than 2048 bytes."
	 "\n", 1
	};

int init_rtv_wimax_cipher(unsigned test_set,
		struct wimax_ref_vector_s *ref_test_vector)
{
	ref_test_vector->length =
	    wimax_reference_length[test_set - 1];

	ref_test_vector->plaintext =
		(uint8_t *)malloc(NO_OF_BYTES(ref_test_vector->length));
	if (unlikely(!ref_test_vector->plaintext)) {
		pr_err("failed to allocate WIMAX plaintext\n");
		return -ENOMEM;
	}
	memcpy(ref_test_vector->plaintext,
	       wimax_reference_gmh[test_set - 1],
	       WIMAX_GMH_SIZE);
	memcpy(ref_test_vector->plaintext + WIMAX_GMH_SIZE,
	       wimax_reference_payload[test_set - 1],
	       NO_OF_BYTES(ref_test_vector->length) - WIMAX_GMH_SIZE);

	ref_test_vector->ciphertext =
		(uint8_t *)malloc(NO_OF_BYTES(ref_test_vector->length) +
				  WIMAX_PN_SIZE +
				  WIMAX_ICV_SIZE +
				  WIMAX_FCS_SIZE);
	if (unlikely(!ref_test_vector->ciphertext)) {
		pr_err("failed to allocate WIMAX ciphertext\n");
		return -ENOMEM;
	}
	memcpy(ref_test_vector->ciphertext,
	       wimax_reference_enc_gmh[test_set - 1],
	       WIMAX_GMH_SIZE);
	memcpy(ref_test_vector->ciphertext + WIMAX_GMH_SIZE,
	       wimax_reference_enc_pn[test_set - 1],
	       WIMAX_PN_SIZE);
	memcpy(ref_test_vector->ciphertext + WIMAX_GMH_SIZE + WIMAX_PN_SIZE,
	       wimax_reference_enc_payload[test_set - 1],
	       NO_OF_BYTES(ref_test_vector->length) - WIMAX_GMH_SIZE);
	memcpy(ref_test_vector->ciphertext + WIMAX_PN_SIZE +
			NO_OF_BYTES(ref_test_vector->length),
	       wimax_reference_enc_icv[test_set - 1],
	       WIMAX_ICV_SIZE);
	memcpy(ref_test_vector->ciphertext +
	       WIMAX_PN_SIZE + NO_OF_BYTES(ref_test_vector->length) +
			WIMAX_ICV_SIZE,
	       wimax_reference_fcs[test_set - 1],
	       WIMAX_FCS_SIZE);

	return 0;
}

int init_rtv_wimax_aes_ccm_128(struct test_param *crypto_info)
{
	struct protocol_info *proto = crypto_info->proto;
	struct wimax_params *wimax_params = proto->proto_params;
	struct wimax_ref_vector_s *ref_test_vector = proto->proto_vector;
	int err;

	ref_test_vector->key = (uintptr_t)malloc(WIMAX_KEY_SIZE);
	if (unlikely(!ref_test_vector->key)) {
		pr_err("failed to allocate WIMAX key\n");
		return -ENOMEM;
	}

	memcpy((uint8_t *)(uintptr_t)ref_test_vector->key,
	       wimax_reference_key[crypto_info->test_set - 1],
	       WIMAX_KEY_SIZE);

	if (CIPHER == crypto_info->mode) {
		err = init_rtv_wimax_cipher(crypto_info->test_set,
				ref_test_vector);
		if (unlikely(err))
			return err;
	}

	/* set the WiMAX PDB for test */
	memcpy(&ref_test_vector->pn,
	       &wimax_reference_pn[crypto_info->test_set - 1],
	       WIMAX_PN_SIZE);

	if (wimax_params->ar) {
		ref_test_vector->decap_opts = WIMAX_PDBOPTS_AR;
		ref_test_vector->ar_len = wimax_params->ar_len;
	}

	if (PERF == crypto_info->mode) {
		if (wimax_params->ofdma)
			ref_test_vector->protinfo = OP_PCL_WIMAX_OFDMA;
		else
			ref_test_vector->protinfo = OP_PCL_WIMAX_OFDM;

		if (wimax_params->fcs) {
			ref_test_vector->encap_opts = WIMAX_PDBOPTS_FCS;
			ref_test_vector->decap_opts |= WIMAX_PDBOPTS_FCS;
		}
	} else {
		ref_test_vector->encap_opts =
			wimax_reference_pdb_opts[crypto_info->test_set - 1];
		ref_test_vector->decap_opts |=
			wimax_reference_pdb_opts[crypto_info->test_set - 1];
		ref_test_vector->protinfo =
			wimax_reference_protinfo[crypto_info->test_set - 1];
	}

	return 0;
}

void set_enc_buf_cb(struct qm_fd *fd, uint8_t *buf,
			  struct test_param *crypto_info)
{
	struct protocol_info *proto = crypto_info->proto;
	struct wimax_params *wimax_params = proto->proto_params;
	struct wimax_ref_vector_s *ref_test_vector = proto->proto_vector;
	uint8_t plain_data = 0;
	int i;

	/*
	 * Copy the input plain-text data.
	 * For WiMAX in PERF mode set the input plain-text data
	 * as GMH aware frames.
	 */
	if (CIPHER == crypto_info->mode) {
		memcpy(buf, ref_test_vector->plaintext, crypto_info->buf_size);
	} else {
		/* GMH Header Type bit shall be set to zero. */
		buf[0] &= 0x7f;
		/*
		 * Set CRC indicator bit to value one if FCS
		 * is included in the PDU.
		 */
		if (wimax_params->fcs)
			buf[1] |= 0x40;
		/* Set the input frame length */
		buf[1] &= ~0x7;
		buf[1] |= (crypto_info->buf_size >> 8) & 0x7;
		buf[2] = crypto_info->buf_size & 0xFF;

		for (i = WIMAX_GMH_SIZE; i < crypto_info->buf_size; i++)
			buf[i] = plain_data++;
	}
}

int test_enc_match_cb(int fd_ind, uint8_t *enc_buf,
			    struct test_param *crypto_info)
{
	struct wimax_ref_vector_s *ref_test_vector =
			crypto_info->proto->proto_vector;

	if ((fd_ind == 0) &&
	    (test_vector_match((uint32_t *)enc_buf,
			(uint32_t *)ref_test_vector->ciphertext,
			crypto_info->rt.output_buf_size * BITS_PER_BYTE) != 0))
		return -1;

	/*
	 * Even if the ciphertext of the encapsulation output frame is not
	 * predictable due to the Packet Number incrementation, the correctness
	 * of the output header can be checked.
	 */
	if (fd_ind &&
	    test_vector_match((uint32_t *)enc_buf,
			      (uint32_t *)ref_test_vector->ciphertext,
			      WIMAX_GMH_SIZE * BITS_PER_BYTE) != 0)
		 return -1;
	return 0;
}

int test_dec_match_cb(int fd_ind, uint8_t *dec_buf,
			    struct test_param *crypto_info)
{
	struct wimax_ref_vector_s *ref_test_vector =
				crypto_info->proto->proto_vector;
	uint8_t plain_data = 0;
	int i;

	if (CIPHER == crypto_info->mode) {
		if ((fd_ind == 0) &&
		    (test_vector_match((uint32_t *)dec_buf,
				       (uint32_t *)ref_test_vector->plaintext,
				       ref_test_vector->length) != 0))
			return -1;
	} else
		for (i = WIMAX_GMH_SIZE; i < crypto_info->buf_size; i++)
			if (dec_buf[i] != plain_data++)
				return -1;

	return 0;
}

static void *create_descriptor(bool mode, void *params)
{
	struct test_param *crypto_info = (struct test_param *)params;
	struct protocol_info *proto = crypto_info->proto;
	struct wimax_ref_vector_s *ref_test_vector = proto->proto_vector;
	struct sec_descriptor_t *prehdr_desc;
	struct alginfo cipher_info;
	uint32_t *shared_desc = NULL;
	unsigned shared_desc_len;
	int i;
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

	cipher_info.key = ref_test_vector->key;
	cipher_info.keylen = WIMAX_KEY_SIZE;
	cipher_info.key_enc_flags = 0;
	if (ENCRYPT == mode)
		cnstr_shdsc_wimax_encap(shared_desc,
					&shared_desc_len,
					ref_test_vector->encap_opts,
					ref_test_vector->pn,
					ref_test_vector->protinfo,
					&cipher_info);
	else
		cnstr_shdsc_wimax_decap(shared_desc,
					&shared_desc_len,
					ref_test_vector->decap_opts,
					ref_test_vector->pn,
					ref_test_vector->ar_len,
					ref_test_vector->protinfo,
					&cipher_info);

	prehdr_desc->prehdr.hi.word = shared_desc_len & SEC_PREHDR_SDLEN_MASK;

	pr_debug("SEC %s shared descriptor:\n", proto->name);

	for (i = 0; i < shared_desc_len; i++)
		pr_debug("0x%x\n", *shared_desc++);

	return prehdr_desc;
}

/**
 * @brief	Parse WiMAX related command line options
 *
 */
static error_t parse_opts(int key, char *arg, struct argp_state *state)
{
	struct parse_input_t *input = state->input;
	uint32_t *p_proto_params = input->proto_params;
	struct test_param *crypto_info = input->crypto_info;
	struct wimax_params *wimax_params;

	/*
	 * If the protocol was not selected, then it makes no sense to go
	 * further.
	 */
	if (!crypto_info->proto)
		return 0;

	wimax_params = crypto_info->proto->proto_params;

	switch (key) {
	case 'a':
		*p_proto_params |= BMASK_WIMAX_OFDMA_EN;
		fprintf(stdout, "WiMAX OFDMa selected\n");
		break;

	case 'f':
		*p_proto_params |= BMASK_WIMAX_FCS_EN;
		fprintf(stdout, "WiMAX FCS enabled\n");
		break;

	case 'w':
		*p_proto_params |= BMASK_WIMAX_AR_EN;
		wimax_params->ar_len = atoi(arg);
		fprintf(stdout, "Anti-Replay Length = %d\n", atoi(arg));
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

/**
 * @brief	Check SEC parameters provided by user for WiMAX are valid
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
	struct wimax_params *wimax_params = proto->proto_params;
	unsigned int ar_len;

	/* Only anti-replay is allowed CIPHER mode */
	if ((CIPHER == crypto_info->mode) &&
	    ((g_proto_params & BMASK_WIMAX_AR_EN) != g_proto_params)) {
		fprintf(stderr,
			"error: WiMAX Invalid Parameters: only anti-replay is allowed in CIPHER mode\n"
			"see --help option\n");
			return -EINVAL;
	}

	if ((PERF == crypto_info->mode) &&
	    (crypto_info->buf_size > WIMAX_MAX_FRAME_SIZE)) {
		fprintf(stderr,
			"error: WiMAX Invalid Parameters: Invalid buffer size\n"
			"see --help option\n");
		return -EINVAL;
	}

	/*
	 * For WiMAX in CIPHER mode only the first frame
	 * from the first iteration can be verified if it is matching
	 * with the corresponding test vector, due to
	 * the PN incrementation by SEC for each frame processed.
	 */
	if (CIPHER == crypto_info->mode && crypto_info->itr_num != 1) {
		crypto_info->itr_num = 1;
		printf("WARNING: Running WiMAX in CIPHER mode with only one iteration\n");
	}

	if (g_proto_params & BMASK_WIMAX_AR_EN) {
		ar_len = wimax_params->ar_len;
		if ((ar_len > 64) || (ar_len < 0)) {
			fprintf(stderr,
				"error: WiMAX Anti-Replay window length cannot be greater than 64 packets\n"
				"see --help option\n");
			return -EINVAL;
		}
	}

	/* Copy the params to the relevant structure */
	wimax_params->ofdma = g_proto_params & BMASK_WIMAX_OFDMA_EN ? 1 : 0;
	wimax_params->fcs = g_proto_params & BMASK_WIMAX_FCS_EN ? 1 : 0;
	wimax_params->ar = g_proto_params & BMASK_WIMAX_AR_EN ? 1 : 0;

	/* Set the WiMAX encap callback */
	proto->set_enc_buf_cb = set_enc_buf_cb;

	/* Set the WiMAX test callbacks */
	proto->test_enc_match_cb = test_enc_match_cb;
	proto->test_dec_match_cb = test_dec_match_cb;

	return 0;
}

static int get_buf_size(struct test_param *crypto_info)
{
	struct protocol_info *proto = crypto_info->proto;
	struct wimax_params *wimax_params = proto->proto_params;

	return 2 * crypto_info->buf_size + WIMAX_PN_SIZE +
			CIPHER == crypto_info->mode || wimax_params->fcs ?
					WIMAX_FCS_SIZE : 0;
}

/**
 * @brief	Set buffer sizes for input/output frames
 * @param[in]	crypto_info - test parameters
 * @return	0 on success, otherwise -EINVAL value
 */
static int set_buf_size(struct test_param *crypto_info)
{
	struct protocol_info *proto = crypto_info->proto;
	struct wimax_params *wimax_params = proto->proto_params;
	struct runtime_param *p_rt = &(crypto_info->rt);

	p_rt->input_buf_capacity = crypto_info->buf_size;
	p_rt->input_buf_length = crypto_info->buf_size;
	p_rt->output_buf_size = crypto_info->buf_size +
							WIMAX_PN_SIZE +
							WIMAX_ICV_SIZE;
	if ((CIPHER == crypto_info->mode) || wimax_params->fcs)
		crypto_info->rt.output_buf_size += WIMAX_FCS_SIZE;

	return 0;
}

/**
 * @brief	Verifies if user gave a correct test set
 * @param[in]	crypto_info - test parameters
 * @return	0 on success, otherwise -EINVAL value
 */
static int validate_test_set(struct test_param *crypto_info)
{
	if ((crypto_info->test_set > 0) && (crypto_info->test_set < 5))
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
struct protocol_info *register_wimax(void)
{
	unsigned num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	struct protocol_info *proto_info = calloc(1, sizeof(*proto_info));

	if (unlikely(!proto_info)) {
		pr_err("failed to allocate protocol structure in %s",
		       __FILE__);
		return NULL;
	}

	SAFE_STRNCPY(proto_info->name, "WIMAX", sizeof(proto_info->name));
	proto_info->unregister = unregister_wimax;
	proto_info->argp_children = &argp_children;
	proto_info->init_ref_test_vector = init_rtv_wimax_aes_ccm_128;
	proto_info->setup_sec_descriptor = create_descriptor;
	proto_info->validate_opts = validate_opts;
	proto_info->get_buf_size = get_buf_size;
	proto_info->set_buf_size = set_buf_size;
	proto_info->validate_test_set = validate_test_set;
	proto_info->proto_params = calloc(1, sizeof(struct wimax_params));
	if (unlikely(!proto_info->proto_params)) {
		pr_err("failed to allocate protocol parameters in %s",
		       __FILE__);
		goto err;
	}

	proto_info->proto_vector =
		calloc(1, sizeof(struct wimax_ref_vector_s));
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
void unregister_wimax(struct protocol_info *proto_info)
{
	int i;
	struct wimax_ref_vector_s *wimax_ref_vector;

	if (unlikely(proto_info))
		return;

	for (i = 0; i < proto_info->num_cpus * FQ_PER_CORE * 2; i++)
		if (proto_info->descr[i].descr)
			__dma_mem_free(proto_info->descr[i].descr);

	wimax_ref_vector = proto_info->proto_vector;
	if (wimax_ref_vector->ciphertext)
		free(wimax_ref_vector->ciphertext);

	if (wimax_ref_vector->plaintext)
		free(wimax_ref_vector->plaintext);

	if ((wimax_ref_vector->key))
		free((void *)wimax_ref_vector->key);

	free(proto_info->proto_vector);
	free(proto_info->proto_params);
	free(proto_info);
}
