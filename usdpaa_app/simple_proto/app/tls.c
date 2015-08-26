/* Copyright 2013 Freescale Semiconductor, Inc.
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "tls.h"
#include "tls_test_vector.h"

/* Forward declarations */
static error_t parse_opts(int, char *, struct argp_state *);
static void unregister_tls(struct protocol_info *);

struct argp_option tls_options[] = {
	{"version", 'g', "VERSION", 0,
	 "Select TLS version:\n"
	 "0 = SSL30  (not supported)\n"
	 "1 = TLS10\n"
	 "2 = TLS11  (not supported)\n"
	 "3 = TLS12  (not supported)\n"
	 "4 = DTLS10 (not supported)\n"},
	{"cipher", 'j', "CIPHER", 0,
	 "Ciphering algorithm:\n"
	 "0 = AES-CBC\n"},
	{"integrity", 'k', "INTEGRITY", 0,
	 "Integrity algorithm:\n"
	 "0 = HMAC-SHA1\n"},
	{0}
};

/* Parser for TLS command line options */
static struct argp tls_argp = {
	tls_options, parse_opts
};

static struct argp_child argp_children = {
	&tls_argp , 0, "TLS protocol options", 0};


static void set_enc_buf_cb(struct qm_fd *fd, uint8_t *buf,
			   struct test_param *crypto_info)
{
	struct protocol_info *proto = crypto_info->proto;
	struct tls_ref_vector_s *rtv = proto->proto_vector;
	struct qm_sg_entry *sgentry;
	dma_addr_t addr;
	unsigned hdr_size, trailer_size, i;
	static uint8_t plain_data;

	addr = qm_fd_addr(fd);
	sgentry = __dma_mem_ptov(addr);

	/* set input buffer length */
	sgentry++;
	hdr_size = TLS_TYPE_SIZE + TLS_VERSION_SIZE + TLS_LEN_SIZE;
	trailer_size = rtv->icv_size;
	if (rtv->block_cipher)
		trailer_size += rtv->pad_size;

	sgentry->length = crypto_info->rt.input_buf_capacity -
			  (hdr_size + trailer_size);

	if (CIPHER == crypto_info->mode)
		memcpy(buf, rtv->plaintext, crypto_info->buf_size);
	else
		for (i = 0; i < crypto_info->buf_size; i++)
			buf[i] = plain_data++;
}

static void set_dec_buf_cb(struct qm_fd *fd, uint8_t *buf,
			   struct test_param *crypto_info)
{
	struct protocol_info *proto = crypto_info->proto;
	struct tls_ref_vector_s *rtv = proto->proto_vector;
	struct qm_sg_entry *sgentry;
	dma_addr_t addr;
	unsigned hdr_size, trailer_size;

	addr = qm_fd_addr(fd);
	sgentry = __dma_mem_ptov(addr);

	/* set output buffer length */
	hdr_size = TLS_TYPE_SIZE + TLS_VERSION_SIZE + TLS_LEN_SIZE;
	trailer_size = rtv->icv_size;
	if (rtv->block_cipher)
		trailer_size += rtv->pad_size;

	sgentry->length = crypto_info->rt.input_buf_capacity -
			  (hdr_size + trailer_size);
}

static int test_enc_match_cb(int fd_ind, uint8_t *enc_buf,
			     struct test_param *crypto_info)
{
	struct tls_ref_vector_s *rtv = crypto_info->proto->proto_vector;

	if (!fd_ind &&
	    test_vector_match((uint32_t *)enc_buf,
			(uint32_t *)rtv->ciphertext,
			crypto_info->rt.output_buf_size * BITS_PER_BYTE) != 0)
		return -1;

	return 0;
}

static int test_dec_match_cb(int fd_ind, uint8_t *dec_buf,
			     struct test_param *crypto_info)
{
	struct tls_ref_vector_s *rtv = crypto_info->proto->proto_vector;
	unsigned i;
	static uint8_t plain_data;


	if (CIPHER == crypto_info->mode)
		return test_vector_match((uint32_t *)dec_buf,
					 (uint32_t *)rtv->plaintext,
					 rtv->length);
	else
		for (i = 0; i < crypto_info->buf_size; i++)
			if (dec_buf[i] != plain_data++)
				return -1;
	return 0;
}

