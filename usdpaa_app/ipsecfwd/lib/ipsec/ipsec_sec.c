/*
 * Copyright (C) 2011 - 2012 Freescale Semiconductor, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <usdpaa/dma_mem.h>
#include <flib/desc.h>
#include <flib/protoshared.h>
#include "ipsec_sec.h"
#include "app_common.h"

/**
 * @details  IPSec ESP encapsulation protocol-level shared descriptor.
 *           Requires an MDHA split key.
 * @ingroup sharedesc_group
 *
 * @param[in,out] descbuf    Pointer to buffer used for descriptor construction
 * @param[in,out] bufsize    Pointer to descriptor size to be written back upon
 *      completion
 * @param [in] ps            If 36/40bit addressing is desired, this parameter
 *      must be non-zero.
 * @param[in] pdb         Pointer to the PDB to be used with this descriptor.
 *      This structure will be copied inline to the descriptor under
 *      construction. No error checking will be made. Refer to the
 *      block guide for a details of the encapsulation PDB.
 * @param[in] cipherdata  Pointer to block cipher transform definitions. Valid
 *      algorithm values: one of OP_PCL_IPSEC_*
 * @param[in] authdata    Pointer to authentication transform definitions. Note
 *      that since a split key is to be used, the size of the split key itself
 *      is specified. Valid algorithm values: one of OP_PCL_IPSEC_*
 **/
static inline void cnstr_shdsc_ipsec_encap_hb(uint32_t *descbuf,
					   unsigned *bufsize,
					   unsigned short ps,
					   struct ipsec_encap_pdb *pdb,
					   struct alginfo *cipherdata,
					   struct alginfo *authdata)
{
	struct program prg;
	struct program *program = &prg;

	LABEL(keyjmp);
	REFERENCE(pkeyjmp);
	LABEL(hdr);
	REFERENCE(phdr);

	PROGRAM_CNTXT_INIT(descbuf, 0);
	if (ps)
		PROGRAM_SET_36BIT_ADDR();
	phdr = SHR_HDR(SHR_WAIT, hdr, 0);
	ENDIAN_DATA((uint8_t *)pdb,
		    sizeof(struct ipsec_encap_pdb) + pdb->ip_hdr_len);
	SET_LABEL(hdr);
	pkeyjmp = JUMP(IMM(keyjmp), LOCAL_JUMP, ALL_TRUE, BOTH|SHRD);
	KEY(MDHA_SPLIT_KEY, authdata->key_enc_flags, PTR(authdata->key),
	    authdata->keylen, IMMED);
	KEY(KEY1, cipherdata->key_enc_flags, PTR(cipherdata->key),
	    cipherdata->keylen, IMMED);
	SET_LABEL(keyjmp);
	PROTOCOL(OP_TYPE_ENCAP_PROTOCOL,
		 OP_PCLID_IPSEC,
		 cipherdata->algtype | authdata->algtype);
	PATCH_JUMP(pkeyjmp, keyjmp);
	PATCH_HDR(phdr, hdr);
	*bufsize = PROGRAM_FINALIZE();
}



/**
 \brief Initializes the SEC40 descriptor
	for encapsulation with preheader and Initialization descriptor
 \param[in] sa Pointer to the SA Info structure
 \param[in] ip_header Pointer to the IP Header
 \param[in] next_header
 \param[in] ealg Encryption Algo Info
 \param[in] aalg Authentication Algo Info
 \return Pointer to the IPSec Encap SEC40 descriptor structure
 */
