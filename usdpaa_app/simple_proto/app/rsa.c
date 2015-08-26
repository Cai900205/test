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

#include "rsa.h"
#include "rsa_test_vector.h"

/* Forward declarations */
static error_t parse_opts(int, char *, struct argp_state *);
static void unregister_rsa(struct protocol_info *);

struct argp_option rsa_options[] = {
	{"form", 'b', "FORM",  0,
	 "Select RSA Decrypt Private Key form:"
	 "\n\t 1 = Form 1 (default)"
	 "\n\t 2 = Form 2"
	 "\n\t 3 = Form 3"
	 "\n"},
	{0}
};

/* Parser for RSA command line options */
static struct argp rsa_argp = {
	rsa_options, parse_opts
};

static struct argp_child argp_children = {
		&rsa_argp, 0, "RSA protocol options", 4};

static void free_rsa_rtv(struct rsa_ref_vector_s *rtv)
{
	if (rtv->f)
		__dma_mem_free(rtv->f);
	if (rtv->g)
		__dma_mem_free(rtv->g);
	if (rtv->e)
		__dma_mem_free(rtv->e);
	if (rtv->n)
		__dma_mem_free(rtv->n);
	if (rtv->d)
		__dma_mem_free(rtv->d);
	if (rtv->c)
		__dma_mem_free(rtv->c);
	if (rtv->p)
		__dma_mem_free(rtv->p);
	if (rtv->q)
		__dma_mem_free(rtv->q);
	if (rtv->dp)
		__dma_mem_free(rtv->dp);
	if (rtv->dq)
		__dma_mem_free(rtv->dq);
	if (rtv->tmp1)
		__dma_mem_free(rtv->tmp1);
	if (rtv->tmp2)
		__dma_mem_free(rtv->tmp2);
	free(rtv->e_pdb);
	free(rtv->d_pdb);
}

static int init_rsa_enc_pdb(struct test_param *crypto_info, int mode)
{
	struct protocol_info *proto = crypto_info->proto;
	struct rsa_ref_vector_s *rtv = proto->proto_vector;

	memcpy(rtv->f, rsa_ref_input[crypto_info->test_set - 1], rtv->f_len);
	rtv->n = __dma_mem_memalign(L1_CACHE_BYTES, rtv->n_len);
	if (!rtv->n)
		return -ENOMEM;
	memcpy(rtv->n, rsa_ref_modulus[crypto_info->test_set - 1], rtv->n_len);
	rtv->e = __dma_mem_memalign(L1_CACHE_BYTES, rtv->e_len);
	if (!rtv->e)
		return -ENOMEM;
	memcpy(rtv->e, rsa_ref_pub_exp[crypto_info->test_set - 1], rtv->e_len);

	if (mode == RSA_MODE_64B) {
		struct rsa_encrypt_pdb_64b rsa_enc;
		rtv->e_pdb = malloc(sizeof(struct rsa_encrypt_pdb_64b));
		if (!rtv->e_pdb)
			return -ENOMEM;
		rsa_enc.header = (rtv->sgf << RSA_ENC_SGF_SHIFT) |
		      (rtv->e_len << RSA_ENC_E_LEN_SHIFT) |
		      (rtv->n_len);
		rsa_enc.f_ref_high = high_32b(__dma_mem_vtop(rtv->f));
		rsa_enc.f_ref_low = low_32b(__dma_mem_vtop(rtv->f));
		rsa_enc.g_ref_high = high_32b(__dma_mem_vtop(rtv->g));
		rsa_enc.g_ref_low = low_32b(__dma_mem_vtop(rtv->g));
		rsa_enc.e_ref_high = high_32b(__dma_mem_vtop(rtv->e));
		rsa_enc.e_ref_low = low_32b(__dma_mem_vtop(rtv->e));
		rsa_enc.n_ref_high = high_32b(__dma_mem_vtop(rtv->n));
		rsa_enc.n_ref_low = low_32b(__dma_mem_vtop(rtv->n));
		rsa_enc.f_len = rtv->f_len;
		memcpy(rtv->e_pdb, &rsa_enc,
		       sizeof(struct rsa_encrypt_pdb_64b));
		rtv->e_pdb_size = sizeof(struct rsa_encrypt_pdb_64b);
	} else {
		struct rsa_encrypt_pdb rsa_enc;
		rtv->e_pdb = malloc(sizeof(struct rsa_encrypt_pdb));
		if (!rtv->e_pdb)
			return -ENOMEM;
		rsa_enc.header = (rtv->sgf << RSA_ENC_SGF_SHIFT) |
			      (rtv->e_len << RSA_ENC_E_LEN_SHIFT) |
			      (rtv->n_len);
		rsa_enc.f_ref = low_32b(__dma_mem_vtop(rtv->f));
		rsa_enc.g_ref = low_32b(__dma_mem_vtop(rtv->g));
		rsa_enc.e_ref = low_32b(__dma_mem_vtop(rtv->e));
		rsa_enc.n_ref = low_32b(__dma_mem_vtop(rtv->n));
		rsa_enc.f_len = rtv->f_len;
		memcpy(rtv->e_pdb, &rsa_enc, sizeof(struct rsa_encrypt_pdb));
		rtv->e_pdb_size = sizeof(struct rsa_encrypt_pdb);
	}

	return 0;
}

