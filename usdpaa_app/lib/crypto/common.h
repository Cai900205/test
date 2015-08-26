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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __CRYPTO_COMMON_H
#define __CRYPTO_COMMON_H

#include <usdpaa/compat.h>
#include <usdpaa/fsl_qman.h>
#include <stdbool.h>

#define BUFF_SIZE	9600 /**< Maximum buffer size that can be
					  provided by user */

#define DMAMEM_SIZE	0x1000000	/**< The size of the DMAable memory zone
					 * It is shared across all the existing
					 * processors in the system (i.e. one
					 * zone from which each processor
					 * allocates its structures
					 */

struct test_cb {
	void *(*set_sec_descriptor) (bool, void *);
	int (*is_enc_match) (void *, struct qm_fd fd[]);
	int (*is_dec_match) (void *, struct qm_fd fd[]);
	void (*set_enc_buf) (void *, struct qm_fd fd[]);
	void (*set_dec_buf) (void *, struct qm_fd fd[]);
	int (*get_num_of_iterations) (void *);
	void (*set_num_of_iterations) (void *, unsigned int);
	int (*get_num_of_buffers) (void *);
	long (*get_num_of_cpus) (void);
	enum test_mode (*get_test_mode) (void *);
	uint8_t(*requires_authentication) (void *);
	pthread_barrier_t *(*get_thread_barrier) (void);
	int (*enc_done_cbk) (void *, int);
	int (*dec_done_cbk) (void *, int);
};

#endif /* __CRYPTO_COMMON_H */
