/**
 \file ip_accept.h
 \brief This file is designed to encapsulate all the validations needed to
  be done to accept an IP Packet.
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

#ifndef __IP_ACCEPT_H
#define __IP_ACCEPT_H

#include "ppam_if.h"

#include "net/annotations.h"

/**
 \brief		Accept a new IPv4 datagram. Statistics will be updated
 \param[in]	ctxt	Context
 \param[in]	notes	Annotaion
 \param[in]	ip_hdr	Pointer to the header of the IP Packet received
 \return	Status
 \note		After this function completes, all ingress IP statistics have
		been updated, and the frame is now ready for processing
 */
enum IP_STATUS ip_accept_preparsed(const struct ppam_rx_hash *ctxt,
				   struct annotations_t *notes,
				   struct iphdr *ip_hdr,
				   enum state);

/**
 \brief		IP Options processing, and if dst is still null, call ip_route_input()
 \param[in]	ctxt	Context
 \param[in]	notes	Annotaion
 \param[in]	ip_hdr	Pointer to the header of the IP Packet
 \return	Status
 */
enum IP_STATUS ip_accept_finish(const struct ppam_rx_hash *ctxt,
				struct annotations_t *notes,
				struct iphdr *ip_hdr,
				enum state);

#endif	/* __IP_INPUT_H */
