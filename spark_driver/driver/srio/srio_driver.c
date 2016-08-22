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
#include <error.h>
#include <math.h>
#include "srio_driver.h"

static int __fsl_srio_get_port_num(struct srio_dev *srio_dev,
				 const struct device_node *srio_node)
{
	const struct device_node *child;
	int i = 0;

	for_each_child_node(srio_node, child)
		i++;

	return i;
}

static int fsl_srio_port_init(struct srio_dev *srio_dev,
			      const struct device_node *node, uint32_t id)
{
	const struct device_node *srio_node;
	struct srio_port *srio_port;
	const uint32_t *dt_range, *cell_index;
	uint64_t law_start, law_size;
	uint32_t paw, aw, sw;
	char port_uio_name[PATH_MAX];
	int ret;

	srio_node = node;
	cell_index = of_get_property(srio_node, "cell-index", NULL);
	if (!cell_index) {
		ret = -ENODEV;
		error(0, -ret, "%s(): of_get_property cell-index", __func__);
		return ret;
	}

	dt_range = of_get_property(srio_node, "ranges", NULL);
	if (!dt_range) {
		ret = -ENODEV;
		error(0, -ret, "%s(): of_get_property ranges", __func__);
		return ret;
	}

	aw = of_n_addr_cells(srio_node);
	sw = of_n_size_cells(srio_node);
	paw = of_n_addr_cells(srio_node);
	law_start = of_read_number(dt_range + aw, paw);
	law_size = of_read_number(dt_range + aw + paw, sw);

	srio_port = &srio_dev->port[id];
	srio_port->port_id = *cell_index;
	srio_port->win_range.start = law_start;
	srio_port->win_range.size = law_size;

	snprintf(port_uio_name, PATH_MAX - 1, "/dev/srio-uio-port%d",
			srio_port->port_id);

	srio_port->port_fd = open(port_uio_name, O_RDWR);
	if (srio_port->port_fd  < 0) {
		ret = -errno;
		error(0, -ret, "%s(): Srio uio port", __func__);
		return ret;
	}

	srio_port->mem_win = mmap(NULL, srio_port->win_range.size,
				  PROT_READ | PROT_WRITE, MAP_SHARED,
				  srio_port->port_fd, 0);
	if (srio_port->mem_win == MAP_FAILED) {
		ret = -errno;
		error(0, -ret, "%s(): Srio window", __func__);
		goto err_mem_map;
	}

	return 0;

err_mem_map:
	close(srio_port->port_fd);

	return ret;
}

/* This function maps the SRIO registers and windows */
int fsl_srio_uio_init(struct srio_dev **srio)
{
	int ret;
	struct srio_dev *sriodev;
	const struct device_node *srio_node, *child;
	const uint32_t *regs_addr_ptr;
	uint64_t  regs_addr;
	int i;

	sriodev = (struct srio_dev *)malloc(sizeof(struct srio_dev));
	if (!sriodev)
		return -errno;
	memset(sriodev, 0, sizeof(struct srio_dev));
	*srio = sriodev;

	srio_node = of_find_compatible_node(NULL, NULL, "fsl,srio");
	if (!srio_node) {
		ret = -ENODEV;
		error(0, -ret, "%s(): compatible", __func__);
		goto err_of_compatible;
	}

	regs_addr_ptr = of_get_address(srio_node, 0, &sriodev->regs_size, NULL);
	if (!regs_addr_ptr) {
		ret = -ENODEV;
		error(0, -ret, "%s(): of_get_address", __func__);
		goto err_of_compatible;
	}

	regs_addr = of_translate_address(srio_node, regs_addr_ptr);
	if (!regs_addr) {
		ret = -ENODEV;
		error(0, -ret, "%s(): of_translate_address", __func__);
		goto err_of_compatible;
	}

	sriodev->port_num = __fsl_srio_get_port_num(sriodev, srio_node);
	if (sriodev->port_num == 0) {
		ret = -ENODEV;
		error(0, -ret, "%s(): Srio port", __func__);
		goto err_of_compatible;

	}

	sriodev->reg_fd = open("/dev/srio-uio-regs", O_RDWR);
	if (sriodev->reg_fd < 0) {
		ret = -errno;
		error(0, -ret, "%s(): Srio uio regs", __func__);
		goto err_of_compatible;
	}

	sriodev->rio_regs = mmap(NULL, sriodev->regs_size,
				 PROT_READ | PROT_WRITE, MAP_SHARED,
				 sriodev->reg_fd, 0);
	if (sriodev->rio_regs == MAP_FAILED) {
		ret = -errno;
		error(0, -ret, "%s(): Srio regs", __func__);
		goto err_reg_map;
	}

	sriodev->port = malloc(sizeof(struct srio_port) * sriodev->port_num);
	if (!sriodev->port) {
		ret = -errno;
		error(0, -ret, "%s(): Port memory", __func__);
		goto err_port_malloc;
	}
	memset(sriodev->port, 0, sizeof(struct srio_port) * sriodev->port_num);

	i = 0;
	for_each_child_node(srio_node, child) {
		ret = fsl_srio_port_init(sriodev, child, i);
		if (ret < 0)
			goto err_port_malloc;
		i++;
	}

	return 0;

err_port_malloc:
	munmap(sriodev->rio_regs, sriodev->regs_size);
err_reg_map:
	close(sriodev->reg_fd);
err_of_compatible:
	free(sriodev);
	*srio = NULL;

	return ret;
}