static int init_rsa_dec_form1_pdb(struct test_param *crypto_info, int mode)
{
	struct protocol_info *proto = crypto_info->proto;
	struct rsa_ref_vector_s *rtv = proto->proto_vector;

	rtv->d_len =  rsa_ref_private_exp_len[crypto_info->test_set - 1];
	memcpy(rtv->g, rsa_ref_result[crypto_info->test_set - 1], rtv->n_len);
	rtv->n = __dma_mem_memalign(L1_CACHE_BYTES, rtv->n_len);
	if (!rtv->n)
		return -ENOMEM;
	memcpy(rtv->n, rsa_ref_modulus[crypto_info->test_set - 1], rtv->n_len);
	rtv->d = __dma_mem_memalign(L1_CACHE_BYTES, rtv->d_len);
	if (!rtv->d)
		return -ENOMEM;
	memcpy(rtv->d, rsa_ref_priv_exp[crypto_info->test_set - 1], rtv->d_len);

	if (mode == RSA_MODE_64B) {
		struct rsa_dec_pdb_form1_64b rsa_dec;
		rtv->d_pdb = malloc(sizeof(struct rsa_dec_pdb_form1_64b));
		if (!rtv->d_pdb)
			return -ENOMEM;
		rsa_dec.header = (rtv->sgf << RSA_DEC1_SGF_SHIFT) |
		      (rtv->d_len << RSA_DEC1_D_LEN_SHIFT) |
		      (rtv->n_len);
		rsa_dec.f_ref_high = high_32b(__dma_mem_vtop(rtv->f));
		rsa_dec.f_ref_low = low_32b(__dma_mem_vtop(rtv->f));
		rsa_dec.g_ref_high = high_32b(__dma_mem_vtop(rtv->g));
		rsa_dec.g_ref_low = low_32b(__dma_mem_vtop(rtv->g));
		rsa_dec.d_ref_high = high_32b(__dma_mem_vtop(rtv->d));
		rsa_dec.d_ref_low = low_32b(__dma_mem_vtop(rtv->d));
		rsa_dec.n_ref_high = high_32b(__dma_mem_vtop(rtv->n));
		rsa_dec.n_ref_low = low_32b(__dma_mem_vtop(rtv->n));
		memcpy(rtv->d_pdb, &rsa_dec,
		       sizeof(struct rsa_dec_pdb_form1_64b));
		rtv->d_pdb_size = sizeof(struct rsa_dec_pdb_form1_64b);
	} else {
		struct rsa_dec_pdb_form1 rsa_dec;
		rtv->d_pdb = malloc(sizeof(struct rsa_dec_pdb_form1));
		if (!rtv->d_pdb)
			return -ENOMEM;
		rsa_dec.header = (rtv->sgf << RSA_DEC1_SGF_SHIFT) |
			      (rtv->d_len << RSA_DEC1_D_LEN_SHIFT) |
			      (rtv->n_len);
		rsa_dec.f_ref = low_32b(__dma_mem_vtop(rtv->f));
		rsa_dec.g_ref = low_32b(__dma_mem_vtop(rtv->g));
		rsa_dec.d_ref = low_32b(__dma_mem_vtop(rtv->d));
		rsa_dec.n_ref = low_32b(__dma_mem_vtop(rtv->n));
		memcpy(rtv->d_pdb, &rsa_dec, sizeof(struct rsa_dec_pdb_form1));
		rtv->d_pdb_size = sizeof(struct rsa_dec_pdb_form1);
	}

	return 0;
}

