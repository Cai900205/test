/*
 * Copyright (C) 2011 Freescale Semiconductor, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LIB_IPSEC_IPSEC_DECAP_H
#define LIB_IPSEC_IPSEC_DECAP_H 1

#include "ip/ip.h"
#include "ip/ip_common.h"
#include "ipsec/ipsec_common.h"
#include "ipsec/ipsec.h"
#include "net/annotations.h"
#include "statistics.h"
#include "ip/ip_appconf.h"

/**
 \brief IPsec decapsulation work producer.
	When IP layer protocol determines that the next protocol
	layer is ESP, this protocol handler is invoked. The packet
	it receives in an encrypted packet with an outer clear-text
	IP packet. This routine enqueues the packet to the right
	Frame Queue.
 \param[in] ip_ctxt IP Context
 \param[in] notes Annotations
 \param[in] ip_hdr Pointer to the IP Header
 \return Status
 */
enum IP_STATUS ipsec_decap_send(const struct ppam_rx_hash *ip_ctxt,
				struct annotations_t *notes, void *ip_hdr);

/**
 \brief IPsec decapsulation callback handler.
	When SEC40 completes decapsulating packet, that means, it
	decrypts the inner IP header and strips off the outer
	clear-text IP header. SEC40 signals job completion to QMAN
	and via the dqrr_entry_handler one of the cores executes
	this routine. Once in this routine the compound frame
	description is converted to simple, statistics are updated
	and then the packet is handed over to IP layer for validity
	check and sending the packet through egress port.
 \param[in] ipsec_ctxt Per FQ Context
 \param[in] fd Pointer to the Frame Descriptor
 \return none
 */
void ipsec_decap_cb(const struct ipsec_context_t *ipsec_ctxt,
		    const struct qm_fd *fd, void *data __always_unused);

/**
 \brief This routine is called on a per tunnel creation basis.
	For decapsulation process, SEC40 initialization descriptor
	(ID) is created here. The pair of Frame Queues created
	for this tunnel in order to interact with SEC40 are
	initialized here.
 \param[in] entry IPSec tunnel entry
 \return none
 */
void ipsec_decap_init(struct ipsec_tunnel_t *entry);

#endif /* ifndef LIB_IPSEC_IPSEC_DECAP_H */