/* This function releases the srio related resource */
int fsl_srio_uio_finish(struct srio_dev *sriodev)
{
	int i;

	if (!sriodev)
		return -EINVAL;

	for (i = 0; i < sriodev->port_num; i++) {
		munmap(sriodev->port[i].mem_win,
		       sriodev->port[i].win_range.size);
		close(sriodev->port[i].port_fd);
	}

	if (sriodev->reg_fd) {
		munmap(sriodev->rio_regs, sriodev->regs_size);
		close(sriodev->reg_fd);
		free(sriodev->port);
		free(sriodev);
	}

	return 0;
}

/* This function does the srio port connection check */
int fsl_srio_connection(struct srio_dev *sriodev, uint8_t port_id)
{
	uint32_t ccsr;
	uint32_t escsr;
	struct rio_regs *rio_regs;
	struct srio_port *port;

	if (!sriodev || (port_id > sriodev->port_num))
		return -EINVAL;

	port = &sriodev->port[port_id];
	rio_regs = sriodev->rio_regs;
	ccsr = in_be32(&rio_regs->lp_serial.port[port_id].ccsr);
	escsr = in_be32(&rio_regs->lp_serial.port[port_id].escsr);

	/* Checking the port training status */
	if (!(escsr & 0x2)) {
		fprintf(stderr, "Port %d is not ready.\n"
			"Try to restart connection...\n", port_id + 1);
		if (ccsr & SRIO_CCSR_PT) {
			/* Disable ports */
			out_be32(&rio_regs->lp_serial.port[port_id].ccsr, 0);
			/* Set 1x lane */
			out_be32(&rio_regs->lp_serial.port[port_id].ccsr,
				in_be32(&rio_regs->lp_serial.port[port_id].ccsr)
				| SRIO_CCSR_PW0_1X);
			/* Enable ports */
			out_be32(&rio_regs->lp_serial.port[port_id].ccsr,
				in_be32(&rio_regs->lp_serial.port[port_id].ccsr)
				| SRIO_CCSR_OPE_IPE_EN);
		}
		msleep(100);
		escsr = in_be32(&rio_regs->lp_serial.port[port_id].escsr);
		if (!(escsr & 0x2)) {
			error(0, EIO, "%s()", __func__);
			return -EIO;
		}
		fprintf(stderr, "Port %d restart success!\n", port_id + 1);
	}

	port->enable = 1;

	return 0;
}

/*
 * This function checks the srio port connection status, and returns
 * the status flag.
 */
int fsl_srio_port_connected(struct srio_dev *sriodev)
{
	uint32_t port_flag = 0; /* bit0 - port1; bit1 - port2 ... */
	uint32_t i;

	if (!sriodev)
		return -EINVAL;

	for (i = 0; i < sriodev->port_num; i++)
		if (sriodev->port[i].enable)
			port_flag |= 0x01  << i;

	return port_flag;
}