static int init_rsa_dec_form2_pdb(struct test_param *crypto_info, int mode)
{
	struct protocol_info *proto = crypto_info->proto;
	struct rsa_ref_vector_s *rtv = proto->proto_vector;

	rtv->d_len =  rsa_ref_private_exp_len[crypto_info->test_set - 1];
	rtv->p_len = rsa_ref_p_len[crypto_info->test_set - 1];
	rtv->q_len = rsa_ref_q_len[crypto_info->test_set - 1];
	memcpy(rtv->g, rsa_ref_result[crypto_info->test_set - 1], rtv->n_len);
	rtv->d = __dma_mem_memalign(L1_CACHE_BYTES, rtv->d_len);
	if (!rtv->d)
		return -ENOMEM;
	memcpy(rtv->d, rsa_ref_priv_exp[crypto_info->test_set - 1], rtv->d_len);
	rtv->p = __dma_mem_memalign(L1_CACHE_BYTES, rtv->p_len);
	if (!rtv->p)
		return -ENOMEM;
	memcpy(rtv->p, rsa_ref_p[crypto_info->test_set - 1], rtv->p_len);
	rtv->q = __dma_mem_memalign(L1_CACHE_BYTES, rtv->q_len);
	if (!rtv->q)
		return -ENOMEM;
	memcpy(rtv->q, rsa_ref_q[crypto_info->test_set - 1], rtv->q_len);
	rtv->tmp1 = __dma_mem_memalign(L1_CACHE_BYTES, rtv->p_len);
	if (!rtv->tmp1)
		return -ENOMEM;
	rtv->tmp2 = __dma_mem_memalign(L1_CACHE_BYTES, rtv->q_len);
	if (!rtv->tmp2)
		return -ENOMEM;

	if (mode == RSA_MODE_64B) {
		struct rsa_dec_pdb_form2_64b rsa_dec;
		rtv->d_pdb = malloc(sizeof(struct rsa_dec_pdb_form2_64b));
		if (!rtv->d_pdb)
			return -ENOMEM;
		rsa_dec.header = (rtv->sgf << RSA_DEC2_SGF_SHIFT) |
		      (rtv->d_len << RSA_DEC2_D_LEN_SHIFT) |
		      (rtv->n_len);
		rsa_dec.f_ref_high = high_32b(__dma_mem_vtop(rtv->f));
		rsa_dec.f_ref_low = low_32b(__dma_mem_vtop(rtv->f));
		rsa_dec.g_ref_high = high_32b(__dma_mem_vtop(rtv->g));
		rsa_dec.g_ref_low = low_32b(__dma_mem_vtop(rtv->g));
		rsa_dec.d_ref_high = high_32b(__dma_mem_vtop(rtv->d));
		rsa_dec.d_ref_low = low_32b(__dma_mem_vtop(rtv->d));
		rsa_dec.p_ref_high = high_32b(__dma_mem_vtop(rtv->p));
		rsa_dec.p_ref_low = low_32b(__dma_mem_vtop(rtv->p));
		rsa_dec.q_ref_high = high_32b(__dma_mem_vtop(rtv->q));
		rsa_dec.q_ref_low = low_32b(__dma_mem_vtop(rtv->q));
		rsa_dec.tmp1_ref_high = high_32b(__dma_mem_vtop(rtv->tmp1));
		rsa_dec.tmp1_ref_low = low_32b(__dma_mem_vtop(rtv->tmp1));
		rsa_dec.tmp2_ref_high = high_32b(__dma_mem_vtop(rtv->tmp2));
		rsa_dec.tmp2_ref_low = low_32b(__dma_mem_vtop(rtv->tmp2));
		rsa_dec.trailer = (rtv->q_len << RSA_DEC2_Q_LEN_SHIFT) |
				  (rtv->p_len);
		memcpy(rtv->d_pdb, &rsa_dec,
		       sizeof(struct rsa_dec_pdb_form2_64b));
		rtv->d_pdb_size = sizeof(struct rsa_dec_pdb_form2_64b);
	} else {
		struct rsa_dec_pdb_form2 rsa_dec;
		rtv->d_pdb = malloc(sizeof(struct rsa_dec_pdb_form2));
		if (!rtv->d_pdb)
			return -ENOMEM;
		rsa_dec.header = (rtv->sgf << RSA_DEC2_SGF_SHIFT) |
			      (rtv->e_len << RSA_DEC2_D_LEN_SHIFT) |
			      (rtv->n_len);
		rsa_dec.f_ref = low_32b(__dma_mem_vtop(rtv->f));
		rsa_dec.g_ref = low_32b(__dma_mem_vtop(rtv->g));
		rsa_dec.d_ref = low_32b(__dma_mem_vtop(rtv->d));
		rsa_dec.p_ref = low_32b(__dma_mem_vtop(rtv->p));
		rsa_dec.q_ref = low_32b(__dma_mem_vtop(rtv->q));
		rsa_dec.tmp1_ref = low_32b(__dma_mem_vtop(rtv->tmp1));
		rsa_dec.tmp2_ref = low_32b(__dma_mem_vtop(rtv->tmp2));
		rsa_dec.trailer = (rtv->q_len << RSA_DEC2_Q_LEN_SHIFT) |
				  (rtv->p_len);
		memcpy(rtv->d_pdb, &rsa_dec, sizeof(struct rsa_dec_pdb_form2));
		rtv->d_pdb_size = sizeof(struct rsa_dec_pdb_form2);
	}

	return 0;
}

