/*
 * CAAM Protocol Data Block (PDB) definition header file
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

#ifndef CAAM_PDB_H
#define CAAM_PDB_H

/*
 * General IPSec encap/decap PDB definitions
 */
struct ipsec_encap_cbc {
	uint32_t iv[4];
} __packed;

struct ipsec_encap_ctr {
	uint32_t ctr_nonce;
	uint32_t ctr_initial;
	uint32_t iv[2];
} __packed;

struct ipsec_encap_ccm {
	uint32_t salt; /* lower 24 bits */
	uint8_t b0_flags;
	uint8_t ctr_flags;
	uint16_t ctr_initial;
	uint32_t iv[2];
} __packed;

struct ipsec_encap_gcm {
	uint32_t salt; /* lower 24 bits */
	uint32_t rsvd1;
	uint32_t iv[2];
} __packed;

struct hmo {
	uint8_t rsvd1:1;
	uint8_t dfc:1;
	uint8_t dttl:1;
	uint8_t rsvd2:5;
} __packed;

struct ipsec_encap_pdb {
	uint32_t desc_hdr;
	union {
		uint8_t rsvd3;
		struct hmo hmo;
	};
	uint8_t ip_nh;
	uint8_t ip_nh_offset;
	uint8_t options;
	uint32_t seq_num_ext_hi;
	uint32_t seq_num;
	union {
		struct ipsec_encap_cbc cbc;
		struct ipsec_encap_ctr ctr;
		struct ipsec_encap_ccm ccm;
		struct ipsec_encap_gcm gcm;
	};
	uint32_t spi;
	uint16_t rsvd;
	uint16_t ip_hdr_len;
	uint32_t ip_hdr[0]; /* optional IP Header content */
} __packed;

struct ipsec_decap_cbc {
	uint32_t rsvd[2];
} __packed;

struct ipsec_decap_ctr {
	uint32_t salt;
	uint32_t ctr_initial;
} __packed;

struct ipsec_decap_ccm {
	uint32_t salt;
	uint8_t iv_flags;
	uint8_t ctr_flags;
	uint16_t ctr_initial;
} __packed;

struct ipsec_decap_gcm {
	uint32_t salt;
	uint32_t resvd;
} __packed;

struct ipsec_decap_pdb {
	uint32_t desc_hdr;
	uint16_t ip_hdr_len;
	uint8_t ip_nh_offset;
	uint8_t options;
	union {
		struct ipsec_decap_cbc cbc;
		struct ipsec_decap_ctr ctr;
		struct ipsec_decap_ccm ccm;
		struct ipsec_decap_gcm gcm;
	};
	uint32_t seq_num_ext_hi;
	uint32_t seq_num;
	uint32_t anti_replay[2];
	uint32_t end_index[0];
} __packed;

/*
 * IEEE 802.11i WiFi Protocol Data Block
 */
#define WIFI_PDBOPTS_FCS	0x01
#define WIFI_PDBOPTS_AR		0x40

struct wifi_encap_pdb {
	uint32_t desc_hdr;
	uint16_t mac_hdr_len;
	uint8_t rsvd;
	uint8_t options;
	uint8_t iv_flags;
	uint8_t pri;
	uint16_t pn1;
	uint32_t pn2;
	uint16_t frm_ctrl_mask;
	uint16_t seq_ctrl_mask;
	uint8_t rsvd1[2];
	uint8_t cnst;
	uint8_t key_id;
	uint8_t ctr_flags;
	uint8_t rsvd2;
	uint16_t ctr_init;
} __packed;

struct wifi_decap_pdb {
	uint32_t desc_hdr;
	uint16_t mac_hdr_len;
	uint8_t rsvd;
	uint8_t options;
	uint8_t iv_flags;
	uint8_t pri;
	uint16_t pn1;
	uint32_t pn2;
	uint16_t frm_ctrl_mask;
	uint16_t seq_ctrl_mask;
	uint8_t rsvd1[4];
	uint8_t ctr_flags;
	uint8_t rsvd2;
	uint16_t ctr_init;
} __packed;

