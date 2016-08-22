/*
 * CAAM Descriptor Construction Library
 * Application level usage definitions and prototypes
 */
/* Copyright (c) 2008-2011 Freescale Semiconductor, Inc.
 * All rights reserved.
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DCL_H
#define DCL_H

#ifdef __cplusplus
	extern "C" {
#endif

#include <usdpaa/compat.h>
#include "desc.h"
#include "pdb.h"


/*
 * Section 1 - Descriptor command construction definitions
 * Under development and NOT to be used at present, these are
 * almost guaranteed to change upon review.
 */

enum key_dest {
	KEYDST_KEYREG,
	KEYDST_PK_E,
	KEYDST_AF_SBOX,
	KEYDST_MD_SPLIT
};

enum key_cover {
	KEY_CLEAR,
	KEY_COVERED
};

enum item_inline {
	ITEM_REFERENCE,
	ITEM_INLINE
};

enum item_purpose {
	ITEM_CLASS1,
	ITEM_CLASS2
};

enum ref_type {
	PTR_DIRECT,
	PTR_SGLIST
};

enum ctxsave {
	CTX_SAVE,
	CTX_ERASE
};

enum shrnext {
	SHRNXT_SHARED,
	SHRNXT_LENGTH
};

enum execorder {
	ORDER_REVERSE,
	ORDER_FORWARD
};

enum shrst {
	SHR_NEVER,
	SHR_WAIT,
	SHR_SERIAL,
	SHR_ALWAYS,
	SHR_DEFER
};

enum protdir {
	DIR_ENCAP,
	DIR_DECAP
};

enum algdir {
	DIR_ENCRYPT,
	DIR_DECRYPT
};

enum mdstatesel {
	MDSTATE_UPDATE,
	MDSTATE_INIT,
	MDSTATE_FINAL,
	MDSTATE_COMPLETE	/* Full init+final in single operation */
};

enum icvsel {
	ICV_CHECK_OFF,
	ICV_CHECK_ON
};

enum mktrust {
	DESC_SIGN,
	DESC_STD
};

/*
 * Type selectors for cipher types in IPSec protocol OP instructions
 */
#define CIPHER_TYPE_IPSEC_DESCBC              2
#define CIPHER_TYPE_IPSEC_3DESCBC             3
#define CIPHER_TYPE_IPSEC_AESCBC              12
#define CIPHER_TYPE_IPSEC_AESCTR              13
#define CIPHER_TYPE_IPSEC_AES_CCM_ICV8        14
#define CIPHER_TYPE_IPSEC_AES_CCM_ICV12       15
#define CIPHER_TYPE_IPSEC_AES_CCM_ICV16       16
#define CIPHER_TYPE_IPSEC_AES_GCM_ICV8        18
#define CIPHER_TYPE_IPSEC_AES_GCM_ICV12       19
#define CIPHER_TYPE_IPSEC_AES_GCM_ICV16       20

/*
 * Type selectors for authentication in IPSec protocol OP instructions
 */
#define AUTH_TYPE_IPSEC_MD5HMAC_96            1
#define AUTH_TYPE_IPSEC_SHA1HMAC_96           2
#define AUTH_TYPE_IPSEC_AESXCBCMAC_96         6
#define AUTH_TYPE_IPSEC_SHA1HMAC_160          7
#define AUTH_TYPE_IPSEC_SHA2HMAC_256          12
#define AUTH_TYPE_IPSEC_SHA2HMAC_384          13
#define AUTH_TYPE_IPSEC_SHA2HMAC_512          14

/*
 * Command Generator Prototypes
 */
uint32_t *cmd_insert_shared_hdr(uint32_t *descwd, uint8_t startidx,
				 uint8_t desclen, enum ctxsave ctxsave,
				 enum shrst share);

uint32_t *cmd_insert_hdr(uint32_t *descwd, uint8_t startidx,
			  uint8_t desclen, enum shrst share,
			  enum shrnext sharenext, enum execorder reverse,
			  enum mktrust mktrusted);

uint32_t *cmd_insert_key(uint32_t *descwd, uint8_t *key, uint32_t keylen,
			  enum ref_type sgref, enum key_dest dest,
			  enum key_cover cover, enum item_inline imm,
			  enum item_purpose purpose);

