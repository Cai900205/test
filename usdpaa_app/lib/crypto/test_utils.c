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
#include "test_utils.h"

/* Counters to accumulate time taken for packet processing */
uint64_t enc_delta, dec_delta;

static bool ctrl_error;

static void cleanup_handler(void *arg)
{
	int calm_down = 16; /* Chosen through testing */
	struct cleanup_params *param = (struct cleanup_params *)arg;

	if (unlikely(free_sec_fq(param->authnct) != 0)) {
		fprintf(stderr, "error: %s: free_sec_fq failed\n", __func__);
		abort();
	}

	/* Update the number of executed iterations (could be less than the
	 * # of requested iterations because of Ctrl-C.
	 */
	param->crypto_cb->set_num_of_iterations(param->crypto_param,
						param->executed_iterations);

	while (calm_down--) {
		qman_poll_slow();
		/* Take one DQRR @ once */
		qman_poll_dqrr(16);
	}
	qman_thread_finish();

	pr_debug("Leaving thread on cpu %d\n", param->cpu_index);

	free(param);
}

/* This is not actually necessary, the threads can just start up without any
 * ordering requirement. The first cpu will initialise the interfaces before
 * enabling the MACs, and cpus/portals can come online in any order. On
 * simulation however, the initialising thread/cpu *crawls* because the
 * simulator spends most of its time simulating the other cpus in their tight
 * polling loops, whereas having those threads suspended in a barrier allows
 * the simulator to focus on the cpu doing the initialisation. On h/w this is
 * harmless but of no benefit. */
