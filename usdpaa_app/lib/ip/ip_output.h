/**
 \file ip_output.h
 \brief This file captures the post-routing functionality and the
 transmission of the IP packet
 */
/*
 * Copyright (C) 2010,2011 Freescale Semiconductor, Inc.
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

#ifndef __IP_OUTPUT_H
#define __IP_OUTPUT_H

#include "ppam_if.h"

#include "net/annotations.h"

/**
 \brief			Sends IP Packet to respective netdev. If packet length > next_hop mtu, call
			ip_fragment
 \param[in] ctxt	Context
 \param[in] notes	Annotations
 \param[in] ip_hdr	Pointer to the IP Header
 */
enum IP_STATUS ip_send(const struct ppam_rx_hash *ctxt,
		       struct annotations_t *notes, struct iphdr *ip_hdr);

/**
 \brief		Call intervening POSTROUTING hooks for each frame
 \param[in]	ctxt	Context
 \param[in]	notes	Annotations
 \param[in]	ip_hdr	Pointer to the IP Header
 */
enum IP_STATUS ip_output(const struct ppam_rx_hash *ctxt,
			 struct annotations_t *notes,
			 struct iphdr *ip_hdr);

/**
 \brief		Find the correct neighbor for this frame, using ARP tables
 \param[in]	ctxt	Context
 \param[in]	notes	Annotations
 \param[in]	ip_hdr	Pointer to the IP Header
 */
enum IP_STATUS ip_output_finish(const struct ppam_rx_hash *ctxt,
				struct annotations_t *notes,
				struct iphdr *ip_hdr,
				enum state source);

#endif	/* __IP_OUTPUT_H */
