/* Copyright (c) 2011 - 2012 Freescale Semiconductor, Inc.
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

#include <internal/of.h>
#include <usdpaa/of.h>
#include "dma_driver.h"

/* This function maps DMA registers by channels */
int fsl_dma_chan_init(struct dma_ch **dma_ch, uint8_t dma_id, uint8_t ch_id)
{
	int err = 0;
	struct dma_ch *dma_uio;
	char dma_uio_name[PATH_MAX], dma_uio_path[PATH_MAX];
	char dma_sys_path[PATH_MAX], *uio_name;
	char dma_sys_buff[16];
	int dma_uio_fd;
	uint32_t regs_size;
	uint32_t offset;
	size_t len;

	/* DMA uio name */
	snprintf(dma_uio_name, PATH_MAX - 1, "/dev/dma-uio%d-%d",
		 dma_id, ch_id);
	len = readlink(dma_uio_name, dma_uio_path, PATH_MAX);
	if (len < 0) {
		error(0, errno, "No dma_uio_path for device\n");
		return -errno;
	}
	dma_uio_path[len] = 0;
	uio_name = basename(dma_uio_path);

	snprintf(dma_sys_path, PATH_MAX - 1,
		 "/sys/class/uio/%s/maps/map0/offset",
		 uio_name);
	dma_uio_fd = open(dma_sys_path, O_RDONLY);
	if (dma_uio_fd < 0) {
		error(0, errno, "Failed to open %s\n", dma_sys_path);
		return -errno;
	}
	read(dma_uio_fd, dma_sys_buff, sizeof(dma_sys_buff));
	offset = (uint32_t)strtoul(dma_sys_buff, NULL, 0);
	close(dma_uio_fd);

	snprintf(dma_sys_path, PATH_MAX - 1,
		 "/sys/class/uio/%s/maps/map0/size",
		 uio_name);
	dma_uio_fd = open(dma_sys_path, O_RDONLY);
	if (dma_uio_fd < 0) {
		error(0, errno, "Failed to open %s\n", dma_sys_path);
		return 0;
	}
	read(dma_uio_fd, dma_sys_buff, sizeof(dma_sys_buff));
	regs_size = (uint32_t)strtoul(dma_sys_buff, NULL, 0);
	close(dma_uio_fd);

	pr_debug("Get %s: offset = 0x%x; regs_size = 0x%x\n",
		 dma_uio_name, offset, regs_size);

	dma_uio = (typeof(dma_uio))malloc(sizeof(struct dma_ch));
	if (!dma_uio)
		return -errno;

	memset(dma_uio, 0, sizeof(*dma_uio));

	dma_uio->fd = open(dma_uio_name, O_RDWR);
	if (dma_uio->fd < 0) {
		error(0, errno, "%s(): %s", __func__, dma_uio_name);
		err = -errno;
		goto err_dma_open;
	}
	dma_uio->regs = mmap(0, regs_size, PROT_READ | PROT_WRITE,
				MAP_SHARED, dma_uio->fd, 0);
	if (!dma_uio->regs) {
		error(0, errno, "%s(): dma register", __func__);
		err = -errno;
		goto err_dma_mmap;
	}
	dma_uio->regs = (typeof(dma_uio->regs))
			((uint8_t *)dma_uio->regs + offset);
	*dma_ch = dma_uio;

	return 0;

err_dma_mmap:
	close(dma_uio->fd);
err_dma_open:
	free(dma_uio);

	return err;
}

/* This function releases DMA related resource. */
int fsl_dma_chan_finish(struct dma_ch *dma_ch)
{
	if (!dma_ch)
		return -EINVAL;

	if (dma_ch->regs != NULL) {
		munmap(dma_ch->regs, PAGE_SIZE);
		close(dma_ch->fd);
	}

	free(dma_ch);
	return 0;
}

static int dma_basic_mode = 0;
int ips_dma_chan_basic_set_mode(int mode)
{
    dma_basic_mode=mode;
    
    return 0;
}

/* This function initializes DMA controller in direct mode */
int fsl_dma_chan_basic_direct_init(struct dma_ch *dma_ch)
{
	if (!dma_ch)
		return -EINVAL;
	/* Write: Snoop local processor */
    if(!dma_basic_mode) {
	    out_be32(&dma_ch->regs->datr, DMA_DATR_WR_SNOOP);
    } else {
	    out_be32(&dma_ch->regs->datr, 0x40050000);
    }
	/* Read: Snoop local processor*/
	out_be32(&dma_ch->regs->satr, DMA_SATR_RD_SNOOP);
	/* direct mode, single reg write, write dest start dma */
	out_be32(&dma_ch->regs->mr,
		DMA_MR_BWC_DIS | DMA_MR_SRW_EN | DMA_MR_CTM_EN);
	return 0;
}

