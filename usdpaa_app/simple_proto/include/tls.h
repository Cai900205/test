/* Copyright 2013 Freescale Semiconductor, Inc.
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef TLS_H_
#define TLS_H_

#include <argp.h>
#include <inttypes.h>

#include <flib/rta.h>
#include <flib/desc.h>
#include <flib/protoshared.h>

#include <usdpaa/compat.h>

#include <crypto/test_utils.h>

#include "common.h"

#define TLS_SEQNUM_SIZE		8
#define TLS_VERSION_SIZE	2
#define TLS_TYPE_SIZE		1
#define TLS_VERSION_SIZE	2
#define TLS_LEN_SIZE		2

/**
 * TLS parameter options specific defines
 */
#define	BMASK_TLS_VERSION	0x80000000   /**< Type selected for TLS */
#define	BMASK_TLS_CIPHER	0x40000000   /**< Cipher selected for TLS */
#define	BMASK_TLS_INTEGRITY	0x20000000   /**< Integrity selected for TLS */

#define BMASK_TLS_VALID (BMASK_TLS_VERSION |				\
			 BMASK_TLS_CIPHER | BMASK_TLS_INTEGRITY)

/**
 * @def TLS_PAD_SIZE
 * @brief The following macro computes the padding size for TLS packets.
 */
#define TLS_PAD_SIZE(packet_size, icv_size, block_size)			\
	((block_size) - ((packet_size) + (icv_size)) % (block_size))
/**
 * @def TLS10_TEST_ARRAY_OFFSET
 * @brief The following macro computes the index in the TLS test vectors array
 * for TLS10 by using the following property of the
 * test array: for each ciphering algorithm, the various parameters that
 * can be given by the user are indexed by their actual values.
 * In short, this macro uses the linear property of the test vectors arrray.
 */
#define TLS10_TEST_ARRAY_OFFSET(tls_params)			\
	(TLS10_OFFSET +						\
	((tls_params)->cipher_alg *				\
	TLS_AUTH_TYPE_INVALID +					\
	(tls_params)->integrity_alg))

enum tls_version {
	SSL30 = 0,
	TLS10,
	TLS11,
	TLS12,
	DTLS10,
	DTLS12
};

enum cipher_type_tls {
	TLS_CIPHER_TYPE_AES_128_CBC,
	TLS_CIPHER_TYPE_INVALID
};

enum auth_type_tls {
	TLS_AUTH_TYPE_SHA,
	TLS_AUTH_TYPE_INVALID
};

struct tls_params {
	enum tls_version version;
	enum cipher_type_tls cipher_alg;
	enum auth_type_tls integrity_alg;
};

struct protocol_info *register_tls(void);

#endif /* TLS_H_ */
