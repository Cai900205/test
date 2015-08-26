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

#ifndef __PPAM_IF_H
#define __PPAM_IF_H

#include <stdint.h>

/* structs required by ppac.c */
struct ppam_interface {
	/* We simply capture Tx FQIDs as they initialise, in order to use them
	 * when Rx FQs initialise. Indeed, the FQID is the only info we're
	 * passed during Tx FQ init, which is due to the programming model;
	 * we're hooked only on receive, not transmit (it's our receive handler
	 * that *requests* transmit), and the FQ objects used for Tx are
	 * internal to ppac.c and not 1-to-1 with FQIDs. */
	unsigned int num_tx_fqids;
	uint32_t *tx_fqids;
	/* Table descriptors for header manipulations that may be attached
	* to the interface*/
	int *hhm_td;
	int *local_td_ib;
	int *local_td_ob;
	int *vipsec_rx;

	/* Macless interface that receives traffic from this interface */
	int macless_ifindex;
	/* Number of macless interfaces */
	int macless_count;
	/* Statistics counter storage */
	int stats_cnt;
};
struct ppam_rx_error { };
struct ppam_rx_default {
	/* Is non-zero if this network interface is an offline port */
	int am_offline_port;
	union {
		/* Rx-default processing settings */
		struct {
			/* What does Rx handling for an offline port need to do?
			 * Eg. after flipping headers, where does it transmit?
			 * If that's determined per-interface (rather than, say,
			 * per-packet), then store it here... TBD */
			int dummy;
		} offline;
		struct {
			/* Cache the Tx FQID of an offline port here. All Rx
			 * traffic for this interface will be forwarded to that
			 * port unless (a) offline_fqid is zero (meaning there
			 * were no offline ports available), or (b) offline
			 * forwarding is disabled at run-time ("pure reflector"
			 * mode). */
			uint32_t offline_fqid;
		} regular;
	};
};
struct ppam_tx_error { };
struct ppam_tx_confirm { };
struct ppam_rx_hash {
	/* A more general network processing application (eg. routing) would
	 * take into account the contents of the recieved frame when computing
	 * the appropriate Tx FQID. These wrapper structures around each Rx FQ
	 * would typically contain state to assist/optimise that choice of Tx
	 * FQID, as that's one of the reasons for hashing Rx traffic to multiple
	 * FQIDs - each FQID carries proportionally fewer flows than the network
	 * interface itself, and a proportionally higher likelihood of bursts
	 * from the same flow. In "reflector" though, the choice of Tx FQID is
	 * constant for each Rx FQID, and so the only "optimisation" we can do
	 * is to store tx_fqid itself! */
	uint32_t tx_fqid;
};

#endif	/* __PPAM_IF_H */