/*
 * This function return the total port number of rapidio.
 */
int fsl_srio_get_port_num(struct srio_dev *sriodev)
{
	return (int)sriodev->port_num;
}

/* This function copies the srio port info to user */
int fsl_srio_get_port_info(struct srio_dev *sriodev, uint8_t port_id,
			   struct srio_port_info *port, void **range_virt)
{
	uint32_t i;

	if (!port)
		return -EINVAL;

	for (i = 0; i < sriodev->port_num; i++) {
		if (sriodev->port[i].port_id == port_id)
			break;
	}

	if (i == sriodev->port_num)
		return -ENODEV;

	port->range.start = sriodev->port[i].win_range.start;
	port->range.size = sriodev->port[i].win_range.size;
	*range_virt = sriodev->port[i].mem_win;

	return 0;
}

/* This function sets the outbound window protocol type attributes */
int fsl_srio_set_obwin_attr(struct srio_dev *sriodev, uint8_t port_id,
		      uint8_t win_id, uint32_t rd_attr, uint32_t wr_attr)
{
	struct rio_atmu *atmu;

	if (!sriodev || win_id > SRIO_OB_WIN_NUM)
		return -EINVAL;

	atmu = &sriodev->rio_regs->atmu;

	out_be32(&atmu->port[port_id].outbw[win_id].rowar,
		 (in_be32(&atmu->port[port_id].outbw[win_id].rowar)
		  & ~SRIO_ROWAR_WR_MASK) |
		  (rd_attr << SRIO_ROWAR_RDTYP_SHIFT) |
		  (wr_attr << SRIO_ROWAR_WRTYP_SHIFT));

	return 0;
}

/* This function initializes the outbound window all parameters */
int fsl_srio_set_obwin(struct srio_dev *sriodev, uint8_t port_id,
		       uint8_t win_id, uint64_t ob_win_phys,
		       uint64_t ob_win_sys, size_t win_size)
{
	struct rio_atmu *atmu;

	if (!sriodev || win_id > SRIO_OB_WIN_NUM)
		return -EINVAL;

	atmu = &sriodev->rio_regs->atmu;
	out_be32(&atmu->port[port_id].outbw[win_id].rowbar,
		 ob_win_phys >> SRIO_ADDR_SHIFT);
	out_be32(&atmu->port[port_id].outbw[win_id].rowtar,
		 ob_win_sys >> SRIO_ADDR_SHIFT);
	out_be32(&atmu->port[port_id].outbw[win_id].rowtear, 0);
	out_be32(&atmu->port[port_id].outbw[win_id].rowar,
		(in_be32(&atmu->port[port_id].outbw[win_id].rowar) &
		~SRIO_ROWBAR_SIZE_MASK) | SRIO_ROWAR_EN_WIN | win_size);

	return 0;
}

/* This function initializes the inbound window all parameters */
int fsl_srio_set_ibwin(struct srio_dev *sriodev, uint8_t port_id,
		       uint8_t win_id, uint64_t ib_win_phys,
		       uint64_t ib_win_sys, size_t win_size)
{
	struct rio_atmu *atmu;

	if (!sriodev || win_id > SRIO_IB_WIN_NUM)
		return -EINVAL;

	atmu = &sriodev->rio_regs->atmu;

	out_be32(&atmu->port[port_id].inbw[win_id].riwbar,
		 ib_win_sys >> SRIO_ADDR_SHIFT);
	out_be32(&atmu->port[port_id].inbw[win_id].riwtar,
		 ib_win_phys >> SRIO_ADDR_SHIFT);
	out_be32(&atmu->port[port_id].inbw[win_id].riwar,
		 SRIO_RIWAR_MEM | win_size);

	return 0;
}

/* This function makes srio to accept all packages with any id */
int fsl_srio_enable_accept_all(struct srio_dev *sriodev, uint8_t port_id)
{

	if (!sriodev)
		return -EINVAL;

	/* Accept all port package */
	out_be32(&sriodev->rio_regs->impl.port[port_id].accr, SRIO_ISR_AACR_AA);

	return 0;
}

