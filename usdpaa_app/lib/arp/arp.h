/**
 \file arp.h
 \brief This file contains functions for managing the arp table
 */
/*
 * Copyright (C) 2010 - 2012 Freescale Semiconductor, Inc.
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
#ifndef __ARP_H
#define __ARP_H

#include "ppam_if.h"
#include <ppac_interface.h>

#include "net/neigh.h"
#include "net/annotations.h"

#define ARP_RETRANSMIT_INTERVAL	5000

/** \brief		Initializes the ARP table
 *  \param[out] nt	ARP table
 *  \return		On success, zero. On error, a negative value
 as per errno.h
 */
int arp_table_init(struct neigh_table_t *nt);

/**
 \brief Handler function when arp packet received
 \param[in] notes pointer to Annotations structure
 \param[in] data  pointer to packet data
 \param[out] NULL
 */
void arp_handler(const struct ppam_interface *p,
		 const struct annotations_t *notes, void *data);

#endif	/* __ARP_H */
