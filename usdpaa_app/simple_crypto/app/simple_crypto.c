/**
  \file	simple_crypto.c
  \brief	Basic SEC 4.0 test application. It operates on user defined SEC
  parameters through CP application and reports throughput for
  various SEC 4.0 raw algorithm.
 */
/* Copyright (c) 2011 Freescale Semiconductor, Inc.
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

#include "simple_crypto.h"

struct ref_vector_s ref_test_vector;

/* Number of active cpus */
long ncpus;

pthread_barrier_t app_barrier;

enum rta_sec_era rta_sec_era;
int32_t user_sec_era = -1;
int32_t hw_sec_era = -1;

/*
 * brief	Initialises the reference test vector for aes-cbc
 * details	Initializes key, length and other variables for the algorithm
 * param[in]	crypto_info - test parameters
 * return	None
 */
void init_rtv_aes_cbc(struct test_param *crypto_info)
{
	strcpy(algorithm, "AES_CBC");
	ref_test_vector.key =
		(uintptr_t)aes_cbc_reference_key[crypto_info->test_set - 1];
	ref_test_vector.iv.init_vec =
	    aes_cbc_reference_iv[crypto_info->test_set - 1];
	ref_test_vector.length =
	    aes_cbc_reference_length[crypto_info->test_set - 1];
	ref_test_vector.plaintext =
	    aes_cbc_reference_plaintext[crypto_info->test_set - 1];
	ref_test_vector.ciphertext =
	    aes_cbc_reference_ciphertext[crypto_info->test_set - 1];
}

/*
 * brief	Initialises the reference test vector for tdes-cbc
 * details	Initializes key, length and other variables for the algorithm
 * param[in]	crypto_info - test parameters
 * return	None
 */
void init_rtv_tdes_cbc(struct test_param *crypto_info)
{
	strcpy(algorithm, "TDES_CBC");
	ref_test_vector.key =
		(uintptr_t)tdes_cbc_reference_key[crypto_info->test_set - 1];
	ref_test_vector.iv.init_vec =
	    tdes_cbc_reference_iv[crypto_info->test_set - 1];
	ref_test_vector.length =
	    tdes_cbc_reference_length[crypto_info->test_set - 1];
	ref_test_vector.plaintext =
	    tdes_cbc_reference_plaintext[crypto_info->test_set - 1];
	ref_test_vector.ciphertext =
	    tdes_cbc_reference_ciphertext[crypto_info->test_set - 1];
}

/*
 * brief	Initialises the reference test vector for snow-f8
 * details	Initializes key, length and other variables for the algorithm
 * param[in]	crypto_info - test parameters
 * return	None
 */
void init_rtv_snow_f8(struct test_param *crypto_info)
{
	strcpy(algorithm, "SNOW_F8");
	ref_test_vector.key =
		(uintptr_t)snow_f8_reference_key[crypto_info->test_set - 1];
	ref_test_vector.iv.f8.count =
	    snow_f8_reference_count[crypto_info->test_set - 1];
	ref_test_vector.iv.f8.bearer =
	    snow_f8_reference_bearer[crypto_info->test_set - 1];
	ref_test_vector.iv.f8.direction =
	    snow_f8_reference_dir[crypto_info->test_set - 1];
	ref_test_vector.length =
	    snow_f8_reference_length[crypto_info->test_set - 1];
	ref_test_vector.plaintext =
	    snow_f8_reference_plaintext[crypto_info->test_set - 1];
	ref_test_vector.ciphertext =
	    snow_f8_reference_ciphertext[crypto_info->test_set - 1];
}

/*
 * brief	Initialises the reference test vector for snow-f9
 * details	Initializes key, length and other variables for the algorithm
 * param[in]	crypto_info - test parameters
 * return	None
 */
void init_rtv_snow_f9(struct test_param *crypto_info)
{
	strcpy(algorithm, "SNOW_F9");
	ref_test_vector.key =
		(uintptr_t)snow_f9_reference_key[crypto_info->test_set - 1];
	ref_test_vector.iv.f9.count =
	    snow_f9_reference_count[crypto_info->test_set - 1];
	ref_test_vector.iv.f9.fresh =
	    snow_f9_reference_fresh[crypto_info->test_set - 1];
	ref_test_vector.iv.f9.direction =
	    snow_f9_reference_dir[crypto_info->test_set - 1];
	ref_test_vector.length =
	    snow_f9_reference_length[crypto_info->test_set - 1];
	ref_test_vector.plaintext =
	    snow_f9_reference_plaintext[crypto_info->test_set - 1];
	ref_test_vector.ciphertext = NULL;
	ref_test_vector.digest =
	    snow_f9_reference_digest[crypto_info->test_set - 1];
	crypto_info->authnct = 1;
}

/*
 * brief	Initialises the reference test vector for kasumi-f8
 * details	Initializes key, length and other variables for the algorithm
 * param[in]	crypto_info - test parameters
 * return	None
 */
void init_rtv_kasumi_f8(struct test_param *crypto_info)
{
	strcpy(algorithm, "KASUMI_F8");
	ref_test_vector.key =
		(uintptr_t)kasumi_f8_reference_key[crypto_info->test_set - 1];
	ref_test_vector.iv.f8.count =
	    kasumi_f8_reference_count[crypto_info->test_set - 1];
	ref_test_vector.iv.f8.bearer =
	    kasumi_f8_reference_bearer[crypto_info->test_set - 1];
	ref_test_vector.iv.f8.direction =
	    kasumi_f8_reference_dir[crypto_info->test_set - 1];
	ref_test_vector.length =
	    kasumi_f8_reference_length[crypto_info->test_set - 1];
	ref_test_vector.plaintext =
	    kasumi_f8_reference_plaintext[crypto_info->test_set - 1];
	ref_test_vector.ciphertext =
	    kasumi_f8_reference_ciphertext[crypto_info->test_set - 1];
}

/*
 * brief	Initialises the reference test vector for kasumi-f9
 * details	Initializes key, length and other variables for the algorithm
 * param[in]	crypto_info - test parameters
 * return	None
 */
