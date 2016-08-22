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

#ifndef __FSL_PM_H__
#define __FSL_PM_H__

#undef  FSL_PM_ON
#ifdef FSL_PM_ON
#define FSL_PM_GET		fsl_pm_get
#define FSL_PM_COMPARE	fsl_pm_compare
#define FSL_PM_BEGIN	fsl_pm_begin
#define FSL_PM_END		fsl_pm_end
#define FSL_PM_DUMP		fsl_pm_dump

#else
#define FSL_PM_GET(event)
#define FSL_PM_COMPARE(curr, prev)
#define FSL_PM_BEGIN(idx)
#define FSL_PM_END(idx)
#define FSL_PM_DUMP(idx)

#endif

/*
 * user space PM code, user performance monitor counter 0-3
 */
#define PMR_UPMC0       0x00
#define PMR_UPMC1       0x01
#define PMR_UPMC2       0x02
#define PMR_UPMC3       0x03

static inline register_t mfpmr(int reg)
{
	register_t ret;
	asm volatile ("mfpmr %0, %1":"=r" (ret):"i"(reg):"memory");
	return ret;
}

static inline u32 fsl_pm_get(int event)
{
	switch (event) {
	case 0:
		return mfpmr(PMR_UPMC0);
	case 1:
		return mfpmr(PMR_UPMC1);
	case 2:
		return mfpmr(PMR_UPMC2);
	case 3:
		return mfpmr(PMR_UPMC3);
	}
	return 0;
}

void fsl_pm_compare(u32 *curr, u32 *prev);
void fsl_pm_begin(u8 idx);
void fsl_pm_end(u8 idx);
void fsl_pm_dump(u8 idx);

#endif