/* This function disables the fearture of accepting all packages.
 * This function should be used before setting device id and target id.
 */
int fsl_srio_disable_accept_all(struct srio_dev *sriodev, uint8_t port_id)
{
	if (!sriodev)
		return -EINVAL;

	/* Disable accepting all port package */
	out_be32(&sriodev->rio_regs->impl.port[port_id].accr, 0);

	return 0;
}

/* This function sets srio port unique device id */
int fsl_srio_set_deviceid(struct srio_dev *sriodev, uint8_t port_id,
			  uint32_t dev_id)
{
	if (!sriodev)
		return -EINVAL;

	out_be32(&sriodev->rio_regs->impl.port[port_id].adidcsr,
		SRIO_ADIDCSR_ADE_EN | (dev_id << SRIO_ADIDCSR_ADID_SHIFT));

	return 0;
}

/* This function sets srio port outbound window target id for transmission */
int fsl_srio_set_targetid(struct srio_dev *sriodev, uint8_t port_id,
			  uint8_t win_id, uint32_t target_id)
{
	struct rio_atmu *atmu;

	if (!sriodev)
		return -EINVAL;

	atmu = &sriodev->rio_regs->atmu;

	out_be32(&atmu->port[port_id].outbw[win_id].rowtar,
		(in_be32(&atmu->port[port_id].outbw[win_id].rowtar) &
		~SRIO_ROWTAR_TREXAD_MASK) |
		(target_id << SRIO_ROWTAR_TREXAD_SHIFT));

	return 0;
}

/* This function sets the number of segments divided from a srio port window.
 * The parameter - seg_num. seg_num can be 1/2/4
 */
int fsl_srio_set_seg_num(struct srio_dev *sriodev, uint8_t port_id,
			 uint8_t win_id, uint8_t seg_num)
{
	struct rio_atmu *atmu;
	uint32_t nseg = 0;

	if (!sriodev || (win_id > SRIO_IB_WIN_NUM) ||
	    (seg_num > SRIO_MAX_SEG_NUM))
		return -EINVAL;

	atmu = &sriodev->rio_regs->atmu;

	if (seg_num)
		nseg = (uint32_t)log2(seg_num);
	else
		nseg = 0;

	if (nseg < 0)
		return nseg;

	out_be32(&atmu->port[port_id].outbw[win_id].rowar,
		(in_be32(&atmu->port[port_id].outbw[win_id].rowar) &
		~SRIO_ROWAR_NSEG_MASK) | (nseg << SRIO_ROWAR_NSGE_SHIFT));

	return 0;
}

/* Get the segment number of a window */
int fsl_srio_get_seg_num(struct srio_dev *sriodev, uint8_t port_id,
			 uint8_t win_id)
{
	struct rio_atmu *atmu;
	uint32_t seg_num;

	if (!sriodev || (win_id > SRIO_IB_WIN_NUM))
		return -EINVAL;

	atmu = &sriodev->rio_regs->atmu;
	seg_num = in_be32(&atmu->port[port_id].outbw[win_id].rowar) &
			SRIO_ROWAR_NSEG_MASK;
	seg_num = seg_num >> SRIO_ROWAR_NSGE_SHIFT;
	if (seg_num)
		seg_num = 1 << seg_num;
	else
		seg_num = 0;

	return seg_num;

}

