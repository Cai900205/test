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

#include "ipsec.h"
#include "ipsec_test_vector.h"

/* Forward declarations */
static error_t parse_opts(int, char *, struct argp_state *);
static void unregister_ipsec(struct protocol_info *);

struct argp_option ipsec_options[] = {
	{"cipher", 'h', "CIPHER", 0,
	 "Ciphering algorithm:\n"
	 "0 = 3DES\n"},
	{"integrity", 'q', "INTEGRITY", 0,
	 "Integrity algorithm:\n"
	 "0 = HMAC_MD5_96\n"},
	{0}
};

/* Parser for IPSEC command line options */
static struct argp ipsec_argp = {
	ipsec_options, parse_opts
};

static struct argp_child argp_children = {
	&ipsec_argp , 0, "IPSEC protocol options", 0};

static void set_enc_buf_cb(struct qm_fd *fd, uint8_t *buf,
			   struct test_param *crypto_info)
{
	struct protocol_info *proto = crypto_info->proto;
	struct ipsec_ref_vector_s *rtv = proto->proto_vector;
	struct qm_sg_entry *sgentry;
	dma_addr_t addr;
	unsigned i, iphdr_size, esp_hdr_size, esp_trailer_size;
	static uint8_t plain_data;

	addr = qm_fd_addr(fd);
	sgentry = __dma_mem_ptov(addr);

	/* set input buffer length */
	sgentry++;
	esp_hdr_size = IPSEC_SPI_SIZE + IPSEC_SEQNUM_SIZE + rtv->iv_size;
	esp_trailer_size = rtv->pad_size + IPSEC_PAD_LEN_SIZE + IPSEC_N_SIZE +
			   rtv->icv_size;

	sgentry->length = crypto_info->rt.input_buf_capacity -
			  (rtv->e_pdb->ip_hdr_len + esp_hdr_size +
			   esp_trailer_size);

	/*
	 * For IPsec in PERF mode set the input plain-text data
	 * as IP aware packets.
	 */
	if (CIPHER == crypto_info->mode) {
		memcpy(buf, rtv->plaintext, crypto_info->buf_size);
	} else if (!(rtv->e_pdb->options & PDBOPTS_ESP_IPVSN)) {
		iphdr_size = sizeof(struct iphdr);
		memcpy(buf, rtv->iphdr, iphdr_size);
		for (i = iphdr_size; i < crypto_info->buf_size; i++)
			buf[i] = plain_data++;
	}
}

static void set_dec_buf_cb(struct qm_fd *fd, uint8_t *buf,
			   struct test_param *crypto_info)
{
	struct protocol_info *proto = crypto_info->proto;
	struct ipsec_ref_vector_s *rtv = proto->proto_vector;
	struct qm_sg_entry *sgentry;
	dma_addr_t addr;
	unsigned esp_hdr_size, esp_trailer_size;

	addr = qm_fd_addr(fd);
	sgentry = __dma_mem_ptov(addr);

	/* set output buffer length */
	esp_hdr_size = IPSEC_SPI_SIZE + IPSEC_SEQNUM_SIZE + rtv->iv_size;
	esp_trailer_size = rtv->icv_size;

	sgentry->length = crypto_info->rt.input_buf_capacity -
			  (rtv->e_pdb->ip_hdr_len + esp_hdr_size +
			   esp_trailer_size);
}

static int test_enc_match_cb(int fd_ind, uint8_t *enc_buf,
			     struct test_param *crypto_info)
{
	struct ipsec_ref_vector_s *rtv = crypto_info->proto->proto_vector;

	if (!fd_ind &&
	    test_vector_match((uint32_t *)enc_buf, (uint32_t *)rtv->ciphertext,
			crypto_info->rt.output_buf_size * BITS_PER_BYTE) != 0)
		return -1;

	return 0;
}

static int test_dec_match_cb(int fd_ind, uint8_t *dec_buf,
			     struct test_param *crypto_info)
{
	struct ipsec_ref_vector_s *rtv = crypto_info->proto->proto_vector;
	unsigned i, iphdr_size;
	static uint8_t plain_data;

