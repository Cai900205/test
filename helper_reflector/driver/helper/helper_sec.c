/* Copyright (c) 2014 Freescale Semiconductor, Inc.
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

#include "helper_common.h"
#include "helper_api.h"
#include "helper_sec.h"

/* diffie_hellman & RSA configuration */
#undef SEC_JOBDESC_DH
#undef SEC_JOBDESC_RSA

#define SEC_JOBDESC_LEN  (256 + 8)
#define SEC_OVERHEAD     (64)

typedef int (*sec_create_fd_func) (struct qm_fd *fd,
				   struct packet_desc *desc);
typedef int (*sec_create_pd_func) (struct packet_desc *desc,
				   struct qm_fd *fd);

/* sec tunnel list */
struct list_head sec_interface_list = {
	.prev = &sec_interface_list,
	.next = &sec_interface_list
};

bool sec_split_key_ok = false;

static enum qman_cb_dqrr_result
sec_split_key_dqrr_cb(struct qman_portal *qm,
	struct qman_fq *fq,
	const struct qm_dqrr_entry *dqrr)
{
	DEBUG("split key generated, sts 0x%x\n", dqrr->fd.status);
	sec_split_key_ok = true;
	return qman_cb_dqrr_consume;
}

static int sec_split_key_cipher_get(int algtype)
{
	int cipher;
	switch (algtype) {
	case AUTH_TYPE_IPSEC_MD5HMAC_96:
		cipher = OP_ALG_ALGSEL_MD5;
		break;
	case AUTH_TYPE_IPSEC_SHA1HMAC_96:
		cipher = OP_ALG_ALGSEL_SHA1;
		break;
	case AUTH_TYPE_IPSEC_SHA2HMAC_256:
		cipher = OP_ALG_ALGSEL_SHA256;
		break;
	case AUTH_TYPE_IPSEC_SHA2HMAC_384:
		cipher = OP_ALG_ALGSEL_SHA384;
		break;
	case AUTH_TYPE_IPSEC_SHA2HMAC_512:
		cipher = OP_ALG_ALGSEL_SHA512;
		break;
	default:
		cipher = 0;
		break;
	}

	return cipher;
}

/* generate split key, caller need free the key memory
 */
