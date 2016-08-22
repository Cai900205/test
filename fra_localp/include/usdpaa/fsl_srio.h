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

#ifndef FSL_SRIO_H
#define FSL_SRIO_H

#define	SRIO_OB_WIN_NUM		9	/* SRIO outbound window number */
#define	SRIO_IB_WIN_NUM		5	/* SRIO inbound window number */
#define SRIO_ATTR_FLUSH		0x1
#define SRIO_ATTR_SWRITE	0x3
#define SRIO_ATTR_NWRITE	0x4
#define SRIO_ATTR_NWRITE_R	0x5
#define SRIO_ATTR_MAINTW	0x7
#define SRIO_ATTR_IO_READ_HOME	0x2
#define SRIO_ATTR_NREAD		0x4
#define SRIO_ATTR_MAINTR	0x7
#define SRIO_ATTR_ATOMIC_INC	0xc
#define SRIO_ATTR_ATOMIC_DEC	0xd
#define SRIO_ATTR_ATOMIC_SET	0xe
#define SRIO_ATTR_ATOMIC_CLR	0xf

enum srio_lawbar_size {
	LAWAR_SIZE_BASE = 0xa,
	LAWAR_SIZE_4,
	LAWAR_SIZE_8K,
	LAWAR_SIZE_16K,
	LAWAR_SIZE_32K,
	LAWAR_SIZE_64K,
	LAWAR_SIZE_128K,
	LAWAR_SIZE_256K,
	LAWAR_SIZE_512K,
	LAWAR_SIZE_1M,
	LAWAR_SIZE_2M,
	LAWAR_SIZE_4M,
	LAWAR_SIZE_8M,
	LAWAR_SIZE_16M,
	LAWAR_SIZE_32M,
	LAWAR_SIZE_64M,
	LAWAR_SIZE_128M,
	LAWAR_SIZE_256M,
	LAWAR_SIZE_512M,
	LAWAR_SIZE_1G,
	LAWAR_SIZE_2G
};

struct srio_port_info {
	uint64_t range_start;
	uint64_t range_size;
};

struct srio_dev;

int fsl_srio_uio_init(struct srio_dev **srio);
int fsl_srio_uio_finish(struct srio_dev *sriodev);
int fsl_srio_connection(struct srio_dev *sriodev, uint8_t port_id);
int fsl_srio_set_obwin_attr(struct srio_dev *sriodev, uint8_t port_id,
		      uint8_t win_id, uint32_t rd_attr, uint32_t wr_attr);
int fsl_srio_set_obwin(struct srio_dev *sriodev, uint8_t port_id,
		       uint8_t win_id, uint64_t ob_win_phys,
		       uint64_t ob_win_sys, size_t win_size);
int fsl_srio_set_ibwin(struct srio_dev *sriodev, uint8_t port_id,
		      uint8_t win_id, uint64_t ib_win_phys, uint64_t ib_win_sys,
		      size_t win_size);
int fsl_srio_clr_bus_err(struct srio_dev *sriodev);
int fsl_srio_port_connected(struct srio_dev *sriodev);
int fsl_srio_get_port_num(struct srio_dev *sriodev);
int fsl_srio_get_port_info(struct srio_dev *sriodev, uint8_t port_id,
			   struct srio_port_info *port, void **range_virt);
int fsl_srio_enable_accept_all(struct srio_dev *sriodev, uint8_t port_id);
int fsl_srio_disable_accept_all(struct srio_dev *sriodev, uint8_t port_id);
int fsl_srio_set_deviceid(struct srio_dev *sriodev, uint8_t port_id,
			  uint32_t dev_id);
int fsl_srio_set_targetid(struct srio_dev *sriodev, uint8_t port_id,
			  uint8_t win_id, uint32_t target_id);
int fsl_srio_set_seg_num(struct srio_dev *sriodev, uint8_t port_id,
			 uint8_t win_id, uint8_t seg_num);
int fsl_srio_get_seg_num(struct srio_dev *sriodev, uint8_t port_id,
			 uint8_t win_id);
int fsl_srio_set_seg_attr(struct srio_dev *sriodev, uint8_t port_id,
			 uint8_t win_id, uint8_t seg_id,
			 uint32_t rd_attr, uint32_t wr_attr);
