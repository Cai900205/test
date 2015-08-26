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


#ifndef PDCP_H_
#define PDCP_H_

#include <argp.h>
#include <inttypes.h>

#include <flib/rta.h>
#include <flib/desc.h>
#include <flib/protoshared.h>

#include <usdpaa/compat.h>

#include <crypto/test_utils.h>

#include "common.h"

/**
 * PDCP parameter options specific defines
 */

#define	BMASK_PDCP_TYPE		0x80000000	/**< Type selected for PDCP */
#define	BMASK_PDCP_CIPHER	0x40000000	/**< Cipher seleced for PDCP */
#define	BMASK_PDCP_INTEGRITY	0x20000000	/**< Integrity selected for
						     PDCP */
#define	BMASK_PDCP_DIR_DL	0x10000000	/**< Downlink selected for
						     PDCP */
#define	BMASK_PDCP_SN_SIZE	0x08000000	/**< SN size selected */
#define	BMASK_PDCP_HFN_OV_EN	0x04000000	/**< HFN override enabled. */

#define BMASK_PDCP_CPLANE_VALID	(BMASK_PDCP_TYPE | \
				 BMASK_PDCP_CIPHER | \
				 BMASK_PDCP_INTEGRITY)
#define BMASK_PDCP_UPLANE_VALID	(BMASK_PDCP_TYPE | BMASK_PDCP_CIPHER | \
				 BMASK_PDCP_SN_SIZE)
#define BMASK_PDCP_SHORT_MAC_VALID \
				(BMASK_PDCP_TYPE | BMASK_PDCP_INTEGRITY)

/**
 * @def PDCP_CPLANE_TEST_ARRAY_OFFSET
 * @brief The following macro computes the index in the PDCP test vectors array
 * for control plane processing by using the following property of the
 * test array: for each ciphering algorithm, the various parameters that
 * can be given by the user are indexed by their actual values.
 * In short, this macro uses the linear property of the test vectors arrray.
 */
#define PDCP_CPLANE_TEST_ARRAY_OFFSET(pdcp_params)		\
	(PDCP_CPLANE_OFFSET +					\
	((pdcp_params)->cipher_alg *				\
	PDCP_AUTH_TYPE_INVALID * PDCP_DIR_INVALID +		\
	(pdcp_params)->integrity_alg *				\
	PDCP_DIR_INVALID +					\
	(pdcp_params)->downlink))

/**
 * @def PDCP_UPLANE_TEST_ARRAY_OFFSET
 * @brief The following macro computes the index in the PDCP test vectors array
 * for user plane processing by using the following property of the
 * test array: for each ciphering algorithm, the various parameters that
 * can be given by the user are indexed by their actual values.
 * In short, this macro uses the linear property of the test vectors arrray.
 */
#define PDCP_UPLANE_TEST_ARRAY_OFFSET(pdcp_params)		\
	(PDCP_UPLANE_OFFSET +					\
	(pdcp_params)->cipher_alg *				\
	3 * PDCP_DIR_INVALID +					\
	(pdcp_params)->sn_size *				\
	PDCP_DIR_INVALID +					\
	(pdcp_params)->downlink)

/**
 * @def PDCP_SHORT_MAC_TEST_ARRAY_OFFSET
 * @brief The following macro computes the index in the PDCP test vectors array
 * for Short MAC processing by using the following property of the
 * test array: for each integrity algorithm, the various parameters that
 * can be given by the user are indexed by their actual values.
 * In short, this macro uses the linear property of the test vectors arrray.
 */
#define PDCP_SHORT_MAC_TEST_ARRAY_OFFSET(pdcp_params)		\
	(PDCP_SHORT_MAC_OFFSET +				\
	(pdcp_params)->integrity_alg)

#define PDCP_MAX_KEY_LEN	16 /* bytes */

struct pdcp_params {
	enum pdcp_plane type;
	enum cipher_type_pdcp cipher_alg;
	enum auth_type_pdcp integrity_alg;
	bool downlink;
	enum pdcp_sn_size sn_size;
	bool hfn_ov_en;
	int hfn_ov_val;
};

struct protocol_info *register_pdcp(void);

#endif /* PDCP_H_ */