static int init_rsa_dec_form3_pdb(struct test_param *crypto_info, int mode)
{
	struct protocol_info *proto = crypto_info->proto;
	struct rsa_ref_vector_s *rtv = proto->proto_vector;

	rtv->p_len = rsa_ref_p_len[crypto_info->test_set - 1];
	rtv->q_len = rsa_ref_q_len[crypto_info->test_set - 1];
	memcpy(rtv->g, rsa_ref_result[crypto_info->test_set - 1], rtv->n_len);
	rtv->c = __dma_mem_memalign(L1_CACHE_BYTES, rtv->p_len);
	if (!rtv->c)
		return -ENOMEM;
	memcpy(rtv->c, rsa_ref_qinv[crypto_info->test_set - 1], rtv->p_len);
	rtv->p = __dma_mem_memalign(L1_CACHE_BYTES, rtv->p_len);
	if (!rtv->p)
		return -ENOMEM;
	memcpy(rtv->p, rsa_ref_p[crypto_info->test_set - 1], rtv->p_len);
	rtv->q = __dma_mem_memalign(L1_CACHE_BYTES, rtv->q_len);
	if (!rtv->q)
		return -ENOMEM;
	memcpy(rtv->q, rsa_ref_q[crypto_info->test_set - 1], rtv->q_len);
	rtv->dp = __dma_mem_memalign(L1_CACHE_BYTES, rtv->p_len);
	if (!rtv->dp)
		return -ENOMEM;
	memcpy(rtv->dp, rsa_ref_dp[crypto_info->test_set - 1], rtv->p_len);
	rtv->dq = __dma_mem_memalign(L1_CACHE_BYTES, rtv->q_len);
	if (!rtv->dq)
		return -ENOMEM;
	memcpy(rtv->dq, rsa_ref_dq[crypto_info->test_set - 1], rtv->q_len);
	rtv->tmp1 = __dma_mem_memalign(L1_CACHE_BYTES, rtv->p_len);
	if (!rtv->tmp1)
		return -ENOMEM;
	rtv->tmp2 = __dma_mem_memalign(L1_CACHE_BYTES, rtv->q_len);
	if (!rtv->tmp2)
		return -ENOMEM;

	if (mode == RSA_MODE_64B) {
		struct rsa_dec_pdb_form3_64b rsa_dec;
		rtv->d_pdb = malloc(sizeof(struct rsa_dec_pdb_form3_64b));
		if (!rtv->d_pdb)
			return -ENOMEM;
		rsa_dec.header = (rtv->sgf << RSA_DEC3_SGF_SHIFT) |
		      (rtv->n_len);
		rsa_dec.f_ref_high = high_32b(__dma_mem_vtop(rtv->f));
		rsa_dec.f_ref_low = low_32b(__dma_mem_vtop(rtv->f));
		rsa_dec.g_ref_high = high_32b(__dma_mem_vtop(rtv->g));
		rsa_dec.g_ref_low = low_32b(__dma_mem_vtop(rtv->g));
		rsa_dec.c_ref_high = high_32b(__dma_mem_vtop(rtv->c));
		rsa_dec.c_ref_low = low_32b(__dma_mem_vtop(rtv->c));
		rsa_dec.p_ref_high = high_32b(__dma_mem_vtop(rtv->p));
		rsa_dec.p_ref_low = low_32b(__dma_mem_vtop(rtv->p));
		rsa_dec.q_ref_high = high_32b(__dma_mem_vtop(rtv->q));
		rsa_dec.q_ref_low = low_32b(__dma_mem_vtop(rtv->q));
		rsa_dec.dp_ref_high = high_32b(__dma_mem_vtop(rtv->dp));
		rsa_dec.dp_ref_low = low_32b(__dma_mem_vtop(rtv->dp));
		rsa_dec.dq_ref_high = high_32b(__dma_mem_vtop(rtv->dq));
		rsa_dec.dq_ref_low = low_32b(__dma_mem_vtop(rtv->dq));
		rsa_dec.tmp1_ref_high = high_32b(__dma_mem_vtop(rtv->tmp1));
		rsa_dec.tmp1_ref_low = low_32b(__dma_mem_vtop(rtv->tmp1));
		rsa_dec.tmp2_ref_high = high_32b(__dma_mem_vtop(rtv->tmp2));
		rsa_dec.tmp2_ref_low = low_32b(__dma_mem_vtop(rtv->tmp2));
		rsa_dec.trailer = (rtv->q_len << RSA_DEC3_Q_LEN_SHIFT) |
				  (rtv->p_len);
		memcpy(rtv->d_pdb, &rsa_dec,
		       sizeof(struct rsa_dec_pdb_form3_64b));
		rtv->d_pdb_size = sizeof(struct rsa_dec_pdb_form3_64b);
	} else {
		struct rsa_dec_pdb_form3 rsa_dec;
		rtv->d_pdb = malloc(sizeof(struct rsa_dec_pdb_form3));
		if (!rtv->d_pdb)
			return -ENOMEM;
		rsa_dec.header = (rtv->sgf << RSA_DEC2_SGF_SHIFT) |
			      (rtv->e_len << RSA_DEC2_D_LEN_SHIFT) |
			      (rtv->n_len);
		rsa_dec.f_ref = low_32b(__dma_mem_vtop(rtv->f));
		rsa_dec.g_ref = low_32b(__dma_mem_vtop(rtv->g));
		rsa_dec.c_ref = low_32b(__dma_mem_vtop(rtv->c));
		rsa_dec.p_ref = low_32b(__dma_mem_vtop(rtv->p));
		rsa_dec.q_ref = low_32b(__dma_mem_vtop(rtv->q));
		rsa_dec.dp_ref = low_32b(__dma_mem_vtop(rtv->dp));
		rsa_dec.dq_ref = low_32b(__dma_mem_vtop(rtv->dq));
		rsa_dec.tmp1_ref = low_32b(__dma_mem_vtop(rtv->tmp1));
		rsa_dec.tmp2_ref = low_32b(__dma_mem_vtop(rtv->tmp2));
		rsa_dec.trailer = (rtv->q_len << RSA_DEC3_Q_LEN_SHIFT) |
				  (rtv->p_len);
		memcpy(rtv->d_pdb, &rsa_dec, sizeof(struct rsa_dec_pdb_form3));
		rtv->d_pdb_size = sizeof(struct rsa_dec_pdb_form3);
	}

	return 0;
}

