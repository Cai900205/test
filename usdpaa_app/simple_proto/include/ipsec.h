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


#ifndef IPSEC_H_
#define IPSEC_H_

#include <argp.h>
#include <inttypes.h>

#include <flib/rta.h>
#include <flib/desc.h>
#include <flib/protoshared.h>

#include <usdpaa/compat.h>

#include <crypto/test_utils.h>

#include "ip/ip.h"

#include "common.h"

#define IPSEC_SPI_SIZE			4
#define IPSEC_SEQNUM_SIZE		4
#define IPSEC_PAD_LEN_SIZE		1
#define IPSEC_N_SIZE			1

/**
 * IPSEC parameter options specific defines
 */
#define	BMASK_IPSEC_CIPHER	0x40000000  /**< Cipher seleced for IPSEC */
#define	BMASK_IPSEC_INTEGRITY	0x20000000  /**< Integrity selected for IPSEC */

#define BMASK_IPSEC_VALID (BMASK_IPSEC_CIPHER | BMASK_IPSEC_INTEGRITY)

/**
* @def IPSEC_PAD_SIZE
* @brief The following macro computes the padding size for IPsec packets.
*/
#define IPSEC_PAD_SIZE(payload_size, block_size)			\
	((block_size) -							\
	 ((payload_size) + IPSEC_PAD_LEN_SIZE + IPSEC_N_SIZE) % (block_size))

/**
 * @def IPSEC_TEST_ARRAY_OFFSET
 * @brief The following macro computes the index in the IPsec test vectors array
 * by using the following property of the test array:
 * for each ciphering algorithm, the various parameters that can be given
 * by the user are indexed by their actual values.
 * In short, this macro uses the linear property of the test vectors arrray.
 */
#define IPSEC_TEST_ARRAY_OFFSET(ipsec_params)			\
	((ipsec_params)->c_alg * IPSEC_AUTH_TYPE_INVALID +	\
	 (ipsec_params)->i_alg)

enum cipher_type_ipsec {
	IPSEC_CIPHER_TYPE_TDES,
	IPSEC_CIPHER_TYPE_INVALID
};

enum auth_type_ipsec {
	IPSEC_AUTH_TYPE_HMAC_MD5_96,
	IPSEC_AUTH_TYPE_INVALID
};

struct ipsec_params {
	enum cipher_type_ipsec c_alg;
	enum auth_type_ipsec i_alg;
};

struct protocol_info *register_ipsec(void);

#endif /* IPSEC_H_ */