/* Using this function can set BWC of DMA channel */
int fsl_dma_chan_bwc(struct dma_ch *dma_ch, uint8_t bwc)
{
	if (!dma_ch)
		return -EINVAL;

	out_be32(&dma_ch->regs->mr, (in_be32(&dma_ch->regs->mr) &
		~DMA_BWC_MASK) | (bwc << DMA_BWC_OFFSET));

	return 0;
}

/* This function waits for DMA operation completion or error */
int fsl_dma_wait(struct dma_ch *dma_ch)
{
	if (!dma_ch)
		return -EINVAL;
	while (in_be32(&dma_ch->regs->sr) & DMA_SR_CH_BUSY);
        

	if (in_be32(&dma_ch->regs->sr) & DMA_SR_ERR) {
		error(0, EBUSY, "%s(): dma transfer", __func__);
		out_be32(&dma_ch->regs->sr, ~0);
		return -EBUSY;
	}

	return 0;
}
/* This function starts DMA transmission in direct mode */
int fsl_dma_direct_start(struct dma_ch *dma_ch, dma_addr_t src_phys,
			dma_addr_t dest_phys, uint32_t len)
{
	if (!dma_ch)
		return -EINVAL;

	/* Wait for DMA channel free */
	fsl_dma_wait(dma_ch);
	/* Data len */
	out_be32(&dma_ch->regs->bcr, len);
	/* Prepare the high 4 bit of the 36 bit DMA address */
	out_be32(&dma_ch->regs->satr,
		(in_be32(&dma_ch->regs->satr)
		& ~0xf) | (uint32_t)(src_phys >> 32));
	out_be32(&dma_ch->regs->datr,
		(in_be32(&dma_ch->regs->datr) & ~0xf) |
		(uint32_t)(dest_phys >> 32));
	/* Set the low 32 bit address */
	out_be32(&dma_ch->regs->sar, (uint32_t)src_phys);
	/* Make sure the early data is set */
	__sync_synchronize();
	out_be32(&dma_ch->regs->dar, (uint32_t)dest_phys);
	/* Make sure DMA start */
	__sync_synchronize();

	return 0;
}

/* dma_link_dsc should be link head of the list */
int fsl_dma_chain_link_build(struct dma_link_setup_data *link_data,
			struct dma_link_dsc *link_dsc, uint64_t link_dsc_phys,
			uint32_t link_count)
{
	int i;

	if ((!link_data) || (!link_dsc))
		return -EINVAL;

	/* link descriptor should be 32 byte aligned*/
	if ((uint32_t)link_dsc & DMA_32BYTE_ALIGN_MASK)
		return -EINVAL;

	memset(link_dsc, 0, sizeof(*link_dsc) * link_count);

	for (i = 0; i < link_count; i++) {
		if (link_data[i].src_stride_en)
			link_dsc[i].src_attr |= DMA_SATR_SSME_EN;

		if (link_data[i].src_snoop_en)
			link_dsc[i].src_attr |= DMA_SATR_SREADTTYPE_SNOOP_EN;
		else
			link_dsc[i].src_attr |= DMA_SATR_SREADTTYPE_SNOOP_DIS;

		link_dsc[i].src_attr |= link_data[i].src_addr >> 32;
		link_dsc[i].src_addr = (uint32_t)link_data[i].src_addr;

		if (link_data[i].dst_stride_en)
			link_dsc[i].dst_attr |= DMA_DATR_DSME_EN;

		if (link_data[i].dst_snoop_en)
			link_dsc[i].dst_attr |= DMA_DATR_DWRITETTYPE_SNOOP_EN;
		else
			link_dsc[i].dst_attr |= DMA_DATR_DWRITETTYPE_SNOOP_EN;

		if (link_data[i].dst_nlwr)
			link_dsc[i].dst_attr |= DMA_DATR_NLWR_DIS;

		link_dsc[i].dst_attr |= link_data[i].dst_addr >> 32;
		link_dsc[i].dst_addr = (uint32_t)link_data[i].dst_addr;
		link_dsc[i].byte_count = link_data[i].byte_count;

		if (i < link_count - 1) {
			link_dsc[i].nld_eaddr =
			(link_dsc_phys + sizeof(*link_dsc) * (i + 1)) >> 32;
			link_dsc[i].nld_addr |=
			(uint32_t)(link_dsc_phys + sizeof(*link_dsc) * (i + 1));
		} else {
			link_dsc[i].nld_eaddr = 0;
			link_dsc[i].nld_addr |= DMA_NLNDAR_EOLND_EN;
		}
	}

	return 0;
}