int fsl_srio_set_seg_sgtgtdid(struct srio_dev *sriodev, uint8_t port_id,
			      uint8_t win_id, uint8_t seg_id, uint8_t tdid);
int fsl_srio_set_subseg_num(struct srio_dev *sriodev, uint8_t port_id,
			    uint8_t win_id, uint8_t subseg_num);
int fsl_srio_get_subseg_num(struct srio_dev *sriodev, uint8_t port_id,
			    uint8_t win_id);

/*
 * Set traffic management mode
 * 0 means traffic management disabled
 * 1 means basic traffic management mode
 * 2-0xF is reserved
 */
void fsl_srio_set_tmmode(struct srio_dev *sriodev, uint8_t mode);

/* Disable output buffer drain */
void fsl_srio_drain_disable(struct srio_dev *sriodev, uint8_t port_id);

/*
 * Enable output buffer drain.
 * When set, the output drains packets from the outbound buffer and does not
 * send them out.
 */
void fsl_srio_drain_enable(struct srio_dev *sriodev, uint8_t port_id);

/* Disable port - port is unable to receive/transmit */
void fsl_srio_port_disable(struct srio_dev *sriodev, uint8_t port_id);

/* Enable port - port can receive/transmit */
void fsl_srio_port_enable(struct srio_dev *sriodev, uint8_t port_id);

/*
 * Drop packet disable.
 * This bit is used with the Stop on Port Failed -encountered Enable bit to
 * force certain behavior when the Error Rate Failed Threshold has been met
 * or exceeded.
 */
void fsl_srio_drop_disable(struct srio_dev *sriodev, uint8_t port_id);

/*
 * Drop packet enable.
 * This bit is used with the Stop on Port Failed -encountered Enable bit to
 * force certain behavior when the Error Rate Failed Threshold has been met
 * or exceeded.
 */
void fsl_srio_drop_enable(struct srio_dev *sriodev, uint8_t port_id);

/*
 * Set packet live time out value
 * The packet time-to-live counter prevents the local processor from being
 * stalled when packets cannot be successfully transmitted.
 * The value of this register should always be larger than the link time.
 */
void fsl_srio_set_packet_timeout(struct srio_dev *sriodev,
				 uint8_t port_id, uint32_t value);

/*
 * Set port link time out
 * Timeout value is a nanosecond. All zeros disables the link timeout timer.
 * This value is loaded each time the link time-out timer starts.
 */
void fsl_srio_set_link_timeout(struct srio_dev *sriodev, uint32_t value);

/* Return width of the ports after initialized */
int fsl_srio_port_width(struct srio_dev *sriodev, uint8_t port_id);

/* Return file descriptor of the SRIO device */
int fsl_srio_fd(struct srio_dev *sriodev);

/* Enable SRIO interrupt */
int fsl_srio_irq_enable(struct srio_dev *sriodev);

/* Disable SRIO interrupt */
int fsl_srio_irq_disable(struct srio_dev *sriodev);

/*
 * Process SRIO error interrupt
 * Currently, this function only prints the error information, then clears the
 * error interrupt.
 */
void fsl_srio_irq_handler(struct srio_dev *sriodev);

/*
 * This function sets the threshold value for reporting
 * an error condition due to a degrading link
 */
int fsl_srio_set_err_rate_degraded_threshold(struct srio_dev *sriodev,
					uint8_t port_id, uint8_t threshold);

/*
 * This function sets the threshold value for reporting
 * an error condition due to a possibly broken link.
 */
int fsl_srio_set_err_rate_failed_threshold(struct srio_dev *sriodev,
					uint8_t port_id, uint8_t threshold);

/*
 * This function provides the threshold value for the number of
 * consecutive logical retries
 * op: operation when this error occurs.
 *	0 - Port keeps transmitting;
 *	1 - Drop packet enable;
 *	2 - Stop on port failed-encountered enable;
 *	3 - Drop packet and stop on port failed-encountered enable.
 */
int fsl_srio_set_phy_retry_threshold(struct srio_dev *sriodev, uint8_t port_id,
				uint8_t threshold, uint8_t op);

#endif
