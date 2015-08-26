/*
 \file ipsecfwd.h
 \brief Contains macros and inline functions common to all applications
 */
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

#ifndef __IPSECFWD_H__
#define __IPSECFWD_H__

#include <usdpaa/compat.h>
#include <usdpaa/fsl_qman.h>

#include "ip/ip_common.h"	/* ip_statistics_t */
#include "ipsec/ipsec.h"
#include "ipsec/ipsec_init.h"
#include "ipsec/ipsec_encap.h"
#include "ipsec/ipsec_decap.h"
#include "ipsec/ipsec_common.h"

#include <unistd.h>

#define ETHERNET_ADDR_MAGIC	0x0200
#define LINKLOCAL_NODES_2EXP	10
#define IFACE_COUNT		12
#define LINKLOCAL_NODES		(1 << LINKLOCAL_NODES_2EXP)
#define IP_RC_EXPIRE_JIFFIES	(20*60*JIFFY_PER_SEC)

struct thread_data_t {
	/* Inputs to run_threads_custom() */
	int cpu;
	int index;
	int (*fn)(struct thread_data_t *ctx);
	int total_cpus;
	/* Value used within 'fn' - handle to the pthread; */
	pthread_t id;
	/* Stores fn() return value on return from run_threads_custom(); */
	int result;
	/* application-specific */
	void *appdata;
} ____cacheline_aligned;

#ifdef STATS_TBD
/**
 \brief Initialize IPSec Statistics
 \param[in] void
 \param[out] struct ip_statistics_t *
 */
extern struct ip_statistics_t *ipfwd_stats_init(void);
#endif

extern struct ipsec_context_t *g_ipsec_ctxt[];
extern struct qman_fq *g_splitkey_fq_to_sec;
extern struct qman_fq *g_splitkey_fq_from_sec;

extern const struct qman_fq_cb ipfwd_rx_cb_err;
extern const struct qman_fq_cb ipfwd_tx_cb_err;
extern const struct qman_fq_cb ipfwd_tx_cb_confirm;
extern const struct qman_fq_cb ipfwd_rx_cb;
extern const struct qman_fq_cb ipfwd_rx_cb_pcd;
extern const struct qman_fq_cb ipfwd_tx_cb;
extern const struct qman_fq_cb ipsecfwd_rx_cb_pcd;
extern const struct qman_fq_cb ipsecfwd_tx_cb;
extern const struct qman_fq_cb ipfwd_split_key_cb;
extern int32_t ipsecfwd_create_sa(struct app_ctrl_ipsec_info *ipsec_info,
			struct ipsec_stack_t *ipsec_stack);

#endif	/* __IPSECFWD_H__ */