int fsl_dma_chain_basic_start(struct dma_ch *dma_ch,
				struct dma_link_setup_data *link_data,
				uint64_t link_dsc_phys)
{
	uint32_t reg = 0;

	if (!dma_ch || !link_data)
		return -EINVAL;

	/* Wait for DMA channel free */
	fsl_dma_wait(dma_ch);

	out_be32(&dma_ch->regs->sr, 0);
	/* MR register:
	 * [XFE] = 0		Disable extend chain mode
	 * [CDSM] = 1	Writing link descriptor start dma
	 * [CTM] = 0		Configure dma to chain mode
	 */
	reg = ~DMA_MR_XFE_EN & DMA_MR_CDSM_EN & ~DMA_MR_CTM_EN;
	if (link_data->seg_interrupt_en)
		reg |= DMA_MR_EOSIE_EN;
	if (link_data->link_interrupt_en)
		reg |= DMA_MR_EOLNIE_EN;
	if (link_data->err_interrupt_en)
		reg |= DMA_MR_EIE_EN;
	out_be32(&dma_ch->regs->mr, reg);
    
    fsl_dma_chan_bwc(dma_ch,0xa);
	
    reg = link_data->src_stride_dist |
		(link_data->src_stride_size << DMA_SSR_SSS_SHIFT);
	out_be32(&dma_ch->regs->ssr, reg);

	reg = link_data->dst_stride_dist |
		(link_data->dst_stride_size << DMA_DSR_DSS_SHIFT);
	out_be32(&dma_ch->regs->ssr, reg);

	/* Start dma chain mode */
	out_be32(&dma_ch->regs->eclndar, link_dsc_phys >> 32);
	__sync_synchronize();
	out_be32(&dma_ch->regs->clndar, link_dsc_phys & DMA_CLNDAR_CLNDA_MASK);

	return 0;
}

int ips_dma_change_desc_eaddr(void *desc_addr,dma_addr_t next_desc_addr)
{
	struct dma_link_dsc *link_dsc;
	link_dsc = (struct dma_link_dsc *)desc_addr;
    link_dsc->nld_eaddr = next_desc_addr >> 32;
	link_dsc->nld_addr = 0;
    link_dsc->nld_addr |= (uint32_t)(next_desc_addr);
	return 0;
}


uint32_t  ips_dma_get_current_desc_addr(struct dma_ch *dma_ch)
{
    return in_be32(&dma_ch->regs->clndar);   
}

int ips_dma_enable_cc(struct dma_ch *dma_ch)
{
	uint32_t reg = 0;
	reg = in_be32(&dma_ch->regs->mr);
	reg |= DMA_MR_CC_EN;

	out_be32(&dma_ch->regs->mr,reg);

    return 0;
}

int ips_dma_link_print_desc(void *desc_addr)
{
    struct dma_link_dsc *link_dsc;
    link_dsc=(struct dma_link_dsc *)desc_addr; 
    printf("********DMA link descriptor********\n");
    printf("link_desc src_attr  : %08x\n", link_dsc->src_attr);  
    printf("link_desc src_addr  : %08x\n", link_dsc->src_addr);  
    printf("link_desc dst_attr  : %08x\n", link_dsc->dst_attr);  
    printf("link_desc dst_addr  : %08x\n", link_dsc->dst_addr);  
    printf("link_desc nld_eaddr : %08x\n", link_dsc->nld_eaddr);  
    printf("link_desc nld_addr  : %08x\n", link_dsc->nld_addr);  
    printf("link_desc byte_count: %08x\n", link_dsc->byte_count);  
    printf("link_desc recv      : %08x\n", link_dsc->rev);  
	printf("********End link descriptor********\n");
    return 0;
}

int ips_dma_print_reg(struct dma_ch *dma_ch) 
{
    printf("************ DMA register ************\n");
    printf("mr:%08x\n", in_be32(&dma_ch->regs->mr));
    printf("sr:%08x\n", in_be32(&dma_ch->regs->sr));
    printf("eclndar:%08x\n", in_be32(&dma_ch->regs->eclndar));
    printf("clndar:%08x\n", in_be32(&dma_ch->regs->clndar));
    printf("satr:%08x\n", in_be32(&dma_ch->regs->satr));
    printf("sar:%08x\n", in_be32(&dma_ch->regs->sar));
    printf("datr:%08x\n", in_be32(&dma_ch->regs->datr));
    printf("dar:%08x\n", in_be32(&dma_ch->regs->dar));
    printf("bcr:%08x\n", in_be32(&dma_ch->regs->bcr));
    printf("enlndar:%08x\n", in_be32(&dma_ch->regs->enlndar));
    printf("nlndar:%08x\n", in_be32(&dma_ch->regs->nlndar));
    printf("res:%08x\n", in_be32(&dma_ch->regs->res));
    printf("eclsdar:%08x\n", in_be32(&dma_ch->regs->eclsdar));
    printf("clsdar:%08x\n", in_be32(&dma_ch->regs->clsdar));
    printf("enlsdar:%08x\n", in_be32(&dma_ch->regs->enlsdar));
    printf("nlsdar:%08x\n", in_be32(&dma_ch->regs->nlsdar));
    printf("ssr:%08x\n", in_be32(&dma_ch->regs->ssr));
    printf("dsr:%08x\n", in_be32(&dma_ch->regs->dsr));
    printf("************ DMA register end ********\n");
    return 0;
}