/* This function sets the attribute of segments */
int fsl_srio_set_seg_attr(struct srio_dev *sriodev, uint8_t port_id,
			  uint8_t win_id, uint8_t seg_id,
			  uint32_t rd_attr, uint32_t wr_attr)
{
	struct rio_atmu *atmu;
	uint8_t seg_num;

	if (!sriodev || win_id > SRIO_OB_WIN_NUM)
		return -EINVAL;

	atmu = &sriodev->rio_regs->atmu;
	seg_num = 0x1 << ((in_be32(&atmu->port[port_id].outbw[win_id].rowar) &
		SRIO_ROWAR_NSEG_MASK) >> SRIO_ROWAR_NSGE_SHIFT);

	if (seg_id >= seg_num)
		return -EINVAL;

	if (seg_id == 0)
		out_be32(&atmu->port[port_id].outbw[win_id].rowar,
			 (in_be32(&atmu->port[port_id].outbw[win_id].rowar) &
			 ~SRIO_ROWAR_WR_MASK) |
			 (rd_attr << SRIO_ROWAR_RDTYP_SHIFT) |
			 (wr_attr << SRIO_ROWAR_WRTYP_SHIFT));
	else
		out_be32(&atmu->port[port_id].outbw[win_id].rowsr[seg_id - 1],
		(in_be32(&atmu->port[port_id].outbw[win_id].rowsr[seg_id - 1]) &
			 ~SRIO_ROWSR_WR_MASK) |
			 (rd_attr << SRIO_ROWSR_RDTYP_SHIFT) |
			 (wr_attr << SRIO_ROWSR_WRTYP_SHIFT));

	return 0;
}

/* This function sets the base device id of segment */
int fsl_srio_set_seg_sgtgtdid(struct srio_dev *sriodev, uint8_t port_id,
			      uint8_t win_id, uint8_t seg_id, uint8_t tdid)
{
	struct rio_atmu *atmu;
	uint8_t sseg_num;

	if (!sriodev || (win_id > SRIO_IB_WIN_NUM) ||
	    (seg_id >= SRIO_MAX_SEG_NUM))
		return -EINVAL;

	atmu = &sriodev->rio_regs->atmu;

	sseg_num = 0x1 << ((in_be32(&atmu->port[port_id].outbw[win_id].rowar) &
		SRIO_ROWAR_NSSEG_MASK) >> SRIO_ROWAR_NSSEG_SHIFT);

	if (tdid & (sseg_num - 1))
		return -EINVAL;

	if (seg_id == 0)
		fsl_srio_set_targetid(sriodev, port_id, win_id, tdid);
	else
		out_be32(&atmu->port[port_id].outbw[win_id].rowsr[seg_id - 1],
	       (in_be32(&atmu->port[port_id].outbw[win_id].rowsr[seg_id - 1]) &
			 ~SRIO_ROWSR_TDID_MASK) | tdid);

	return 0;
}

/* This function sets the number of sub segments divided from a segment.
    The paremeter - subseg_num can be 1/2/4/8 */
int fsl_srio_set_subseg_num(struct srio_dev *sriodev, uint8_t port_id,
			    uint8_t win_id, uint8_t subseg_num)
{
	struct rio_atmu *atmu;
	uint32_t nsseg = 0;

	if (!sriodev || (win_id > SRIO_IB_WIN_NUM) ||
		(subseg_num > SRIO_MAX_SUBSEG_NUM))
		return -EINVAL;

	atmu = &sriodev->rio_regs->atmu;

	if (subseg_num)
		nsseg = (uint32_t)log2(subseg_num);
	else
		nsseg = 0;

	if (nsseg < 0)
		return -EINVAL;

	out_be32(&atmu->port[port_id].outbw[win_id].rowar,
		 (in_be32(&atmu->port[port_id].outbw[win_id].rowar) &
		 ~SRIO_ROWAR_NSSEG_MASK) | (nsseg << SRIO_ROWAR_NSSEG_SHIFT));

	return 0;
}

/* Get sub segment number of a window */
int fsl_srio_get_subseg_num(struct srio_dev *sriodev, uint8_t port_id,
			    uint8_t win_id)
{
	struct rio_atmu *atmu;
	uint32_t nsseg = 0;

	if (!sriodev || (win_id > SRIO_IB_WIN_NUM))
		return -EINVAL;

	atmu = &sriodev->rio_regs->atmu;
	nsseg = in_be32(&atmu->port[port_id].outbw[win_id].rowar) &
			 SRIO_ROWAR_NSSEG_MASK;
	nsseg = nsseg >> SRIO_ROWAR_NSSEG_SHIFT;
	if (nsseg)
		nsseg = 1 << nsseg;
	else
		nsseg = 0;

	return nsseg;
}