	if (CIPHER == crypto_info->mode) {
		if (!fd_ind &&
		    (test_vector_match((uint32_t *)dec_buf,
				       (uint32_t *)rtv->plaintext,
				       rtv->length)))
			return -1;
	} else if (!(rtv->e_pdb->options & PDBOPTS_ESP_IPVSN)) {
		iphdr_size = sizeof(struct iphdr);
		if (memcmp(dec_buf, rtv->iphdr, iphdr_size))
			return -1;
		for (i = iphdr_size; i < crypto_info->buf_size; i++)
			if (dec_buf[i] != plain_data++)
				return -1;
	}
	return 0;
}

static int init_ref_test_vector_ipsec(struct test_param *crypto_info)
{
	struct protocol_info *proto = crypto_info->proto;
	struct ipsec_params *ipsec_params = proto->proto_params;
	struct ipsec_ref_vector_s *rtv = proto->proto_vector;
	struct ipsec_encap_pdb *e_pdb;
	struct ipsec_decap_pdb *d_pdb;

	const int proto_offset = IPSEC_TEST_ARRAY_OFFSET(ipsec_params);

	rtv->auth_alginfo = OP_PCL_IPSEC_HMAC_MD5_96;
	rtv->auth_key = (uintptr_t)ipsec_test_auth_key[proto_offset];
	rtv->auth_keylen = ipsec_test_auth_keylen[proto_offset];

	rtv->cipher_alginfo = OP_PCL_IPSEC_3DES;
	rtv->key = (uintptr_t)ipsec_test_cipher_key[proto_offset];
	rtv->cipher_keylen = ipsec_test_cipher_keylen[proto_offset];

	rtv->block_size = DES_BLOCK_SIZE;
	rtv->iv_size = rtv->block_size;
	rtv->icv_size = IPSEC_ICV_MD5_TRUNC_SIZE;

	if (CIPHER == crypto_info->mode) {
		rtv->length = NO_OF_BITS(ipsec_test_data_in_len[proto_offset]);
		rtv->plaintext = ipsec_test_data_in[proto_offset];
		rtv->ciphertext = ipsec_test_data_out[proto_offset];
	} else {
		rtv->iphdr = calloc(1, sizeof(struct iphdr));
		if (!rtv->iphdr) {
			fprintf(stderr, "error: %s: calloc for IP header failed\n",
				__func__);
			goto err;
		}
		/* Version and Header Length */
		rtv->iphdr[0] = 0x45;
		/* Total Length */
		rtv->iphdr[2] = crypto_info->buf_size >> 8;
		rtv->iphdr[3] = crypto_info->buf_size & 0xFF;
	}

	/* set IPSEC encapsulation PDB */
	e_pdb = calloc(1, sizeof(struct ipsec_encap_pdb) +
		       ipsec_opt_ip_hdr_len[proto_offset]);
	if (!e_pdb) {
		fprintf(stderr, "error: %s: calloc for IPSEC encapsulation PDB failed\n",
			__func__);
		goto err;
	}
	e_pdb->options = PDBOPTS_ESP_IPHDRSRC | PDBOPTS_ESP_INCIPHDR |
			 PDBOPTS_ESP_TUNNEL;
	e_pdb->seq_num = ipsec_test_seq_num[proto_offset];
	memcpy(&e_pdb->cbc.iv[2], ipsec_test_iv[proto_offset], rtv->iv_size);
	e_pdb->spi = ipsec_test_spi[proto_offset];
	e_pdb->ip_hdr_len = ipsec_opt_ip_hdr_len[proto_offset];
	memcpy(e_pdb->ip_hdr, ipsec_opt_ip_hdr[proto_offset],
	       e_pdb->ip_hdr_len);
	rtv->e_pdb = e_pdb;

	/* set IPSEC decapsulation PDB */
	d_pdb = calloc(1, sizeof(struct ipsec_decap_pdb));
	if (!d_pdb) {
		fprintf(stderr, "error: %s: calloc for IPSEC decapsulation PDB failed\n",
			__func__);
		goto err;
	}
	d_pdb->ip_hdr_len = ipsec_opt_ip_hdr_len[proto_offset];
	d_pdb->options = PDBOPTS_ESP_OUTFMT | PDBOPTS_ESP_TUNNEL;
	d_pdb->seq_num = ipsec_test_seq_num[proto_offset];
	rtv->d_pdb = d_pdb;

	return 0;
err:
	free(rtv->iphdr);
	free(rtv->e_pdb);
	return -ENOMEM;
}


