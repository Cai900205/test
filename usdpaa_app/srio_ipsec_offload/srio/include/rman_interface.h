/* Copyright (c) 2014 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions aremet:
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

#ifndef _RMAN_IF_H
#define _RMAN_IF_H

#include <rman_cfg.h>
#include <rman_bpool.h>
#include <usdpaa/fsl_qman.h>
#include <usdpaa/fsl_bman.h>
#include <usdpaa/fsl_rman.h>
#include <usdpaa/fsl_srio.h>

struct rman_rx;
struct rman_tx;

#define RM_FD_SIZE		0x20
#define RM_DATA_OFFSET		0x40
#define FD_GET_STATUS(fd)	(((fd)->status) & 0x07ffffff)
#define FD_CLEAR_STATUS(fd)	(((fd)->status) = 0)

#define RM_MBOX_MG_SHIFT 5
#define RM_MBOX_ML_MASK 0x1f
struct rman_rx_cfg {
	uint8_t rio_port;
	uint8_t port_mask;
	uint16_t sid;
	uint16_t sid_mask;
	uint32_t fqid;
	enum RMAN_FQ_MODE fq_mode;
	uint8_t wq;
	struct rio_tran	*tran;
};

struct rman_tx_cfg {
	uint8_t rio_port;
	uint8_t fqs_num;
	uint8_t wq;
	uint16_t did;
	uint32_t fqid;
	struct rio_tran	*tran;
};

struct rman_inb_md {
	/* word0 */
	uint8_t ftype:4; /* rio type */
	uint8_t __reserved0:4;
	uint8_t __reserved1[3];
	/* word1 */
	uint32_t __reserved2;
	/* word2 */
	uint32_t __reserved3;
	/* word3 */
	uint16_t sid;
	uint16_t src;
	/* word4 */
	uint8_t __reserved4:3;
	uint8_t flowlvl:3;
	uint8_t __reserved5:2;
	uint8_t sint:4;
	uint8_t __reserved6:4;
	uint16_t other_attr;
	/* word5 */
	uint16_t did;
	uint16_t dest;
	/* word6 */
	uint32_t __reserved7;
	/* word7 */
	uint32_t count;
};

struct rman_outb_md {
	/* word0 */
	uint8_t ftype:4; /* Descriptor type select */
	uint8_t br:1; /* Buffer release enable */
	uint8_t so:1; /* Strict ordering */
	uint8_t cs:1; /* Completion status */
	uint8_t es:1; /* Error status */
	uint8_t __reserved0[2];
	union {
		uint8_t retry; /* Retry error threshold */
		uint8_t hop_count; /* Hop count in RapidIO port-write packet*/
	} __packed;
	/* word1 */
	uint32_t address;
	/* word2 */
	uint32_t __reserved1:8;
	uint32_t status_fqid:24;
	/* word3 */
	uint16_t did;
	uint16_t dest;
	/* word4 */
	uint8_t mm:1; /* Multicast mode */
	uint8_t __reserved2:2;
	uint8_t flowlvl:3;
	uint8_t __reserved3:2;
	uint8_t tint:4;
	uint8_t __reserved4:4;
	uint16_t other_attr;
	/* word5 */
	uint32_t message_group;
	/* word6 */
	uint32_t message_list;
	/* word7 */
	uint32_t count;
};
/* If create frame queue directly this function returns 1,
 * Otherwise, this function  returns the number of receive frame queue
 * calculated according to transaction configuration and algorithmic rule
 */
int rman_rx_get_fqs_num(struct rman_rx *rx);

/* This function returns this ib index which this rx socket binded to */
int rman_rx_get_ib(struct rman_rx *rx);

/* This function returns this cu index which this rx socket binded to */
int rman_rx_get_cu(struct rman_rx *rx);

/*
 * This function requests the RMan hardware resource-classification unit
 * Returns the pointer of rman_rx on success or %NULL on failure.
 */
struct rman_rx *
rman_rx_init(uint32_t fqid, int fq_mode, struct rio_tran *tran);

/* Configures classification unit to receive the specific rapidio messages. */
int rman_rx_listen(struct rman_rx *rx, uint8_t port, uint8_t port_mask,
		   uint16_t sid, uint16_t sid_mask);

/* Enables rman_rx to receive packets */
void rman_rx_enable(struct rman_rx *rx);

/* Stop rman_rx receiving messages, but do not release any resource*/
void rman_rx_disable(struct rman_rx *rx);

/* Stops classification unit receiveing,  and release rman_rx all resource */
void rman_rx_finish(struct rman_rx *rx);

/*
 * Creates tx frame queue and tx status frame queue
 * Returns the pointer of rman_tx on success or %NULL on failure.
 */
struct rman_tx *
rman_tx_init(uint8_t port, int fqid, int fqs_num, uint8_t wq,
	     struct rio_tran *tran);

/*
 * Enable tx socket multicast mode
 * There is only mailbox transaction supports multicast mode.
 * Messages are limited to one segment and 256 bytes or less
 * when this mode is enabled
 */
int rman_tx_enable_multicast(struct rman_tx *tx, int mg, int ml);

/* Sets rman_tx to receive the completed or/and error status frame */
int rman_tx_status_listen(struct rman_tx *tx, int error_flag,
			  int complete_flag);

/* Connects rman_tx socket to the destination device */
int rman_tx_connect(struct rman_tx *tx, int did);

/* Returns pointer of the RMan tx frame queue. */
struct qman_fq *rman_tx_get_fq(struct rman_tx *tx, int idx);

/* Releases RMan tx and tx status frame queues */
void rman_tx_finish(struct rman_tx *tx);

/* Return pointer to SRIO device */
struct srio_dev *rman_if_get_sriodev(void);

/*
 * This function performs checking SRIO port connection.
 * If the port has been connected returns 0,
 * otherwise returns an error number.
 */
int rman_if_port_connect(uint8_t port_number);

/* Stop all the used SRIO ports */
void rman_if_ports_stop(void);

/* Start all the used SRIO ports */
void rman_if_ports_start(void);

/*
 * Output RMan interface information about SRIO port width and
 * the numbers of sockets and frame queues
 */
void rman_if_status(void);

/* Initializes RMan interface according to RMan configuration */
int rman_if_init(struct srio_dev *sriodev, const struct rman_cfg *cfg);

/* Reconfigure RMan device */
void rman_if_reconfig(const struct rman_cfg *cfg);

/* Releases RMan interface resource */
void rman_if_finish(void);

/* Disable all the running RMan inbound blocks */
void rman_if_rxs_disable(void);

/* Enable all the ready RMan inbound blocks */
void rman_if_rxs_enable(void);

#endif	/* _RMAN_IF_H */