/* This function clears the srio error */
int fsl_srio_clr_bus_err(struct srio_dev *sriodev)
{
	int i;

	if (!sriodev)
		return -EINVAL;

	for (i = 0; i < sriodev->port_num; i++)
		out_be32(&sriodev->rio_regs->lp_serial.port[i].escsr, ~0);

	return 0;
}

void fsl_srio_set_tmmode(struct srio_dev *sriodev, uint8_t mode)
{
	/* currently only support 0/1 mode, 2-f mode is reserved */
	if (mode > 1)
		return;
	out_be32(&sriodev->rio_regs->arch.dsllcsr,
		 in_be32(&sriodev->rio_regs->arch.dsllcsr) |
		 (mode << SRIO_DSLLCSR_TM_OFFSET));
}

void fsl_srio_port_disable(struct srio_dev *sriodev, uint8_t port_id)
{
	out_be32(&sriodev->rio_regs->lp_serial.port[port_id].ccsr,
		in_be32(&sriodev->rio_regs->lp_serial.port[port_id].ccsr) |
		1 << SRIO_CCSR_PD_OFFSET);
}

void fsl_srio_port_enable(struct srio_dev *sriodev, uint8_t port_id)
{
	out_be32(&sriodev->rio_regs->lp_serial.port[port_id].ccsr,
		in_be32(&sriodev->rio_regs->lp_serial.port[port_id].ccsr) &
		~(1 << SRIO_CCSR_PD_OFFSET));
}

void fsl_srio_drop_enable(struct srio_dev *sriodev, uint8_t port_id)
{
	out_be32(&sriodev->rio_regs->lp_serial.port[port_id].ccsr,
		 in_be32(&sriodev->rio_regs->lp_serial.port[port_id].ccsr) |
		 1 << SRIO_CCSR_DPE_OFFSET);
}

void fsl_srio_drop_disable(struct srio_dev *sriodev, uint8_t port_id)
{
	out_be32(&sriodev->rio_regs->lp_serial.port[port_id].ccsr,
		 in_be32(&sriodev->rio_regs->lp_serial.port[port_id].ccsr) &
		 ~(1 << SRIO_CCSR_DPE_OFFSET));
}

void fsl_srio_drain_enable(struct srio_dev *sriodev, uint8_t port_id)
{
	out_be32(&sriodev->rio_regs->impl.port[port_id].pcr,
		 in_be32(&sriodev->rio_regs->impl.port[port_id].pcr) |
		 1 << SRIO_PCR_OBDEN_OFFSET);
}

void fsl_srio_drain_disable(struct srio_dev *sriodev, uint8_t port_id)
{
	out_be32(&sriodev->rio_regs->impl.port[port_id].pcr,
		 in_be32(&sriodev->rio_regs->impl.port[port_id].pcr) &
		 ~(1 << SRIO_PCR_OBDEN_OFFSET));
}

void fsl_srio_set_packet_timeout(struct srio_dev *sriodev,
				 uint8_t port_id, uint32_t value)
{
	out_be32(&sriodev->rio_regs->impl.port[port_id].lopttlcr,
		 value << SRIO_LIVE_TIME_SHIFT);
}

void fsl_srio_set_link_timeout(struct srio_dev *sriodev, uint32_t value)
{
	out_be32(&sriodev->rio_regs->lp_serial.pltoccsr,
		 value << SRIO_LIVE_TIME_SHIFT);
}

int fsl_srio_port_width(struct srio_dev *sriodev, uint8_t port_id)
{
	return (in_be32(&sriodev->rio_regs->lp_serial.port[port_id].ccsr) >>
		SRIO_CCSR_IPW_OFFSET) & SRIO_CCSR_IPW_MASK;
}

int fsl_srio_fd(struct srio_dev *sriodev)
{
	return sriodev->reg_fd;
}

int fsl_srio_irq_enable(struct srio_dev *sriodev)
{
	struct rio_regs *rio_regs;
	int i;

	if (!sriodev)
		return -EINVAL;

	rio_regs = sriodev->rio_regs;
	out_be32(&rio_regs->logical_err.ltleecsr, ~0x0);

	for (i = 0; i < sriodev->port_num; i++)
		out_be32(&rio_regs->phys_err.port[i].erecsr, ~0x0);

	return 0;
}