int worker_fn(struct thread_data *tdata)
{
	uint32_t cpu_index;
	/* Counters to record time */
	uint64_t atb_start_enc = 0;
	uint64_t atb_start_dec = 0;
	int i = 1;
	int iterations = 0, itr_num;
	int buf_num;
	enum test_mode mode;
	static pthread_barrier_t *app_barrier = NULL;
	long ncpus;
	struct test_cb crypto_cb = *((struct test_cb *)tdata->test_cb);
	void *crypto_param = tdata->test_param;
	struct qm_fd *fd = get_fd_base();
	struct cleanup_params *cleanup_params;

	itr_num = crypto_cb.get_num_of_iterations(crypto_param);
	iterations = itr_num;
	buf_num = crypto_cb.get_num_of_buffers(crypto_param);
	mode = crypto_cb.get_test_mode(crypto_param);
	ncpus = crypto_cb.get_num_of_cpus();
	app_barrier = crypto_cb.get_thread_barrier();

	cpu_index = tdata->index;

	cleanup_params = calloc(1, sizeof(struct cleanup_params));
	if (!cleanup_params) {
		perror("allocating cleanup parameters: ");
		abort();
	}
	cleanup_params->cpu_index = tdata->index;
	cleanup_params->crypto_param = crypto_param;
	cleanup_params->crypto_cb = tdata->test_cb;
	cleanup_params->executed_iterations = 0;
	cleanup_params->authnct =
			crypto_cb.requires_authentication(crypto_param);

	pr_debug("\nThis is the thread on cpu %d\n", cpu_index);

	if (unlikely(init_sec_fq(cpu_index, crypto_param, crypto_cb) != 0)) {
		fprintf(stderr, "error: %s: init_sec_fq() failure\n", __func__);
		abort();
	}

	/*
	 * Set the function to be called in case SIGINT is received by
	 * the main thread.
	 */
	pthread_cleanup_push(cleanup_handler, (void *)cleanup_params);
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

	get_pkts_to_sec(buf_num, cpu_index, ncpus);

	while (iterations) {
		/* Set encryption buffer */
		if (!cpu_index) {
			if (itr_num < ONE_MEGA) {
				fprintf(stdout, "Iteration %d started\n", i);
			} else {
				if (1 == (i % ONE_MEGA / 10))
					fprintf(stdout, "Iteration %d started."
						" working....\n", i);
			}
			crypto_cb.set_enc_buf(crypto_param, fd);
		}
		set_enc_pkts_from_sec();

		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		pthread_testcancel(); /* A cancellation point */
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

		if (EINVAL == pthread_barrier_wait(app_barrier)) {
			fprintf(stderr, "error: Encrypt mode:"
				" pthread_barrier_wait failed"
				" before enqueue\n");
			abort();
		}

		if (!cpu_index)
			/* encrypt mode: start time */
			atb_start_enc = mfatb();


		/* Send data to SEC40 for encryption/authentication */
		do_enqueues(ENCRYPT, cpu_index, ncpus, buf_num);

		if (!cpu_index)
			pr_debug("Encrypt mode: Total packet sent to "
				 "SEC = %u\n", buf_num);

		/* Receive encrypted or MAC data from SEC40 */
		enc_qman_poll();

		if (EINVAL == pthread_barrier_wait(app_barrier)) {
			fprintf(stderr, "error: Encrypt mode:"
				" pthread_barrier_wait failed"
				" before enqueue\n");
			abort();
		}

		if (!cpu_index) {
			pr_debug("Encrypt mode: Total packet returned from "
				 "SEC = %d\n", get_enc_pkts_from_sec());

			/* accumulated time difference */
			enc_delta += (mfatb() - atb_start_enc);

			if (unlikely(check_fd_status(buf_num) != 0)) {
				ctrl_error = 1;
				goto error2;
			}

			/* Test ciphertext or MAC generated by SEC40 */
			if (CIPHER == mode) {
				if (unlikely
				    (crypto_cb.is_enc_match(crypto_param, fd) !=
				     0)) {
					ctrl_error = 1;
					goto error2;
				}
			}

			if (unlikely(crypto_cb.enc_done_cbk &&
				     crypto_cb.enc_done_cbk(crypto_param,
							    itr_num -
								iterations))){
				ctrl_error = 1;
				goto error2;
			}

			if (crypto_cb.set_dec_buf)
				/* Set decryption buffer */
				crypto_cb.set_dec_buf(crypto_param, fd);
		}
		set_dec_pkts_from_sec();
error2:
		if (EINVAL == pthread_barrier_wait(app_barrier)) {
			fprintf(stderr, "error: Decrypt mode:"
				" pthread_barrier_wait failed"
				" before enqueue\n");
			abort();
		}

		if (ctrl_error)
			goto err_free_fq;

		if (crypto_cb.requires_authentication(crypto_param))
			goto result;

		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		pthread_testcancel(); /* A cancellation point */
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

		if (!cpu_index)
			/* decrypt mode: start time */
			atb_start_dec = mfatb();

		/* Send data to SEC40 for decryption */
		do_enqueues(DECRYPT, cpu_index, ncpus, buf_num);

		if (!cpu_index)
			pr_debug("Decrypt mode: Total packet sent to "
				 "SEC = %u\n", buf_num);

		/* Recieve decrypted data from SEC40 */
		dec_qman_poll();

		if (EINVAL == pthread_barrier_wait(app_barrier)) {
			fprintf(stderr, "error: Encrypt mode:"
				" pthread_barrier_wait failed"
				" before enqueue\n");
			abort();
		}

		if (!cpu_index) {
			pr_debug("Decrypt mode: Total packet returned from "
				 "SEC = %d\n", get_dec_pkts_from_sec());

			/* accumulated time difference */
			dec_delta += (mfatb() - atb_start_dec);

			if (unlikely(check_fd_status(buf_num) != 0)) {
				ctrl_error = 1;
				goto error2;
			}

			if (unlikely
			    (crypto_cb.is_dec_match(crypto_param, fd) != 0)) {
				ctrl_error = 1;
				goto error3;
			}

			if (unlikely(crypto_cb.dec_done_cbk &&
				     crypto_cb.dec_done_cbk(crypto_param,
							    itr_num -
								iterations)))
				ctrl_error = 1;
		}
error3:
		if (EINVAL == pthread_barrier_wait(app_barrier)) {
			fprintf(stderr, "error: pthread_barrier_wait failed"
				" after test_dec_match\n");
			abort();
		}

		if (ctrl_error)
			goto err_free_fq;

result:
		if (!cpu_index) {
			if (ONE_MEGA > itr_num) {
				fprintf(stdout, "Iteration %d finished\n", i);
			} else {
				if (1 == (i % ONE_MEGA / 10))
					fprintf(stdout, "Iteration %d finished."
						" working....\n", i);
			}
		}

		iterations--;
		cleanup_params->executed_iterations++;
		i++;

		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		pthread_testcancel(); /* A cancellation point */
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	}

err_free_fq:
	pthread_cleanup_pop(1);

	return 0;
}

/*
 * brief	Bitwise comparison of two vectors
 * param[in]	left - The lefthand-side vector to enter the comparison
 * param[in]	right - The righthand-side vector to enter the comparison
 * param[in]	bitlength - The length(in bits) on which the comparison
 *		will be made
 * retval	0 if and only if the vectors are identical up to
 *		(including) the given bitlength.
 * pre	Neither of the buffer pointers is allowed to be NULL.
 * pre	Both buffers must be at least of size ceil
 *		(bitlength / sizeof(32)).
 */