static int init_ref_test_vector_rsa(struct test_param *crypto_info)
{
	struct protocol_info *proto = crypto_info->proto;
	struct rsa_ref_vector_s *rtv = proto->proto_vector;
	struct rsa_params *rsa_params = proto->proto_params;

	rtv->e_len = rsa_ref_exponent_len[crypto_info->test_set - 1];
	rtv->f_len = rsa_ref_input_len[crypto_info->test_set - 1];
	rtv->n_len = rsa_ref_modulus_len[crypto_info->test_set - 1];
	rtv->f = __dma_mem_memalign(L1_CACHE_BYTES, rtv->n_len);
	if (!rtv->f)
		return -ENOMEM;
	rtv->g = __dma_mem_memalign(L1_CACHE_BYTES, rtv->n_len);
	if (!rtv->g)
		return -ENOMEM;

	if (init_rsa_enc_pdb(crypto_info, RSA_MODE_64B))
		goto err;
	switch (rsa_params->form) {
	case RSA_DECRYPT_FORM1:
		if (init_rsa_dec_form1_pdb(crypto_info, RSA_MODE_64B))
			goto err;
		break;
	case RSA_DECRYPT_FORM2:
		if (init_rsa_dec_form2_pdb(crypto_info, RSA_MODE_64B))
			goto err;
		break;
	case RSA_DECRYPT_FORM3:
		if (init_rsa_dec_form3_pdb(crypto_info, RSA_MODE_64B))
			goto err;
		break;
	default:
		fprintf(stderr, "Unknown RSA Decrypt Private Key form %d (should never reach here)\n",
			rsa_params->form);
		return -EINVAL;
	}

	rtv->length = rsa_ref_length[crypto_info->test_set - 1];
	rtv->plaintext = rtv->f;
	rtv->ciphertext = rtv->g;

	return 0;
err:
	fprintf(stderr, "Not enough memory\n");
	free_rsa_rtv(rtv);
	return -ENOMEM;
}

