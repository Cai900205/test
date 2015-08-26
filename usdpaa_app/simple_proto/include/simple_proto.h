/* Copyright 2013 Freescale Semiconductor, Inc.
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __SIMPLE_PROTO_H
#define __SIMPLE_PROTO_H

#include <argp.h>
#include <inttypes.h>

#include <crypto/test_utils.h>
#include <crypto/sec.h>
#include <crypto/thread_priv.h>
#include <crypto/qman.h>

#include "test_vector.h"
#include "common.h"
#include "macsec.h"
#include "wimax.h"
#include "pdcp.h"
#include "srtp.h"
#include "wifi.h"
#include "rsa.h"
#include "tls.h"
#include "ipsec.h"
#include "mbms.h"

/* prepare test buffers, fqs, fds routines */
int prepare_test_frames(struct test_param *crypto_info);
void set_enc_buf(void *params, struct qm_fd fd[]);
void set_dec_buf(void *params, struct qm_fd fd[]);

/* validate test routines */
static int validate_params(uint32_t cmd_args, uint32_t proto_args,
			   struct test_param *crypto_info);
int test_enc_match(void *params, struct qm_fd fd[]);
int test_dec_match(void *params, struct qm_fd fd[]);

error_t parse_opt(int opt, char *arg, struct argp_state *state);

struct protocol_info *(*register_protocol[])(void) = {
		register_macsec,
		register_wimax,
		register_pdcp,
		register_srtp,
		register_wifi,
		register_rsa,
		register_tls,
		register_ipsec,
		register_mbms
};

/* helper routines */
static void set_crypto_cbs(struct test_cb *crypto_cb);
int get_num_of_iterations(void *params);
void set_num_of_iterations(void *params, unsigned int itr_num);
inline int get_num_of_buffers(void *params);
inline enum test_mode get_test_mode(void *params);
inline uint8_t requires_authentication(void *);
inline long get_num_of_cpus(void);
inline pthread_barrier_t *get_thread_barrier(void);
int register_modules(void);
void unregister_modules(void);

#endif /* __SIMPLE_PROTO_H */
