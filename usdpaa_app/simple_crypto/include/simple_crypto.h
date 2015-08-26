/**
\file  simple_crypto.h
\brief Common datatypes, hash-defines of SEC 4.0_TEST Application
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

#ifndef __SIMPLE_CRYPTO_H
#define __SIMPLE_CRYPTO_H

#include <argp.h>
#include <inttypes.h>

#include <flib/rta.h>
#include <flib/desc.h>
#include <flib/protoshared.h>

#include <crypto/test_utils.h>
#include <crypto/sec.h>
#include <crypto/thread_priv.h>
#include <crypto/qman.h>

#include "algo_desc.h"
#include "test_vector.h"

/* SEC4.0 cryptographic raw algorithm supported in the application */
enum sec_algo {
	AES_CBC = 1,
	TDES_CBC,
	SNOW_F8,
	SNOW_F9,
	KASUMI_F8,
	KASUMI_F9,
	CRC,
	HMAC_SHA1,
	SNOW_F8_F9
};

/*
 * This structure is for the user defined SEC parameters
 * given as CLI arguments
 */
struct runtime_param {
	uint32_t output_buf_size;
	uint32_t input_buf_capacity;
	uint32_t input_buf_length;
	uint32_t job_desc_buf_size;
};

struct test_param {
	enum test_mode mode;	/**< test mode */
	unsigned int test_set;	/**< test set number */
	unsigned int buf_size;	/**< buffer size */
	unsigned int buf_num;	/**< total number of buffers, max = 5000 */
	unsigned int itr_num;	/**< number of iteration to repeat SEC operation */
	enum sec_algo algo;		/**< SEC operation to perform */
	struct runtime_param rt; /**< runtime parameter */
	bool valid_params;		/**< valid parameters flag*/
	uint8_t authnct;	/* processing authentication algorithm */
};

struct parse_input_t {
	uint32_t *cmd_params;
	struct test_param *crypto_info;
};

char mode_type[20];		/* string corresponding to integral value */
char algorithm[20];		/* string corresponding to integral value */

/* init reference test vector routines */
void init_rtv_aes_cbc(struct test_param *crypto_info);
void init_rtv_tdes_cbc(struct test_param *crypto_info);
void init_rtv_snow_f8(struct test_param *crypto_info);
void init_rtv_snow_f9(struct test_param *crypto_info);
void init_rtv_kasumi_f8(struct test_param *crypto_info);
void init_rtv_kasumi_f9(struct test_param *crypto_info);
void init_rtv_crc(struct test_param *crypto_info);
void init_rtv_hmac_sha1(struct test_param *crypto_info);
void init_rtv_snow_f8_f9(struct test_param *crypto_info);

/* prepare test buffers, fqs, fds routines */
int prepare_test_frames(struct test_param *crypto_info);
int set_buf_size(struct test_param *crypto_info);
void *setup_preheader(uint32_t shared_desc_len, uint32_t pool_id,
		      uint32_t pool_buf_size, uint8_t absolute,
		      uint8_t add_buf);
static void *setup_init_descriptor(bool mode, struct test_param crypto_info);
void *setup_sec_descriptor(bool mode, void *params);
void set_enc_buf(void *params, struct qm_fd fd[]);
void set_dec_buf(void *params, struct qm_fd fd[]);
void set_dec_auth_buf(void *params, struct qm_fd fd[]);

/* validate test routines */
error_t parse_opt(int opt, char *arg, struct argp_state *state);
int test_enc_match(void *params, struct qm_fd fd[]);
int test_dec_match(void *params, struct qm_fd fd[]);
static int validate_params(uint32_t cmd_args, struct test_param crypto_info);
static int validate_test_set(struct test_param crypto_info);

/* helper routines */
void set_crypto_cbs(struct test_cb *crypto_cb, struct test_param crypto_info);
inline int get_num_of_iterations(void *stuff);
void set_num_of_iterations(void *stuff, unsigned int itr_num);
inline int get_num_of_buffers(void *stuff);
inline enum test_mode get_test_mode(void *stuff);
inline uint8_t requires_authentication(void *);
inline long get_num_of_cpus(void);
inline pthread_barrier_t *get_thread_barrier(void);

#endif /* __SIMPLE_CRYPTO_H */
