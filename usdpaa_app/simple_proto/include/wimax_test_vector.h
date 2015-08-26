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


#ifndef WIMAX_TEST_VECTOR_H_
#define WIMAX_TEST_VECTOR_H_

#include <inttypes.h>
#include <usdpaa/compat.h>

#define WIMAX_GMH_SIZE	6  /**< WiMAX Generic Mac Header size */
#define WIMAX_PN_SIZE	4  /**< WiMAX Packet Number size */
#define WIMAX_KEY_SIZE	16 /**< WiMAX Key size */
#define WIMAX_ICV_SIZE	8  /**< WiMAX Integrity Check Value size */
#define WIMAX_FCS_SIZE	4  /**< WiMAX Frame Check Sequence size */
#define WIMAX_MAX_FRAME_SIZE	2047

/**
 * Structure which defines a WIMAX test vector.
 */
struct wimax_ref_vector_s {
	union {
		uintptr_t key;			/**< Used when the key contents
						     are supposed to be copied
						     by RTA as immediate in the
						     created descriptor. */
		dma_addr_t dma_addr_key;	/**< Used when a pointer to
						     the key is supposed to be
						     used as-is by RTA in the
						     created descriptor. */
	};
	unsigned char cipher_alg;
	unsigned short cipher_keylen;
	unsigned char auth_alg;
	union {
		uintptr_t auth_key;		/**< Used when the key contents
						     are supposed to be copied
						     by RTA as immediate in the
						     created descriptor. */
		dma_addr_t dma_addr_auth_key;	/**< Used when a pointer to
						     the key is supposed to be
						     used as-is by RTA in the
						     created descriptor. */
	};
	unsigned short auth_keylen;
	uint32_t length;
	uint8_t *plaintext;
	uint8_t *ciphertext;
	/*
	 * NOTE: Keep members above unchanged!
	 */
	uint8_t encap_opts;
	uint8_t decap_opts;
	uint32_t pn;
	uint16_t ar_len;
	uint16_t protinfo;
};

static uint8_t wimax_reference_pdb_opts[] = {
	/* Test Set 1 */
	WIMAX_PDBOPTS_FCS,
	/* Test Set 2 */
	WIMAX_PDBOPTS_FCS,
	/* Test Set 3 */
	WIMAX_PDBOPTS_FCS,
	/* Test Set 4 */
	WIMAX_PDBOPTS_FCS,
};

static uint16_t wimax_reference_protinfo[] = {
	/* Test Set 1 */
	OP_PCL_WIMAX_OFDM,
	/* Test Set 2 */
	OP_PCL_WIMAX_OFDMA,
	/* Test Set 3 */
	OP_PCL_WIMAX_OFDM,
	/* Test Set 4 */
	OP_PCL_WIMAX_OFDMA,
};

static uint8_t wimax_reference_gmh[][WIMAX_GMH_SIZE] = {
	/* Test Set 1 */
	{0x00, 0x40, 0x0A, 0x06, 0xC4, 0x30},

	/* Test Set 2 */
	{0x00, 0x40, 0x0A, 0x06, 0xC4, 0x30},

	/* Test Set 3 */
	{0x00, 0x40, 0x27, 0x7E, 0xB2, 0xAD},

	/* Test Set 4 */
	{0x00, 0x40, 0x27, 0x7E, 0xB2, 0xAD},
};

static uint8_t wimax_reference_payload[][33] = {
	/* Test Set 1 */
	{0x00, 0x01, 0x02, 0x03},

	/* Test Set 2 */
	{0x00, 0x01, 0x02, 0x03},

	/* Test Set 3 */
	{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
	0x20},

	/* Test Set 4 */
	{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
	0x20},
};

/* input packet length in bits */
static uint32_t wimax_reference_length[] = {80, 80, 312, 312};

static uint8_t wimax_reference_key[][WIMAX_KEY_SIZE] = {
	/* Test Set 1 */
	{0xD5, 0x0E, 0x18, 0xA8, 0x44, 0xAC, 0x5B, 0xF3,
	0x8E, 0x4C, 0xD7, 0x2D, 0x9B, 0x09, 0x42, 0xE5},

	/* Test Set 2 */
	{0xD5, 0x0E, 0x18, 0xA8, 0x44, 0xAC, 0x5B, 0xF3,
	0x8E, 0x4C, 0xD7, 0x2D, 0x9B, 0x09, 0x42, 0xE5},

	/* Test Set 3 */
	{0xB7, 0x4E, 0xB0, 0xE4, 0xF8, 0x1A, 0xD6, 0x3D,
	0x12, 0x1B, 0x7E, 0x9A, 0xEC, 0xCD, 0x26, 0x8F},

	/* Test Set 4 */
	{0xB7, 0x4E, 0xB0, 0xE4, 0xF8, 0x1A, 0xD6, 0x3D,
	0x12, 0x1B, 0x7E, 0x9A, 0xEC, 0xCD, 0x26, 0x8F},
};