uint32_t *cmd_insert_seq_key(uint32_t *descwd, uint32_t keylen,
			      enum ref_type sgref, enum key_dest dest,
			      enum key_cover cover, enum item_purpose purpose);

uint32_t *cmd_insert_proto_op_ipsec(uint32_t *descwd, uint8_t cipheralg,
				     uint8_t authalg, enum protdir dir);

uint32_t *cmd_insert_proto_op_wimax(uint32_t *descwd, uint8_t mode,
				     enum protdir dir);

uint32_t *cmd_insert_proto_op_wifi(uint32_t *descwd, enum protdir dir);

uint32_t *cmd_insert_proto_op_macsec(uint32_t *descwd, enum protdir dir);

uint32_t *cmd_insert_proto_op_unidir(uint32_t *descwd, uint32_t protid,
				      uint32_t protinfo);

uint32_t *cmd_insert_alg_op(uint32_t *descwd, uint32_t optype,
			     uint32_t algtype, uint32_t algmode,
			     enum mdstatesel mdstate, enum icvsel icv,
			     enum algdir dir);

uint32_t *cmd_insert_pkha_op(uint32_t *descwd, uint32_t pkmode);

uint32_t *cmd_insert_seq_in_ptr(uint32_t *descwd, void *ptr, uint32_t len,
				 enum ref_type sgref);

uint32_t *cmd_insert_seq_out_ptr(uint32_t *descwd, void *ptr, uint32_t len,
				  enum ref_type sgref);

uint32_t *cmd_insert_load(uint32_t *descwd, void *data,
			   uint32_t class_access, uint32_t sgflag,
			   uint32_t dest, uint8_t offset,
			   uint8_t len, enum item_inline imm);

uint32_t *cmd_insert_fifo_load(uint32_t *descwd, dma_addr_t data,
				uint32_t len,
				uint32_t class_access, uint32_t sgflag,
				uint32_t imm, uint32_t ext, uint32_t type);

uint32_t *cmd_insert_seq_load(uint32_t *descwd, uint32_t class_access,
			       uint32_t variable_len_flag, uint32_t dest,
			       uint8_t offset, uint8_t len);

uint32_t *cmd_insert_seq_fifo_load(uint32_t *descwd, uint32_t class_access,
				    uint32_t variable_len_flag,
				    uint32_t data_type, uint32_t len);

uint32_t *cmd_insert_store(uint32_t *descwd, void *data,
			    uint32_t class_access, uint32_t sg_flag,
			    uint32_t src, uint8_t offset,
			    uint8_t len, enum item_inline imm);

uint32_t *cmd_insert_seq_store(uint32_t *descwd, uint32_t class_access,
				uint32_t variable_len_flag, uint32_t src,
				uint8_t offset, uint8_t len);

uint32_t *cmd_insert_fifo_store(uint32_t *descwd, void *data, uint32_t len,
				 uint32_t class_access, uint32_t sgflag,
				 uint32_t imm, uint32_t ext, uint32_t type);

uint32_t *cmd_insert_seq_fifo_store(uint32_t *descwd, uint32_t class_access,
				     uint32_t variable_len_flag,
				     uint32_t out_type, uint32_t len);

uint32_t *cmd_insert_jump(uint32_t *descwd, uint32_t jtype,
			   uint32_t class_type, uint32_t test, uint32_t cond,
			   int8_t offset, uint32_t *jmpdesc);

uint32_t *cmd_insert_move(uint32_t *descwd, uint32_t waitcomp,
			   uint32_t src, uint32_t dst, uint8_t offset,
			   uint8_t length);

uint32_t *cmd_insert_movelen(uint32_t *descwd, uint32_t waitcomp,
			   uint32_t src, uint32_t dst, uint8_t offset,
			   uint8_t mrsel);

uint32_t *cmd_insert_math(uint32_t *descwd, uint32_t func,
			    uint32_t src0, uint32_t src1,
			    uint32_t dest, uint32_t len,
			    uint32_t flagupd, uint32_t stall,
			    uint32_t immediate, uint32_t *data);

/*
 * Section 2 - Simple descriptor construction definitions
 */

struct pk_in_params {
	uint8_t *e;
	uint32_t e_siz;
	uint8_t *n;
	uint32_t n_siz;
	uint8_t *a;
	uint32_t a_siz;
	uint8_t *b;
	uint32_t b_siz;
};

