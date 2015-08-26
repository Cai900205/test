/* Copyright 2014 Freescale Semiconductor, Inc.
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

#ifndef MBMS_H_
#define MBMS_H_

#include <argp.h>
#include <inttypes.h>

#include <flib/rta.h>
#include <flib/desc.h>
#include <flib/protoshared.h>

#include <usdpaa/compat.h>

#include <crypto/test_utils.h>

#include "common.h"

#define BMASK_MBMS_TYPE		0x80000000	/**< Type selected for MBMS */

#define BMASK_MBMS_VALID	(BMASK_MBMS_TYPE)


/*
 * One of the requirements for the BP is to be 256B aligned.
 */
#define MBMS_BUFFER_ALIGN	256

/*
 * The frame offset tries to mimic the FMAN RX port behavior, so it's set to
 * 192B here. The PR field in IC will be manually put by the application.
 */
#define MBMS_BUFFER_OFFSET	192

/*
 * Alignment for the buffer where the descriptor for processing MBMS TYPE 0
 * PDUs will reside.
 */
#define MBMS_TYPE0_DESC_ALIGN	L1_CACHE_BYTES

/*
 * Alignment for the buffer where the descriptor for processing MBMS TYPE 1
 * PDUs will reside. This is needed due to the overlay mechanism employed in
 * the descriptor.
 */
#define MBMS_TYPE1_DESC_ALIGN	256

struct mbms_params {
	enum mbms_pdu_type type;
};

struct protocol_info *register_mbms(void);

#endif /* MBMS_H_ */