static uint8_t wimax_reference_pn[][WIMAX_PN_SIZE] = {
	/* Test Set 1 */
	{0x21, 0x57, 0xF6, 0xBC},

	/* Test Set 2 */
	{0x21, 0x57, 0xF6, 0xBC},

	/* Test Set 3 */
	{0x78, 0xD0, 0x7D, 0x08},

	/* Test Set 4 */
	{0x78, 0xD0, 0x7D, 0x08},
};

static uint8_t wimax_reference_enc_gmh[][WIMAX_GMH_SIZE] = {
	/* Test Set 1 */
	{0x40, 0x40, 0x1A, 0x06, 0xC4, 0x5A},

	/* Test Set 2 */
	{0x40, 0x40, 0x1A, 0x06, 0xC4, 0x5A},

	/* Test Set 3 */
	{0x40, 0x40, 0x37, 0x7E, 0xB2, 0xC7},

	/* Test Set 4 */
	{0x40, 0x40, 0x37, 0x7E, 0xB2, 0xC7},
};

static uint8_t wimax_reference_enc_pn[][WIMAX_PN_SIZE] = {
	/* Test Set 1 */
	{0xBC, 0xF6, 0x57, 0x21},

	/* Test Set 2 */
	{0xBC, 0xF6, 0x57, 0x21},

	/* Test Set 3 */
	{0x08, 0x7D, 0xD0, 0x78},

	/* Test Set 4 */
	{0x08, 0x7D, 0xD0, 0x78},
};

static uint8_t wimax_reference_enc_payload[][33] = {
	/* Test Set 1 */
	{0xE7, 0x55, 0x36, 0xC8},

	/* Test Set 2 */
	{0xE7, 0x55, 0x36, 0xC8},

	/* Test Set 3 */
	{0x71, 0x3F, 0xB1, 0x22, 0xB9, 0x73, 0x4F, 0xDB,
	0xFD, 0x68, 0x2E, 0xAD, 0x9D, 0xCA, 0x9F, 0x44,
	0x1F, 0x62, 0xFE, 0x0F, 0x4A, 0x2C, 0x45, 0xB5,
	0x53, 0x17, 0x3D, 0x66, 0x5B, 0x2D, 0x53, 0xC1,
	0xB3},

	/* Test Set 4 */
	{0x71, 0x3F, 0XB1, 0x22, 0xB9, 0x73, 0x4F, 0xDB,
	0xFD, 0x68, 0x2E, 0xAD, 0x9D, 0xCA, 0x9F, 0x44,
	0x1F, 0x62, 0xFE, 0x0F, 0x4A, 0x2C, 0x45, 0xB5,
	0x53, 0x17, 0x3D, 0x66, 0x5B, 0x2D, 0x53, 0xC1,
	0xB3},
};

static uint8_t wimax_reference_enc_icv[][WIMAX_ICV_SIZE] = {
	/* Test Set 1 */
	{0x27, 0xA8, 0xD7, 0x1B, 0x43, 0x2C, 0xA5, 0x48},

	/* Test Set 2 */
	{0x27, 0xA8, 0xD7, 0x1B, 0x43, 0x2C, 0xA5, 0x48},

	/* Test Set 3 */
	{0xE7, 0xE4, 0x8D, 0x2D, 0xB7, 0x61, 0xCF, 0x94},

	/* Test Set 4 */
	{0xE7, 0xE4, 0x8D, 0x2D, 0xB7, 0x61, 0xCF, 0x94},
};

static uint8_t wimax_reference_fcs[][WIMAX_FCS_SIZE] = {
	/* Test Set 1 */
	{0xCB, 0xB6, 0x5F, 0x48},

	/* Test Set 2 */
	{0x1B, 0xD1, 0xBA, 0x21},

	/* Test Set 3 */
	{0x92, 0x1B, 0x32, 0x41},

	/* Test Set 4 */
	{0xFD, 0x03, 0x7B, 0x1D},
};


#endif /* WIMAX_TEST_VECTOR_H_ */