int fsl_srio_irq_disable(struct srio_dev *sriodev)
{
	struct rio_regs *rio_regs;
	int i;

	if (!sriodev)
		return -EINVAL;

	rio_regs = sriodev->rio_regs;
	out_be32(&rio_regs->logical_err.ltleecsr, 0x0);

	for (i = 0; i < sriodev->port_num; i++)
		out_be32(&rio_regs->phys_err.port[i].erecsr, 0x0);

	return 0;
}

void fsl_srio_irq_handler(struct srio_dev *sriodev)
{
	int i;
	uint32_t port_bits;
	uint32_t reg_ltledcsr, reg_epwisr;
	struct rio_regs *rio_regs;

	rio_regs = sriodev->rio_regs;
	reg_ltledcsr = in_be32(&rio_regs->logical_err.ltledcsr);
	reg_epwisr = in_be32(&rio_regs->impl.com.epwisr);

	if (!(reg_ltledcsr || reg_epwisr))
		return;

	if (reg_ltledcsr) {
		printf("SRIO interrupt info LTLEDCSR: 0x%x\n", reg_ltledcsr);
		out_be32(&rio_regs->logical_err.ltledcsr, 0x0);
	}

	port_bits = reg_epwisr >> 28;

	for (i = 0; i < sriodev->port_num; i++) {
		if (port_bits & (0x8 >> i)) {
			printf("SRIO Port%d interrupt info\t IECSR: 0x%x,"
			       "ESCSR: 0x%x, EDCSR: 0x%x\n",
			       i + 1,
			       in_be32(&rio_regs->impl.port[i].iecsr),
			       in_be32(&rio_regs->lp_serial.port[i].escsr),
			       in_be32(&rio_regs->phys_err.port[i].edcsr));

			out_be32(&rio_regs->impl.port[i].iecsr,
				0x80000000);
			out_be32(&rio_regs->lp_serial.port[i].escsr,
				~0x0);
			out_be32(&rio_regs->phys_err.port[i].edcsr,
				0x0);
		} else
			continue;
	}

	fsl_srio_irq_enable(sriodev);
}

int fsl_srio_set_err_rate_degraded_threshold(struct srio_dev *sriodev,
					uint8_t port_id, uint8_t threshold)
{
	if (!sriodev)
		return -EINVAL;

	out_be32(&sriodev->rio_regs->phys_err.port[port_id].ertcsr,
		(in_be32(&sriodev->rio_regs->phys_err.port[port_id].ertcsr) &
		~(SRIO_REG_8BIT_MASK << SRIO_ERTCSR_ERDTT_OFFSET)) |
		(threshold << SRIO_ERTCSR_ERDTT_OFFSET));

	return 0;
}

int fsl_srio_set_err_rate_failed_threshold(struct srio_dev *sriodev,
					uint8_t port_id, uint8_t threshold)
{
	if (!sriodev)
		return -EINVAL;

	out_be32(&sriodev->rio_regs->phys_err.port[port_id].ertcsr,
		(in_be32(&sriodev->rio_regs->phys_err.port[port_id].ertcsr) &
		~(SRIO_REG_8BIT_MASK << SRIO_ERTCSR_ERFTT_OFFSET)) |
		(threshold << SRIO_ERTCSR_ERFTT_OFFSET));

	return 0;
}

int fsl_srio_set_phy_retry_threshold(struct srio_dev *sriodev, uint8_t port_id,
				uint8_t threshold, uint8_t op)
{
	if (!sriodev)
		return -EINVAL;

	out_be32(&sriodev->rio_regs->impl.com.pretcr,
		(in_be32(&sriodev->rio_regs->impl.com.pretcr) &
		~SRIO_REG_8BIT_MASK) | threshold);

//	out_be32(&sriodev->rio_regs->lp_serial.port[port_id].ccsr,(op<<SRIO_CCSR_SPF_DPE_OFFSET));

/*	out_be32(&sriodev->rio_regs->lp_serial.port[port_id].ccsr,
		(in_be32(&sriodev->rio_regs->lp_serial.port[port_id].ccsr)|0x03));
*/
	return 0;
}