static void *create_descriptor(bool mode, void *params)
{
	struct test_param *crypto_info = (struct test_param *)params;
	struct protocol_info *proto = crypto_info->proto;
	struct rsa_ref_vector_s *rtv = proto->proto_vector;
	struct rsa_params *rsa_params = proto->proto_params;
	struct sec_descriptor_t *prehdr_desc;
	uint32_t *shared_desc = NULL;
	unsigned shared_desc_len = 0;
	int i;
	bool found = 0;

	prehdr_desc = __dma_mem_memalign(L1_CACHE_BYTES,
					 sizeof(struct sec_descriptor_t));
	if (!prehdr_desc) {
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

	rtv->protocmd.optype = OP_TYPE_UNI_PROTOCOL;
	if (ENCRYPT == mode) {
		rtv->protocmd.protid = OP_PCLID_RSAENCRYPT;
		rtv->protocmd.protinfo = OP_PCL_RSAPROT_OP_ENC_F_IN;
		cnstr_shdsc_rsa(shared_desc,
				&shared_desc_len,
/*
 * This is currently hardcoded. The application doesn't allow for
 * proper retrieval of PS.
 */
				1,
				rtv->e_pdb,
				rtv->e_pdb_size,
				&rtv->protocmd);
	} else {
		rtv->protocmd.protid = OP_PCLID_RSADECRYPT;
		switch (rsa_params->form) {
		case RSA_DECRYPT_FORM1:
			rtv->protocmd.protinfo = OP_PCL_RSAPROT_OP_DEC_ND;
			break;
		case RSA_DECRYPT_FORM2:
			rtv->protocmd.protinfo = OP_PCL_RSAPROT_OP_DEC_PQD;
			break;
		case RSA_DECRYPT_FORM3:
			rtv->protocmd.protinfo = OP_PCL_RSAPROT_OP_DEC_PQDPDQC;
			break;
		}
		cnstr_shdsc_rsa(shared_desc,
				&shared_desc_len,
/*
 * This is currently hardcoded. The application doesn't allow for
 * proper retrieval of PS.
 */
				1,
				rtv->d_pdb,
				rtv->d_pdb_size,
				&rtv->protocmd);
	}

	prehdr_desc->prehdr.hi.word = shared_desc_len & SEC_PREHDR_SDLEN_MASK;

	pr_debug("SEC %s shared descriptor:\n", proto->name);

	for (i = 0; i < shared_desc_len; i++)
		pr_debug("0x%x\n", *shared_desc++);

	return prehdr_desc;
}

static int test_enc_match_cb_rsa(int fd_ind, uint8_t *enc_buf,
				struct test_param *crypto_info)
{
	struct rsa_ref_vector_s *ref_test_vector =
				crypto_info->proto->proto_vector;

	return test_vector_match((uint32_t *)ref_test_vector->ciphertext,
			(uint32_t *)rsa_ref_result[crypto_info->test_set - 1],
			ref_test_vector->n_len * BITS_PER_BYTE);
}

static int test_dec_match_cb_rsa(int fd_ind, uint8_t *dec_buf,
			    struct test_param *crypto_info)
{
	struct rsa_ref_vector_s *ref_test_vector =
				crypto_info->proto->proto_vector;
	int position = ref_test_vector->n_len - ref_test_vector->f_len;

	memcpy(ref_test_vector->plaintext,
	       &ref_test_vector->plaintext[position],
	       rsa_ref_input_len[crypto_info->test_set - 1]);
	return test_vector_match((uint32_t *)ref_test_vector->plaintext,
		(uint32_t *)rsa_ref_input[crypto_info->test_set - 1],
		ref_test_vector->f_len * BITS_PER_BYTE);
}

/**
 * @brief	Parse RSA related command line options
 *
 */
static error_t parse_opts(int key, char *arg, struct argp_state *state)
{
	struct parse_input_t *input = state->input;
	struct test_param *crypto_info = input->crypto_info;
	struct rsa_params *rsa_params;

	/*
	 * If the protocol was not selected, then it makes no sense to go
	 * further.
	 */
	if (!crypto_info->proto)
		return 0;

	rsa_params = crypto_info->proto->proto_params;
	switch (key) {
	case 'b':
		rsa_params->form = atoi(arg);
		fprintf(stdout, "RSA Decrypt form = %d\n", rsa_params->form);
		break;

	default:
		printf("%c", key);
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

/**
 * @brief	Check SEC parameters provided by user for RSA are valid
 *		or not.
 * @param[in]	g_proto_params - Bit mask of the optional parameters provided
 *		by user
 * @param[in]	crypto_info - test parameters
 * @return	0 on success, otherwise -EINVAL value
 */
static int validate_rsa_opts(uint32_t g_proto_params,
				struct test_param *crypto_info)
{
	struct protocol_info *proto = crypto_info->proto;
	struct rsa_params *rsa_params = proto->proto_params;

	if ((rsa_params->form < 1) || (rsa_params->form > 3)) {
		fprintf(stderr, "Unknown RSA Decrypt Private Key form %d\n",
			rsa_params->form);
		return -EINVAL;
	}

	proto->test_enc_match_cb = test_enc_match_cb_rsa;
	proto->test_dec_match_cb = test_dec_match_cb_rsa;

	return 0;
}

static int get_buf_size(struct test_param *crypto_info)
{
	return 2 * crypto_info->buf_size;
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

	crypto_info->rt.output_buf_size = crypto_info->buf_size;

	return 0;
}

/**
 * @brief	Verifies if user gave a correct test set
 * @param[in]	crypto_info - test parameters
 * @return	0 on success, otherwise -EINVAL value
 */
static int validate_test_set(struct test_param *crypto_info)
{
	if ((crypto_info->test_set > 0) && (crypto_info->test_set < 3))
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
struct protocol_info *register_rsa(void)
{
	unsigned num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	struct protocol_info *proto_info = calloc(1, sizeof(*proto_info));

	if (!proto_info) {
		pr_err("failed to allocate protocol structure in %s",
		       __FILE__);
		return NULL;
	}

	SAFE_STRNCPY(proto_info->name, "RSA", sizeof(proto_info->name));
	proto_info->unregister = unregister_rsa;
	proto_info->argp_children = &argp_children;
	proto_info->init_ref_test_vector = init_ref_test_vector_rsa;
	proto_info->setup_sec_descriptor = create_descriptor;
	proto_info->get_buf_size = get_buf_size;
	proto_info->set_buf_size = set_buf_size;
	proto_info->validate_test_set = validate_test_set;
	proto_info->validate_opts = validate_rsa_opts;
	proto_info->proto_params = calloc(1, sizeof(struct rsa_params));
	if (!proto_info->proto_params) {
		pr_err("failed to allocate protocol parameters in %s",
		       __FILE__);
		goto err;
	}
	/* If decrypt form is not specified, use form 1 */
	((struct rsa_params *)proto_info->proto_params)->form = 1;
	proto_info->proto_vector =
		calloc(1, sizeof(struct rsa_ref_vector_s));
	if (!proto_info->proto_vector) {
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
	free(proto_info->proto_params);
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
void unregister_rsa(struct protocol_info *proto_info)
{
	int i;

	if (!proto_info)
		return;

	for (i = 0; i < proto_info->num_cpus * FQ_PER_CORE * 2; i++)
		if (proto_info->descr[i].descr)
			__dma_mem_free(proto_info->descr[i].descr);

	free_rsa_rtv((struct rsa_ref_vector_s *)proto_info->proto_vector);
	free(proto_info->proto_vector);
	free(proto_info->proto_params);
	free(proto_info);
}
