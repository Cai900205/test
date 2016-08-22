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
#ifndef SEC_H
#define SEC_H

#include <internal/of.h>

/**
 * SEC QI error codes
 *
 */
#define SEC_QI_ERR_BITS         0x50000000
#define SEC_QI_ERR_MASK         0xff000000
#define SEC_QI_STA_MASK         0x000000ff
#define SEC_QI_ERR_BPD          0x00000008

/**
 * @brief	Reads the SEC ERA from DTS by using the of library
 * @returns	-1 if the SEC ERA could not be read (i.e. the property does
 *		not exist in DTS), the SEC ERA otherwise
 */
static inline int sec_get_of_era(void)
{
	const struct device_node *caam_node;

	for_each_compatible_node(caam_node, NULL, "fsl,sec-v4.0") {
		const uint32_t *prop = (uint32_t *)of_get_property(caam_node,
				"fsl,sec-era",
				NULL);
		return prop ? *prop : -1;
	}

	return -1;
}


#endif /* SEC_H */
