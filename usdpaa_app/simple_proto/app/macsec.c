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
#include "macsec.h"
#include "macsec_test_vector.h"

/* Forward declarations */
static error_t parse_opts(int key, char *arg, struct argp_state *state);
static void unregister_macsec(struct protocol_info *proto_info);

struct argp_option macsec_options[] = {
	{"algo", 'o', "CIPHER TYPE",  0,
	 "OPTIONAL PARAMETER"
	 "\n\nSelect between GCM/GMAC processing (default: GCM)"
	 "\n0 = GCM"
	 "\n1 = GMAC"
	 "\n"},
	{0}
};

/* Parser for MACsec command line options */
static struct argp macsec_argp = {
	macsec_options, parse_opts
};

static struct argp_child argp_children = {
		&macsec_argp, 0, "MACsec protocol options", 3};

/**
 * @brief	Initializes the reference test vector for MACsec
 * @details	Initializes key, length and other variables for the protocol
 * @param[in]	crypto_info - test parameters
 * @return	0 if success, error code otherwise
 */
int init_rtv_macsec(struct test_param *crypto_info)
{
	struct protocol_info *proto = crypto_info->proto;
	struct macsec_params *macsec_params = proto->proto_params;
	struct macsec_ref_vector_s *ref_test_vector = proto->proto_vector;

	if (macsec_params->cipher_alg == MACSEC_CIPHER_TYPE_GMAC) {
		crypto_info->test_set += MACSEC_GMAC_TEST_ID;
		crypto_info->authnct = 1;
	}

	ref_test_vector->key =
		(uintptr_t)macsec_reference_key[crypto_info->test_set - 1];

	/* set the MACsec pdb params for test */
	ref_test_vector->ethertype =
	    macsec_reference_sectag_etype[crypto_info->test_set - 1];
	ref_test_vector->tci_an =
	    macsec_reference_sectag_tcian[crypto_info->test_set - 1];
	ref_test_vector->pn =
	    macsec_reference_iv_pn[crypto_info->test_set - 1];
	ref_test_vector->sci =
	    macsec_reference_iv_sci[crypto_info->test_set - 1];

	if (CIPHER == crypto_info->mode) {
		ref_test_vector->length =
		    macsec_reference_length[crypto_info->test_set - 1];
		ref_test_vector->plaintext =
		    macsec_reference_plaintext[crypto_info->test_set - 1];
		ref_test_vector->ciphertext =
		    macsec_reference_ciphertext[crypto_info->test_set - 1];
	}

	return 0;
}

/**
 * @brief	Set PN constant in MACsec shared descriptor
 * @details	Inside this routine, context is erased, PN is read from
 *		descriptor buffer before operation is performed, and after the
 *		operation is updated in descriptor buffer and also saved in
 *		memory.The SEC automatically increments the PN inside the
 *		descriptor buffer after executing the MACsec PROTOCOL command,
 *		so the next packet that will be processed by SEC will be
 *		encapsulated/decapsulated with an incremented PN. This routine
 *		is needed for MACsec's tests using a single golden pattern
 *		packet reinjected for multiple times.
 * @param[in]	shared_desc - pointer to descriptor buffer
 * @param[in]	shared_desc_len - shared descriptor length
 * @return	None
 */
void macsec_set_pn_constant(uint32_t *shared_desc, unsigned *shared_desc_len)
{
	struct program prg;
	struct program *program = &prg;
	uint32_t op_line, tmp;
	uint32_t tmp_buf[64];
	int i, op_idx = 0, save_lines = 0;
	unsigned extra_instr = 4;

	/* to mute compiler warnings */
	prg.current_instruction = 0;

	for (i = 0; i < *shared_desc_len; i++) {
		tmp = shared_desc[i];
		if ((tmp & CMD_MASK) == CMD_OPERATION)
			op_idx = i;
	}

	if (!op_idx)
		/* there isn't an operation instruction in descbuf */
		return;

	if ((*shared_desc_len + extra_instr) > MAX_DESCRIPTOR_SIZE)
		/* we can't modify this descriptor; it will overflow */
		return;

	if (op_idx < *shared_desc_len - 1) {
		/* operation is not the last instruction in descbuf */
		save_lines = *shared_desc_len - 1 - op_idx;
		for (i = 0; i < save_lines; i++)
			tmp_buf[i] = shared_desc[op_idx + 1 + i];
	}

	/* save operation instruction */
	op_line = shared_desc[op_idx];

	/* RTA snippet code to update shared descriptor */
	program->buffer = shared_desc;
	program->current_pc = op_idx;

	/*
	 * Use CONTEXT2 to save the current value of PN. CONTEXT2 _should_ be
	 * unused by MACSEC protocol.
	 */
	MOVE(DESCBUF, 5 * 4, CONTEXT2, 0, IMM(4), WITH(0));
	program->buffer[program->current_pc++] = op_line;
	MOVE(CONTEXT2, 0, DESCBUF, 5 * 4, IMM(4), WITH(WAITCOMP));
	STORE(SHAREDESCBUF, 5 * 4, NONE, 4, 0);
	/* Wait for all bus transactions to finish before stopping. */
	JUMP(IMM(0), HALT_STATUS, ALL_TRUE, WITH(CALM));

	/* erase context in shared desc header */
	*shared_desc &= ~HDR_SAVECTX;

	/* update length in shared desc header */
	*shared_desc_len += extra_instr;
	*shared_desc &= ~HDR_SD_LENGTH_MASK;
	*shared_desc |= *shared_desc_len & HDR_SD_LENGTH_MASK;

	/* copy the rest of the instructions in buffer */
	for (i = 0; i < save_lines; i++)
		shared_desc[program->current_pc + i] = tmp_buf[i];
}