static int sec_create_split_key(struct authparams *auth)
{

	/* fqid[0] - to_sec, fqid[1] - from_sec */
	struct qman_fq *split_fqs = NULL;
	struct preheader_s *preheader = NULL;
	u8 *split_key = NULL, *alg_key, *job_desc;
	u32 cipher;
	u32 fqids[2] = { 0 };
	u16 bufsize = 512;

	struct qm_fd fd;
	struct qm_sg_entry *sg;
	struct qm_mcc_initfq opts;
	u32 flags;
	u32 ctx_a_excl;
	u32 ctx_a_len;
	int ret = 0;

	DEBUG("generate sec split key\n");

	split_fqs =
		(struct qman_fq *)DMA_MEM_ALLOC(L1_CACHE_BYTES,
						sizeof(struct qman_fq) * 2);
	preheader =
		(struct preheader_s *)DMA_MEM_ALLOC(L1_CACHE_BYTES,
						SEC_JOBDESC_LEN);
	if (split_fqs == NULL || preheader == NULL) {
		TRACE("error: dma mem allocation failed\n");
		ret = -ENOMEM;
		goto err;
	}

	memset(split_fqs, 0, sizeof(struct qman_fq) * 2);
	memset(preheader, 0, SEC_JOBDESC_LEN);

	/* this is suppose not to happen */
	ret = qman_alloc_fqid_range(fqids, 2, 0, 0);
	if (ret < 0) {
		TRACE("error: qman fqid allocation failed\n");
		ret = -1;
		goto err;
	}

	/* init the to fq, fqid[0] */
	flags = QMAN_FQ_FLAG_LOCKED | QMAN_FQ_FLAG_TO_DCPORTAL;
	ret = qman_create_fq(fqids[0], flags, &split_fqs[0]);
	if (0 != ret) {
		TRACE("qman_create_fq failed for split key to sec FQ ID\n");
		goto err;
	}

	flags = QMAN_INITFQ_FLAG_SCHED;
	opts.we_mask = QM_INITFQ_WE_DESTWQ |
		QM_INITFQ_WE_CONTEXTA | QM_INITFQ_WE_CONTEXTB;
	opts.fqd.context_a.opaque = vtop(preheader);
	opts.fqd.context_b = fqids[1];
	opts.fqd.dest.channel = qm_channel_caam;
	opts.fqd.dest.wq = 0;
	ret = qman_init_fq(&split_fqs[0], flags, &opts);
	if (0 != ret) {
		TRACE("Unable to Init CAAM Egress FQ\n");
		goto err;
	}

	/* init the from fq, fqid[1] */
	flags = QMAN_FQ_FLAG_NO_ENQUEUE | QMAN_FQ_FLAG_LOCKED;
	split_fqs[1].cb.dqrr = sec_split_key_dqrr_cb;
	ret = qman_create_fq(fqids[1], flags, &split_fqs[1]);
	if (0 != ret) {
		TRACE("qman_create_fq failed for split key from sec FQ\n");
		goto err;
	}

	flags = QMAN_INITFQ_FLAG_SCHED | QMAN_INITFQ_FLAG_LOCAL;
	opts.we_mask = QM_INITFQ_WE_DESTWQ |
		QM_INITFQ_WE_CONTEXTA | QM_INITFQ_WE_FQCTRL;
	opts.fqd.dest.wq = 1;
	opts.fqd.fq_ctrl = QM_FQCTRL_CTXASTASHING | QM_FQCTRL_HOLDACTIVE;
	ctx_a_excl = (QM_STASHING_EXCL_DATA | QM_STASHING_EXCL_CTX);
	ctx_a_len = (1 << 2) | 1;
	opts.fqd.context_a.hi = (ctx_a_excl << 24) | (ctx_a_len << 16);
	ret = qman_init_fq(&split_fqs[1], flags, &opts);
	if (0 != ret) {
		TRACE("Unable to initialize ingress FQ for SEC4.0\n");
		goto err;
	}

	/* construct split key input frame */
	split_key = (u8 *) DMA_MEM_ALLOC(L1_CACHE_BYTES, bufsize);
	if (split_key == NULL) {
		TRACE("error: dma mem allocation failed\n");
		ret = -ENOMEM;
		goto err;
	}
	memset(split_key, 0, bufsize);

	/* split-key (128) + JOB_DESC (256) + key (64) + SG (64) */
	job_desc = &split_key[128]; /* 256B */
	alg_key = &split_key[384];  /* 64B */
	sg = (struct qm_sg_entry *)&split_key[448]; /* 64B */
	memcpy(alg_key, auth->key, auth->keylen);

	/* Create Job Desc */
	bufsize = 256;
	cipher = sec_split_key_cipher_get(auth->algtype);
	ret = cnstr_jobdesc_mdsplitkey((u32 *) job_desc, &bufsize,
					   alg_key, cipher, split_key);
	if (0 != ret)
		goto err;
	DBPKT("mds job desc", job_desc, bufsize * 4);

	/* output buffer */
	sg[0].addr = vtop(split_key);
	sg[0].offset = 0;
	sg[0].length = 128;
	sg[0].bpid = 0;
	sg[0].extension = 0;
	sg[0].final = 0;

	/* input buffer */
	sg[1].addr = vtop(job_desc);
	sg[1].length = bufsize * 4;
	sg[1].offset = 0;
	sg[1].bpid = 0;
	sg[1].extension = 0;
	sg[1].final = 1;

	fd.addr = vtop(sg);
	fd.bpid = 0;
	fd._format1 = qm_fd_compound;
	fd.cong_weight = 0;
	fd.cmd = 0;

	/* Enqueue on the to_sec split FQ */
	sec_split_key_ok = false;
	qman_enqueue(&split_fqs[0], &fd, 0);
	while (!sec_split_key_ok)
		qman_poll();

	auth->key = split_key;
	auth->keylen = sec_split_key_size_get(auth->algtype);
	DBPKT("split key", auth->key, auth->keylen);

	/* clean up the fqs & fqids */
	fsl_fq_clean(&split_fqs[0]);
	fsl_fq_clean(&split_fqs[1]);
	qman_release_fqid_range(fqids[0], 2);

err:
	DMA_MEM_FREE(split_fqs);
	DMA_MEM_FREE(preheader);

	return 0;
}

/* ask SEC to allocate buffer from bman */
static int sec_create_fd_simple(struct qm_fd *fd, struct packet_desc *desc)
{
	u32 offset = desc->payload_addr - desc->pkt_buffer_addr;
	u32 length = desc->payload_length;
	int bpid = desc->pool;

	/* compound fd -> & sg[0] */
	fd->addr = vtop(desc->pkt_buffer_addr);
	fd->bpid = bpid;
	fd->format = qm_fd_contig;
	fd->offset = offset;
	fd->length20 = length;
	fd->cmd = 0;

	return 0;
}

