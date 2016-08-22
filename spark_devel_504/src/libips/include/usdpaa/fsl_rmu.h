/* Copyright (c) 2012 Freescale Semiconductor, Inc.
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

#ifndef FSL_RMU_H
#define FSL_RMU_H

#define RMU_UNIT_MSG0 0
#define RMU_UNIT_MSG1 1
#define RMU_UNIT_DBELL 4

#define SRIO_MAX_PORT	2
#define SRIO_MAX_DEVID 0xffff

#define MSG_TX_DESC_RING_ENTRY	32
#define MSG_TX_BUFF_RING_ENTRY	32
#define MSG_RX_BUFF_RING_ENTRY	32
#define DBELL_RX_BUFF_RING_ENTRY	32

#define MSG_DESC_SIZE	32
#define MSG_FRAME_SIZE	4096
#define DBELL_FRAME_SIZE	0x08

struct rmu_ring {
	void *virt;
	dma_addr_t phys;
	uint32_t cell_size;
	uint32_t entries;
};

struct rmu_unit {
	void *regs;	/* unit register map */
	int32_t fd;	/* unit uio device fd */
};

struct dbell_info {
	uint16_t res1;
	uint16_t tid;
	uint16_t sid;
	uint16_t info;
};

struct msg_tx_info {
	uint32_t port;
	uint32_t destid;
	uint32_t mbox;
	uint32_t priority;
	void *virt_buffer;
	dma_addr_t phys_buffer;
	size_t len;
};

int fsl_rmu_unit_uio_init(struct rmu_unit **unit, uint8_t unit_id);
int fsl_rmu_unit_uio_finish(struct rmu_unit *unit);
int fsl_msg_send_err_clean(struct rmu_unit *unit);
int fsl_dbell_send_err_clean(struct rmu_unit *unit);
int fsl_msg_send_wait(struct rmu_unit *unit);
int fsl_dbell_send_wait(struct rmu_unit *unit);
int fsl_rmu_msg_inb_handler(struct rmu_unit *unit,
				void *info, struct rmu_ring *rx_ring);
int fsl_rmu_dbell_inb_handler(struct rmu_unit *unit,
				void *info, struct rmu_ring *rx_ring);
int fsl_rmu_dbell_send(struct rmu_unit *unit,
		uint8_t port, uint32_t destid, uint8_t priority, uint16_t data);
int fsl_add_outb_msg(struct rmu_unit *unit, struct rmu_ring *desc_ring,
				struct msg_tx_info *tx_info);
int fsl_rmu_msg_outb_init(struct rmu_unit *unit, struct rmu_ring *desc_ring);
int fsl_rmu_msg_inb_init(struct rmu_unit *unit, struct rmu_ring *rx_ring);
int fsl_rmu_dbell_inb_init(struct rmu_unit *unit, struct rmu_ring *rx_ring);
#endif