void init_rtv_kasumi_f9(struct test_param *crypto_info)
{
	strcpy(algorithm, "KASUMI_F9");
	ref_test_vector.key =
		(uintptr_t)kasumi_f9_reference_key[crypto_info->test_set - 1];
	ref_test_vector.iv.f9.count =
	    kasumi_f9_reference_count[crypto_info->test_set - 1];
	ref_test_vector.iv.f9.fresh =
	    kasumi_f9_reference_fresh[crypto_info->test_set - 1];
	ref_test_vector.iv.f9.direction =
	    kasumi_f9_reference_dir[crypto_info->test_set - 1];
	ref_test_vector.length =
	    kasumi_f9_reference_length[crypto_info->test_set - 1];
	ref_test_vector.plaintext =
	    kasumi_f9_reference_plaintext[crypto_info->test_set - 1];
	ref_test_vector.ciphertext = NULL;
	ref_test_vector.digest =
	    kasumi_f9_reference_digest[crypto_info->test_set - 1];
	crypto_info->authnct = 1;
}

/*
 * brief	Initialises the reference test vector for crc
 * details	Initializes key, length and other variables for the algorithm
 * param[in]	crypto_info - test parameters
 * return	None
 */
void init_rtv_crc(struct test_param *crypto_info)
{
	strcpy(algorithm, "CRC");
	ref_test_vector.length =
			crc_reference_length[crypto_info->test_set - 1];
	ref_test_vector.plaintext =
	    crc_reference_plaintext[crypto_info->test_set - 1];
	ref_test_vector.ciphertext = NULL;
	ref_test_vector.digest =
			crc_reference_digest[crypto_info->test_set - 1];
	crypto_info->authnct = 1;
}

/*
 * brief	Initialises the reference test vector for hmac-sha1
 * details	Initializes key, length and other variables for the algorithm
 * param[in]	crypto_info - test parameters
 * return	None
 */
void init_rtv_hmac_sha1(struct test_param *crypto_info)
{
	strcpy(algorithm, "HMAC_SHA1");
	ref_test_vector.key =
		(uintptr_t)hmac_sha1_reference_key[crypto_info->test_set - 1];
	ref_test_vector.length =
	    hamc_sha1_reference_length[crypto_info->test_set - 1];
	ref_test_vector.plaintext =
	    hmac_sha1_reference_plaintext[crypto_info->test_set - 1];
	ref_test_vector.ciphertext = NULL;
	ref_test_vector.digest =
	    hamc_sha1_reference_digest[crypto_info->test_set - 1];
	crypto_info->authnct = 1;
}

/*
 * brief	Initialises the reference test vector for snow-f8-f9
 * details	Initializes key, length and other variables for the algorithm
 * param[in]	crypto_info - test parameters
 * return	None
 */
void init_rtv_snow_f8_f9(struct test_param *crypto_info)
{
	strcpy(algorithm, "SNOW_F8_F9");
	ref_test_vector.length =
	    snow_enc_f8_f9_reference_length[crypto_info->test_set - 1];
	ref_test_vector.plaintext =
	    snow_enc_f8_f9_reference_plaintext[crypto_info->test_set - 1];
	ref_test_vector.ciphertext =
	    snow_enc_f8_f9_reference_ciphertext[crypto_info->test_set - 1];
}

/* Function pointer to reference test vector for suported algos */
void (*init_ref_test_vector[]) (struct test_param *crypto_info) = {
	    init_rtv_aes_cbc,
	    init_rtv_tdes_cbc,
	    init_rtv_snow_f8,
	    init_rtv_snow_f9,
	    init_rtv_kasumi_f8,
	    init_rtv_kasumi_f9,
	    init_rtv_crc,
	    init_rtv_hmac_sha1,
	    init_rtv_snow_f8_f9};

/*
 * brief	Init test vector, calculates output buffer size and creates
 *		compound FDs
 * param[in]	crypto_info - test parameters
 * return	0 on success, otherwise -ve value
 */
int prepare_test_frames(struct test_param *crypto_info)
{
	int err = 0;
	extern struct qm_fd *fd;

	/*
	 * Allocate FD array
	 */
	fd = calloc(crypto_info->buf_num, sizeof(struct qm_fd));
	if (!fd) {
		error(err, err, "error: allocating FD array");
		return -ENOMEM;
	}

	if (PERF == crypto_info->mode) {
		strcpy(mode_type, "PERF");
		crypto_info->test_set = 1;
	}

	init_ref_test_vector[crypto_info->algo - 1] (crypto_info);

	if (CIPHER == crypto_info->mode) {
		strcpy(mode_type, "CIPHER");
		crypto_info->buf_size = NO_OF_BYTES(ref_test_vector.length);
	}

	err = set_buf_size(crypto_info);
	if (err)
		error(err, err, "error: set output buffer size");

	err = create_compound_fd(crypto_info->buf_num,
				 &(struct compound_fd_params){
					crypto_info->rt.output_buf_size,
					crypto_info->rt.input_buf_capacity,
					crypto_info->rt.input_buf_length,
					0});
	if (err)
		error(err, err, "error: create_compound_fd() failed");

	fprintf(stdout, "Processing %s for %d Frames\n", algorithm,
		crypto_info->buf_num);
	fprintf(stdout, "%s mode, buffer length = %d\n", mode_type,
		crypto_info->buf_size);
	fprintf(stdout, "Number of iterations = %d\n", crypto_info->itr_num);
	fprintf(stdout, "\nStarting threads for %ld cpus\n", ncpus);

	return err;
}

/**
 * @brief	Get the total buffer size used for input & output frames
 *		for a specific protocol
 * @param[in]	crypto_info - test parameters
 * @return	Size necessary for storing both the input and the output
 *		buffers
 */
static int get_buf_size(struct test_param *crypto_info)
{
	unsigned int total_size = crypto_info->buf_size;

	switch (crypto_info->algo) {
	case AES_CBC:
	case TDES_CBC:
	case SNOW_F8:
	case KASUMI_F8:
		total_size += crypto_info->buf_size;
		break;
	case SNOW_F9:
		total_size += SNOW_F9_DIGEST_SIZE;
		break;
	case KASUMI_F9:
		total_size += KASUMI_F9_DIGEST_SIZE;
		break;
	case CRC:
		total_size += CRC_DIGEST_SIZE;
		break;
	case HMAC_SHA1:
		total_size += HMAC_SHA1_DIGEST_SIZE;
		break;
	case SNOW_F8_F9:
		total_size += crypto_info->buf_size + SNOW_F9_DIGEST_SIZE +
				MAX(SNOW_JDESC_ENC_F8_F9_LEN,
				    SNOW_JDESC_DEC_F8_F9_LEN);
		break;
	}

	return total_size;
}
/*
 * brief	Set buffer sizes for input/output frames
 * param[in]	crypto_info - test parameters
 * return	0 on success, otherwise -ve value
 */
