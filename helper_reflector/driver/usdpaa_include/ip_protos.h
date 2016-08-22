/**
 \file ip_protos.h
 \brief This file contails data structure, and defines related
 to different IP Protocol Types
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

#ifndef __IP_PROTOS_H
#define __IP_PROTOS_H

#include "ppam_if.h"

#include "net/annotations.h"

/**< Definition of Protocol Handler function pointer */
typedef enum IP_STATUS (*ip_proto_handler_t) (const struct ppam_rx_hash *ctxt,
					      const struct annotations_t *notes,
					      void *ip_data);

struct ip_protos_bundle_t {
	ip_proto_handler_t handler;
};

/**
\brief Protocol Handler Structure
\details This object contains the Different Protocol handler function pointers
*/
struct ip_protos_t {
	struct ip_protos_bundle_t proto_data[IPPROTO_MAX];
	/**< Protocol Handler function pointer Array*/
};

/**
 \brief Create IP protos
 \return Protocol Handler
 */
int ip_protos_init(struct ip_protos_t *protos);

/**
 \brief set ip protocol handler
 \param[inout] protos Pointer to the protocol handler structure
 \param[in] handler protocol handler function pointer
 \param[in] proto_id Protocol Type
 */
void ip_protos_set_handler(struct ip_protos_t *protos,
			   ip_proto_handler_t handler,
			   void *, int proto_id);

/**
 \brief		Executes the Protocol handler depending on th eProtocol Id
 \param[in]	ctxt		Context for FQ
 \param[in]	proto_id	Protocol Id
 \param[in]	notes		Pointer to the Prepended Data from FMAN
 \param[in]	ip_hdr		Pointer to the IP Header in the Frame
 \return	Returns Status
 */
static inline enum IP_STATUS ip_protos_exec(const struct ppam_rx_hash *ctxt,
					    int proto_id,
					    const struct annotations_t *notes,
					    struct iphdr *ip_hdr)
{
	void *ip_data;
	ip_proto_handler_t handler;
	enum IP_STATUS retval;

	retval = IP_STATUS_STOP;
	handler = ctxt->protos->proto_data[proto_id].handler;
	if (unlikely(handler == NULL)) {
		printf("HANDLER IS NULL");
	} else {
		ip_data = (void *)((char *) ip_hdr + ((ip_hdr->ihl) * 4));
		retval = handler(ctxt, notes, ip_data);
	}

	return retval;
}

#endif	/* __IP_PROTOS_H */