static void *create_descriptor(bool mode, void *params)
{
	struct test_param *crypto_info = (struct test_param *)params;
	struct protocol_info *proto = crypto_info->proto;
	struct macsec_params *macsec_params = proto->proto_params;
	struct macsec_ref_vector_s *ref_test_vector = proto->proto_vector;
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
	cipher_info.keylen = MACSEC_KEY_SIZE;
	cipher_info.key_enc_flags = 0;
	cipher_info.algtype =
			macsec_params->cipher_alg;
	if (ENCRYPT == mode)
		cnstr_shdsc_macsec_encap(shared_desc,
					 &shared_desc_len,
					 &cipher_info,
					 ref_test_vector->sci,
					 ref_test_vector->ethertype,
					 ref_test_vector->tci_an,
					 ref_test_vector->pn);

	else
		cnstr_shdsc_macsec_decap(shared_desc,
					 &shared_desc_len,
					 &cipher_info,
					 ref_test_vector->sci,
					 ref_test_vector->pn);
	macsec_set_pn_constant(shared_desc, &shared_desc_len);

	prehdr_desc->prehdr.hi.word = shared_desc_len & SEC_PREHDR_SDLEN_MASK;

	pr_debug("SEC %s shared descriptor:\n", proto->name);

	for (i = 0; i < shared_desc_len; i++)
		pr_debug("0x%x\n", *shared_desc++);

	return prehdr_desc;
}

/**
 * @brief	Parse MACSEC related command line options
 *
 */
static error_t parse_opts(int key, char *arg, struct argp_state *state)
{
	struct parse_input_t *input = state->input;
	struct test_param *crypto_info = input->crypto_info;
	struct macsec_params *macsec_params;

	/*
	 * If the protocol was not selected, then it makes no sense to go
	 * further.
	 */
	if (!crypto_info->proto)
		return 0;

	macsec_params = crypto_info->proto->proto_params;

	switch (key) {
	case 'o':
		macsec_params->cipher_alg = atoi(arg);
		printf("MACSEC processing = %d\n", macsec_params->cipher_alg);
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

/*
 * @brief	Check SEC parameters provided by user for MACSEC are valid
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
	struct macsec_params *macsec_params =
			(struct macsec_params *)proto->proto_params;

	if ((macsec_params->cipher_alg == MACSEC_CIPHER_TYPE_GMAC) &&
	    (rta_sec_era < RTA_SEC_ERA_5)) {
		fprintf(stderr,
			"error: Unsupported MACsec algorithm for SEC ERAs 2-4\n");
		return -EINVAL;
	}

	return 0;
}

static int get_buf_size(struct test_param *crypto_info)
{
	return 2 * crypto_info->buf_size + MACSEC_ICV_SIZE +
			MACSEC_SECTAG_SIZE;
}

/**
 * @brief	Set buffer sizes for input/output frames
 * @param[in]	crypto_info - test parameters
 * @return	0 on success, otherwise -EINVAL value
 */
static int set_buf_size(struct test_param *crypto_info)
{
	struct runtime_param *p_rt = &(crypto_info->rt);

	p_rt->input_buf_capacity = crypto_info->buf_size;
	p_rt->input_buf_length = crypto_info->buf_size;

	p_rt->output_buf_size =
		    crypto_info->buf_size + MACSEC_ICV_SIZE +
		    MACSEC_SECTAG_SIZE;

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
struct protocol_info *register_macsec(void)
{
	unsigned num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	struct protocol_info *proto_info = NULL;


	proto_info = calloc(1, sizeof(*proto_info));
	if (unlikely(!proto_info)) {
		pr_err("failed to allocate protocol structure in %s",
		       __FILE__);
		return NULL;
	}

	SAFE_STRNCPY(proto_info->name, "MACsec", sizeof(proto_info->name));
	proto_info->unregister = unregister_macsec;
	proto_info->argp_children = &argp_children;
	proto_info->init_ref_test_vector = init_rtv_macsec;
	proto_info->setup_sec_descriptor = create_descriptor;
	proto_info->validate_opts = validate_opts;
	proto_info->get_buf_size = get_buf_size;
	proto_info->set_buf_size = set_buf_size;
	proto_info->validate_test_set = validate_test_set;
	proto_info->proto_params = calloc(1, sizeof(struct macsec_params));
	if (unlikely(!proto_info->proto_params)) {
		pr_err("failed to allocate protocol parameters in %s",
		       __FILE__);
		goto err;
	}

	proto_info->proto_vector =
		calloc(1, sizeof(struct macsec_ref_vector_s));
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
void unregister_macsec(struct protocol_info *proto_info)
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