int cnstr_seq_jobdesc(uint32_t *jobdesc, uint16_t *jobdescsz,
		      uint32_t *shrdesc, uint16_t shrdescsz,
		      void *inbuf, uint32_t insize,
		      void *outbuf, uint32_t outsize);

int cnstr_jobdesc_blkcipher_cbc(uint32_t *descbuf, uint16_t *bufsz,
				uint8_t *data_in, uint8_t *data_out,
				uint32_t datasz,
				uint8_t *key, uint32_t keylen,
				uint8_t *iv, uint32_t ivlen,
				enum algdir dir, uint32_t cipher,
				uint8_t clear);

int32_t cnstr_jobdesc_hmac(uint32_t *descbuf, uint16_t *bufsize,
			   uint8_t *msg, uint32_t msgsz, uint8_t *digest,
			   uint8_t *key, uint32_t cipher, uint8_t *icv,
			   uint8_t clear);

int cnstr_jobdesc_pkha_rsaexp(uint32_t *descbuf, uint16_t *bufsz,
			      struct pk_in_params *pkin, uint8_t *out,
			      uint32_t out_siz, uint8_t clear);

int cnstr_jobdesc_mdsplitkey(uint32_t *descbuf, uint16_t *bufsize,
			     uint8_t *key, uint32_t cipher,
			     uint8_t *padbuf);

int cnstr_jobdesc_aes_gcm(uint32_t *descbuf, uint16_t *bufsize,
			  uint8_t *key, uint32_t keylen, uint8_t *ctx,
			  enum mdstatesel mdstate, enum icvsel icv,
			  enum algdir dir, uint8_t *in, uint8_t *out,
			  uint16_t size, uint8_t *mac);

int cnstr_jobdesc_kasumi_f8(uint32_t *descbuf, uint16_t *bufsize,
			    uint8_t *key, uint32_t keylen,
			    enum algdir dir, uint32_t *ctx,
			    uint8_t *in, uint8_t *out, uint16_t size);

int cnstr_jobdesc_kasumi_f9(uint32_t *descbuf, uint16_t *bufsize,
			    uint8_t *key, uint32_t keylen,
			    enum algdir dir, uint32_t *ctx,
			    uint8_t *in, uint16_t size, uint8_t *mac);

/*
 * Section 3 - Single-pass descriptor construction definitions
 */

/*
 * Section 4 - Protocol descriptor construction definitions
 */

int cnstr_jobdesc_dsaverify(uint32_t *descbuf, uint16_t *bufsz,
			    struct dsa_verify_pdb *dsadata, uint8_t *msg,
			    uint32_t msg_sz, uint8_t clear);

/* If protocol descriptor, IPV4 or 6? */
enum protocolvers {
	PDB_IPV4,
	PDB_IPV6
};

/* If antireplay in PDB, how big? */
enum antirply_winsiz {
	PDB_ANTIRPLY_NONE,
	PDB_ANTIRPLY_32,
	PDB_ANTIRPLY_64
};

/* Tunnel or Transport (for next-header byte) ? */
enum connect_type {
	PDB_TUNNEL,
	PDB_TRANSPORT
};

/* Extended sequence number support? */
enum esn {
	PDB_NO_ESN,
	PDB_INCLUDE_ESN
};

/* Decapsulation output format */
enum decap_out {
	PDB_OUTPUT_COPYALL,
	PDB_OUTPUT_DECAP_PDU
};

/* IV source */
enum ivsrc {
	PDB_IV_FROM_PDB,
	PDB_IV_FROM_RNG
};

/*
 * Request parameters for specifying authentication data
 * for a single-pass or protocol descriptor
 */
struct authparams {
	uint8_t   algtype;  /* Select algorithm */
	uint8_t  *key;      /* Key as an array of bytes */
	uint32_t  keylen;   /* Length of key in bits */
};

/*
 * Request parameters for specifying blockcipher data
 * for a single-pass or protocol descriptor
 */
struct cipherparams {
	uint8_t   algtype;
	uint8_t  *key;
	uint32_t  keylen;
};


/* Generic IPSec - to be deprecated */
struct seqnum {
	enum esn              esn;
	enum antirply_winsiz  antirplysz;
};