static int init_ref_test_vector_tls10(struct test_param *crypto_info)
{
	struct protocol_info *proto = crypto_info->proto;
	struct tls_params *tls_params = proto->proto_params;
	struct tls_ref_vector_s *rtv = proto->proto_vector;
	struct tls_block_pdb *e_pdb, *d_pdb;

	const int proto_offset = TLS10_TEST_ARRAY_OFFSET(tls_params);

	rtv->auth_key = (uintptr_t)tls_test_auth_key[proto_offset];
	rtv->auth_keylen = tls_test_auth_keylen[proto_offset];

	rtv->key = (uintptr_t)tls_test_crypto_key[proto_offset];
	rtv->cipher_keylen = tls_test_cipher_keylen[proto_offset];

	rtv->protcmd.protid = OP_PCLID_TLS10;
	rtv->protcmd.protinfo = OP_PCL_TLS10_AES_128_CBC_SHA;
	rtv->block_cipher = 1;
	rtv->icv_size = SHA1_DIGEST_SIZE;
	rtv->block_size = AES_BLOCK_SIZE;
	rtv->pad_size = TLS_PAD_SIZE(crypto_info->buf_size, rtv->icv_size,
				     rtv->block_size);

	if (CIPHER == crypto_info->mode) {
		rtv->length = NO_OF_BITS(tls_test_data_in_len[proto_offset]);
		rtv->plaintext = tls_test_data_in[proto_offset];
		rtv->ciphertext = tls_test_data_out[proto_offset];
	}

	/* set TLS10 encapsulation PDB */
	e_pdb = calloc(1, sizeof(struct tls_block_pdb));
	if (!e_pdb) {
		fprintf(stderr, "error: %s: calloc for TLS10 encapsulation PDB failed\n",
			__func__);
		return -ENOMEM;
	}
	e_pdb->tls_enc.type = tls_test_type[proto_offset];
	memcpy(e_pdb->tls_enc.version, tls_test_version[proto_offset],
	       TLS_VERSION_SIZE);
	memcpy(e_pdb->tls_enc.seq_num, tls_test_seq_num[proto_offset],
	       TLS_SEQNUM_SIZE);
	memcpy(e_pdb->iv, tls_test_iv[proto_offset], AES_BLOCK_SIZE);
	rtv->e_pdb = (uint8_t *)e_pdb;

	/* set TLS10 decapsulation PDB */
	d_pdb = calloc(1, sizeof(struct tls_block_pdb));
	if (!d_pdb) {
		free(e_pdb);
		fprintf(stderr, "error: %s: calloc for TLS10 decapsulation PDB failed\n",
			__func__);
		return -ENOMEM;
	}
	memcpy(d_pdb->tls_dec.seq_num, tls_test_seq_num[proto_offset],
	       TLS_SEQNUM_SIZE);
	memcpy(d_pdb->iv, tls_test_iv[proto_offset], AES_BLOCK_SIZE);
	rtv->d_pdb = (uint8_t *)d_pdb;

	return 0;
}

static int init_ref_test_vector_tls(struct test_param *crypto_info)
{
	struct tls_params *tls_params = crypto_info->proto->proto_params;

	switch (tls_params->version) {
	case TLS10:
		if (init_ref_test_vector_tls10(crypto_info))
			goto err;
		break;
	default:
		fprintf(stderr,
			"Unknown TLS version %d (should never reach here)\n",
			tls_params->version);
		return -EINVAL;
	}

	return 0;
err:
	return -ENOMEM;
}

static void *create_descriptor(bool mode, void *params)
{
	struct test_param *crypto_info = (struct test_param *)params;
	struct protocol_info *proto = crypto_info->proto;
	struct tls_params *tls_params = proto->proto_params;
	struct tls_ref_vector_s *rtv = proto->proto_vector;
	struct sec_descriptor_t *prehdr_desc;
	struct alginfo cipher_info, auth_info;
	uint32_t *shared_desc = NULL;
	unsigned shared_desc_len = 0;
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

	cipher_info.key = rtv->key;
	cipher_info.keylen = rtv->cipher_keylen;
	cipher_info.key_enc_flags = 0;
	auth_info.key = rtv->auth_key;
	auth_info.keylen = rtv->auth_keylen;
	auth_info.key_enc_flags = 0;

	switch (tls_params->version) {
	case TLS10:
		if (ENCRYPT == mode) {
			rtv->protcmd.optype = OP_TYPE_ENCAP_PROTOCOL;
			cnstr_shdsc_tls(shared_desc,
					&shared_desc_len,
					1,
					rtv->e_pdb,
					sizeof(struct tls_block_pdb),
					&rtv->protcmd,
					&cipher_info,
					&auth_info);
		} else {
			rtv->protcmd.optype = OP_TYPE_DECAP_PROTOCOL;
			cnstr_shdsc_tls(shared_desc,
					&shared_desc_len,
					1,
					rtv->d_pdb,
					sizeof(struct tls_block_pdb),
					&rtv->protcmd,
					&cipher_info,
					&auth_info);
		}
		break;
	default:
		fprintf(stderr, "error: %s: Invalid TLS version\n", __func__);
		return NULL;
	}

	prehdr_desc->prehdr.hi.word = shared_desc_len & SEC_PREHDR_SDLEN_MASK;

	pr_debug("SEC %s shared descriptor:\n", proto->name);

	for (i = 0; i < shared_desc_len; i++)
		pr_debug("0x%x\n", *shared_desc++);

	return prehdr_desc;
}

