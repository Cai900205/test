/*
 * Copyright 2012 Freescale Semiconductor, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor "AS IS" AND ANY
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

#ifndef _FRA_FMAN_PORT_H
#define _FRA_FMAN_PORT_H

#include <fra_common.h>
#include <usdpaa/usdpaa_netcfg.h>

struct fman_rx_nonhash {
	struct qman_fq fq;
	void *pvt;
	nonhash_handler handler;
} ____cacheline_aligned;

struct fra_fman_port {
	struct list_head node;
	const struct fra_fman_port_cfg *cfg;
	const struct fm_eth_port_cfg *port_cfg;
	uint32_t txfqs_num;
	struct qman_fq *tx_fqs;
	struct fman_rx_nonhash rx_error;
	struct fman_rx_nonhash rx_default;
	struct fman_rx_nonhash tx_error;
	struct fman_rx_nonhash tx_confirm;
	struct list_head list; /* list of "fman_pcd_range" */
} ____cacheline_aligned;

struct fman_rx_hash {
		struct qman_fq fq;
#ifdef FRA_ORDER_RESTORATION
		/* Rather than embedding a whole ORP object, we embed only the
		 * ORPID so that it takes less (stashable) space. We can plug
		 * the ORPID into a temporary ORP object on the fly when
		 * performing ORP-enabled enqueues. */
		u32 orp_id;
#endif
		struct hash_opt opt;
		void *pvt;
		hash_handler handler;
} ____cacheline_aligned;

struct fman_pcd_range {
	struct list_head list;
	uint32_t count;
	struct fman_rx_hash hash[0];
} ____cacheline_aligned;

/* Queries and returns the pointer of the FMan port according to the name */
struct fra_fman_port *get_fra_fman_port(const char *name);

/*
 * Each pcd range contains a set of the frame queues, each queue has a hash
 * opt, which is used to indicate the frame queue to enqueue.
 * This function returns the pointer of the hash opt
 */
struct hash_opt *
fman_pcd_range_get_opt(struct fman_pcd_range *pcd_range, int idx);

/* Bind the hash opt to a fman tx fq */
int opt_bindto_fman_tx(struct hash_opt *opt, struct fra_fman_port *port,
		       int idx);

/*
 * This function creates rx error and rx default frame queues using the
 * default configuration. If non_hash is false, the hash frame queues are
 * also created using the specific work queue and channel ID.
 */
int fman_rx_init(struct fra_fman_port *port, int non_hash, uint8_t wq,
		 u16 channel);

/*
 * Listen to rx default frame, if received frame, handler function will be
 * called, and pvt will be passed as a parameter.
 */
void fman_rx_default_listen(struct fra_fman_port *port,
			    void *pvt, nonhash_handler handler);

/*
 * Listen to rx error frame, if received frame, handler function will be
 * called, and pvt will be passed as a parameter.
 */
void fman_rx_error_listen(struct fra_fman_port *port,
			  void *pvt, nonhash_handler handler);
/*
 * Listen to rx hash frame, if received frame, the handler function will be
 * called, and pvt will be passed as a parameter.
 */
void fman_rx_hash_listen(struct fra_fman_port *port,
			 void *pvt, hash_handler handler);

/* This function creates tx error tx confirm and tx frame queues */
int fman_tx_init(struct fra_fman_port *port, uint32_t fqid,
		 int fqs_num, uint8_t wq);

/*
 * Listen to tx confirm frame, if received frame, handler function will be
 * called,  and pvt will be passed as a parameter.
 */
void fman_tx_confirm_listen(struct fra_fman_port *port,
			    void *pvt, nonhash_handler handler);

/*
 * Listen to tx error frame, if received frame, handler function will be
 * called, and pvt will be passed as a parameter.
 */
void fman_tx_error_listen(struct fra_fman_port *port,
			  void *pvt, nonhash_handler handler);

/*
 * Send a packet described by fd, using the specific FMan port and frame queue
 * defined in hash_opt
 */
int fman_send_frame(struct hash_opt *opt, const struct qm_fd *fd);

/* Initialize the FMan port according to the configuration */
int fman_port_init(const struct fra_fman_port_cfg *cfg,
		   const struct usdpaa_netcfg_info *netcfg);

/* Enable fman port receiving packets */
void fman_port_enable_rx(const struct fra_fman_port *port);

/*
 * Disable FMan port receiving
 * Teardown the rx error rx default rx hash frame queues
 */
void fman_port_finish_rx(struct fra_fman_port *port);

/* Teardown the tx error tx confirm and tx frame queues */
void fman_port_finish_tx(struct fra_fman_port *port);

/* Release FMan port resource */
void fman_port_finish(struct fra_fman_port *port);

/* Release all FMan ports */
void fman_ports_finish(void);

#endif /* _FRA_FMAN_PORT_H */