int test_vector_match(uint32_t *left, uint32_t *right, uint32_t bitlen)
{
	uint8_t reminder_bitlen;
	uint32_t bitmasks[32];
	uint32_t i;

	if (!left || !right) {
		fprintf(stderr, "error: Wrong parameters to %s\n", __func__);
		abort();
	}

	/* initialize bitmasks */
	bitmasks[0] = 0xFFFFFFFF;
	for (i = 1; i <= 31; i++)
		bitmasks[i] = bitmasks[i - 1] >> 1;

	/* compare the full 32-bit quantities */
	for (i = 0; i < (bitlen >> 5); i++) {
		if (left[i] != right[i]) {
			fprintf(stderr, "error: %s(): Bytes at offset %d don't"
				" match (0x%x, 0x%x)\n", __func__, i, left[i],
				right[i]);
			return -1;
		}
	}

	/* compare the reminder dword starting with its most significant
	 *  bits
	 */
	reminder_bitlen = bitlen & 0x1F;
	if (reminder_bitlen) {
		/* compare left[bitlen >> 5] with right[bitlen >> 5]
		 *  on the remaining number of bits
		 */
		uint32_t left_last = left[bitlen >> 5];
		uint32_t right_last = right[bitlen >> 5];

		if ((left_last | bitmasks[reminder_bitlen])
		    != (right_last | bitmasks[reminder_bitlen])) {
			fprintf(stderr, "error: %s(): Last bytes (%d) don't"
				" match on full %d bitlength\n", __func__,
				bitlen >> 5, reminder_bitlen);
			return -1;
		}
	}

	return 0;
}

/*
 * brief	In case of succesful test, calculates throughput on encrypt/
 *		decrypt directions; in case of error, it prints 'test failed'
 * param[in]	itr_num - number of iterations per test
 * param[in]	buf_num - number of buffers per test
 * param[in]	buf_size - size of frame buffer
 * return	None
 */
void validate_test(unsigned int itr_num, unsigned int buf_num,
		   unsigned int buf_size)
{
	uint64_t cpu_freq, throughput;
	int err = 0;

	if (!ctrl_error) {
		/* Read cpu frequency from /poc/cpuinfo */
		err = get_cpu_frequency(&cpu_freq);
		if (err)
			error(err, err, "error: get cpu frequency failed");

		throughput = ((cpu_freq * BITS_PER_BYTE * buf_size) *
				(itr_num * buf_num)) / enc_delta ;

		fprintf(stdout, "%s: Throughput = %" PRIu64 " Mbps\n",
			"Encrypt", throughput);

		if (dec_delta != 0) {
			throughput = ((cpu_freq * BITS_PER_BYTE * buf_size) *
					(itr_num * buf_num)) / dec_delta;
			fprintf(stdout, "%s: Throughput = %" PRIu64 " Mbps\n",
				"Decrypt", throughput);
		} else {
			fprintf(stdout, "%s: Throughput = N/A\n", "Decrypt");
		}

		fprintf(stdout, "SEC 4.0 TEST PASSED\n");
	} else {
		fprintf(stdout, "info: TEST FAILED\n");
	}
}

/*
 * brief	Read cpu frequency from /proc/cpuinfo
 * param[out]	cpu_freq
 * return	0 - if status is correct (i.e. 0)
 *		-1 - if SEC returned an error status (i.e. non 0)
 */
int get_cpu_frequency(uint64_t *cpu_freq)
{
	int err = 0;
	FILE *p_cpuinfo;
	char buf[255], cpu_f[20];

	p_cpuinfo = fopen("/proc/cpuinfo", "rb");

	if (NULL == p_cpuinfo) {
		fprintf(stderr, "error: opening file /proc/cpuinfo");
		return -1;
	}

	while (fgets(buf, 255, p_cpuinfo)) {
		if (strstr(buf, "clock")) {
			strncpy(cpu_f, &buf[9], 20);
			break;
		}
	}

	*cpu_freq = strtoul(cpu_f, NULL, 10);	/* cpu_freq in MHz */
	if (ERANGE == errno || EINVAL == errno)
		error(err, err, "error: could not read cpu frequency"
		      " from /proc/cpuinfo");

	return err;
}