/* allocate a new buffer as the output for compound fd */
static int sec_create_fd_comp_type1(struct qm_fd *comp_fd,
					struct packet_desc *desc)
{
	struct qm_sg_entry *sg;
	struct bm_buffer buf;
	u64 addr = vtop(desc->pkt_buffer_addr);
	u32 offset = desc->payload_addr - desc->pkt_buffer_addr;
	u32 length = desc->payload_length;
	int bpid = HLP_PKT_SEC_BPID;
	int ret;

	ret = bman_acquire(get_bman_pool(bpid), &buf, 1, 0);
	if (ret < 0) {
		TRACE("allocate buffer from pool %d failed\n", bpid);
		return ret;
	}

	/* sg structure is at the beginning of buffer */
	sg = (struct qm_sg_entry *)ptov(buf.addr);
	memset(sg, 0, sizeof(struct qm_sg_entry) * 2);

	/* output buffer -> new allocated */
	sg[0].addr = buf.addr;
	sg[0].offset = offset;
	sg[0].length = length + SEC_OVERHEAD;
	sg[0].bpid = bpid;
	sg[0].extension = 0;
	sg[0].final = 0;

	/* input buffer -> original fd buffer */
	sg[1].addr = addr;
	sg[1].length = length;
	sg[1].offset = offset;
	sg[1].bpid = desc->pool;
	sg[1].extension = 0;
	sg[1].final = 1;

	/* compound fd -> & sg[0] */
	comp_fd->addr = buf.addr;
	comp_fd->bpid = bpid;
	comp_fd->_format1 = qm_fd_compound;
	comp_fd->cong_weight = 0;
	comp_fd->cmd = 0;
	comp_fd->offset = 0;

	return 0;
}

/* the input and output share same buffer, need ensure
 * enough annotation space
 */
static int sec_create_fd_comp_type2(struct qm_fd *comp_fd,
					struct packet_desc *desc)
{
	struct qm_sg_entry *sg;
	u64 addr = vtop(desc->pkt_buffer_addr);
	u32 offset = desc->payload_addr - desc->pkt_buffer_addr;
	u32 length = desc->payload_length;
	int bpid = desc->pool;
	int extra_spaces;

	extra_spaces = SEC_OVERHEAD + sizeof(struct qm_sg_entry) * 2;
	if (offset < extra_spaces) {
		TRACE("no enough spaces for compound frame type 2, offset %d\n",
			  offset);
		return -1;
	}

	/* sg structure is at the beginning of buffer */
	sg = (struct qm_sg_entry *)desc->pkt_buffer_addr;
	memset(sg, 0, sizeof(struct qm_sg_entry) * 2);

	/* output buffer -> right after the sg entry */
	sg[0].addr = addr;
	sg[0].offset = sizeof(struct qm_sg_entry) * 2;
	sg[0].length = length + SEC_OVERHEAD;
	sg[0].bpid = bpid;
	sg[0].extension = 0;
	sg[0].final = 0;

	/* input buffer -> original fd buffer */
	sg[1].addr = addr;
	sg[1].length = length;
	sg[1].offset = offset;
	sg[1].bpid = bpid;
	sg[1].extension = 0;
	sg[1].final = 1;

	/* compound fd -> & sg[0] */
	comp_fd->addr = addr;
	comp_fd->bpid = bpid;
	comp_fd->_format1 = qm_fd_compound;
	comp_fd->cong_weight = 0;
	comp_fd->cmd = 0;
	comp_fd->offset = 0;

	return 0;
}

static int sec_create_pd_simple(struct packet_desc *desc, struct qm_fd *fd)
{
	/* generate the packet desc from the output */
	desc->pkt_buffer_addr = (u32) ptov(fd->addr);
	desc->payload_addr = desc->pkt_buffer_addr + fd->offset;
	desc->payload_length = fd->length20;
	desc->pool = fd->bpid;
	return 0;
}

/* release the input buffer to bman */
static int sec_create_pd_comp_type1(struct packet_desc *desc,
					struct qm_fd *comp_fd)
{
	struct qm_sg_entry *sg;
	struct bm_buffer buf;

	sg = (struct qm_sg_entry *)ptov(comp_fd->addr);

	/* generate the packet descriptor from the output */
	desc->pkt_buffer_addr = (u32) ptov(sg[0].addr);
	desc->payload_addr = desc->pkt_buffer_addr + sg[0].offset;
	desc->payload_length = sg[0].length;
	desc->pool = sg[0].bpid;

