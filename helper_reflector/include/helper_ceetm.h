/* Copyright (c) 2014 Freescale Semiconductor, Inc.
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

#ifndef _HELPER_CEETM_H_
#define _HELPER_CEETM_H_

#define CEETM_FQID_BASE         0xf00000
#define CEETM_CQ_NUM            16
#define CEETM_GROUP_NUM         2
#define CEETM_LNI_NUM           8

#define CEETM_LNI_OAL           24
#define CEETM_BPS_ROUNDING      1

/* ceetm configuration */
struct ceetm_shaper_config {
	u8 enabled;
	u8 coupled;
	u32 cr;
	u32 cr_token_limit;
	u32 er;
	u32 er_token_limit;
};

struct ceetm_group_config {
	u8 index;		/* priority */
	u8 opcode;
};

struct ceetm_cq_config {
	u8 index;		/* priority */
	u8 weighty:5;
	u8 weightx:3;
	u16 ccg_mask;
	struct qm_ceetm_ccg_params *ccg_params;
};

struct ceetm_channel_config {
	u8 num_of_cqs;
	u8 num_of_groups;
	struct ceetm_shaper_config shaper;
	struct ceetm_group_config group[CEETM_GROUP_NUM];
	struct ceetm_cq_config cq[CEETM_CQ_NUM];
};

struct ceetm_lni_config {
	struct ceetm_shaper_config shaper;
};

/* ceetm parameters */
struct interface_ceetm_param {
	u32 port;
	u8 num_of_channels;
	struct ceetm_lni_config lni;
	struct ceetm_channel_config channel[0];
};

/* ceetm interface */
struct interface_ceetm {
	struct qm_ceetm_lni *lni;
	struct qm_ceetm_sp *sp;
	u32 fqid_base;
};

#endif