/*ctx add*/
int srio_link(struct srio_dev *sriodev,uint8_t port_id)
{
	uint32_t ccsr;
	uint32_t escsr;
	struct rio_regs *rio_regs;
	//struct srio_port *port=NULL;

	if (!sriodev || (port_id > sriodev->port_num))
		return -EINVAL;

	//port = &sriodev->port[port_id];
	rio_regs = sriodev->rio_regs;
	ccsr = in_be32(&rio_regs->lp_serial.port[port_id].ccsr);
	escsr = in_be32(&rio_regs->lp_serial.port[port_id].escsr);
        /*fprintf(stderr, "ccsr:%08x escsr:%08x\n",ccsr,escsr);*/
        if((escsr & 0x2)&&(ccsr &0x10000000)&&(!(ccsr & 0x8000000)))
	{
		return 1;
	}
	else
	{
		return 0;
	}	
}

int fsl_srio_ret_enable(struct srio_dev *sriodev)
{
	struct rio_regs *rio_regs;
	int i;
	if (!sriodev)
		return -EINVAL;
	rio_regs = sriodev->rio_regs;
	out_be32(&rio_regs->logical_err.ltleecsr, ~0x0);

	for (i = 0; i < sriodev->port_num; i++)
		out_be32(&rio_regs->phys_err.port[i].erecsr, ~0x0);

	return 0;
}
int fsl_srio_ret_disable(struct srio_dev *sriodev)
{
	struct rio_regs *rio_regs;

	if (!sriodev)
		return -EINVAL;

	rio_regs = sriodev->rio_regs;
	out_be32(&rio_regs->logical_err.ltleecsr, 0x00000000);

	//for (i = 0; i < sriodev->port_num; i++)
	//	out_be32(&rio_regs->phys_err.port[i].erecsr, 0x0);

	return 0;
}

int fsl_srio_retrain(struct srio_dev *sriodev,int i)
{
    struct rio_regs *rio_regs;
	uint32_t ccsr;
	uint32_t escsr;
	uint32_t pcr;
	if (!sriodev)
		return -EINVAL;
	//clear bus error
    out_be32(&sriodev->rio_regs->lp_serial.port[i].escsr, ~0);

    rio_regs = sriodev->rio_regs;
	ccsr = in_be32(&rio_regs->lp_serial.port[i].ccsr);
	pcr = in_be32(&rio_regs->impl.port[i].pcr);
    ccsr = ccsr | (1<<SRIO_CCSR_PD_OFFSET);
    pcr =  pcr | (1<<SRIO_PCR_OBDEN_OFFSET);
    out_be32(&rio_regs->lp_serial.port[i].ccsr, ccsr);
	out_be32(&rio_regs->impl.port[i].pcr, pcr);
    out_be32(&rio_regs->lp_serial.port[i].escsr, ~0);
    out_be32(&rio_regs->impl.port[i].iecsr,
			0x80000000);
	out_be32(&rio_regs->lp_serial.port[i].escsr,
			0x07120204);
			//~0x0);
	out_be32(&rio_regs->phys_err.port[i].edcsr,
			0x0);
    usleep(5); 
	ccsr = in_be32(&rio_regs->lp_serial.port[i].ccsr);
	pcr = in_be32(&rio_regs->impl.port[i].pcr);

    pcr = pcr & (~(1<<SRIO_PCR_OBDEN_OFFSET));
    ccsr = ccsr & (~(1<<SRIO_CCSR_PD_OFFSET));
    ccsr = ccsr | (0x06600000);
	out_be32(&rio_regs->lp_serial.port[i].ccsr, ccsr);
	out_be32(&rio_regs->impl.port[i].pcr, pcr);
    printf("ccsr:%08x\n",ccsr);
    do{
        escsr=in_be32(&rio_regs->lp_serial.port[i].escsr);
        printf("port%d Escsr:%08x\n",i,escsr);
    }while(!((escsr&0x00000002)&&(!(escsr&0x00010000))));
		
    out_be32(&rio_regs->lp_serial.port[i].escsr, ~0);
    return 0;
}