	/* release the input buffer */
	DBPKT("compound sg[1]", ptov(sg[1].addr), sg[1].offset + 512);
	buf.addr = sg[1].addr;
	buf.bpid = sg[1].bpid;
	fsl_free_bman_buffer(get_bman_pool(buf.bpid), &buf, 1, 0);
	return 0;
}

static int sec_create_pd_comp_type2(struct packet_desc *desc,
					struct qm_fd *comp_fd)
{
	struct qm_sg_entry *sg;

	/* generate the packet desc from the output */
	sg = (struct qm_sg_entry *)ptov(comp_fd->addr);
	desc->pkt_buffer_addr = (u32) ptov(sg[0].addr);
	desc->payload_addr = desc->pkt_buffer_addr + sg[0].offset;
	desc->payload_length = sg[0].length;
	desc->pool = sg[0].bpid;
	return 0;
}

static void *sec_create_shdsc_ipsec(struct sec_ipsec_param *param)
{
	struct sec_descriptor_t *shared_desc = NULL;
	u16 shared_desc_len = 0;
	int ret = -1;

	DEBUG("generate ipsec shared descriptor\n");

	shared_desc = (struct sec_descriptor_t *)
		DMA_MEM_ALLOC(L1_CACHE_BYTES, SEC_JOBDESC_LEN);
	if (!shared_desc)
		return NULL;

	memset(shared_desc, 0, SEC_JOBDESC_LEN);

	/* generate split key */
	ret = sec_create_split_key(&param->auth);
	if (ret != 0) {
		TRACE("split key create failure\n");
		return NULL;
	}

	/* convert key length to bit based */
	param->auth.keylen *= 8;
	param->cipher.keylen *= 8;

	/* generate shared descriptor */
	if (param->encryption == sec_enc) {
		ret = cnstr_shdsc_ipsec_encap((u32 *)&shared_desc->descbuf,
						  &shared_desc_len,
						  &param->pdb_enc,
						  (u8 *) param->pdb_enc.ip_hdr,
						  &param->cipher, &param->auth);
	} else if (param->encryption == sec_dec) {
		ret = cnstr_shdsc_ipsec_decap((u32 *)&shared_desc->descbuf,
						  &shared_desc_len,
						  &param->pdb_dec,
						  &param->cipher, &param->auth);
	}

	/* free the split key buffer once descriptor generated */
	DMA_MEM_FREE(param->auth.key);
	if (0 > ret) {
		TRACE("construct shared desc failed, 0x%x\n", ret);
		DMA_MEM_FREE(shared_desc);
		return NULL;
	}

	/* replace share desc length, hardcode the output buffer offset */
	shared_desc->prehdr.hi.field.idlen = shared_desc_len;
	shared_desc->prehdr.lo.field.offset = 1;    /* 64B */
	DBPKT("shared desc", (u8 *)&shared_desc->descbuf,
		  shared_desc_len * 4);

	return shared_desc;
}

static void *sec_create_shdsc_snow(struct sec_snow_param *param)
{
	struct sec_descriptor_t *shared_desc = NULL;
	u16 shared_desc_len = 0;
	int ret = -1;

	DEBUG("generate snow shared descriptor\n");

	shared_desc = (struct sec_descriptor_t *)
		DMA_MEM_ALLOC(L1_CACHE_BYTES, SEC_JOBDESC_LEN);
	if (!shared_desc)
		return NULL;

	memset(shared_desc, 0, SEC_JOBDESC_LEN);

	param->key_len *= 8;
	if (param->type == sec_snow_f8) {
		ret = cnstr_shdsc_snow_f8((u32 *) shared_desc,
					  &shared_desc_len,
					  param->key,
					  param->key_len,
					  param->encryption,
					  param->count,
					  param->bearer, param->direction, 1);

	} else if (param->type == sec_snow_f9) {
		ret = cnstr_shdsc_snow_f9((u32 *) shared_desc,
					  &shared_desc_len,
					  param->key,
					  param->key_len,
					  param->encryption,
					  param->count,
					  param->fresh,
					  param->direction, 0, 254);
	}
	if (0 > ret) {
		TRACE("construct shared desc failed, 0x%x\n", ret);
		DMA_MEM_FREE(shared_desc);
		return NULL;
	}

	/* replace share desc length, hardcode the output buffer offset */
	shared_desc->prehdr.hi.field.idlen = shared_desc_len;
	shared_desc->prehdr.lo.field.offset = 1;    /* 64B */
	DBPKT("shared desc", (u8 *)&shared_desc->descbuf,
		  shared_desc_len * 4);

	return shared_desc;
}

