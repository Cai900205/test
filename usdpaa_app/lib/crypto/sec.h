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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __CRYPTO_SEC_H
#define __CRYPTO_SEC_H

#include <usdpaa/dma_mem.h>
#include <flib/rta.h>
#include <fsl_sec/sec.h>
#include "common.h"

#define MAX_DESCRIPTOR_SIZE      64
#define SEC_PREHDR_SDLEN_MASK    0x0000007F /**< Bit mask for PreHeader length
						 field */

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

extern enum rta_sec_era rta_sec_era;

struct sec_descriptor_t {
	struct preheader_s prehdr;
	uint32_t descbuf[MAX_DESCRIPTOR_SIZE];
};

/**
 * @brief	Verifies if SEC Era version set by user is valid; in case the
 *		user didn't specify a SEC ERA, the application with run w/ a
 *		default value (SEC ERA 2)
 * @param[in]	user_sec_era - the SEC ERA the user requested
 * @param[in]	hw_sec_era - the SEC ERA as it was read from HW
 * @return	0 on success, otherwise -1 value
 */
static inline int validate_sec_era_version(int32_t user_sec_era,
					   int32_t hw_sec_era)
{
	int ret;

	if (user_sec_era < 0) {
		if (hw_sec_era < 0) {
			printf("WARNING: Running with default SEC Era version 2!\n");
			rta_set_sec_era(RTA_SEC_ERA_2);
		} else {
			printf("Using SEC Era version = %d read from HW\n",
			       hw_sec_era);
			ret = rta_set_sec_era(INTL_SEC_ERA(hw_sec_era));
			if (ret)
				goto err;
		}
	} else {
		ret = rta_set_sec_era(INTL_SEC_ERA(user_sec_era));
		if (ret)
			goto err;

		if (!(hw_sec_era < 0) && (user_sec_era != hw_sec_era)) {
			printf("WARNING: Requested SEC Era version %d, but SEC Era read from HW is %d\n",
			       user_sec_era,
			       hw_sec_era);
		}
	}

	pr_debug("Running with SEC ERA %d\n", USER_SEC_ERA(rta_get_sec_era()));

	return 0;

err:
	fprintf(stderr, "error: Unsupported SEC Era version by RTA\n");
	return -1;
}


#endif /* __CRYPTO_SEC_H */
