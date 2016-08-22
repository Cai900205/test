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

#ifndef _HELPER_SEC_H_
#define _HELPER_SEC_H_

#include <fsl_sec/dcl.h>

#define MAX_DESCRIPTOR_SIZE 64

/* IPSec parameter for tunnel mgmt */
struct packet_desc;
typedef int (*sec_handler_func) (u32 handle, struct packet_desc *desc);

struct preheader_s {
	union {
		uint32_t word;
		struct {
			uint16_t rsvd63_48;
			unsigned int rsvd47_39:9;
			unsigned int idlen:7;
		} field;
	} __packed hi;

	union {
		uint32_t word;
		struct {
			unsigned int rsvd31_30:2;
			unsigned int fsgt:1;
			unsigned int lng:1;
			unsigned int offset:2;
			unsigned int abs:1;
			unsigned int add_buf:1;
			uint8_t pool_id;
			uint16_t pool_buffer_size;
		} field;
	} __packed lo;
} __packed;

struct sec_descriptor_t {
	struct preheader_s prehdr;
	uint32_t descbuf[MAX_DESCRIPTOR_SIZE];
};

enum {
	sec_type_ipsec = 0,
	sec_type_kasumi,
	sec_type_snow,
	sec_type_rsa,
	sec_type_diffie_hellman,
	sec_type_max
};

enum {
	sec_buffer_simple = 0,
	sec_buffer_comp_type1,
	sec_buffer_comp_type2,
	sec_buffer_type_max
};

enum {
	sec_enc = DIR_ENCRYPT,
	sec_dec = DIR_DECRYPT
};

enum {
	sec_snow_f8 = 0,
	sec_snow_f9
};

enum {
	sec_kasumi_f8 = 0,
	sec_kasumi_f9,
};

enum {
	sec_rsa_dec_form_type1 = 0,
	sec_rsa_dec_form_type2,
	sec_rsa_dec_form_type3
};

struct sec_ipsec_param {
	u8 encryption;
	struct cipherparams cipher;
	struct authparams auth;
	union {
		struct ipsec_encap_pdb pdb_enc;
		struct ipsec_decap_pdb pdb_dec;
	};
	/* space for outer IP header, right after the PDB */
	u32 resved[8];
};

struct sec_kasumi_param {
	u8 type;		/* f8 or f9 */
	u8 encryption;
	u8 key_len;
	u8 *key;
	u8 direction;
	u32 count;
	union {
		u32 bearer;	/* f8 */
		u32 fresh;	/* f9 */
	};
};

struct sec_snow_param {
	u8 type;		/* f8 or f9 */
	u8 encryption;
	u8 key_len;
	u8 *key;
	u8 direction;
	u32 count;
	union {
		u32 bearer;	/* f8 */
		u32 fresh;	/* f9 */
	};
};

struct sec_rsa_param {
	u8 encryption;
	u8 *vector;
	union {
		struct sec_rsa_enc {
			u8 *n;
			u8 *e;
			u8 *f;	/* plain */
			u8 *g;	/* encryption */
			u16 n_len;
			u16 e_len;
			u16 f_len;
		} enc;

		struct sec_rsa_dec {
			u8 ftype;	/* the type of form f1, f2 or f3 */
			u8 *f;	/* plain */
			u8 *g;	/* encryption */
			u32 *f_len;	/* the length of plain packet */
			union {
				struct sec_rsa_dec_f1 {
					u8 *n;
					u8 *d;
					u16 n_len;
					u16 d_len;
				} f1;
				struct sec_rsa_dec_f2 {
					u8 *d;
					u8 *p;
					u8 *q;
					u8 *t1;
					u8 *t2;
					u16 n_len;
					u16 d_len;
					u16 p_len;
					u16 q_len;
				} f2;
				struct sec_rsa_dec_f3 {
					u8 *p;
					u8 *q;
					u8 *dp;
					u8 *dq;
					u8 *c;
					u8 *t1;
					u8 *t2;
					u16 n_len;
					u16 p_len;
					u16 q_len;
				} f3;
			};
		} dec;
	};
};

struct sec_dh_param {
	u8 *vector;
	u16 n_len;
	u16 l_len;
	u8 *q;
	u8 *r;			/* 0xff for DH14 for IKE */
	u8 *w;			/* party B's public key */
	u8 *s;			/* party A's private key */
	u8 *z;			/* the output */
	u8 *ab;			/* valid for ECC only */
	u32 protoinfo;
};

struct sec_interface_param {
	u32 handle;		/* output to refer the sec_interface */
	u32 type;
	u32 fqid_to_sec;
	u32 fqid_from_sec;
	u32 channel;
	u32 pool_id;
	u32 pool_buffer_size;
	u32 buffer_type;
	sec_handler_func handler_from_sec;
	union {
		struct sec_ipsec_param ipsec;
		struct sec_kasumi_param kasumi;
		struct sec_snow_param snow;
		struct sec_rsa_param rsa;
		struct sec_dh_param dh;
	};
};

struct sec_interface {
	struct list_head node;
	struct qman_fq fq_to_sec;
	struct qman_fq fq_from_sec;
	struct sec_descriptor_t *desc;
	u32 alg_type;
	u32 buffer_type;
	sec_handler_func handler_from_sec;
	union {
		struct sec_ipsec_param ipsec;
		struct sec_kasumi_param kasumi;
		struct sec_snow_param snow;
		struct sec_rsa_param rsa;
		struct sec_dh_param dh;
	};
};

/*
 * hardcode the digest/split key size as SEC user manual
 * Table10-230. Split Key Sizes in Memory for Various Hash Algorithms
 */
static inline int sec_digest_size_get(int auth_type)
{
	int size;
	switch (auth_type) {
	case AUTH_TYPE_IPSEC_MD5HMAC_96:
		size = 16;
		break;
	case AUTH_TYPE_IPSEC_SHA1HMAC_96:
		size = 20;
		break;
	case AUTH_TYPE_IPSEC_SHA2HMAC_256:
		size = 32;
		break;
	case AUTH_TYPE_IPSEC_SHA2HMAC_384:
		size = 64;
		break;
	case AUTH_TYPE_IPSEC_SHA2HMAC_512:
		size = 64;
		break;
	default:
		size = 0;
		break;
	}
	return size;
}

static inline int sec_split_key_size_get(int auth_type)
{
	return sec_digest_size_get(auth_type) * 2;
}

int sec_send(u32 handle, struct packet_desc *desc);
int sec_interface_init(struct sec_interface_param *param);
void sec_interface_clean(void);

#endif
