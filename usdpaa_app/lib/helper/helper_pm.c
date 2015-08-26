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

#include "helper_common.h"
#include "helper_pm.h"

#define PM_STATS_NUM 32

__PERCPU struct {
	u32 cycles[2];
	u32 instrs[2];
	u32 prefetch[2];
	u32 reserved[2];
} pm_stats[PM_STATS_NUM];

void fsl_pm_begin(u8 idx)
{
	pm_stats[idx].cycles[0] = mfpmr(PMR_UPMC0);
	pm_stats[idx].instrs[0] = mfpmr(PMR_UPMC1);
}

void fsl_pm_end(u8 idx)
{
	pm_stats[idx].cycles[1] = mfpmr(PMR_UPMC0);
	pm_stats[idx].instrs[1] = mfpmr(PMR_UPMC1);
}

void fsl_pm_dump(u8 idx)
{
	u32 cycles = pm_stats[idx].cycles[1];
	u32 instrs = pm_stats[idx].instrs[1];

	if (cycles >= pm_stats[idx].cycles[0])
		cycles -= pm_stats[idx].cycles[0];
	else
		cycles += 0xffffffff - pm_stats[idx].cycles[0];

	if (instrs >= pm_stats[idx].instrs[0])
		instrs -= pm_stats[idx].instrs[0];
	else
		instrs += 0xffffffff - pm_stats[idx].instrs[0];

	TRACE("pm stats [%02d]: cycles 0x%08x, instrs 0x%08x\n",
	      idx, cycles, instrs);
}

void fsl_pm_compare(u32 *curr, u32 *prev)
{
	u32 cycles, instrs;

	if (curr[0] > prev[0])
		cycles = curr[0] - prev[0];
	else
		cycles = (0xffffffff - curr[0]) + prev[0];

	if (curr[1] > prev[1])
		instrs = curr[1] - prev[1];
	else
		instrs = (0xffffffff - curr[1]) + prev[1];

	TRACE("pm: cycles 0x%08x, instrs 0x%08x\n", cycles, instrs);
}