static void *sec_create_shdsc_kasumi(struct sec_kasumi_param *param)
{
	struct sec_descriptor_t *shared_desc = NULL;
	u16 shared_desc_len = 0;
	int ret = -1;

	DEBUG("generate snow shared descriptor\n");

	shared_desc = (struct sec_descriptor_t *)
		DMA_MEM_ALLOC(L1_CACHE_BYTES, SEC_JOBDESC_LEN);
	if (!shared_desc)
		return NULL;

	memset(shared_desc, 0, SEC_JOBDESC_LEN);

	param->key_len *= 8;
	if (param->type == sec_kasumi_f8) {
		ret = cnstr_shdsc_kasumi_f8((u32 *) shared_desc,
						&shared_desc_len,
						param->key,
						param->key_len,
						param->encryption,
						param->count,
						param->bearer,
						param->direction, 1);
	} else if (param->type == sec_kasumi_f9) {
		ret = cnstr_shdsc_kasumi_f9((u32 *) shared_desc,
						&shared_desc_len,
						param->key,
						param->key_len,
						param->encryption,
						param->count,
						param->fresh,
						param->direction, 0, 254);
	}

	if (0 > ret) {
		TRACE("construct shared desc failed, 0x%x\n", ret);
		DMA_MEM_FREE(shared_desc);
		return NULL;
	}

	/* replace share desc length, hardcode the output buffer offset */
	shared_desc->prehdr.hi.field.idlen = shared_desc_len;
	shared_desc->prehdr.lo.field.offset = 1;    /* 64B */
	DBPKT("shared desc", (u8 *)&shared_desc->descbuf,
		  shared_desc_len * 4);

	return shared_desc;
}

static void *sec_create_shdsc_null(void)
{
	struct sec_descriptor_t *shared_desc = NULL;

	DEBUG("generate header for job descriptor\n");
	shared_desc = (struct sec_descriptor_t *)
		DMA_MEM_ALLOC(L1_CACHE_BYTES, SEC_JOBDESC_LEN);
	if (!shared_desc)
		return NULL;

	memset(shared_desc, 0, SEC_JOBDESC_LEN);

	return shared_desc;
}

static int sec_create_jobdsc_rsa(struct sec_rsa_param *param, struct qm_fd *fd)
{
#ifndef SEC_JOBDESC_RSA
	return 0;
#else
	struct qm_sg_entry *sg = (struct qm_sg_entry *)ptov(fd->addr);
	u8 *src = (u8 *) ptov(sg[1].addr + sg[1].offset);
	u8 *dst = (u8 *) ptov(sg[0].addr + sg[0].offset);
	u8 *descbuf = (u8 *) ptov(sg[1].addr);
	u16 size;
	int ret = 0;

	DBG_MEMSET(src, 0, 1024);
	DBG_MEMSET(dst, 0, 1024);

	/* job descriptor and its length shall be set
	 * to the input of SG structure
	 */
	if (param->encryption) {
		if (param->enc.g == NULL)
			param->enc.g = dst;
		if (param->enc.f == NULL) {
			param->enc.f = src;
			param->enc.f_len = sg[1].length;
		}
		ret = cnstr_jobdesc_rsa_enc((u32 *) descbuf, &size,
					param->enc.n_len,
					param->enc.e_len,
					param->enc.f_len,
					vtop(param->enc.n),
					vtop(param->enc.e),
					vtop(param->enc.f),
					vtop(param->enc.g));
	} else {
		if (param->dec.g == NULL)
			param->dec.g = src;
		if (param->dec.f == NULL) {
			param->dec.f_len = (u32 *) dst;
			param->dec.f = dst + 0x10;
		}

		if (param->dec.ftype == sec_rsa_dec_form_type1) {
			ret = cnstr_jobdesc_rsa_dec_f1((u32 *) descbuf, &size,
					   param->dec.f1.n_len,
					   param->dec.f1.d_len,
					   vtop(param->dec.g),
					   vtop(param->dec.f),
					   vtop(param->dec.f1.n),
					   vtop(param->dec.f1.d),
					   vtop(param->dec.f_len));
		} else if (param->dec.ftype == sec_rsa_dec_form_type2) {
			ret = cnstr_jobdesc_rsa_dec_f2((u32 *) descbuf, &size,
					   param->dec.f2.n_len,
					   param->dec.f2.d_len,
					   param->dec.f2.p_len,
					   param->dec.f2.q_len,
					   vtop(param->dec.g),
					   vtop(param->dec.f),
					   vtop(param->dec.f2.d),
					   vtop(param->dec.f2.p),
					   vtop(param->dec.f2.q),
					   vtop(param->dec.f2.t1),
					   vtop(param->dec.f2.t2),
					   vtop(param->dec.f_len));
		} else if (param->dec.ftype == sec_rsa_dec_form_type3) {
			ret = cnstr_jobdesc_rsa_dec_f3((u32 *) descbuf, &size,
					   param->dec.f3.n_len,
					   param->dec.f3.p_len,
					   param->dec.f3.q_len,
					   vtop(param->dec.f3.p),
					   vtop(param->dec.f3.q),
					   vtop(param->dec.f3.dp),
					   vtop(param->dec.f3.dq),
					   vtop(param->dec.f3.c),
					   vtop(param->dec.f3.t1),
					   vtop(param->dec.f3.t2),
					   vtop(param->dec.g),
					   vtop(param->dec.f),
					   vtop(param->dec.f_len));
		}
	}

	/* job descriptor buffer starts from sg[1] offset 0 */
	/* sg[1].addr = vtop(descbuf); */
	sg[1].offset = 0;
	sg[1].length = size * 4;

	DBPKT("RSA job descriptor", descbuf, sg[1].length);
	DBPKT("RSA vectors", param->vector, 1024);

	return ret;
#endif
}

