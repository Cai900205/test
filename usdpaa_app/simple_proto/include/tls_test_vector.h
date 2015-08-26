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


#ifndef TLS_TEST_VECTOR_H_
#define TLS_TEST_VECTOR_H_

#include <inttypes.h>
#include <usdpaa/compat.h>

/*
 * TLS test vectors and related structures.
 */
#define TLS10_OFFSET	0

/**
 * Structure which defines a TLS test vector.
 */
struct tls_ref_vector_s {
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
	struct protcmd protcmd;
	unsigned short icv_size;
	unsigned short block_size;
	unsigned short pad_size;
	unsigned short block_cipher; /* non-zero if a block cipher is used */
	uint8_t *e_pdb;
	uint8_t *d_pdb;
};

/* TLS10 AES-CBC & HMAC-SHA1 test vectors were taken from Cedar */
static uint8_t *tls_test_seq_num[] = {
	/* TLS10 AES-CBC & HMAC-SHA1 */
	(uint8_t[]){0x6B, 0x50, 0x54, 0xFE, 0x50, 0x32, 0xB1, 0x65},

};

static uint8_t tls_test_type[] = {
	0x17,
};

static uint8_t *tls_test_version[] = {
	(uint8_t[]){0x03, 0x01},
};

static uint8_t *tls_test_iv[] = {
	/* TLS10 AES-CBC & HMAC-SHA1 */
	(uint8_t[]){0xEB, 0x8C, 0x48, 0xFF, 0x89, 0xCB, 0x85, 0x4F, 0xC0, 0x90,
		    0x81, 0xCC, 0x47, 0xED, 0xFC, 0x86},

};

static uint8_t *tls_test_crypto_key[] = {
	/* TLS10 AES-CBC & HMAC-SHA1 */
	(uint8_t[]){0xD4, 0x8B, 0xFC, 0xEA, 0x9C, 0x9D, 0x8E, 0x32, 0x44, 0xD7,
		    0xD7, 0xE9, 0xF1, 0xF7, 0xDE, 0x60},
};

static uint8_t *tls_test_auth_key[] = {
	/* TLS10 AES-CBC & HMAC-SHA1 */
	(uint8_t[]){0x6E, 0x73, 0x30, 0x7F, 0xF6, 0x23, 0xE0, 0x3F, 0xAF, 0xB3,
		    0xDA, 0x08, 0xE8, 0x94, 0x33, 0x03, 0x0A, 0x3C, 0x2C, 0xC8,
		    0xD0, 0x43, 0x38, 0x65, 0xFB, 0x33, 0x05, 0x87, 0xDB, 0x40,
		    0xB3, 0xC4, 0xF1, 0x9E, 0x25, 0xCE, 0x6F, 0x5C, 0x2C, 0xD7},
};
static uint32_t tls_test_auth_keylen[] = { 40 };
static uint32_t tls_test_cipher_keylen[] = { 16 };

static uint8_t *tls_test_data_in[] = {
	/* TLS10 AES-CBC & HMAC-SHA1 */
	(uint8_t[]){0x73, 0xAF, 0xEA, 0x79, 0xC8, 0x1E, 0x47, 0x83, 0xC6, 0x95,
		    0x31, 0x39, 0x03, 0xC4, 0x18, 0xF1, 0x2B, 0x4C, 0x1A, 0x34,
		    0x50, 0x6D, 0x73, 0x29, 0xD2, 0x0F, 0x40, 0xC4, 0x19, 0x6F,
		    0xE2, 0xD7, 0x87, 0x1A, 0x99, 0x68, 0x16, 0x09, 0xC3, 0xE7,
		    0x7E, 0x17, 0x7D, 0x64, 0x9B, 0xA5, 0x39, 0x53, 0xA6, 0x88,
		    0x20, 0xA2, 0x0A, 0x17, 0x8F, 0xEF, 0x57, 0x19, 0xC7, 0xF3,
		    0x5C, 0x4A, 0xBE, 0x2E},
};

static uint32_t tls_test_data_in_len[] = {
	/* TLS10 AES-CBC & HMAC-SHA1 */
	64,
};

static uint8_t *tls_test_data_out[] = {
	/* TLS10 AES-CBC & HMAC-SHA1 */
	(uint8_t[]){0x17, 0x03, 0x01, 0x00, 0x60, 0xC9, 0x10, 0x20, 0x83, 0xA1,
		    0x4E, 0x60, 0x65, 0x86, 0x12, 0x17, 0xCB, 0x4C, 0xEB, 0x75,
		    0x20, 0xE0, 0x13, 0x06, 0x01, 0x2A, 0x69, 0x3A, 0xBD, 0x8D,
		    0xD6, 0x97, 0x15, 0xD8, 0xCE, 0xA5, 0x15, 0x4C, 0x5F, 0x02,
		    0x27, 0x89, 0xFC, 0xFC, 0xF4, 0x31, 0x18, 0x4D, 0xCC, 0x10,
		    0x0B, 0xD1, 0xC2, 0x77, 0x27, 0x46, 0xA2, 0xD4, 0xEB, 0xE3,
		    0x57, 0xB5, 0xCB, 0x05, 0x17, 0x32, 0x85, 0x09, 0x60, 0x29,
		    0x21, 0x64, 0x7A, 0xFF, 0x0E, 0x16, 0xA8, 0xD7, 0x73, 0xAF,
		    0x27, 0xCF, 0xCB, 0xCE, 0xF1, 0xEB, 0x4F, 0xF6, 0x35, 0x22,
		    0xED, 0x83, 0x57, 0x2B, 0xB3, 0x9D, 0x66, 0xD1, 0x4A, 0xF3,
		    0x77},
};

#endif /* TLS_TEST_VECTOR_H_ */
