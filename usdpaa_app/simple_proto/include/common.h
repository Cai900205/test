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

#ifndef PROTOCOLS_H_
#define PROTOCOLS_H_

#include <inttypes.h>
#include <stdbool.h>

#include <usdpaa/fsl_qman.h>
#include <mutex.h>

#define DES_BLOCK_SIZE		8
#define AES_BLOCK_SIZE		16
#define SHA1_DIGEST_SIZE	20

/**< Integer number of 32-bit items */
#define NO_OF_WORDS(bitlen) \
	(((bitlen) >> 5) + ((31 + ((bitlen) & 0x1F)) >> 5))

/**< Integer number of 8-bit items */
#define NO_OF_BYTES(bitlen) (((bitlen) >> 3) + !!((bitlen) & 0x7))

/**< Integer number of bits in given bytes  */
#define NO_OF_BITS(bytelen)	(bytelen << 3)

/**
 * @struct	runtime_param
 * @details	Structure used for the user defined SEC parameters
 *		given as CLI arguments
 */
struct runtime_param {
	uint32_t output_buf_size;
	uint32_t input_buf_capacity;
	uint32_t input_buf_length;
};

struct parse_input_t {
	uint32_t *cmd_params;
	uint32_t *proto_params;	/**< protocol specific parameters */
	struct test_param *crypto_info;
};

struct desc_storage {
	uint32_t *descr;
	bool mode;
};

/**
 * @struct	test_param
 * @details	Structure used to hold parameters for test
 */
struct test_param {
	enum test_mode mode;	/**< test mode */
	unsigned int test_set;	/**< test set number */
	unsigned int buf_size;	/**< buffer size */
	unsigned int buf_num;	/**< total number of buffers, max = 5000 */
	unsigned int itr_num;	/**< number of iteration to repeat SEC
				     operation */
	unsigned int sel_proto;	/**< Selected SEC protocol number. */
	struct runtime_param rt;/**< runtime parameter */
	unsigned authnct;	/**< Indicate if both encapsulation &
				     decapsulation is required. */
	struct protocol_info *proto;	/**< Pointer to the selected protocol
					     structure, used for accessing
					     relevant information throughout
					     the application. */
};

struct protocol_info {
	char name[20];
	struct argp_child *argp_children;
	int (*init_ref_test_vector)(struct test_param *);
	void (*set_enc_buf_cb)(struct qm_fd *, uint8_t*, struct test_param *);
				/**< callback used for setting per-protocol
				     parameters on the encap direction */
	void (*set_dec_buf_cb)(struct qm_fd *, uint8_t*, struct test_param *);
				/**< callback used for setting per-protocol
				     parameters on the decap direction */
	int (*test_enc_match_cb)(int, uint8_t*, struct test_param *);
				/**< callback used for validating the encap
				     result (per protocol) */
	int (*test_dec_match_cb)(int, uint8_t*, struct test_param *);
				/**< callback used for validating the encap
				     result (per protocol) */
	void *(*setup_sec_descriptor)(bool, void *);
	int (*validate_opts)(uint32_t, struct test_param *);
	int (*get_buf_size)(struct test_param *);
	int (*set_buf_size)(struct test_param *);
	int (*validate_test_set)(struct test_param *);
	void *proto_vector;	/**< Per-protocol test-vector */
	void *proto_params;	/**< Per-protocol defined parameters. */
	void (*unregister)(struct protocol_info *);
				/**< Callback for deregistering the protocol */
	struct desc_storage *descr;
				/**< Descriptor storage */
	spinlock_t desc_wlock;	/**< Mutex for ensuring that the descriptors
				     are written in an ordered fashion in the
				     descr member. */
	unsigned short buf_align;	/**< Alignment of buffers */
	int (*check_status)(unsigned *, void*);
				/**< Callback for checking the status returned
				     by SEC in fd_status. */
	unsigned num_cpus;	/**< Number of online CPUs in the system */
	int (*enc_done_cbk) (void *, int);
				/**< Callback for encapsulation end of test
				     checks */
	int (*dec_done_cbk) (void *, int);
				/**< Callback for decapsulation end of test
				     checks */
};

#endif /* PROTOCOLS_H_ */