void *create_encapsulation_sec_descriptor(struct ipsec_tunnel_t *sa,
					  struct iphdr *ip_header,
					  uint8_t next_header)
{
	struct ipsec_encap_descriptor_t *preheader_initdesc;
	unsigned desc_len;
	struct ipsec_encap_pdb *pdb;
	struct alginfo cipher;
	struct alginfo auth;
	unsigned char *buff_start = NULL;

	preheader_initdesc = __dma_mem_memalign(L1_CACHE_BYTES,
				sizeof(struct ipsec_encap_descriptor_t));
	if (preheader_initdesc == NULL) {
		fprintf(stderr, "error: %s: No More Buffers left\n", __func__);
		return NULL;
	}
	memset(preheader_initdesc, 0, sizeof(struct ipsec_encap_descriptor_t));

	buff_start = (unsigned char *)&preheader_initdesc->descbuf;

	pdb = (struct ipsec_encap_pdb *)malloc(sizeof(struct ipsec_encap_pdb) +
					       sizeof(struct iphdr));
	if (pdb == NULL) {
		fprintf(stderr, "error: %s: malloc for ipsec_encap_pdb failed\n",
			__func__);
		return NULL;
	}
	memset(pdb, 0, sizeof(struct ipsec_encap_pdb) + sizeof(struct iphdr));
	pdb->ip_nh = next_header;
	if (sa->is_esn)
		pdb->seq_num_ext_hi = (uint32_t)(sa->seq_num >> 32);
	pdb->seq_num = (uint32_t)(sa->seq_num);
	pdb->spi = sa->spi;
	pdb->ip_hdr_len = sizeof(struct iphdr);
	pdb->options = PDBOPTS_ESP_TUNNEL | PDBOPTS_ESP_INCIPHDR |
		       PDBOPTS_ESP_IPHDRSRC | PDBOPTS_ESP_IVSRC |
		       PDBOPTS_ESP_UPDATE_CSUM;
	if (sa->is_esn)
		pdb->options |= PDBOPTS_ESP_ESN;
	pdb->hmo = PDBHMO_ESP_ENCAP_DTTL;
	memcpy(pdb->ip_hdr, ip_header, pdb->ip_hdr_len);

	cipher.algtype = sa->ealg->alg_type;
	cipher.key = (uintptr_t)sa->ealg->alg_key;
	cipher.keylen = sa->ealg->alg_key_len;
	cipher.key_enc_flags = 0;

	auth.algtype = sa->aalg->alg_type;
	auth.key = (uintptr_t)sa->aalg->alg_key_ptr;
	auth.keylen = sa->aalg->alg_key_len;
	auth.key_enc_flags = ENC;

	/* Now construct */
/*
 * Pointer Size parameter is currently hardcoded.
 * The application doesn't allow for proper retrieval of PS.
 */
	if (sa->hb_tunnel) {
		cnstr_shdsc_ipsec_encap_hb((uint32_t *)buff_start, &desc_len,
					   1, pdb, &cipher, &auth);
	} else {
		cnstr_shdsc_ipsec_encap((uint32_t *)buff_start, &desc_len,
					1, pdb, &cipher, &auth);
	}

	free(pdb);

	pr_debug("Desc len in %s is %x\n", __func__, desc_len);

	preheader_initdesc->prehdr.hi.field.idlen = desc_len;
	preheader_initdesc->prehdr.lo.field.offset = 1;

	preheader_initdesc->prehdr.lo.field.pool_id = sec_bpid;
	preheader_initdesc->prehdr.lo.field.pool_buffer_size =
		(uint16_t)(DMA_MEM_BP3_SIZE);

	return preheader_initdesc;
}

/**
 * @details IPSec ESP decapsulation protocol-level sharedesc
 *          Requires an MDHA split key.
 * @ingroup sharedesc_group
 *
 * @param[in,out] descbuf    Pointer to buffer used for descriptor construction
 * @param[in,out] bufsize    Pointer to descriptor size to be written back upon
 *      completion
 * @param [in] ps            If 36/40bit addressing is desired, this parameter
 *      must be non-zero.
 * @param[in] pdb         Pointer to the PDB to be used with this descriptor.
 *      This structure will be copied inline to the descriptor under
 *      construction. No error checking will be made. Refer to the
 *      block guide for details about the decapsulation PDB.
 * @param[in] cipherdata  Pointer to block cipher transform definitions. Valid
 *      algorithm values: one of OP_PCL_IPSEC_*
 * @param[in] authdata    Pointer to authentication transform definitions. Note
 *      that since a split key is to be used, the size of the split key itself
 *      is specified. Valid algorithm values: one of OP_PCL_IPSEC_*
 **/
static inline void cnstr_shdsc_ipsec_decap_hb(uint32_t *descbuf,
					   unsigned *bufsize,
					   unsigned short ps,
					   struct ipsec_decap_pdb *pdb,
					   struct alginfo *cipherdata,
					   struct alginfo *authdata)
{
	struct program prg;
	struct program *program = &prg;

	LABEL(keyjmp);
	REFERENCE(pkeyjmp);
	LABEL(hdr);
	REFERENCE(phdr);