int set_buf_size(struct test_param *crypto_info)
{
	struct runtime_param *p_rt = &(crypto_info->rt);

	p_rt->input_buf_capacity = crypto_info->buf_size;
	p_rt->input_buf_length = crypto_info->buf_size;

	/* Store the number of bytes for the job descriptor.
	 * The SNOW_JDESC_ENC_F8_F9_LEN and SNOW_JDESC_DEC_F8_F9_LEN macros
	 * return the length in 32 bit words.
	 * Calculate the maximum between the encrypt and decrypt descriptors
	 * because the same compound FD will be used for both processes, so
	 * will allocate the memory area that can hold both descriptors.
	 */
	p_rt->job_desc_buf_size = MAX(SNOW_JDESC_ENC_F8_F9_LEN,
				      SNOW_JDESC_DEC_F8_F9_LEN);

	switch (crypto_info->algo) {
	case AES_CBC:
	case TDES_CBC:
	case SNOW_F8:
		p_rt->output_buf_size = crypto_info->buf_size;
		break;
	case SNOW_F9:
		p_rt->output_buf_size = SNOW_F9_DIGEST_SIZE;
		break;
	case KASUMI_F8:
		p_rt->output_buf_size = crypto_info->buf_size;
		break;
	case KASUMI_F9:
		p_rt->output_buf_size = KASUMI_F9_DIGEST_SIZE;
		break;
	case CRC:
		p_rt->output_buf_size = CRC_DIGEST_SIZE;
		break;
	case HMAC_SHA1:
		p_rt->output_buf_size = HMAC_SHA1_DIGEST_SIZE;
		break;
	case SNOW_F8_F9:
		p_rt->output_buf_size =
		    crypto_info->buf_size + SNOW_F9_DIGEST_SIZE;

		/* For this algorithm a  Job Descriptor will be added to the
		 * head of the SEC frame. Increase the buffer capacity and
		 * length. The same buffer will be used for holding the
		 * plain-text data + encrypt job descriptor and later the
		 * encrypted data + SNOW F9 digest + decrypt job descriptor.
		 */
		p_rt->input_buf_capacity +=
		    p_rt->job_desc_buf_size + SNOW_F9_DIGEST_SIZE;
		p_rt->input_buf_length += p_rt->job_desc_buf_size;

		break;
	default:
		fprintf(stderr, "error: %s: algorithm not supported\n",
			__func__);
		return -EINVAL;
	}

	return 0;
}

/*
 * brief	Setup preheader for shared descriptor
 * return	0 on success, otherwise -ve value
 */
void *setup_preheader(uint32_t shared_desc_len, uint32_t pool_id,
		      uint32_t pool_buf_size, uint8_t absolute, uint8_t add_buf)
{
	struct preheader_s *prehdr = NULL;

	prehdr = __dma_mem_memalign(L1_CACHE_BYTES, sizeof(struct preheader_s));
	memset(prehdr, 0, sizeof(struct preheader_s));

	if (unlikely(!prehdr)) {
		fprintf(stderr, "error: %s: dma_mem_memalign failed for"
			" preheader\n", __func__);
		return NULL;
	}

	/* the shared descriptor length is 0, meaning that no shared
	   descriptor follows the preheader in the context A */
	prehdr->hi.field.idlen = shared_desc_len;
	prehdr->lo.field.abs = absolute;
	prehdr->lo.field.add_buf = add_buf;
	prehdr->lo.field.pool_id = pool_id;
	prehdr->lo.field.pool_buffer_size = pool_buf_size;

	return (void *)prehdr;
}

/*
 * brief	Set parameters for descriptor init
 * param[in]	mode - Encrypt/Decrypt
 * param[in]	params - pointer to test parameters
 * return	Shared descriptor pointer on success, otherwise NULL
 */
void *setup_sec_descriptor(bool mode, void *params)
{
	struct test_param crypto_info = *((struct test_param *)params);
	void *descriptor = NULL;

	if (SNOW_F8_F9 == crypto_info.algo) {
		descriptor = setup_preheader(
				 0, /* shared descriptor length is 0, meaning
				       there is no shared desc in context A */
				 0, /* pool buffer id */
				 0, /* pool buffer size */
				 0, /* abs = 0 and add_buf = 0 means that the
				       output buffer is provided inside
				       compound frame */
				 0); /* add_buf = 0 */

	} else {
		descriptor = setup_init_descriptor(mode, crypto_info);
	}
	return descriptor;
}

/*
 * brief	Create SEC 4.0 shared descriptor consists of sequence of
 *		commands to SEC 4.0 with necessary key, iv etc initialisation
 * param[in]	mode -	To check whether descriptor is for encryption or
 *		decryption
 * param[in]	crypto_info - test parameters
 * return	Shared descriptor pointer on success, otherwise NULL
 */
