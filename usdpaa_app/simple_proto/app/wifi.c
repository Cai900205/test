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
#include "wifi.h"
#include "wifi_test_vector.h"

/* Forward declarations */
static void unregister_wifi(struct protocol_info *proto_info);

static int validate_test_set(struct test_param *crypto_info)
{
	if ((crypto_info->test_set > 0) && (crypto_info->test_set < 3))
		return 0;

	fprintf(stderr, "error: Invalid Parameters: Test set number is invalid\n");
	return -EINVAL;
}

int test_enc_match_cb_wifi(int fd_ind, uint8_t *enc_buf,
			    struct test_param *crypto_info)
{
	struct wifi_ref_vector_s *ref_test_vector =
			crypto_info->proto->proto_vector;

	if (!fd_ind &&
	    test_vector_match((uint32_t *)enc_buf,
			      (uint32_t *)ref_test_vector->ciphertext,
			      crypto_info->rt.output_buf_size * BITS_PER_BYTE))
		return -1;

	return 0;
}

/**
 * @brief	Check SEC parameters provided by user for WiFi are valid
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

	/*
	 * For WiFi in CIPHER mode only the first frame
	 * from the first iteration can be verified if it is matching
	 * with the corresponding test vector, due to
	 * the PN increment by SEC for each frame processed.
	 */
	if (CIPHER == crypto_info->mode && crypto_info->itr_num != 1) {
		crypto_info->itr_num = 1;
		printf("WARNING: Running WiFi in CIPHER mode with only one iteration\n");
	}

	proto->test_enc_match_cb = test_enc_match_cb_wifi;

	return 0;
}

int init_rtv_wifi_ccmp(struct test_param *crypto_info)
{
	struct protocol_info *proto = crypto_info->proto;
	struct wifi_ref_vector_s *ref_test_vector = proto->proto_vector;

	ref_test_vector->key =
		(uintptr_t)wifi_reference_key[crypto_info->test_set - 1];

	/* set the WiFi pdb params for test */
	ref_test_vector->mac_hdr_len =
			wifi_reference_mac_hdr_len[crypto_info->test_set - 1];
	ref_test_vector->pn = wifi_reference_pn[crypto_info->test_set - 1];
	ref_test_vector->priority =
			wifi_reference_pri[crypto_info->test_set - 1];
	ref_test_vector->key_id =
			wifi_reference_key_id[crypto_info->test_set - 1];

	if (CIPHER == crypto_info->mode) {
		ref_test_vector->length =
		    wifi_reference_length[crypto_info->test_set - 1];
		ref_test_vector->plaintext =
		    wifi_reference_plaintext[crypto_info->test_set - 1];
		ref_test_vector->ciphertext =
		    wifi_reference_ciphertext[crypto_info->test_set - 1];
	}

	return 0;
}

static int get_buf_size(struct test_param *crypto_info)
{
	return 2 * crypto_info->buf_size + WIFI_CCM_SIZE +
			WIFI_ICV_SIZE + WIFI_FCS_SIZE;
}

static int set_buf_size(struct test_param *crypto_info)
{
	struct runtime_param *p_rt = &(crypto_info->rt);

	p_rt->input_buf_capacity = crypto_info->buf_size;
	p_rt->input_buf_length = crypto_info->buf_size;

	crypto_info->rt.output_buf_size =
				    crypto_info->buf_size + WIFI_CCM_HDR_SIZE +
				    WIFI_ICV_SIZE;

	return 0;
}

/**
 * @brief	Create SEC shared descriptor
 * @param[in]	mode -	To check whether descriptor is for encryption or
 *		decryption
 * @param[in]	crypto_info - test parameters
 * @return	Shared descriptor pointer on success, otherwise NULL
 */
static void *create_descriptor(bool mode, void *params)
{
	struct test_param *crypto_info = (struct test_param *)params;
	struct protocol_info *proto = crypto_info->proto;
	struct wifi_ref_vector_s *ref_test_vector = proto->proto_vector;
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
	cipher_info.keylen = WIFI_KEY_SIZE;
	cipher_info.key_enc_flags = 0;

	if (ENCRYPT == mode)
		cnstr_shdsc_wifi_encap(shared_desc,
				       &shared_desc_len,
/*
* This is currently hardcoded. The application doesn't allow for
* proper retrieval of PS.
*/
				       0,
				       ref_test_vector->mac_hdr_len,
				       ref_test_vector->pn,
				       ref_test_vector->priority,
				       ref_test_vector->key_id,
				       &cipher_info);
	else
		cnstr_shdsc_wifi_decap(shared_desc,
				       &shared_desc_len,
/*
* This is currently hardcoded. The application doesn't allow for
* proper retrieval of PS.
*/
				       0,
				       ref_test_vector->mac_hdr_len,
				       ref_test_vector->pn,
				       ref_test_vector->priority,
				       &cipher_info);

	prehdr_desc->prehdr.hi.word = shared_desc_len & SEC_PREHDR_SDLEN_MASK;

	pr_debug("SEC %s shared descriptor:\n", proto->name);

	for (i = 0; i < shared_desc_len; i++)
		pr_debug("0x%x\n", *shared_desc++);

	return prehdr_desc;
}

/**
 * @brief	Allocates the necessary structures for a protocol, sets the
 *		callbacks for the protocol and returns the allocated chunk.
 * @return	NULL if an error occurred, pointer to the protocol structure
 *		otherwise.
 */
struct protocol_info *register_wifi(void)
{
	unsigned num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	struct protocol_info *proto_info = NULL;


	proto_info = calloc(1, sizeof(*proto_info));
	if (unlikely(!proto_info)) {
		pr_err("failed to allocate protocol structure in %s",
		       __FILE__);
		return NULL;
	}

	SAFE_STRNCPY(proto_info->name, "WiFi", sizeof(proto_info->name));
	proto_info->unregister = unregister_wifi;
	proto_info->init_ref_test_vector = init_rtv_wifi_ccmp;
	proto_info->setup_sec_descriptor = create_descriptor;
	proto_info->validate_opts = validate_opts;
	proto_info->get_buf_size = get_buf_size;
	proto_info->set_buf_size = set_buf_size;
	proto_info->validate_test_set = validate_test_set;

	proto_info->proto_vector =
		calloc(1, sizeof(struct wifi_ref_vector_s));
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
void unregister_wifi(struct protocol_info *proto_info)
{
	int i;

	if (!proto_info)
		return;

	for (i = 0; i < proto_info->num_cpus * FQ_PER_CORE * 2; i++)
		if (proto_info->descr[i].descr)
			__dma_mem_free(proto_info->descr[i].descr);

	free(proto_info->proto_vector);
	free(proto_info);
}