static int sec_create_jobdsc_dh(struct sec_dh_param *param, struct qm_fd *fd)
{
#ifndef SEC_JOBDESC_DH
	return 0;
#else
	struct qm_sg_entry *sg = (struct qm_sg_entry *)ptov(fd->addr);
	u8 *src = (u8 *) ptov(sg[1].addr + sg[1].offset);
	u8 *dst = (u8 *) ptov(sg[0].addr + sg[0].offset);
	u8 *descbuf = (u8 *) ptov(sg[1].addr);
	u16 size;
	int ret = 0;

	DBG_MEMSET(src, 0, 1024);
	DBG_MEMSET(dst, 0, 1024);

	/* the output buffer */
	param->z = dst;

	/* the public & private key if it's not configured */
	if (param->w == 0 || param->s == 0) {
		param->w = src;
		param->s = src + param->l_len;
	}

	ret = cnstr_jobdesc_dh((u32 *) descbuf, &size,
				   param->n_len,
				   param->l_len,
				   vtop(param->q),
				   vtop(param->r),
				   vtop(param->w),
				   vtop(param->s),
				   vtop(param->z),
				   (param->ab ? vtop(param->ab) : 0),
				   param->protoinfo);

	/* job descriptor buffer starts from sg[1] offset 0 */
	/* sg[1].addr = vtop(descbuf); */
	sg[1].offset = 0;
	sg[1].length = size * 4;

	DBPKT("DH job descriptor", descbuf, sg[1].length);
	DBPKT("DH vectors", param->vector, 1024);

	return ret;
#endif
}

int sec_send(u32 handle, struct packet_desc *desc)
{
	static sec_create_fd_func create_fd_func_array[] = {
		sec_create_fd_simple,
		sec_create_fd_comp_type1,
		sec_create_fd_comp_type2
	};

	struct sec_interface *sif = (struct sec_interface *)handle;
	struct qm_fd new_fd;
	int flags = 0;
	int ret = -1;

	ret = create_fd_func_array[sif->buffer_type] (&new_fd, desc);
	if (ret < 0)
		return ret;

	/* add logic for job descriptor, per-packet handling */
	if (sif->alg_type == sec_type_rsa) {
		if (sif->buffer_type == sec_buffer_comp_type1)
			ret = sec_create_jobdsc_rsa(&sif->rsa, &new_fd);
	} else if (sif->alg_type == sec_type_diffie_hellman) {
		if (sif->buffer_type == sec_buffer_comp_type1)
			ret = sec_create_jobdsc_dh(&sif->dh, &new_fd);
	}
#if defined(FSL_FQ_HOLDACTIVE_ENABLE) || defined(FSL_FQ_ORP_ENABLE)
	{
		struct packet_desc_private *pdesc_priv =
			(struct packet_desc_private *)desc->priv;

		if (pdesc_priv->fq_type & FQ_TYPE_ORP) {
			ret = fsl_send_frame_orp(&sif->fq_to_sec,
				&new_fd, flags,
				(struct qman_fq *)pdesc_priv->fq,
				pdesc_priv->seqnum);
		} else if (pdesc_priv->fq_type & FQ_TYPE_HOLDACTIVE) {
			flags = QMAN_ENQUEUE_FLAG_DCA |
				QMAN_ENQUEUE_FLAG_DCA_PTR(pdesc_priv->dqrr);
			ret = fsl_send_frame(&sif->fq_to_sec, &new_fd, flags);
		} else
			ret = fsl_send_frame(&sif->fq_to_sec, &new_fd, flags);
	}
#else
	ret = fsl_send_frame(&sif->fq_to_sec, &new_fd, flags);
#endif

	/* release the PD when sending packet without error */
	if (ret == 0)
		fsl_packet_desc_free(desc, HLP_PKT_DESC_BPID);

	return ret;
}