static void *setup_init_descriptor(bool mode, struct test_param crypto_info)
{
	struct sec_descriptor_t *prehdr_desc;
	struct alginfo alginfo;
	uint32_t *shared_desc = NULL;
	unsigned shared_desc_len;
	int i, length;

	prehdr_desc = __dma_mem_memalign(L1_CACHE_BYTES,
					 sizeof(struct sec_descriptor_t));
	if (unlikely(!prehdr_desc)) {
		fprintf(stderr, "error: %s: dma_mem_memalign failed for"
			" preheader\n", __func__);
		return NULL;
	}

	memset(prehdr_desc, 0, sizeof(struct sec_descriptor_t));
	shared_desc = (typeof(shared_desc)) &prehdr_desc->descbuf;

	switch (crypto_info.algo) {
	case AES_CBC:
		alginfo.key = ref_test_vector.key;
		alginfo.keylen = AES_CBC_KEY_LEN;
		alginfo.key_enc_flags = 0;
		cnstr_shdsc_cbc_blkcipher(shared_desc, &shared_desc_len,
					  &alginfo,
					  ref_test_vector.iv.init_vec,
					  AES_CBC_IV_LEN,
					  mode ? DIR_ENC : DIR_DEC,
					  OP_ALG_ALGSEL_AES);
		break;

	case TDES_CBC:
		alginfo.key = ref_test_vector.key;
		alginfo.keylen = TDES_CBC_KEY_LEN;
		alginfo.key_enc_flags = 0;
		cnstr_shdsc_cbc_blkcipher(shared_desc, &shared_desc_len,
					  &alginfo,
					  ref_test_vector.iv.init_vec,
					  TDES_CBC_IV_LEN,
					  mode ? DIR_ENC : DIR_DEC,
					  OP_ALG_ALGSEL_3DES);
		break;

	case SNOW_F8:
		alginfo.key = ref_test_vector.key;
		alginfo.keylen = F8_KEY_LEN;
		alginfo.key_enc_flags = 0;
		cnstr_shdsc_snow_f8(shared_desc, &shared_desc_len,
				    &alginfo,
				    mode ? DIR_ENC : DIR_DEC,
				    ref_test_vector.iv.f8.count,
				    ref_test_vector.iv.f8.bearer,
				    ref_test_vector.iv.f8.direction);
		break;

	case SNOW_F9:
		if (DECRYPT == mode) {
			fprintf(stderr, "error: %s: enc bit not selected as"
				" protect\n", __func__);
			return NULL;
		}

		if (CIPHER == crypto_info.mode)
			length = ref_test_vector.length;
		else
			length = crypto_info.buf_size * BITS_PER_BYTE;

		alginfo.key = ref_test_vector.key;
		alginfo.keylen = F9_KEY_LEN;
		alginfo.key_enc_flags = 0;
		cnstr_shdsc_snow_f9(shared_desc, &shared_desc_len,
				    &alginfo,
				    DIR_ENC, ref_test_vector.iv.f9.count,
				    ref_test_vector.iv.f9.fresh,
				    ref_test_vector.iv.f9.direction, length);
		break;

	case KASUMI_F8:
		alginfo.key = ref_test_vector.key;
		alginfo.keylen = F8_KEY_LEN;
		alginfo.key_enc_flags = 0;
		cnstr_shdsc_kasumi_f8(shared_desc, &shared_desc_len,
				      &alginfo,
				      mode ? DIR_ENC : DIR_DEC,
				      ref_test_vector.iv.f8.count,
				      ref_test_vector.iv.f8.bearer,
				      ref_test_vector.iv.f8.direction);
		break;

	case KASUMI_F9:
		if (DECRYPT == mode) {
			fprintf(stderr, "error: %s: enc bit not selected as"
				" protect\n", __func__);
			return NULL;
		}

		if (CIPHER == crypto_info.mode)
			length = ref_test_vector.length;
		else
			length = crypto_info.buf_size * BITS_PER_BYTE;

		alginfo.key = ref_test_vector.key;
		alginfo.keylen = F9_KEY_LEN;
		alginfo.key_enc_flags = 0;
		cnstr_shdsc_kasumi_f9(shared_desc, &shared_desc_len,
				      &alginfo,
				      DIR_ENC, ref_test_vector.iv.f9.count,
				      ref_test_vector.iv.f9.fresh,
				      ref_test_vector.iv.f9.direction, length);
		break;

	case CRC:
		if (DECRYPT == mode) {
			fprintf(stderr, "error: %s: enc bit not selected as"
				" protect\n", __func__);
			return NULL;
		}

		cnstr_shdsc_crc(shared_desc, &shared_desc_len);
		break;

	case HMAC_SHA1:
		if (DECRYPT == mode) {
			fprintf(stderr, "error: %s: enc bit not selected as"
				" protect\n", __func__);
			return NULL;
		}

		alginfo.algtype = OP_ALG_ALGSEL_SHA1;
		alginfo.key = ref_test_vector.key;
		alginfo.key_enc_flags = 0;
		cnstr_shdsc_hmac(shared_desc, &shared_desc_len, &alginfo, NULL);
		break;

	default:
		fprintf(stderr, "error: %s: algorithm not supported\n",
			__func__);
		return NULL;
	}

	prehdr_desc->prehdr.hi.field.idlen = shared_desc_len;

	pr_debug("SEC4.0 %s shared descriptor:\n", algorithm);

	for (i = 0; i < shared_desc_len; i++)
		pr_debug("0x%x\n", *shared_desc++);

	return prehdr_desc;
}

/*
 * brief	Initialize input buffer plain text data	and set	output buffer
 *		as 0 in compound frame descriptor
 * param[in]	params - test parameters
 * param[in]	struct qm_fd - frame descriptors list
 * return       None
 */
void set_enc_buf(void *params, struct qm_fd fd[])
{
	struct test_param crypto_info = *((struct test_param *)params);
	struct qm_sg_entry *sgentry;
	uint8_t *in_buf;
	uint8_t plain_data = 0;
	dma_addr_t addr;
	uint32_t i, ind;

	for (ind = 0; ind < crypto_info.buf_num; ind++) {
		addr = qm_fd_addr_get64(&fd[ind]);
		sgentry = __dma_mem_ptov(addr);

		sgentry++;
		addr = qm_sg_entry_get64(sgentry);
		in_buf = __dma_mem_ptov(addr);

		/* In case of SNOW_F8_F9 algorithm, a Job Descriptor must be
		 * inlined at the head of the input buffer. Set the encrypt
		 * job descriptor here.
		 */
		if (SNOW_F8_F9 == crypto_info.algo) {
			/* Update the input frame length. If this is not the
			 *  first iteration, the input len will remain set
			 *  from the decryption phase. The input len for
			 *  encrypt is different than for decrypt.
			 */

			sgentry->length = crypto_info.rt.input_buf_capacity -
			    SNOW_F9_DIGEST_SIZE;

			/* Update the out frame length. If this is not the
			 *  first iteration, the output len will remain set
			 *  from the decryption phase. The output len for
			 *  encrypt is different than for decrypt.
			 */
			sgentry--;
			sgentry->length = crypto_info.rt.output_buf_size;

			memcpy(in_buf, snow_jdesc_enc_f8_f9,
			       crypto_info.rt.job_desc_buf_size);
			in_buf += crypto_info.rt.job_desc_buf_size;
		}
		/* Copy the input plain-text data */
		for (i = 0; i < crypto_info.buf_size; i++) {
			if (CIPHER == crypto_info.mode)
				memcpy(in_buf, ref_test_vector.plaintext,
				       crypto_info.buf_size);
			else
				in_buf[i] = plain_data++;
		}
	}
}