/*
 * IEEE 802.16 WiMAX Protocol Data Block
 */
#define WIMAX_PDBOPTS_FCS	0x01
#define WIMAX_PDBOPTS_AR	0x40 /* decap only */

struct wimax_encap_pdb {
	uint32_t desc_hdr;
	uint8_t rsvd[3];
	uint8_t options;
	uint32_t nonce;
	uint8_t b0_flags;
	uint8_t ctr_flags;
	uint16_t ctr_init;
	/* begin DECO writeback region */
	uint32_t pn;
	/* end DECO writeback region */
} __packed;

struct wimax_decap_pdb {
	uint32_t desc_hdr;
	uint8_t rsvd[3];
	uint8_t options;
	uint32_t nonce;
	uint8_t iv_flags;
	uint8_t ctr_flags;
	uint16_t ctr_init;
	/* begin DECO writeback region */
	uint32_t pn;
	uint8_t rsvd1[2];
	uint16_t antireplay_len;
	uint64_t antireplay_scorecard;
	/* end DECO writeback region */
} __packed;

/*
 * IEEE 801.AE MacSEC Protocol Data Block
 */
#define MACSEC_PDBOPTS_FCS	0x01
#define MACSEC_PDBOPTS_AR	0x40 /* used in decap only */

struct macsec_encap_pdb {
	uint32_t desc_hdr;
	uint16_t aad_len;
	uint8_t rsvd;
	uint8_t options;
	uint64_t sci;
	uint16_t ethertype;
	uint8_t tci_an;
	uint8_t rsvd1;
	/* begin DECO writeback region */
	uint32_t pn;
	/* end DECO writeback region */
} __packed;

struct macsec_decap_pdb {
	uint32_t desc_hdr;
	uint16_t aad_len;
	uint8_t rsvd;
	uint8_t options;
	uint64_t sci;
	uint8_t rsvd1[3];
	/* begin DECO writeback region */
	uint8_t antireplay_len;
	uint32_t pn;
	uint64_t antireplay_scorecard;
	/* end DECO writeback region */
} __packed;

/*
 * SSL/TLS/DTLS Protocol Data Blocks
 */

#define TLS_PDBOPTS_ARS32	0x40
#define TLS_PDBOPTS_ARS64	0xc0
#define TLS_PDBOPTS_OUTFMT	0x08
#define TLS_PDBOPTS_IV_WRTBK	0x02 /* 1.1/1.2/DTLS only */
#define TLS_PDBOPTS_EXP_RND_IV	0x01 /* 1.1/1.2/DTLS only */

struct tls_block_encap_pdb {
	uint32_t desc_hdr;
	uint8_t type;
	uint8_t version[2];
	uint8_t options;
	uint64_t seq_num;
	uint32_t iv[4];
} __packed;

struct tls_stream_encap_pdb {
	uint32_t desc_hdr;
	uint8_t type;
	uint8_t version[2];
	uint8_t options;
	uint64_t seq_num;
	uint8_t i;
	uint8_t j;
	uint8_t rsvd1[2];
} __packed;

struct dtls_block_encap_pdb {
	uint32_t desc_hdr;
	uint8_t type;
	uint8_t version[2];
	uint8_t options;
	uint16_t epoch;
	uint16_t seq_num[3];
	uint32_t iv[4];
} __packed;

struct tls_block_decap_pdb {
	uint32_t desc_hdr;
	uint8_t rsvd[3];
	uint8_t options;
	uint64_t seq_num;
	uint32_t iv[4];
} __packed;

struct tls_stream_decap_pdb {
	uint32_t desc_hdr;
	uint8_t rsvd[3];
	uint8_t options;
	uint64_t seq_num;
	uint8_t i;
	uint8_t j;
	uint8_t rsvd1[2];
} __packed;

