/* Copyright (c) 2013 Freescale Semiconductor, Inc.
 * All rights reserved.
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

#ifndef __CEETM_CFG_PARSER_H
#define	__CEETM_CFG_PARSER_H

#include <internal/compat.h>

/* Structure contains information of groupA or groupB */
struct ceetm_group_info {
	uint8_t type;
	uint8_t idx;
	uint8_t cr_eligible;
	uint8_t er_eligible;
};
/* Structure contains information of channel in CEETM cfg xml */
struct ceetm_channel_info {
	struct list_head list;
	uint8_t num_group;
	uint8_t is_shaping;
	uint32_t cr;
	uint32_t er;
	struct ceetm_group_info *group_a;
	struct ceetm_group_info *group_b;
	struct list_head cq_list;
};

/* Structure contains information of SP CQ and WBFS CQ */
struct ceetm_cq_info {
	struct list_head list;
	uint8_t idx;
	uint8_t cr_eligible;
	uint8_t er_eligible;
	uint8_t group_id;
	uint8_t weight;
};

struct ceetm_lni_info {
	uint8_t is_shaping;
	uint32_t cr;
	uint32_t er;
	struct list_head channel_list;
};

/* cfg_file@ : CEETM queue hierarchy configuration file (XML).
 *             Which has CEETM channels, class queues and groups
 *
 * Parse the CEETM queue hierarchy configuration files (XML) and
 * extract CEETM channels, class queues and groups into a data structure.
 * */
struct ceetm_lni_info *ceetm_cfg_parse(const char *cfg_file);

/* Free the resource of CEETM queues */
void ceetm_cfg_clean(void);

#endif