/*
 * brief	Initialize input buffer as cipher text data and	set output
 *		buffer as 0 in compound frame descriptor
 * param[in]	params - test parameters
 * param[in]	struct qm_fd - frame descriptors list
 * return       None
 */
void set_dec_buf(void *params, struct qm_fd fd[])
{
	struct test_param crypto_info = *((struct test_param *)params);
	struct qm_sg_entry *sg_out;
	struct qm_sg_entry *sg_in;
	dma_addr_t addr;
	uint32_t length;
	uint16_t offset;
	uint8_t bpid;
	uint32_t ind;

	for (ind = 0; ind < crypto_info.buf_num; ind++) {
		addr = qm_fd_addr_get64(&fd[ind]);
		sg_out = __dma_mem_ptov(addr);
		sg_in = sg_out + 1;

		addr = qm_sg_addr(sg_out);
		length = sg_out->length;
		offset = sg_out->offset;
		bpid = sg_out->bpid;

		qm_sg_entry_set64(sg_out, qm_sg_addr(sg_in));
		sg_out->length = sg_in->length;
		sg_out->offset = sg_in->offset;
		sg_out->bpid = sg_in->bpid;

		qm_sg_entry_set64(sg_in, addr);
		sg_in->length = length;
		sg_in->offset = offset;
		sg_in->bpid = bpid;
	}
}

/*
 * brief	Initialize input buffer as authenticated text data and set
 *		output buffer as 0 in compound frame descriptor
 * param[in]	params - test parameters
 * param[in]	struct qm_fd - frame descriptors list
 * return       None
 */
void set_dec_auth_buf(void *params, struct qm_fd fd[])
{
	struct test_param crypto_info = *((struct test_param *)params);
	struct qm_sg_entry *sg_out;
	struct qm_sg_entry *sg_in;
	uint8_t *in_buf = NULL;
	dma_addr_t addr;
	uint8_t *dec_job_descriptor = NULL;
	uint32_t ind;

	if (SNOW_F8_F9 != crypto_info.algo) {
		fprintf(stderr, "error: %s: algorithm not supported\n",
			__func__);
		return;
	}

	for (ind = 0; ind < crypto_info.buf_num; ind++) {
		addr = qm_fd_addr_get64(&fd[ind]);
		sg_out = __dma_mem_ptov(addr);

		sg_in = sg_out + 1;

		addr = qm_sg_entry_get64(sg_in);
		in_buf = __dma_mem_ptov(addr);
		memset(in_buf, 0, crypto_info.rt.input_buf_capacity);

		/* Convert the descriptor to an array of uint8_t items */
		dec_job_descriptor = (uint8_t *) snow_jdesc_dec_f8_f9;

		/* A Job Descriptor must be inlined at the head of the input
		 * buffer. Set the decrypt job descriptor here. */
		memcpy(in_buf, dec_job_descriptor,
		       crypto_info.rt.job_desc_buf_size);
		in_buf += crypto_info.rt.job_desc_buf_size;

		/* Validate that the output buffer size is equal with the
		 *  size of the reference cyphertext for decryption
		 */
		if (crypto_info.rt.output_buf_size !=
		    (snow_dec_f8_f9_reference_length[crypto_info.test_set - 1] /
		     8)) {
			fprintf(stderr,
				"error: Invalid output buffer" " length\n");
			abort();
		}

		/* Use the reference encrypted data as input for decryption */
		memcpy(in_buf, snow_dec_f8_f9_reference_ciphertext
		       [crypto_info.test_set - 1],
		       crypto_info.rt.output_buf_size);

		sg_in->length =
		    crypto_info.rt.output_buf_size +
		    crypto_info.rt.job_desc_buf_size;

		/* The output buffer will contain only the decoded F8 data */
		sg_out->length = crypto_info.buf_size;
	}
}

/*
 * brief	The OPTIONS field contains a pointer to a vector of struct
 *		argp_option's
 *
 * details	structure has the following fields
 *		name - The name of this option's long option (may be zero)
 *		key - The KEY to pass to the PARSER function when parsing this
 *		option,	and the name of this option's short option, if it is
 *		a printable ascii character
 *
 *		ARG - The name of this option's argument, if any;
 *
 *		FLAGS - Flags describing this option; some of them are:
 *			OPTION_ARG_OPTIONAL - The argument to this option is
 *					      optional
 *			OPTION_ALIAS	- This option is an alias for the
 *					      previous option
 *			OPTION_HIDDEN	    - Don't show this option in
 *						--help output
 *
 *		DOC - A documentation string for this option, shown in
 *			--help output
 *
 * note		An options vector should be terminated by an option with
 *		all fields zero
 */
static struct argp_option options[] = {
	{"mode", 'm', "TEST MODE", 0,
	 "\n\r\ttest mode: provide following number"
		"\n\r\t\t1 for perf"
		"\n\r\t\t2 for cipher"
		"\n\r\tFollowing two combinations are valid only"
		"\n\r\tand all options are mandatory:"
		"\n\r\t\t-m 1 -s <buf_size> -n <buf_num_per_core>"
		"\n\r\t\t-o <algo> -l <itr_num>"
		"\n\r\t\t-m 2 -t <test_set> -n <buf_num_per_core>"
		"\n\r\t\t-o <algo> -l <itr_num>\n"},
	{"algo", 'o', "ALGORITHM", 0,
	 "\n\r\tCryptographic operation to perform by SEC4.0,"
		"\n\r\tprovide following number"
		"\n\r\t\t1 for AES_CBC"
		"\n\r\t\t2 for TDES_CBC"
		"\n\r\t\t3 for SNOW_F8"
		"\n\r\t\t4 for SNOW_F9"
		"\n\r\t\t5 for KASUMI_F8"
		"\n\r\t\t6 for KASUMI_F9"
		"\n\r\t\t7 for CRC"
		"\n\r\t\t8 for HMAC_SHA1"
		"\n\r\t\t9 for SNOW_F8_F9(only with PERF mode)\n"},
	{"itrnum", 'l', "ITERATIONS", 0,
	 "\n\r\tNumber of iteration to repeat\n"},
	{"bufnum", 'n', "TOTAL BUFFERS", 0,
	 "Total number of buffers (depends on protocol & buffer size)."
	 "\n"},
	 {"bufsize", 's', "BUFSIZE", 0,
	 "OPTION IS VALID ONLY IN PERF MODE"
	 "\n\nBuffer size (64, 128 ... up to 9600)."},
	{"ncpus", 'c', "CPUS", 0,
	 "\n\r\tOPTIONAL PARAMETER\n"
		"\n\r\tNumber of cpus to work for the"
		"\n\r\tapplication(1-8)\n", 0},
	{"testset", 't', "TEST SET", 0,
	 "\n\r\tOPTION IS VALID ONLY IN CIPHER MODE"
		"\n\r\t\t provide following test set number:"
		"\n\r\t\t test set for AES_CBC is 1 to 4"
		"\n\r\t\t test set for TDES_CBC is 1 to 2"
		"\n\r\t\t test set for SNOW_F8 is 1 to 5"
		"\n\r\t\t test set for SNOW_F9 is 1 to 5"
		"\n\r\t\t test set for KASUMI_F8 is 1 to 5"
		"\n\r\t\t test set for KASUMI_F9 is 1 to 5"
		"\n\r\t\t test set for CRC is 1 to 5"
		"\n\r\t\t test set for HMAC_SHA1 is 1 to 2"
		"\n\r\t\t test set for SNOW_F8_F9 is 1\n"},
	{"sec_era", 'e', "ERA", 0,
		 "\n\r\tOPTIONAL PARAMETER\n"
		 "\n\r\tSEC Era version on the targeted platform(2-5)\n", 0},
	{}
};