struct dtls_block_decap_pdb {
	uint32_t desc_hdr;
	uint8_t rsvd[3];
	uint8_t options;
	uint16_t epoch;
	uint16_t seq_num[3];
	uint32_t iv[4];
	uint64_t antireplay_scorecard;
} __packed;

/*
 * SRTP Protocol Data Blocks
 */
#define SRTP_PDBOPTS_MKI	0x08
#define SRTP_PDBOPTS_AR		0x40

struct srtp_encap_pdb {
	uint32_t desc_hdr;
	uint8_t x_len;
	uint8_t mki_len;
	uint8_t n_tag;
	uint8_t options;
	uint32_t cnst0;
	uint8_t rsvd[2];
	uint16_t cnst1;
	uint16_t salt[7];
	uint16_t cnst2;
	uint32_t rsvd1;
	uint32_t roc;
	uint32_t opt_mki;
} __packed;

struct srtp_decap_pdb {
	uint32_t desc_hdr;
	uint8_t x_len;
	uint8_t mki_len;
	uint8_t n_tag;
	uint8_t options;
	uint32_t cnst0;
	uint8_t rsvd[2];
	uint16_t cnst1;
	uint16_t salt[7];
	uint16_t cnst2;
	uint16_t rsvd1;
	uint16_t seq_num;
	uint32_t roc;
	uint64_t antireplay_scorecard;
} __packed;

/*
 * DSA/ECDSA Protocol Data Blocks
 * Two of these exist: DSA-SIGN, and DSA-VERIFY. They are similar
 * except for the treatment of "w" for verify, "s" for sign,
 * and the placement of "a,b".
 */
#define DSA_PDB_SGF_SHIFT	24
#define DSA_PDB_SGF_MASK	(0xff << DSA_PDB_SGF_SHIFT)
#define DSA_PDB_SGF_Q		(0x80 << DSA_PDB_SGF_SHIFT)
#define DSA_PDB_SGF_R		(0x40 << DSA_PDB_SGF_SHIFT)
#define DSA_PDB_SGF_G		(0x20 << DSA_PDB_SGF_SHIFT)
#define DSA_PDB_SGF_W		(0x10 << DSA_PDB_SGF_SHIFT)
#define DSA_PDB_SGF_S		(0x10 << DSA_PDB_SGF_SHIFT)
#define DSA_PDB_SGF_F		(0x08 << DSA_PDB_SGF_SHIFT)
#define DSA_PDB_SGF_C		(0x04 << DSA_PDB_SGF_SHIFT)
#define DSA_PDB_SGF_D		(0x02 << DSA_PDB_SGF_SHIFT)
#define DSA_PDB_SGF_AB_SIGN	(0x02 << DSA_PDB_SGF_SHIFT)
#define DSA_PDB_SGF_AB_VERIFY	(0x01 << DSA_PDB_SGF_SHIFT)

#define DSA_PDB_L_SHIFT		7
#define DSA_PDB_L_MASK		(0x3ff << DSA_PDB_L_SHIFT)

#define DSA_PDB_N_MASK		0x7f

struct dsa_sign_pdb {
	uint32_t desc_hdr;
	uint32_t sgf_ln; /* Use DSA_PDB_ defintions per above */
	uint8_t *q;
	uint8_t *r;
	uint8_t *g;	/* or Gx,y */
	uint8_t *s;
	uint8_t *f;
	uint8_t *c;
	uint8_t *d;
	uint8_t *ab;  /* ECC only */
	uint8_t *u;
} __packed;

struct dsa_verify_pdb {
	uint32_t desc_hdr;
	uint32_t sgf_ln;
	uint8_t *q;
	uint8_t *r;
	uint8_t *g;	/* or Gx,y */
	uint8_t *w;  /* or Wx,y */
	uint8_t *f;
	uint8_t *c;
	uint8_t *d;
	uint8_t *tmp; /* temporary data block */
	uint8_t *ab;  /* only used if ECC processing */
} __packed;

#endif
