/**
 \file ipfwd.h
 \brief Common datatypes, externs and hash-defines of IPv4 Forwarding
	 Application
 */
/*
 * Copyright (C) 2012 Freescale Semiconductor, Inc.
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

#ifndef __IPFWD_H
#define __IPFWD_H

#include <usdpaa/compat.h>
#include <usdpaa/fsl_qman.h>

#include "ip/ip_common.h"	/* ip_statistics_t */

#include <assert.h>
#include <unistd.h>

#define ETHERNET_ADDR_MAGIC	0x0200
#ifdef STATS_TBD
extern struct ip_statistics_t *ipfwd_stats_init(void);
#endif

extern const struct qman_fq_cb ipfwd_rx_cb_err;
extern const struct qman_fq_cb ipfwd_tx_cb_err;
extern const struct qman_fq_cb ipfwd_tx_cb_confirm;
extern const struct qman_fq_cb ipfwd_rx_cb;
extern const struct qman_fq_cb ipfwd_rx_cb_pcd;
extern const struct qman_fq_cb ipfwd_tx_cb;

#endif	/* __IPFWD_H */