static enum qman_cb_dqrr_result sec_recv(struct qman_portal *qm,
					 struct qman_fq *fq,
					 const struct qm_dqrr_entry *dqrr)
{
	static sec_create_pd_func create_pd_func_array[] = {
		sec_create_pd_simple,
		sec_create_pd_comp_type1,
		sec_create_pd_comp_type2
	};

	struct sec_interface *sif =
		container_of(fq, struct sec_interface, fq_from_sec);
	struct qm_fd *fd = (struct qm_fd *)&dqrr->fd;
	struct packet_desc *desc;
	int ret;

	DEBUG("fqid 0x%x, status 0x%x, addr_lo 0x%x, fmt %d\n",
		  fq->fqid, dqrr->fd.status, dqrr->fd.addr_lo, dqrr->fd.format);

	if (dqrr->fd.status != 0) {
		TRACE("SEC error, fqid 0x%x, status 0x%x\n",
			  fq->fqid, dqrr->fd.status);
		fsl_drop_frame(&dqrr->fd);
		return qman_cb_dqrr_consume;
	}

	/* fill in packet descriptor for the encypted packet */
	desc = (struct packet_desc *)
		fsl_packet_desc_alloc(ptov(dqrr->fd.addr), 0);
	if (desc == NULL) {
		fsl_drop_frame(&dqrr->fd);
		return qman_cb_dqrr_consume;
	}

	ret = create_pd_func_array[sif->buffer_type] (desc, fd);
	desc->port = 0;
	desc->queue = 0;
	desc->priv[0] = 0;

	/* callback to inform the SEC output coming */
	ret = sif->handler_from_sec((u32) sif, desc);
	if (ret < 0)
		fsl_pkt_drop(desc);

	return qman_cb_dqrr_consume;
}

/* general SEC in-out operation */
static int sec_interface_fq_init(struct sec_interface *sif,
				 u32 fqid_to_sec, u32 fqid_from_sec,
				 u16 channel)
{
	int ret;
	u32 flags;
	u32 ctx_a_excl;
	u32 ctx_a_len;
	u64 addr;
	struct qm_mcc_initfq opts;

	TRACE("init sec fqs\n");

	/* init to-sec-fq */
	flags = QMAN_FQ_FLAG_TO_DCPORTAL | QMAN_FQ_FLAG_LOCKED;
	ret = qman_create_fq(fqid_to_sec, flags, &sif->fq_to_sec);
	if (0 != ret) {
		TRACE("create fqid 0x%x failed, 0x%x\n", fqid_to_sec, ret);
		return ret;
	}

	flags = QMAN_INITFQ_FLAG_SCHED;
	addr = vtop(sif->desc);
	opts.we_mask = QM_INITFQ_WE_DESTWQ | QM_INITFQ_WE_CONTEXTA |
		QM_INITFQ_WE_CONTEXTB;
	opts.fqd.context_a.hi = (u32) (addr >> 32);
	opts.fqd.context_a.lo = (u32) (addr);
	opts.fqd.context_b = fqid_from_sec;
	opts.fqd.dest.channel = qm_channel_caam;
	opts.fqd.dest.wq = 0;
	ret = qman_init_fq(&sif->fq_to_sec, flags, &opts);
	if (0 != ret) {
		TRACE("init fqid 0x%x failed, 0x%x\n", fqid_to_sec, ret);
		return ret;
	}

	/* init from-sec-fq */
	flags = QMAN_FQ_FLAG_NO_ENQUEUE | QMAN_FQ_FLAG_LOCKED;
	ret = qman_create_fq(fqid_from_sec, flags, &sif->fq_from_sec);
	if (0 != ret) {
		TRACE("create fqid 0x%x failed, 0x%x\n", fqid_from_sec, ret);
		return ret;
	}

	sif->fq_from_sec.cb.dqrr = sec_recv;
	flags = QMAN_INITFQ_FLAG_SCHED;
	opts.we_mask = QM_INITFQ_WE_DESTWQ |
		QM_INITFQ_WE_CONTEXTA | QM_INITFQ_WE_FQCTRL;
	opts.fqd.fq_ctrl = QM_FQCTRL_CTXASTASHING | QM_FQCTRL_HOLDACTIVE;
	ctx_a_excl = (QM_STASHING_EXCL_DATA | QM_STASHING_EXCL_CTX);
	ctx_a_len = (1 << 2) | 1;
	opts.fqd.context_a.hi = (ctx_a_excl << 24) | (ctx_a_len << 16);
	opts.fqd.dest.channel = channel;
	opts.fqd.dest.wq = 1;
	ret = qman_init_fq(&sif->fq_from_sec, flags, &opts);
	if (0 != ret) {
		TRACE("init fqid 0x%x failed, 0x%x\n", fqid_from_sec, ret);
		return ret;
	}

	return 0;
}