static void *create_descriptor(bool mode, void *params)
{
	struct test_param *crypto_info = (struct test_param *)params;
	struct protocol_info *proto = crypto_info->proto;
	struct ipsec_ref_vector_s *rtv = proto->proto_vector;
	struct sec_descriptor_t *prehdr_desc;
	struct alginfo cipher_info, auth_info;
	uint32_t *shared_desc = NULL;
	unsigned i, shared_desc_len = 0;
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

	cipher_info.algtype = rtv->cipher_alginfo;
	cipher_info.key = rtv->key;
	cipher_info.keylen = rtv->cipher_keylen;
	cipher_info.key_enc_flags = 0;
	auth_info.algtype = rtv->auth_alginfo;
	auth_info.key = rtv->auth_key;
	auth_info.keylen = rtv->auth_keylen;
	auth_info.key_enc_flags = 0;

	if (ENCRYPT == mode)
		cnstr_shdsc_ipsec_encap(shared_desc, &shared_desc_len, 1,
					rtv->e_pdb, &cipher_info, &auth_info);
	else
		cnstr_shdsc_ipsec_decap(shared_desc, &shared_desc_len, 1,
					rtv->d_pdb, &cipher_info, &auth_info);

	prehdr_desc->prehdr.hi.word = shared_desc_len & SEC_PREHDR_SDLEN_MASK;

	pr_debug("SEC %s shared descriptor:\n", proto->name);

	for (i = 0; i < shared_desc_len; i++)
		pr_debug("0x%x\n", *shared_desc++);

	return prehdr_desc;
}

/**
 * @brief      Parse IPSEC related command line options
 *
 */
