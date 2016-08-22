/* Copyright (c) 2011-2012 Freescale Semiconductor, Inc.
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

#ifndef _FSL_RMAN_H
#define _FSL_RMAN_H

#include <limits.h>
#include <usdpaa/fsl_rman_ib.h>

#define RMAN_MAX_NUM_OF_CHANNELS 2

/* Rman interrupt error mask */
/* Outbound transaction error */
#define RMAN_OTE_ERROR_MASK	0x01
/* Inbound transaction error */
#define RMAN_ITE_ERROR_MASK	0x02
/* Outbound frame queue enqueue rejection */
#define RMAN_OFER_ERROR_MASK	0x04
/* Inbound frame queue enqueue rejection */
#define RMAN_IFER_ERROR_MASK	0x08
/* Buffer allocation error */
#define RMAN_BAE_ERROR_MASK	0x10
/* Type9 interrupt coalescing drop threshold exceed */
#define RMAN_T9IC_ERROR_MASK	0x20
/* Type8 interrypt coalescing drop threshold exceed */
#define RMAN_T8IC_ERROR_MASK	0x40
/* Message format error */
#define RMAN_MFE_ERROR_MASK	0x80

struct rman_cfg {
	uint8_t fq_bits[RIO_TYPE_NUM];
	uint8_t bpid[RIO_TYPE_NUM];
	uint8_t md_create;
	uint8_t osid; /* Outbound segmentation interleaving disable */
	uint8_t sgbpid;
	uint8_t efq; /* Error frame queue enable */
};

struct rman_dev;

/**
 * rman_global_fd - get the file descriptor refers to global register
 *
 * This function returns the file descriptor refers to global register,
 * application can read or detect SIGIO signal from this fd to
 * get interrupt events .
 */
int rman_global_fd(void);

/* Enable rman interrupt */
void rman_interrupt_enable(void);

/* Get rman interrupt status */
int rman_interrupt_status(void);

/* Clear rman interrupt status */
void rman_interrupt_clear(void);

/* Return inbound message manager busy status */
int rman_rx_busy(void);

/* Return outbound message manager busy status*/
int rman_tx_busy(void);

/* Reset rman device */
void rman_reset(void);

/* Return inbound block number */
int rman_get_ib_number(struct rman_dev *rmdev);

/**
 * rman_get_channel_id - get the default transmission channel id
 * @rmdev: RMan device info
 * @index: RMan channel index
 *
 * RMan device has two channels, each channel is assigned to QMan channel.
 * This function returns the corresponding QMan channel id.
 */
int rman_get_channel_id(const struct rman_dev *rmdev, int index);

/**
 * rman_dev_init - initialize the RMan device
 *
 * This function firstly opens RMan uio file, then maps the RMan register space,
 * finally initialize inbound block units.
 * Returns the pointer of rman_dev on success or %NULL on failure.
 */
struct rman_dev *rman_dev_init(void);

/**
 * rman_dev_config - configure the RMan device
 * @rmdev: RMan device info
 * @cfg: RMan device configuration
 *
 * This function configures RMan according to RMan configuration
 * Returns %0 on success or %-EINVAL on failure.
 */
int rman_dev_config(struct rman_dev *rmdev, const struct rman_cfg *cfg);

/**
 * rman_dev_finish - release the RMan resource
 * @rmdev: RMan device info
 *
 * Releases All the RMan resource.
 */
void rman_dev_finish(struct rman_dev *rmdev);

#endif /*_ FSL_RMAN_H */