/**
 * @brief       Parse TLS related command line options
 *
 */
static error_t parse_opts(int key, char *arg, struct argp_state *state)
{
	struct parse_input_t *input = state->input;
	uint32_t *p_proto_params = input->proto_params;
	struct test_param *crypto_info = input->crypto_info;
	struct tls_params *tls_params;

	/*
	 * If the protocol was not selected, then it makes no sense to go
	 * further.
	 */
	if (!crypto_info->proto)
		return 0;

	tls_params = crypto_info->proto->proto_params;
	switch (key) {
	case 'g':
		tls_params->version = atoi(arg);
		*p_proto_params |= BMASK_TLS_VERSION;
		fprintf(stdout, "TLS type = %d\n", tls_params->version);
		break;
	case 'j':
		tls_params->cipher_alg = atoi(arg);
		*p_proto_params |= BMASK_TLS_CIPHER;
		fprintf(stdout, "TLS cipher algorithm = %d\n",
			tls_params->cipher_alg);
		break;
	case 'k':
		tls_params->integrity_alg = atoi(arg);
		*p_proto_params |= BMASK_TLS_INTEGRITY;
		fprintf(stdout, "TLS integrity algorithm = %d\n",
			tls_params->integrity_alg);
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

/**
 * @brief       Check SEC parameters provided by user for TLS are valid
 *              or not.
 * @param[in]   g_proto_params - Bit mask of the optional parameters provided
 *              by user
 * @param[in]   crypto_info - test parameters
 * @return      0 on success, otherwise -EINVAL value
 */
static int validate_opts(uint32_t g_proto_params,
			 struct test_param *crypto_info)
{
	struct tls_params *tls_params = crypto_info->proto->proto_params;

	if (BMASK_TLS_VALID ^ g_proto_params) {
		fprintf(stderr,
			"error: TLS Invalid Parameters\n"
			"see --help option\n");
		return -EINVAL;
	}

	switch (tls_params->version) {
	case TLS10:
		break;
	case SSL30:
	case TLS11:
	case TLS12:
	case DTLS10:
	case DTLS12:
		fprintf(stderr, "error: %s: TLS version %d not supported\n",
			__func__, tls_params->version);
		return -EINVAL;
	default:
		fprintf(stderr, "error: %s: Invalid TLS version\n", __func__);
		return -EINVAL;
	}

	switch (tls_params->cipher_alg) {
	case TLS_CIPHER_TYPE_AES_128_CBC:
		break;
	default:
		fprintf(stderr,
			"error: TLS Invalid Parameters: Invalid cipher algorithm\n"
			"see --help option\n");
		return -EINVAL;
	}

	switch (tls_params->integrity_alg) {
	case TLS_AUTH_TYPE_SHA:
		break;
	default:
		fprintf(stderr,
			"error: TLS Invalid Parameters: Invalid integrity algorithm\n"
			"see --help option\n");
		return -EINVAL;
	}

	/*
	 * For TLS in CIPHER mode only the first packet from the first iteration
	 * can be verified if it is matching with the corresponding test vector,
	 * due to the Sequence Number increment by SEC for each packet
	 * processed.
	 */
	if (CIPHER == crypto_info->mode && crypto_info->itr_num != 1) {
		crypto_info->itr_num = 1;
		printf("WARNING: Running TLS in CIPHER mode with only one iteration\n");
	}

	return 0;
}

static int get_buf_size(struct test_param *crypto_info)
{
	unsigned hdr_size, trailer_size;

	hdr_size = TLS_TYPE_SIZE + TLS_VERSION_SIZE + TLS_LEN_SIZE;
	/* assume maximum icv and padding size */
	trailer_size = SHA1_DIGEST_SIZE + AES_BLOCK_SIZE;

	return 2 * (hdr_size + crypto_info->buf_size + trailer_size);
}

/**
 * @brief       Set buffer sizes for input/output frames
 * @param[in]   crypto_info - test parameters
 * @return      0 on success, otherwise -EINVAL value
 */
static int set_buf_size(struct test_param *crypto_info)
{
	struct protocol_info *proto = crypto_info->proto;
	struct tls_params *tls_params = proto->proto_params;
	struct tls_ref_vector_s *rtv = proto->proto_vector;
	struct runtime_param *p_rt = &(crypto_info->rt);
	unsigned hdr_size, trailer_size;

	p_rt->input_buf_length = crypto_info->buf_size;

	switch (tls_params->version) {
	case TLS10:
		hdr_size = TLS_TYPE_SIZE + TLS_VERSION_SIZE + TLS_LEN_SIZE;
		trailer_size = rtv->icv_size + rtv->pad_size;
		p_rt->output_buf_size = crypto_info->buf_size + hdr_size +
					trailer_size;
		p_rt->input_buf_capacity = p_rt->output_buf_size;
		break;
	default:
		fprintf(stderr, "error: %s: TLS version %d not supported\n",
			__func__,
			tls_params->version);
		return -EINVAL;
	}

	return 0;
}

/**
 * @brief       Verifies if user gave a correct test set
 * @param[in]   crypto_info - test parameters
 * @return      0 on success, otherwise -EINVAL value
 */
static int validate_test_set(struct test_param *crypto_info)
{
	if (crypto_info->test_set == 1)
		return 0;

	fprintf(stderr,
		"error: Invalid Parameters: Test set number is invalid\n");
	return -EINVAL;
}

/**
 * @brief       Allocates the necessary structures for a protocol, sets the
 *              callbacks for the protocol and returns the allocated chunk.
 * @return      NULL if an error occurred, pointer to the protocol structure
 *              otherwise.
 */
struct protocol_info *register_tls(void)
{
	unsigned num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	struct protocol_info *proto_info = calloc(1, sizeof(*proto_info));

	if (unlikely(!proto_info)) {
		pr_err("failed to allocate protocol structure in %s",
		       __FILE__);
		return NULL;
	}

	SAFE_STRNCPY(proto_info->name, "TLS", sizeof(proto_info->name));
	proto_info->unregister = unregister_tls;
	proto_info->argp_children = &argp_children;
	proto_info->init_ref_test_vector = init_ref_test_vector_tls;
	proto_info->set_enc_buf_cb = set_enc_buf_cb;
	proto_info->set_dec_buf_cb = set_dec_buf_cb;
	proto_info->test_enc_match_cb = test_enc_match_cb;
	proto_info->test_dec_match_cb = test_dec_match_cb;
	proto_info->setup_sec_descriptor = create_descriptor;
	proto_info->validate_opts = validate_opts;
	proto_info->get_buf_size = get_buf_size;
	proto_info->set_buf_size = set_buf_size;
	proto_info->validate_test_set = validate_test_set;

	proto_info->proto_params = calloc(1, sizeof(struct tls_params));
	if (unlikely(!proto_info->proto_params)) {
		pr_err("failed to allocate protocol parameters in %s",
		       __FILE__);
		goto err;
	}

	proto_info->proto_vector =
		calloc(1, sizeof(struct tls_ref_vector_s));
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
 * @brief       Deallocates the structures for a protocol (allocated on
 *              registration) and frees any other memory that was allocated
 *              during the protocol processing.
 * @param[in]   proto_info - protocol parameters
 * @return      None
 *
 */
void unregister_tls(struct protocol_info *proto_info)
{
	int i;

	struct tls_ref_vector_s *rtv = proto_info->proto_vector;
	if (!proto_info)
		return;

	for (i = 0; i < proto_info->num_cpus * FQ_PER_CORE * 2; i++)
		if (proto_info->descr[i].descr)
			__dma_mem_free(proto_info->descr[i].descr);

	free(rtv->e_pdb);
	free(rtv->d_pdb);

	free(proto_info->proto_vector);
	free(proto_info->proto_params);
	free(proto_info);
}