static error_t parse_opts(int key, char *arg, struct argp_state *state)
{
	struct parse_input_t *input = state->input;
	uint32_t *p_proto_params = input->proto_params;
	struct test_param *crypto_info = input->crypto_info;
	struct ipsec_params *ipsec_params;

	/*
	 * If the protocol was not selected, then it makes no sense to go
	 * further.
	 */
	if (!crypto_info->proto)
		return 0;

	ipsec_params = crypto_info->proto->proto_params;
	switch (key) {
	case 'h':
		ipsec_params->c_alg = atoi(arg);
		*p_proto_params |= BMASK_IPSEC_CIPHER;
		fprintf(stdout, "IPSEC cipher algorithm = %d\n",
			ipsec_params->c_alg);
		break;
	case 'q':
		ipsec_params->i_alg = atoi(arg);
		*p_proto_params |= BMASK_IPSEC_INTEGRITY;
		fprintf(stdout, "IPSEC integrity algorithm = %d\n",
			ipsec_params->i_alg);
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

/**
 * @brief       Check SEC parameters provided by user for IPSEC are valid
 *              or not.
 * @param[in]   g_proto_params - Bit mask of the optional parameters provided
 *              by user
 * @param[in]   crypto_info - test parameters
 * @return      0 on success, otherwise -EINVAL value
 */
static int validate_opts(uint32_t g_proto_params,
			 struct test_param *crypto_info)
{
	struct ipsec_params *ipsec_params = crypto_info->proto->proto_params;


	if (BMASK_IPSEC_VALID ^ g_proto_params) {
		fprintf(stderr,
			"error: IPSEC Invalid Parameters\n"
			"see --help option\n");
		return -EINVAL;
	}

	switch (ipsec_params->c_alg) {
	case IPSEC_CIPHER_TYPE_TDES:
		break;
	default:
		fprintf(stderr,
			"error: IPSEC Invalid Parameters: Invalid cipher algorithm\n"
			"see --help option\n");
		return -EINVAL;
	}

	switch (ipsec_params->i_alg) {
	case IPSEC_AUTH_TYPE_HMAC_MD5_96:
		break;
	default:
		fprintf(stderr,
			"error: IPSEC Invalid Parameters: Invalid integrity algorithm\n"
			"see --help option\n");
		return -EINVAL;
	}

	/*
	 * For IPSEC in CIPHER mode only the first packet from the first
	 * iteration can be verified if it is matching with the corresponding
	 * test vector, due to the fact that SEC updates
	 * the Initialization Vector after each packet processed
	 */
	if (CIPHER == crypto_info->mode && crypto_info->itr_num != 1) {
		crypto_info->itr_num = 1;
		printf("WARNING: Running IPSEC in CIPHER mode with only one iteration\n");
	}

	return 0;
}

static int get_buf_size(struct test_param *crypto_info)
{
	unsigned esp_hdr_size, esp_trailer_size;

	/* assume maximum IV size (IV size is the cipher block size) */
	esp_hdr_size = IPSEC_SPI_SIZE + IPSEC_SEQNUM_SIZE + AES_BLOCK_SIZE;
	/* assume maximum icv and padding size */
	esp_trailer_size = SHA1_DIGEST_SIZE + AES_BLOCK_SIZE;

	/* assume optional header */
	return 2 * (sizeof(struct iphdr) + esp_hdr_size +
		    crypto_info->buf_size + esp_trailer_size);
}

/**
 * @brief       Set buffer sizes for input/output frames
 * @param[in]   crypto_info - test parameters
 * @return      0 on success
 */
static int set_buf_size(struct test_param *crypto_info)
{
	struct ipsec_ref_vector_s *rtv = crypto_info->proto->proto_vector;
	struct runtime_param *p_rt = &(crypto_info->rt);
	unsigned esp_hdr_size, esp_trailer_size;

	p_rt->input_buf_length = crypto_info->buf_size;

	esp_hdr_size = IPSEC_SPI_SIZE + IPSEC_SEQNUM_SIZE + rtv->iv_size;

	rtv->pad_size = IPSEC_PAD_SIZE(crypto_info->buf_size, rtv->block_size);
	esp_trailer_size = rtv->pad_size + IPSEC_PAD_LEN_SIZE + IPSEC_N_SIZE +
			   rtv->icv_size;

	p_rt->output_buf_size = crypto_info->buf_size + rtv->e_pdb->ip_hdr_len +
				esp_hdr_size + esp_trailer_size;
	p_rt->input_buf_capacity = p_rt->output_buf_size;

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
struct protocol_info *register_ipsec(void)
{
	unsigned num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	struct protocol_info *proto_info = calloc(1, sizeof(*proto_info));

	if (unlikely(!proto_info)) {
		pr_err("failed to allocate protocol structure in %s",
		       __FILE__);
		return NULL;
	}

	SAFE_STRNCPY(proto_info->name, "IPSEC", sizeof(proto_info->name));
	proto_info->unregister = unregister_ipsec;
	proto_info->argp_children = &argp_children;
	proto_info->init_ref_test_vector = init_ref_test_vector_ipsec;
	proto_info->set_enc_buf_cb = set_enc_buf_cb;
	proto_info->set_dec_buf_cb = set_dec_buf_cb;
	proto_info->test_enc_match_cb = test_enc_match_cb;
	proto_info->test_dec_match_cb = test_dec_match_cb;
	proto_info->setup_sec_descriptor = create_descriptor;
	proto_info->validate_opts = validate_opts;
	proto_info->get_buf_size = get_buf_size;
	proto_info->set_buf_size = set_buf_size;
	proto_info->validate_test_set = validate_test_set;

	proto_info->proto_params = calloc(1, sizeof(struct ipsec_params));
	if (unlikely(!proto_info->proto_params)) {
		pr_err("failed to allocate protocol parameters in %s",
		       __FILE__);
		goto err;
	}

	proto_info->proto_vector = calloc(1, sizeof(struct ipsec_ref_vector_s));
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
void unregister_ipsec(struct protocol_info *proto_info)
{
	int i;
	struct ipsec_ref_vector_s *rtv = proto_info->proto_vector;

	if (!proto_info)
		return;

	for (i = 0; i < proto_info->num_cpus * FQ_PER_CORE * 2; i++)
		if (proto_info->descr[i].descr)
			__dma_mem_free(proto_info->descr[i].descr);

	free(rtv->iphdr);
	free(rtv->e_pdb);
	free(rtv->d_pdb);

	free(proto_info->proto_vector);
	free(proto_info->proto_params);
	free(proto_info);
}
