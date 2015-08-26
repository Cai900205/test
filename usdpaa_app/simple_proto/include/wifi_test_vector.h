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


#ifndef WIFI_TEST_VECTOR_H_
#define WIFI_TEST_VECTOR_H_

#define WIFI_CCM_SIZE		8
#define WIFI_ICV_SIZE		8
#define WIFI_FCS_SIZE		4

#define WIFI_MAX_TEST_PLAIN_PACKET_SIZE	44
#define WIFI_MAX_TEST_ENCRYPT_PACKET_SIZE	\
					(WIFI_MAX_TEST_PLAIN_PACKET_SIZE +\
					 WIFI_CCM_SIZE +\
					 WIFI_ICV_SIZE)

struct wifi_ref_vector_s {
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
	uint16_t mac_hdr_len;
	uint64_t pn;
	uint8_t priority;
	uint8_t key_id;
};

/* WIFI Test Vectors - Taken from IEEE 802.11 - 03/131r4 document */
#define WIFI_KEY_SIZE      16
#define WIFI_CCM_HDR_SIZE  8
#define WIFI_ICV_SIZE      8

static uint8_t wifi_reference_key[][WIFI_KEY_SIZE] = {
	/* Test set 1 */
	{0xc9, 0x7c, 0x1f, 0x67, 0xce, 0x37, 0x11, 0x85, 0x51, 0x4a, 0x8a, 0x19,
	 0xf2, 0xbd, 0xd5, 0x2f},
	/* Test set 1 */
	{0x8f, 0x7a, 0x05, 0x3f, 0xa5, 0x77, 0xa5, 0x59, 0x75, 0x29, 0x27, 0x20,
	 0x97, 0xa6, 0x03, 0xd5}
};

static uint16_t wifi_reference_mac_hdr_len[] = {
	/* Test set 1 */
	0x0018,
	/* Test set 2 */
	0x0018
};

static uint64_t wifi_reference_pn[] = {
	/* Test set 1 */
	0xB5039776E70C,
	/* Test set 2 */
	0x31F3CBBA97EA,
};

static uint8_t wifi_reference_pri[] = {
	/* Test set 1 */
	0x00,
	/* Test set 2 */
	0x00
};

static uint8_t wifi_reference_key_id[] = {
	/* Test set 1 */
	0x20,
	/* Test set 2 */
	0xa0
};

/* length in bits */
static uint32_t wifi_reference_length[] = {352, 352};

static uint8_t wifi_reference_plaintext[][WIFI_MAX_TEST_PLAIN_PACKET_SIZE] = {
	/* Test set 1 */
	{0x08, 0x48, 0xc3, 0x2c, 0x0f, 0xd2, 0xe1, 0x28, 0xa5, 0x7c, 0x50, 0x30,
	 0xf1, 0x84, 0x44, 0x08, 0xab, 0xae, 0xa5, 0xb8, 0xfc, 0xba, 0x80, 0x33,
	 0xf8, 0xba, 0x1a, 0x55, 0xd0, 0x2f, 0x85, 0xae, 0x96, 0x7b, 0xb6, 0x2f,
	 0xb6, 0xcd, 0xa8, 0xeb, 0x7e, 0x78, 0xa0, 0x50},
	 /* Test set 2 */
	{0x38, 0xc0, 0x6a, 0x51, 0xea, 0x10, 0x0c, 0x84, 0x68, 0x50, 0xee, 0xc1,
	 0x76, 0x2c, 0x88, 0xde, 0xaf, 0x2e, 0xe9, 0xf4, 0x6a, 0x07, 0xe0, 0xcc,
	 0x83, 0xa0, 0x63, 0x4b, 0x5e, 0xd7, 0x62, 0x7e, 0xb9, 0xdf, 0x22, 0x5e,
	 0x05, 0x74, 0x03, 0x42, 0xde, 0x19, 0x41, 0x17}
};

static uint8_t wifi_reference_ciphertext[][WIFI_MAX_TEST_ENCRYPT_PACKET_SIZE] = {
	/* Test set 1 */
	{0x08, 0x48, 0xc3, 0x2c, 0x0f, 0xd2, 0xe1, 0x28, 0xa5, 0x7c, 0x50, 0x30,
	 0xf1, 0x84, 0x44, 0x08, 0xab, 0xae, 0xa5, 0xb8, 0xfc, 0xba, 0x80, 0x33,
	 0x0c, 0xe7, 0x00, 0x20, 0x76, 0x97, 0x03, 0xb5, 0xf3, 0xd0, 0xa2, 0xfe,
	 0x9a, 0x3d, 0xbf, 0x23, 0x42, 0xa6, 0x43, 0xe4, 0x32, 0x46, 0xe8, 0x0c,
	 0x3c, 0x04, 0xd0, 0x19, 0x78, 0x45, 0xce, 0x0b, 0x16, 0xf9, 0x76, 0x23
	},
	/* Test set 2 */
	{0x38, 0xc0, 0x6a, 0x51, 0xea, 0x10, 0x0c, 0x84, 0x68, 0x50, 0xee, 0xc1,
	 0x76, 0x2c, 0x88, 0xde, 0xaf, 0x2e, 0xe9, 0xf4, 0x6a, 0x07, 0xe0, 0xcc,
	 0xea, 0x97, 0x00, 0xa0, 0xba, 0xcb, 0xf3, 0x31, 0x81, 0x4b, 0x69, 0x65,
	 0xd0, 0x5b, 0xf2, 0xb2, 0xed, 0x38, 0xd4, 0xbe, 0xb0, 0x69, 0xfe, 0x82,
	 0x71, 0x4a, 0x61, 0x0b, 0x54, 0x2f, 0xbf, 0x8d, 0xa0, 0x6a, 0xa4, 0xae
	}
};

#endif /* WIFI_TEST_VECTOR_H_ */