/*
 * brief	Parse a single option
 *
 * param[in]	key - An integer specifying which option this is (taken	from
 *		the KEY field in each struct argp_option), or a special key
 *		specifying something else. We do not use any special key here
 *
 * param[in]	arg - For an option KEY, the string value of its argument, or
 *		NULL if it has none
 *
 * param[in]	state - A pointer to a struct argp_state, containing various
 *		useful information about the parsing state; used here are the
 *		INPUT field, which reflects the INPUT argument to argp_parse
 *
 * return	It should return either 0, meaning success, ARGP_ERR_UNKNOWN,
 *		meaning the given KEY wasn't recognized, or an errno value
 *		indicating some other error
 */
error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct parse_input_t *input = state->input;
	struct test_param *crypto_info = input->crypto_info;
	uint32_t *p_cmd_params = input->cmd_params;

	switch (key) {
	case 'm':
		crypto_info->mode = atoi(arg);
		*p_cmd_params |= BMASK_SEC_TEST_MODE;
		fprintf(stdout, "Test mode = %s\n", arg);
		break;

	case 't':
		crypto_info->test_set = atoi(arg);
		*p_cmd_params |= BMASK_SEC_TEST_SET;
		fprintf(stdout, "Test set = %d\n", crypto_info->test_set);
		break;

	case 's':
		crypto_info->buf_size = atoi(arg);
		*p_cmd_params |= BMASK_SEC_BUFFER_SIZE;
		fprintf(stdout, "Buffer size = %d\n", crypto_info->buf_size);
		break;

	case 'n':
		crypto_info->buf_num = atoi(arg);
		*p_cmd_params |= BMASK_SEC_BUFFER_NUM;
		fprintf(stdout, "Number of Buffers per core = %d\n",
			crypto_info->buf_num);
		break;

	case 'o':
		crypto_info->algo = atoi(arg);
		*p_cmd_params |= BMASK_SEC_ALG;
		fprintf(stdout, "SEC4.0 cryptographic operation = %s\n", arg);
		break;

	case 'l':
		crypto_info->itr_num = atoi(arg);
		*p_cmd_params |= BMASK_SEC_ITR_NUM;
		fprintf(stdout, "Number of iteration = %d\n",
			crypto_info->itr_num);
		break;

	case 'c':
		ncpus = atoi(arg);
		fprintf(stdout, "Number of cpus = %ld\n", ncpus);
		break;

	case 'e':
		user_sec_era = atoi(arg);
		printf("User SEC Era version = %d\n", user_sec_era);
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

/*
 * brief	Verifies if user gave a correct test set
 * param[in]	crypto_info - test parameters
 * return	0 on success, otherwise -ve value
 */
static int validate_test_set(struct test_param crypto_info)
{
	switch (crypto_info.algo) {
	case AES_CBC:
		if (crypto_info.test_set > 0 && crypto_info.test_set < 5)
			return 0;
		else
			goto err;
	case TDES_CBC:
		if (crypto_info.test_set > 0 && crypto_info.test_set < 3)
			return 0;
		else
			goto err;
	case SNOW_F8:
		if (crypto_info.test_set > 0 && crypto_info.test_set < 6)
			return 0;
		else
			goto err;
	case SNOW_F9:
		if (crypto_info.test_set > 0 && crypto_info.test_set < 6)
			return 0;
		else
			goto err;
	case KASUMI_F8:
		if (crypto_info.test_set > 0 && crypto_info.test_set < 6)
			return 0;
		else
			goto err;
	case KASUMI_F9:
		if (crypto_info.test_set > 0 && crypto_info.test_set < 6)
			return 0;
		else
			goto err;
	case CRC:
		if (crypto_info.test_set > 0 && crypto_info.test_set < 6)
			return 0;
		else
			goto err;
	case HMAC_SHA1:
		if (crypto_info.test_set > 0 && crypto_info.test_set < 3)
			return 0;
		else
			goto err;
	case SNOW_F8_F9:
		if (crypto_info.test_set > 0 && crypto_info.test_set < 2)
			return 0;
		else
			goto err;
	default:
		fprintf(stderr, "error: Invalid Parameters: Invalid SEC"
			" algorithm\n");
		return -EINVAL;
	}
err:
	fprintf(stderr, "error: Invalid Parameters: Test set number is"
		" invalid\n");
	return -EINVAL;
}

/*
 * brief	Check SEC 4.0 parameters provided by user whether valid or not
 * param[in]	g_cmd_params - Bit mask of all parameters provided by user
 * param[in]	crypto_info - test parameters
 * return	0 on success, otherwise -ve value
 */
static int validate_params(uint32_t g_cmd_params, struct test_param crypto_info)
{
	unsigned int total_size, max_num_buf;

	if ((PERF == crypto_info.mode)
	    && BMASK_SEC_PERF_MODE == g_cmd_params) {
		/* do nothing */
	} else if ((CIPHER == crypto_info.mode)
		   && g_cmd_params == BMASK_SEC_CIPHER_MODE) {
		if (validate_test_set(crypto_info) != 0) {
			fprintf(stderr, "error: Invalid Parameters: Invalid"
				" test set\nsee --help option\n");
			return -EINVAL;
		}
	} else {
		fprintf(stderr, "error: Invalid Parameters: provide a valid"
			" combination of mandatory arguments\n"
			"see --help option\n");
		return -EINVAL;
	}

	if (PERF == crypto_info.mode && (crypto_info.buf_size == 0 ||
					 crypto_info.buf_size %
					 L1_CACHE_BYTES != 0
					 || crypto_info.buf_size > BUFF_SIZE)) {
		fprintf(stderr,
			"error: Invalid Parameters: Invalid buffer size\n"
			"see --help option\n");
		return -EINVAL;
	}

	total_size = sizeof(struct sg_entry_priv_t) +
			get_buf_size(&crypto_info);
	/*
	 * The total usdpaa_mem space required for processing the frames
	 * is split as follows:
	 * - SEC descriptors: one per each FQ multiplied by the # of FQs per
	 *		      core (the "from SEC" FQ doesn't have a descriptor)
	 * - QMan FQ structures: two per each FQ, multiplied by the # of FQs
	 *		       per core
	 * - I/O buffers & S/G entries multiplied by # of buffers
	 */
	max_num_buf = (DMAMEM_SIZE -
			FQ_PER_CORE * (SEC_DESCRIPTORS_SIZE + QMAN_FQS_SIZE)) /
			ALIGN(total_size, L1_CACHE_BYTES);

	/*
	 * Because the usdpaa_mem allocator is using some memory as well, at
	 * the end of the USDPAA reserved memory zone, just to be sure that in
	 * the worst case scenario (many small unaligned buffers) there's no
	 * issue, subtract 1 buffer from the calculated number
	 */
	max_num_buf--;

	if (crypto_info.buf_num == 0 || crypto_info.buf_num > max_num_buf) {
		fprintf(stderr, "error: Invalid Parameters: Invalid number of buffers:\n"
				"Maximum number of buffers for the selected frame size is %d\n"
				"see --help option\n", max_num_buf);
			return -EINVAL;
	}

	if (SNOW_F8_F9 == crypto_info.algo && PERF == crypto_info.mode) {
		fprintf(stdout, "info: PERF mode is not supported for"
			" SNOW_F8_F9");
		return -EINVAL;
	}

	if (validate_sec_era_version(user_sec_era, hw_sec_era))
		return -EINVAL;

	switch (crypto_info.algo) {
	case AES_CBC:
	case TDES_CBC:
	case SNOW_F8:
	case SNOW_F9:
	case KASUMI_F8:
	case KASUMI_F9:
	case CRC:
	case HMAC_SHA1:
	case SNOW_F8_F9:
		break;
	default:
		fprintf(stderr, "error: Invalid Parameters: SEC algorithm not"
			" supported\nsee --help option\n");
		return -EINVAL;
	}

	return 0;
}

/*
 * brief	Compare encrypted data returned by SEC 4.0 with	standard
 *		cipher text
 * param[in]	params - test parameters
 * param[in]	struct qm_fd - frame descriptor list
 * return	    0 on success, otherwise -ve value
 */
int test_enc_match(void *params, struct qm_fd fd[])
{
	struct test_param crypto_info = *((struct test_param *)params);
	struct qm_sg_entry *sgentry;
	uint8_t *enc_buf;
	dma_addr_t addr;
	uint32_t ind;
	uint8_t authnct = crypto_info.authnct;

	for (ind = 0; ind < crypto_info.buf_num; ind++) {
		addr = qm_fd_addr_get64(&fd[ind]);
		sgentry = __dma_mem_ptov(addr);

		addr = qm_sg_entry_get64(sgentry);
		enc_buf = __dma_mem_ptov(addr);

		if (test_vector_match((uint32_t *) enc_buf,
				      authnct ? (uint32_t *)
				      ref_test_vector.digest : (uint32_t *)
				      ref_test_vector.ciphertext,
				      authnct ? crypto_info.rt.output_buf_size *
				      BITS_PER_BYTE : ref_test_vector.length) !=
		    0) {
			if (!authnct)
				fprintf(stderr, "error: %s: Encrypted frame %d"
					" with CIPHERTEXT test vector doesn't"
					" match\n", __func__, ind + 1);
			else
				fprintf(stderr, "error: %s digest match"
					" failed\n", algorithm);

			print_frame_desc(&fd[ind]);
			return -1;
		}
	}

	if (!authnct)
		fprintf(stdout, "All %s encrypted frame match found with"
			" cipher text\n", algorithm);
	else
		fprintf(stdout, "All %s digest successfully matched\n",
			algorithm);

	return 0;
}

/*
 * brief	Compare decrypted data returned by SEC 4.0 with plain text
 *		input data
 * param[in]	params - test parameters
 * param[in]	struct qm_fd - frame descriptor list
 * return	0 on success, otherwise -ve value
 */
int test_dec_match(void *params, struct qm_fd fd[])
{
	struct test_param crypto_info = *((struct test_param *)params);
	struct qm_sg_entry *sgentry;
	uint8_t *dec_buf;
	uint8_t plain_data = 0;
	dma_addr_t addr;
	uint32_t i, ind;

	for (ind = 0; ind < crypto_info.buf_num; ind++) {
		addr = qm_fd_addr_get64(&fd[ind]);
		sgentry = __dma_mem_ptov(addr);

		addr = qm_sg_entry_get64(sgentry);
		dec_buf = __dma_mem_ptov(addr);
		if (CIPHER == crypto_info.mode) {
			if (test_vector_match((uint32_t *) dec_buf, (uint32_t *)
					      ref_test_vector.plaintext,
					      ref_test_vector.length) != 0) {
				fprintf(stderr, "error: %s: Decrypted frame %d"
					" with PLAINTEXT test vector doesn't"
					" match\n", __func__, ind + 1);
				print_frame_desc(&fd[ind]);
				return -1;
			}
		} else {
			for (i = 0; i < crypto_info.buf_size; i++) {
				if (dec_buf[i] != plain_data) {
					fprintf(stderr, "error: %s: %s"
						" decrypted frame %d doesn't"
						" match!\n", __func__,
						algorithm, ind + 1);
					print_frame_desc(&fd[ind]);
					return -1;
				}
				plain_data++;
			}
		}
	}
	fprintf(stdout, "All %s decrypted frame matches initial text\n",
		algorithm);

	return 0;
}

/* argp structure itself of argp parser */
static struct argp argp = { options, parse_opt, NULL, NULL, NULL, NULL, NULL };

/*
 * brief	Main function of SEC 4.0 Test Application
 * param[in]	argc - Argument count
 * param[in]	argv - Argument list pointer
 */
int main(int argc, char *argv[])
{
	long num_online_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	struct thread_data thread_data[num_online_cpus];
	int err;
	uint32_t g_cmd_params = 0, i;
	struct test_param crypto_info;
	struct parse_input_t input;
	struct test_cb crypto_cb;

	ncpus = num_online_cpus;
	memset(&crypto_info, 0, sizeof(struct test_param));

	input.cmd_params = &g_cmd_params;
	input.crypto_info = &crypto_info;

	/* Parse and check input arguments */
	argp_parse(&argp, argc, argv, 0, 0, &input);

	/* Get the number of cores */
	if (ncpus < 1 || ncpus > num_online_cpus) {
		fprintf(stderr, "error: Invalid Parameters: Number of cpu's"
			" given in argument is more than the active cpu's\n");
		exit(-EINVAL);
	}

	fprintf(stdout, "\nWelcome to FSL SEC 4.0 application!\n");

	err = of_init();
	if (err)
		error(err, err, "error: of_init() failed");

	hw_sec_era = sec_get_of_era();

	err = validate_params(g_cmd_params, crypto_info);
	if (err)
		error(err, err, "error: validate_params failed!");

	/* map DMA memory */
	dma_mem_generic = dma_mem_create(DMA_MAP_FLAG_ALLOC, NULL, DMAMEM_SIZE);
	if (!dma_mem_generic) {
		pr_err("DMA memory initialization failed\n");
		exit(EXIT_FAILURE);
	}

	/* Initialize barrier for all the threads! */
	err = pthread_barrier_init(&app_barrier, NULL, ncpus);
	if (err)
		error(err, err, "error: unable to initialize pthread_barrier");

	err = qman_global_init();
	if (err)
		error(err, err, "error: qman global init failed");

	/* Prepare and create compound fds */
	err = prepare_test_frames(&crypto_info);
	if (err)
		error(err, err, "error: preparing test frames failed");

	set_crypto_cbs(&crypto_cb, crypto_info);

	for (i = 0; i < ncpus; i++) {
		thread_data[i].test_param = (void *)(&crypto_info);
		thread_data[i].test_cb = (void *)(&crypto_cb);
	}

	/* Starting threads on all active cpus */
	err = start_threads(thread_data, ncpus, 1, worker_fn);
	if (err)
		error(err, err, "error: start_threads failure");

	/* Wait for all the threads to finish */
	wait_threads(thread_data, ncpus);

	validate_test(crypto_info.itr_num, crypto_info.buf_num,
		      crypto_info.buf_size);

	free_fd(crypto_info.buf_num);
	of_finish();
	exit(EXIT_SUCCESS);
}

/*
 * brief	Returns number of iterations for test
 * param[in]	params - test parameters
 * return	Number of iterations for test
 */
int get_num_of_iterations(void *params)
{
	struct test_param crypto_info = *((struct test_param *)params);

	return crypto_info.itr_num;
}

/**
 * @brief	Sets the number of executed iterations for a test
 * @param[in]	params - test parameters
 * @return	Number of iterations executed for the test
 */
void set_num_of_iterations(void *params, unsigned int itr_num)
{
	((struct test_param *)params)->itr_num = itr_num;
}

/*
 * brief	Returns number of buffers for test
 * param[in]	params - test parameters
 * return	Number of buffers for test
 */
inline int get_num_of_buffers(void *params)
{
	struct test_param crypto_info = *((struct test_param *)params);

	return crypto_info.buf_num;
}

/*
 * brief	Returns test mode - CIPHER/PERF
 * param[in]	params - test parameters
 * return	Test mode - CIPHER/PERF
 */
inline enum test_mode get_test_mode(void *params)
{
	struct test_param crypto_info = *((struct test_param *)params);

	return crypto_info.mode;
}

/*
 * brief	Returns if test requires authentication
 * param[in]	params - test parameters
 * return	0 - doesn't require authentication/1 - requires authentication
 */
inline uint8_t requires_authentication(void *params)
{
	return ((struct test_param *)params)->authnct;
}

/*
 * brief	Returns number of cpus for test
 * param[in]	params - test parameters
 * return	Number of cpus for test
 */
inline long get_num_of_cpus(void)
{
	return ncpus;
}

/*
 * brief	Returns thread barrier for test
 * param[in]	None
 * return	Thread barrier
 */
inline pthread_barrier_t *get_thread_barrier(void)
{
	return &app_barrier;
}

/*
 * brief	Set specific callbacks for test
 * param[in]	crypto_cb - structure that holds reference to test callbacks
 * return	None
 */
void set_crypto_cbs(struct test_cb *crypto_cb, struct test_param crypto_info)
{
	memset(crypto_cb, 0, sizeof(struct test_cb));

	crypto_cb->set_sec_descriptor = setup_sec_descriptor;
	crypto_cb->is_enc_match = test_enc_match;
	crypto_cb->is_dec_match = test_dec_match;
	crypto_cb->set_enc_buf = set_enc_buf;

	/* Set decryption buffer */
	if (SNOW_F8_F9 == crypto_info.algo)
		crypto_cb->set_dec_buf = set_dec_auth_buf;
	else if (!crypto_info.authnct)
		crypto_cb->set_dec_buf = set_dec_buf;
	else
		crypto_cb->set_dec_buf = NULL;

	crypto_cb->get_num_of_iterations = get_num_of_iterations;
	crypto_cb->set_num_of_iterations = set_num_of_iterations;
	crypto_cb->get_num_of_buffers = get_num_of_buffers;
	crypto_cb->get_test_mode = get_test_mode;
	crypto_cb->get_num_of_cpus = get_num_of_cpus;
	crypto_cb->requires_authentication = requires_authentication;
	crypto_cb->get_thread_barrier = get_thread_barrier;
}
