/* Copyright (c) 2011-2013 Freescale Semiconductor, Inc.
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

#if defined(B4860) || defined(T4240) || defined(T2080)

#include <ppac.h>
#include "fm_vsp_ext.h"
#include "fm_port_ext.h"
#include "ppam_if.h"

#include <inttypes.h>
#include <error.h>

#include "vsp_api.h"

struct bpools {
	unsigned int bpid;
	unsigned int size;
	unsigned int num;
};

struct bpools bpool[] = {
		{-1, VSP_BP_SIZE, VSP_BP_NUM},
		{-1, VSP_BP_SIZE, VSP_BP_NUM},
		{-1, VSP_BP_SIZE, VSP_BP_NUM}
};

static t_Handle			vsp[NUM_VSP];
static t_Handle			fm_obj;

int vsp_init(int fman_id, int fm_port_number)
{
	t_FmBufferPrefixContent fmBufferPrefixContent;
	t_FmVspParams		fmVspParams;
	int			i, ret = E_OK;

	ret = bpool_init();
	if (ret < 0) {
			error(0, ret, "Buffer pools init failed\n");
			return -ENOMEM;
	}

	fm_obj = FM_Open(fman_id);
	if (!fm_obj) {
		ret = -EINVAL;
		error(0, ret, "FM_Open NULL handle.\n");
		return ret;
	}

	memset(&fmVspParams, 0, sizeof(fmVspParams));

	fmVspParams.h_Fm = fm_obj;
	fmVspParams.portParams.portId = fm_port_number;
	fmVspParams.portParams.portType = e_FM_PORT_TYPE_RX;
	fmVspParams.extBufPools.numOfPoolsUsed = 1;

	/*VSP  buffer prefix configuration*/
	memset(&fmBufferPrefixContent, 0, sizeof(fmBufferPrefixContent));
	fmBufferPrefixContent.privDataSize = 16;
	fmBufferPrefixContent.passPrsResult = TRUE;
	fmBufferPrefixContent.passTimeStamp = TRUE;
	fmBufferPrefixContent.passHashResult = FALSE;
	fmBufferPrefixContent.passAllOtherPCDInfo = FALSE;
	fmBufferPrefixContent.dataAlign = 64;

	memset(vsp, 0, NUM_VSP * sizeof(t_Handle));
	for (i = 0; i < NUM_VSP; i++) {
		fmVspParams.relativeProfileId = i;
		fmVspParams.extBufPools.extBufPool[0].id = bpool[i].bpid;
		fmVspParams.extBufPools.extBufPool[0].size = bpool[i].size;

		vsp[i] = FM_VSP_Config(&fmVspParams);
		if (!vsp[i]) {
			ret = -EINVAL;
			error(0, ret, "FM_VSP_Config NULL vsp handle for "
					"vspid %d.\n", i);
			return ret;
		}

		ret = FM_VSP_ConfigBufferPrefixContent(vsp[i],
				&fmBufferPrefixContent);
		if (ret != E_OK) {
			error(0, ret, "FM_VSP_ConfigBufferPrefixContent error "
					"for vspid %d; err: %d\n", i, ret);
			return ret;
		}

		/* VSP final configuration */
		ret = FM_VSP_Init(vsp[i]);
		if (ret != E_OK) {
			error(0, ret, "FM_VSP_Init error: %d\n", ret);
			return ret;
		}

	}

	return ret;
}

int bpool_init(void)
{
	int i = 0, err, ret;

	/* - map DMA mem */
	dma_mem_generic = dma_mem_create(DMA_MAP_FLAG_ALLOC, NULL,
			DMA_MAP_SIZE);
	if (!dma_mem_generic) {
		error(0 , -EINVAL , "dma_mem initialization failed.\n");
		return -EINVAL;
	}

	for (i = 0; i < NUM_VSP; i++) {
		ret = bman_alloc_bpid(&bpool[i].bpid);
		if (ret < 0) {
			error(0, ret, "bp(%d) allocation failure\n",
					bpool[i].bpid);
			return ret;
		}
		err = ppac_prepare_bpid(bpool[i].bpid , bpool[i].num,
				bpool[i].size, 256, 1, NULL, NULL);
		if (err) {
			error(0, err, "bp(%d) init failure\n", bpool[i].bpid);
			return err;

		}
	}
	return E_OK;
}

int vsp_clean(void)
{
	int err = E_OK;
	int i = 0;

	for (i = 0; i < NUM_VSP; i++)
		if (vsp[i]) {
			err = FM_VSP_Free(vsp[i]);
			if (err != E_OK) {
				error(0, -err, "Error FM_VSP_Free: %d", err);
				return err;
			}
		}
	FM_Close(fm_obj);

	for (i = 0; i < NUM_VSP; i++)
		bman_release_bpid(bpool[i].bpid);

	return E_OK;
}

#endif /* defined(B4860) || defined(T4240) || defined(T2080) */
