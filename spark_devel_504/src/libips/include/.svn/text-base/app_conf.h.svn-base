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


#ifndef APP_CONF_H_
#define APP_CONF_H_

/* The contiguous memory map for 'dma_mem' uses the DMA_MEM_*** constants. The
 * first part of the memory map is used to seed buffer pools, as indicated by
 * these constants, and the ad-hoc buffer allocation will be confined to the
 * area following that range, which will be limited only be the size of the DMA
 * memory region allocated by the kernel. Note, we include the BPID here too
 * (even though it has nothing to do with the DMA driver), because it means the
 * app code has all the definitions it needs for seeding buffer pools.
 */
#define DMA_MEM_BP1_BPID	7
#define DMA_MEM_BP1_SIZE	320
#define DMA_MEM_BP1_NUM		0 /* 0*320==0 (0MB) */
#define DMA_MEM_BP2_BPID	8
#define DMA_MEM_BP2_SIZE	704
#define DMA_MEM_BP2_NUM		0 /* 0*704==0 (0MB) */
#define DMA_MEM_BP3_BPID	9
#define DMA_MEM_BP3_SIZE	1728
#define DMA_MEM_BP3_NUM		0x2000 /* 0x2000*1728==13.5MB */
#define DMA_MEM_BPOOL \
	(DMA_MEM_BP1_SIZE * DMA_MEM_BP1_NUM + \
	DMA_MEM_BP2_SIZE * DMA_MEM_BP2_NUM + \
	DMA_MEM_BP3_SIZE * DMA_MEM_BP3_NUM) /* 13.5MB */

#endif /* APP_CONF_H_ */