int32_t cnstr_shdsc_ipsec_encap(uint32_t *descbuf, uint16_t *bufsize,
				    struct ipsec_encap_pdb *pdb,
				    uint8_t *opthdr,
				    struct cipherparams *cipherdata,
				    struct authparams *authdata);

int32_t cnstr_shdsc_ipsec_decap(uint32_t *descbuf, uint16_t *bufsize,
				struct ipsec_decap_pdb *pdb,
				struct cipherparams *cipherdata,
				struct authparams *authdata);


int32_t cnstr_shdsc_wimax_encap(uint32_t *descbuf, uint16_t *bufsize,
				struct wimax_encap_pdb *pdb,
				struct cipherparams *cipherdata,
				uint8_t mode);

int32_t cnstr_shdsc_wimax_decap(uint32_t *descbuf, uint16_t *bufsize,
				struct wimax_decap_pdb *pdb,
				struct cipherparams *cipherdata,
				uint8_t mode);

int32_t cnstr_shdsc_macsec_encap(uint32_t *descbuf, uint16_t *bufsize,
				 struct macsec_encap_pdb *pdb,
				 struct cipherparams *cipherdata);

int32_t cnstr_shdsc_macsec_decap(uint32_t *descbuf, uint16_t *bufsize,
				 struct macsec_decap_pdb *pdb,
				 struct cipherparams *cipherdata);

/*
 * Non protocol sharedesc constructors
 */
int32_t cnstr_shdsc_snow_f8(uint32_t *descbuf, uint16_t *bufsize,
			    uint8_t *key, uint32_t keylen,
			    enum algdir dir, uint32_t count,
			    uint8_t bearer, uint8_t direction,
			    uint8_t clear);

int32_t cnstr_shdsc_snow_f9(uint32_t *descbuf, uint16_t *bufsize,
			    uint8_t *key, uint32_t keylen,
			    enum algdir dir, uint32_t count,
			    uint32_t fresh, uint8_t direction,
			    uint8_t clear, uint32_t datalen);

int32_t cnstr_shdsc_cbc_blkcipher(uint32_t *descbuf, uint16_t *bufsize,
				  uint8_t *key, uint32_t keylen,
				  uint8_t *iv, uint32_t ivlen,
				  enum algdir dir, uint32_t cipher,
				  uint8_t clear);

int32_t cnstr_shdsc_hmac(uint32_t *descbuf, uint16_t *bufsize,
			 uint8_t *key, uint32_t cipher, uint8_t *icv,
			 uint8_t clear);

int32_t cnstr_shdsc_kasumi_f8(uint32_t *descbuf, uint16_t *bufsize,
				uint8_t *key, uint32_t keylen,
				enum algdir dir, uint32_t count,
				uint8_t bearer, uint8_t direction,
				uint8_t clear);

int32_t cnstr_shdsc_kasumi_f9(uint32_t *descbuf, uint16_t *bufsize,
				uint8_t *key, uint32_t keylen,
				enum algdir dir, uint32_t count,
				uint32_t fresh, uint8_t direction,
				uint8_t clear, uint32_t datalen);

int32_t cnstr_shdsc_crc(uint32_t *descbuf, uint16_t *bufsize,
				uint8_t clear);

int32_t cnstr_pcl_shdsc_3gpp_rlc_decap(uint32_t *descbuf, uint16_t *bufsize,
				       uint8_t *key, uint32_t keysz,
				       uint32_t count, uint32_t bearer,
				       uint32_t direction,
				       uint16_t payload_sz, uint8_t clear);

int32_t cnstr_pcl_shdsc_3gpp_rlc_encap(uint32_t *descbuf, uint16_t *bufsize,
				       uint8_t *key, uint32_t keysz,
				       uint32_t count, uint32_t bearer,
				       uint32_t direction,
				       uint16_t payload_sz);

/*
 * Section 5 - disassembler definitions
 */

/* Disassembler options */
#define DISASM_SHOW_OFFSETS	0x01 /* display instruction indices */
#define DISASM_SHOW_RAW		0x02 /* display each raw instruction */

void desc_hexdump(uint32_t *descdata, uint32_t  size, uint32_t wordsperline,
		  int8_t *indentstr);

void caam_desc_disasm(uint32_t *desc, uint32_t opts);

#ifdef __cplusplus
	}
#endif

#endif /* DCL_H */