static void sec_interface_internal_free(struct sec_interface *sif)
{
	if (sif->alg_type == sec_type_rsa)
		DMA_MEM_FREE(sif->rsa.vector);
	else if (sif->alg_type == sec_type_diffie_hellman)
		DMA_MEM_FREE(sif->dh.vector);
}

/* init sec interface to create those related FQs, descriptors, and
 * split key if have, a handler is returned to caller to get access
 * on the tunnel.
 * the init procedure can only be done on single core.
 */
int sec_interface_init(struct sec_interface_param *param)
{
	int ret;
	struct sec_interface *sif;

	DEBUG("init sec interface\n");

	sif = (struct sec_interface *)
		DMA_MEM_ALLOC(L1_CACHE_BYTES, sizeof(*sif));
	if (sif == NULL) {
		TRACE("no DMA memory available\n");
		return -ENOMEM;
	}
	memset(sif, 0, sizeof(*sif));

	/* generate shared descriptor */
	if (param->type == sec_type_ipsec) {
		memcpy(&sif->ipsec, &param->ipsec,
			   sizeof(struct sec_ipsec_param));
		sif->desc = sec_create_shdsc_ipsec(&param->ipsec);
	} else if (param->type == sec_type_kasumi) {
		memcpy(&sif->kasumi, &param->kasumi,
			   sizeof(struct sec_kasumi_param));
		sif->desc = sec_create_shdsc_kasumi(&param->kasumi);
	} else if (param->type == sec_type_snow) {
		memcpy(&sif->snow, &param->snow, sizeof(struct sec_snow_param));
		sif->desc = sec_create_shdsc_snow(&param->snow);
	} else if (param->type == sec_type_rsa) {
		/* create the header only for job descriptor */
		sif->desc = sec_create_shdsc_null();
		memcpy(&sif->rsa, &param->rsa, sizeof(struct sec_rsa_param));
	} else if (param->type == sec_type_diffie_hellman) {
		sif->desc = sec_create_shdsc_null();
		memcpy(&sif->dh, &param->dh, sizeof(struct sec_dh_param));
	}

	/* configure the buffer pool used by SEC */
	if (sif->desc) {
		sif->desc->prehdr.lo.field.pool_id = param->pool_id;
		sif->desc->prehdr.lo.field.pool_buffer_size =
			param->pool_buffer_size;
	}
	DBPKT("shared desc header", (u8 *) sif->desc, 8);

	/* init 2 sec fqs */
	sif->alg_type = param->type;
	sif->buffer_type = param->buffer_type;
	sif->handler_from_sec = param->handler_from_sec;
	ret = sec_interface_fq_init(sif, param->fqid_to_sec,
		param->fqid_from_sec, param->channel);
	if (ret < 0) {
		DMA_MEM_FREE(sif->desc);
		DMA_MEM_FREE(sif);
		return ret;
	}

	/* link to sec interface list */
	list_add(&sif->node, &sec_interface_list);

	/* assign the interface as the handle */
	param->handle = (u32) sif;

	return 0;
}

void sec_interface_clean(void)
{
	struct sec_interface *sif, *tmp;

	DEBUG("clean up sec interfaces\n");

	/* 1. free 2 sec fqs
	 * 2. free the descriptor space
	 * 3. free the list node
	 */
	list_for_each_entry_safe(sif, tmp, &sec_interface_list, node) {
		/* delink the node */
		list_del(&sif->node);

		/* free the FQ related */
		fsl_fq_clean(&sif->fq_from_sec);
		fsl_fq_clean(&sif->fq_to_sec);

		sec_interface_internal_free(sif);
		DMA_MEM_FREE(sif->desc);
		DMA_MEM_FREE(sif);
	}
}