	PROGRAM_CNTXT_INIT(descbuf, 0);
	if (ps)
		PROGRAM_SET_36BIT_ADDR();
	phdr = SHR_HDR(SHR_WAIT, hdr, 0);
	ENDIAN_DATA((uint8_t *)pdb, sizeof(struct ipsec_decap_pdb));
	SET_LABEL(hdr);
	pkeyjmp = JUMP(IMM(keyjmp), LOCAL_JUMP, ALL_TRUE, BOTH|SHRD);
	KEY(MDHA_SPLIT_KEY, authdata->key_enc_flags, PTR(authdata->key),
	    authdata->keylen, IMMED);
	KEY(KEY1, cipherdata->key_enc_flags, PTR(cipherdata->key),
	    cipherdata->keylen, IMMED);
	SET_LABEL(keyjmp);

	/* Workaround to assert ok-to-share. This works only for ARS=off */
	LOAD(IMM(0), DCTRL, LDOFF_CHG_SHARE_OK_NO_PROP, 0, WITH(0));
	PROTOCOL(OP_TYPE_DECAP_PROTOCOL,
		 OP_PCLID_IPSEC,
		 cipherdata->algtype | authdata->algtype);
	PATCH_JUMP(pkeyjmp, keyjmp);
	PATCH_HDR(phdr, hdr);
	*bufsize = PROGRAM_FINALIZE();
}


/**
 \brief Initializes the SEC40 descriptor for Decapsulation with preheader and
	Initialization descriptor
 \param[in] sa Pointer to the SA Info structure
 \param[in] ealg Encryption Algo Info
 \param[in] aalg Authentication Algo Info
 \return Pointer to the IPSec Encap SEC40 descriptor structure
 */
void
*create_decapsulation_sec_descriptor(struct ipsec_tunnel_t *sa)
{
	struct ipsec_decap_descriptor_t *preheader_initdesc;
	unsigned desc_len;
	struct ipsec_decap_pdb pdb;
	struct alginfo cipher;
	struct alginfo auth;
	unsigned char *buff_start = NULL;

	preheader_initdesc = __dma_mem_memalign(L1_CACHE_BYTES,
				sizeof(struct ipsec_encap_descriptor_t));
	if (preheader_initdesc == NULL) {
		fprintf(stderr, "error: %s: No More Buffers left\n", __func__);
		return NULL;
	}
	memset(preheader_initdesc, 0, sizeof(struct ipsec_decap_descriptor_t));

	buff_start = (unsigned char *)&preheader_initdesc->descbuf;

	memset(&pdb, 0, sizeof(struct ipsec_decap_pdb));
	if (sa->is_esn)
		pdb.seq_num_ext_hi = (uint32_t)(sa->seq_num >> 32);
	pdb.seq_num = (uint32_t)(sa->seq_num);
	pdb.ip_hdr_len = sizeof(struct iphdr);

	pdb.options = PDBOPTS_ESP_TUNNEL | PDBOPTS_ESP_OUTFMT |
		      PDBOPTS_ESP_ARSNONE;
	if (sa->is_esn)
		pdb.options |= PDBOPTS_ESP_ESN;

	cipher.algtype = sa->ealg->alg_type;
	cipher.key = (uintptr_t)sa->ealg->alg_key;
	cipher.keylen = sa->ealg->alg_key_len;
	cipher.key_enc_flags = 0;

	auth.algtype = sa->aalg->alg_type;
	auth.key = (uintptr_t)sa->aalg->alg_key_ptr;
	auth.keylen = sa->aalg->alg_key_len;
	auth.key_enc_flags = ENC;

	/* Now construct */
/*
 * Pointer Size parameter is currently hardcoded.
 * The application doesn't allow for proper retrieval of PS.
 */
	if (sa->hb_tunnel) {
		cnstr_shdsc_ipsec_decap_hb((uint32_t *)buff_start, &desc_len,
					   1, &pdb, &cipher, &auth);
	} else {
		cnstr_shdsc_ipsec_decap((uint32_t *)buff_start, &desc_len, 1,
					&pdb, &cipher, &auth);
	}

	pr_debug("Desc len in %s is %x\n", __func__, desc_len);

	preheader_initdesc->prehdr.hi.field.idlen = desc_len;
	/* 1 burst length to be reserved */
	preheader_initdesc->prehdr.lo.field.offset = 1;

	preheader_initdesc->prehdr.lo.field.pool_id = sec_bpid;
	preheader_initdesc->prehdr.lo.field.pool_buffer_size =
		(uint16_t)(DMA_MEM_BP3_SIZE);

	return preheader_initdesc;
}
