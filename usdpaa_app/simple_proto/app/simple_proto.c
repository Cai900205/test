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
#include "simple_proto.h"

pthread_barrier_t app_barrier;

long ncpus;

struct protocol_info *proto;
struct ref_vector_s *ref_test_vector;

enum rta_sec_era rta_sec_era;
int32_t user_sec_era = -1;
int32_t hw_sec_era = -1;

/*
 * Array of pointers to various protocol informations. Each registered protocol
 * will allocate a structure that will be referenced from this array
 * by using this variable
 */
static struct protocol_info **protocols;

/*
 * The total number of registered protocols
 */
static int proto_num;

/***********************************************/

/**
 * @brief	Calculates output buffer size and creates compound FDs
 * @param[in]	crypto_info - test parameters
 * @return	0 on success, otherwise error value
 */
int prepare_test_frames(struct test_param *crypto_info)
{
	int err = 0;
	extern struct qm_fd *fd;
	char mode_type[20];

	/*
	 * Allocate FD array
	 */
	fd = calloc(crypto_info->buf_num, sizeof(struct qm_fd));
	if (!fd) {
		error(err, err, "error: allocating FD array");
		return -ENOMEM;
	}

	if (PERF == crypto_info->mode) {
		strncpy(mode_type, "PERF", sizeof(mode_type));
		crypto_info->test_set = 1;
	}

	err = proto->init_ref_test_vector(crypto_info);
	if (unlikely(err)) {
		error(err, err, "error: initializing test vector");
		return err;
	}

	/*
	 * Set the global reference test vector variable to point to the
	 * reference test vector, as selected by the protocol.
	 */
	ref_test_vector = proto->proto_vector;

	if (CIPHER == crypto_info->mode) {
		strcpy(mode_type, "CIPHER");
		crypto_info->buf_size = NO_OF_BYTES(ref_test_vector->length);
	}

	err = proto->set_buf_size(crypto_info);
	if (err)
		error(err, err, "error: set output buffer size");

	err = create_compound_fd(crypto_info->buf_num,
				&(struct compound_fd_params){
					crypto_info->rt.output_buf_size,
					crypto_info->rt.input_buf_capacity,
					crypto_info->rt.input_buf_length,
					proto->buf_align});
	if (err)
		error(err, err, "error: create_compound_fd() failed");

	printf("Processing %s for %d Frames\n", proto->name,
	       crypto_info->buf_num);
	printf("%s mode, buffer length = %d\n", mode_type,
	       crypto_info->buf_size);
	printf("Number of iterations = %d\n", crypto_info->itr_num);
	printf("\nStarting threads for %ld cpus\n", ncpus);

	return err;
}

/**
 * @brief	Initialize input buffer plain text data and set output buffer
 *		as 0 in compound frame descriptor
 * @param[in]	params - test parameters
 * @param[in]	struct qm_fd - frame descriptors list
 * @return       None
 */
void set_enc_buf(void *params, struct qm_fd fd[])
{
	struct test_param *crypto_info = (struct test_param *)params;
	struct qm_sg_entry *sgentry;
	uint8_t *buf;
	uint8_t plain_data = 0;
	dma_addr_t addr, out_buf, in_buf;
	uint32_t i, ind;

	for (ind = 0; ind < crypto_info->buf_num; ind++) {
		addr = qm_fd_addr(&fd[ind]);

		/* set output buffer and length */
		sgentry = __dma_mem_ptov(addr);
		sgentry->length = crypto_info->rt.output_buf_size;

		out_buf = addr + sizeof(struct sg_entry_priv_t);
		qm_sg_entry_set64(sgentry, out_buf);

		/* set input buffer and length */
		sgentry++;
		sgentry->length = crypto_info->rt.input_buf_capacity;

		in_buf = out_buf + crypto_info->rt.output_buf_size;
		qm_sg_entry_set64(sgentry, in_buf);

		buf = __dma_mem_ptov(in_buf);

		if (proto->set_enc_buf_cb)
			proto->set_enc_buf_cb(&fd[ind], buf, crypto_info);
		else
			if (CIPHER == crypto_info->mode)
				memcpy(buf, ref_test_vector->plaintext,
				       crypto_info->buf_size);
			else
				for (i = 0; i < crypto_info->buf_size; i++)
					buf[i] = plain_data++;
	}
}

/**
 * @brief	Initialize input buffer as cipher text data and set output
 *		buffer as 0 in compound frame descriptor
 * @param[in]	params - test parameters
 * @param[in]	struct qm_fd - frame descriptors list
 * @return       None
 */
void set_dec_buf(void *params, struct qm_fd fd[])
{
	struct test_param *crypto_info = (struct test_param *)params;
	struct qm_sg_entry *sg_out;
	struct qm_sg_entry *sg_in;
	dma_addr_t addr;
	uint32_t length;
	uint16_t offset;
	uint8_t bpid;
	uint32_t ind;

	for (ind = 0; ind < crypto_info->buf_num; ind++) {
		addr = qm_fd_addr(&fd[ind]);
		sg_out = __dma_mem_ptov(addr);
		sg_in = sg_out + 1;

		addr = qm_sg_addr(sg_out);
		length = sg_out->length;
		offset = sg_out->offset;
		bpid = sg_out->bpid;

		qm_sg_entry_set64(sg_out, qm_sg_addr(sg_in));
		sg_out->length = sg_in->length;
		sg_out->offset = sg_in->offset;
		sg_out->bpid = sg_in->bpid;

		qm_sg_entry_set64(sg_in, addr);
		sg_in->length = length;
		sg_in->offset = offset;		sg_in->bpid = bpid;

		if (proto->set_dec_buf_cb) {
			uint8_t *buf =  __dma_mem_ptov(addr);
			proto->set_dec_buf_cb(&fd[ind], buf, crypto_info);
		}
	}
}

/*****************************************************************************/
char protocol_string[1024] = "Cryptographic operation to perform by SEC:\n";

struct argp_option options[] = {
	{"mode", 'm', "TEST MODE", 0,
	"Test mode:"
	"\n\t1 for perf"
	"\n\t2 for cipher"
	"\n\nFollowing two combinations are valid only"
	" and all options are mandatory:"
	"\n-m 1 -s <buf_size> -n <buf_num_per_core> -p <proto> -l <itr_num>"
	"\n-m 2 -t <test_set> -n <buf_num_per_core> -p <proto> -l <itr_num>"
	"\n"},
	{"proto", 'p', "PROTOCOL", 0, protocol_string},
	{"itrnum", 'l', "ITERATIONS", 0,
	"Number of iterations to repeat"
	"\n"},
	{"bufnum", 'n', "TOTAL BUFFERS", 0,
	"Total number of buffers (depends on protocol & buffer size)."
	"\n"},
	{"bufsize", 's', "BUFSIZE", 0,
	"OPTION IS VALID ONLY IN PERF MODE"
	"\n\nBuffer size (64, 128 ... up to 9600)."
	"\n"},
	{"ncpus", 'c', "CPUS", 0,
	"OPTIONAL PARAMETER"
	"\n\nNumber of cpus to work for the application(1-8)"
	"\n"},
	{"testset", 't', "TEST SET", 0,
	"OPTION IS VALID ONLY IN CIPHER MODE"
	"\n"},
	{"sec_era", 'e', "ERA", 0,
	"OPTIONAL PARAMETER"
	"\n\nSEC Era version on the targeted platform(2-5)"
	"\n"},
	{0}
};

/**
 * @brief	The OPTIONS field contains a pointer to a vector of struct
 *		argp_option's
 *
 * @details	structure has the following fields
 *		name - The name of this option's long option (may be zero)
 *		key - The KEY to pass to the PARSER function when parsing this
 *		option,	and the name of this option's short option, if it is
 *		a printable ascii character
 *
 *		ARG - The name of this option's argument, if any;
 *
 *		FLAGS - Flags describing this option; some of them are:
 *			OPTION_ARG_OPTIONAL - The argument to this option is
 *				optional
 *			OPTION_ALIAS	- This option is an alias for the
 *				previous option
 *			OPTION_HIDDEN	    - Don't show this option in
 *				--help output
 *
 *		DOC - A documentation string for this option, shown in
 *			--help output
 *
 * @note	An options vector should be terminated by an option with
 *		all fields zero
 */

/**
 * @brief	Parse a single option
 *
 * @param[in]	opt - An integer specifying which option this is (taken	from
 *		the KEY field in each struct argp_option), or a special key
 *		specifying something else. We do not use any special key here
 *
 * @param[in]	arg - For an option KEY, the string value of its argument, or
 *		NULL if it has none
 *
 * @return	It should return either 0, meaning success, ARGP_ERR_UNKNOWN,
 *		meaning the given KEY wasn't recognized, or an errno value
 *		indicating some other error
 */
error_t parse_opt(int opt, char *arg, struct argp_state *state)
{
	struct parse_input_t *input = state->input;
	struct test_param *crypto_info = input->crypto_info;
	uint32_t *p_cmd_params = input->cmd_params;
	int i;
	const struct argp *pr_argp;

	switch (opt) {
	case ARGP_KEY_INIT:
		/*
		 * in case ARGP_NO_HELP flag is not set, glibc adds an
		 * internal parser as root argp; the struct argp passed by our
		 * program is copied as the first child of the new root arg.
		 */
		if (!(state->flags & ARGP_NO_HELP))
			pr_argp = state->root_argp->children[0].argp;
		else
			pr_argp = state->root_argp;

		for (i = 0; pr_argp->children[i].argp; i++)
			state->child_inputs[i] = input;
		break;
	case 'm':
		crypto_info->mode = atoi(arg);
		*p_cmd_params |= BMASK_SEC_TEST_MODE;
		printf("Test mode = %s\n", arg);
		break;

	case 't':
		crypto_info->test_set = atoi(arg);
		*p_cmd_params |= BMASK_SEC_TEST_SET;
		printf("Test set = %d\n", crypto_info->test_set);
		break;

	case 's':
		crypto_info->buf_size = atoi(arg);
		*p_cmd_params |= BMASK_SEC_BUFFER_SIZE;
		printf("Buffer size = %d\n", crypto_info->buf_size);
		break;

	case 'n':
		crypto_info->buf_num = atoi(arg);
		*p_cmd_params |= BMASK_SEC_BUFFER_NUM;
		printf("Number of Buffers per core = %d\n",
		       crypto_info->buf_num);
		break;

	case 'p':
		crypto_info->sel_proto = atoi(arg) - 1;
		*p_cmd_params |= BMASK_SEC_ALG;
		crypto_info->proto = protocols[crypto_info->sel_proto];
		printf("SEC cryptographic operation = %s\n", arg);
		break;

	case 'l':
		crypto_info->itr_num = atoi(arg);
		*p_cmd_params |= BMASK_SEC_ITR_NUM;
		printf("Number of iteration = %d\n", crypto_info->itr_num);
		break;

	case 'c':
		ncpus = atoi(arg);
		printf("Number of cpus = %ld\n", ncpus);
		break;

	case 'e':
		user_sec_era = atoi(arg);
		printf("User SEC Era version = %d\n", user_sec_era);
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

/**
 * @brief	Check SEC parameters provided by user whether valid or not
 * @param[in]	g_cmd_params - Bit mask of all parameters provided by user
 * @param[in]	g_proto_params - Bit mask of protocol specific parameters, as
 *		provided by the user
 * @param[in]	crypto_info - test parameters
 * @return	0 on success, otherwise -EINVAL value
 */
static int validate_params(uint32_t g_cmd_params, uint32_t g_proto_params,
			   struct test_param *crypto_info)
{
	unsigned int total_size, max_num_buf;


	if (crypto_info->sel_proto > proto_num) {
		fprintf(stderr,
			"error: Invalid Parameters: SEC protocol not supported\n"
			"see --help option\n");
			return -EINVAL;
	}

	/*
	 * For speedier access, use a local variable to access the selected
	 * protocol info and also store it in the test parameters.
	 */
	proto = crypto_info->proto;

	if (!proto) {
		pr_err("Invalid protocol selected");
		return -EINVAL;
	}

	if ((PERF == crypto_info->mode) &&
	    BMASK_SEC_PERF_MODE == g_cmd_params) {
		/* do nothing */
	} else if ((CIPHER == crypto_info->mode) &&
		    g_cmd_params == BMASK_SEC_CIPHER_MODE) {
		if (proto->validate_test_set(crypto_info) != 0) {
			fprintf(stderr,
				"error: Invalid Parameters: Invalid test set\n"
				"see --help option\n");
			return -EINVAL;
		}
	} else {
		fprintf(stderr,
			"error: Invalid Parameters: provide a valid mode for testing (CIPHER or PERF)\n"
			"see --help option\n");
		return -EINVAL;
	}

	if ((PERF == crypto_info->mode) &&
	    (crypto_info->buf_size == 0 ||
	     crypto_info->buf_size % L1_CACHE_BYTES != 0 ||
	     crypto_info->buf_size > BUFF_SIZE)) {
		fprintf(stderr,
			"error: Invalid Parameters: Invalid number of buffers\nsee --help option\n");
		return -EINVAL;
	}

	total_size = sizeof(struct sg_entry_priv_t) +
			proto->get_buf_size(crypto_info);
	/*
	 * The total usdpaa_mem space required for processing the frames
	 * is split as follows:
	 * - SEC descriptors: one per each FQ multiplied by the # of FQs per
	 *		      core (the "from SEC" FQ doesn't have a descriptor)
	 * - QMan FQ structures: two per each FQ, multiplied by the # of FQs
	 *		       per core
	 * - I/O buffers & S/G entries multiplied by # of buffers
	 */
	max_num_buf = (DMAMEM_SIZE -
			FQ_PER_CORE * (SEC_DESCRIPTORS_SIZE + QMAN_FQS_SIZE)) /
			ALIGN(total_size, L1_CACHE_BYTES);

	/*
	 * Because the usdpaa_mem allocator is using some memory as well, at
	 * the end of the USDPAA reserved memory zone, just to be sure that in
	 * the worst case scenario (many small unaligned buffers) there's no
	 * issue, subtract 1 buffer from the calculated number
	 */
	max_num_buf--;

	if (crypto_info->buf_num == 0 || crypto_info->buf_num > max_num_buf) {
		fprintf(stderr, "error: Invalid Parameters: Invalid number of buffers:\n"
				"Maximum number of buffers for the selected frame size is %d\n"
				"see --help option\n", max_num_buf);
			return -EINVAL;
	}

	if (validate_sec_era_version(user_sec_era, hw_sec_era))
		return -EINVAL;

	if (proto->validate_opts)
		return proto->validate_opts(g_proto_params, crypto_info);

	return 0;
}

/**
 * @brief	Compare encrypted data returned by SEC with	standard
 *		cipher text
 * @param[in]	params - test parameters
 * @param[in]	struct qm_fd - frame descriptor list
 * @return	    0 on success, otherwise -1 value
 */
int test_enc_match(void *params, struct qm_fd fd[])
{
	struct test_param *crypto_info = (struct test_param *)params;
	struct qm_sg_entry *sgentry;
	uint8_t *enc_buf;
	dma_addr_t addr;
	uint32_t ind;

	for (ind = 0; ind < crypto_info->buf_num; ind++) {
		addr = qm_fd_addr_get64(&fd[ind]);
		sgentry = __dma_mem_ptov(addr);

		addr = qm_sg_entry_get64(sgentry);
		enc_buf = __dma_mem_ptov(addr);

		if (proto->test_enc_match_cb) {
			if (proto->test_enc_match_cb(ind, enc_buf, crypto_info))
				goto err;
		} else {
			if (test_vector_match((uint32_t *)enc_buf,
					(uint32_t *)ref_test_vector->ciphertext,
					crypto_info->rt.output_buf_size *
						BITS_PER_BYTE) != 0)
				goto err;
		}
	}

	printf("All %s encrypted frame match found with cipher text\n",
		proto->name);

	return 0;

err:
	fprintf(stderr,
		"error: %s: Encapsulated frame %d doesn't match with test vector\n",
		__func__, ind + 1);

	return -1;
}

/**
 * @brief     Compare decrypted data returned by SEC with plain text
 *            input data.
 * @details   WiMAX decapsulated output frame contains the decapsulation
 *            Generic Mac Header (EC is set to zero, length is reduced
 *            as appropiate and HCS is recomputed). For performance mode,
 *            plaintext input packets are not GMH aware and a match between
 *            decapsulation output frames and encapsulation input frames
 *            cannot be guaranteed at GMH level.
 * @param[in] params       test parameters
 * @param[in] struct qm_fd frame descriptor list
 * @return                 0 on success, otherwise -1
 */
int test_dec_match(void *params, struct qm_fd fd[])
{
	struct test_param *crypto_info = (struct test_param *)params;
	struct qm_sg_entry *sgentry;
	uint8_t *dec_buf;
	uint8_t plain_data = 0;
	dma_addr_t addr;
	uint32_t i, ind;

	for (ind = 0; ind < crypto_info->buf_num; ind++) {
		addr = qm_fd_addr_get64(&fd[ind]);
		sgentry = __dma_mem_ptov(addr);

		addr = qm_sg_entry_get64(sgentry);
		dec_buf = __dma_mem_ptov(addr);

		if (proto->test_dec_match_cb) {
			if (proto->test_dec_match_cb(ind,
						     dec_buf,
						     crypto_info))
				goto err;
		} else {
			if (CIPHER == crypto_info->mode) {
				if (test_vector_match((uint32_t *)dec_buf,
					(uint32_t *)ref_test_vector->plaintext,
					ref_test_vector->length) != 0)
					goto err;
			} else {
				for (i = 0; i < crypto_info->buf_size; i++) {
					if (dec_buf[i] != plain_data)
						goto err;
					plain_data++;
				}
			}
		}
	}
	printf("All %s decrypted frame matches initial text\n",
	       proto->name);

	return 0;

err:
	if (CIPHER == crypto_info->mode)
		fprintf(stderr,
			"error: %s: Decapsulated frame %d doesn't match with test vector\n",
			__func__, ind + 1);
	else
		fprintf(stderr,
			"error: %s: %s decrypted frame %d doesn't match!\n",
			 __func__, proto->name, ind + 1);

	print_frame_desc(&fd[ind]);
	return -1;
}

/*
 * "Children" structure for splitting the command line options on a
 * per-protocol basis
 */
static struct argp_child *argp_children;

/* argp structure itself of argp parser */
static struct argp argp = { options, parse_opt, NULL, NULL,
				NULL, NULL, NULL };

/**
 * @brief	Main function of SEC Test Application
 * @param[in]	argc - Argument count
 * @param[in]	argv - Argument list pointer
 */
int main(int argc, char *argv[])
{
	long num_online_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	struct thread_data thread_data[num_online_cpus];
	int err;
	uint32_t g_cmd_params = 0, g_proto_params = 0, i;
	struct test_param crypto_info;
	struct parse_input_t input;
	struct test_cb crypto_cb;

	ncpus = num_online_cpus;

	/* Register available protocol modules */
	if (unlikely(register_modules())) {
		pr_err("module registration failed");
		exit(-EINVAL);
	}

	memset(&crypto_info, 0x00, sizeof(struct test_param));
	input.cmd_params = &g_cmd_params;
	input.proto_params = &g_proto_params;
	input.crypto_info = &crypto_info;

	/* Parse and check input arguments */
	argp_parse(&argp, argc, argv, 0, 0, &input);

	/* Get the number of cores */
	if (ncpus < 1 || ncpus > num_online_cpus) {
		fprintf(stderr,
			"error: Invalid Parameters: Number of cpu's given in argument is more than the active cpu's\n");
		exit(-EINVAL);
	}

	err = of_init();
	if (err)
		error(err, err, "error: of_init() failed");

	hw_sec_era = sec_get_of_era();

	err = validate_params(g_cmd_params, g_proto_params, &crypto_info);
	if (err)
		error(err, err, "error: validate_params failed!");

	/* map DMA memory */
	dma_mem_generic = dma_mem_create(DMA_MAP_FLAG_ALLOC, NULL, DMAMEM_SIZE);
	if (!dma_mem_generic) {
		pr_err("DMA memory initialization failed\n");
		exit(EXIT_FAILURE);
	}

	/* Initialize barrier for all the threads! */
	err = pthread_barrier_init(&app_barrier, NULL, ncpus);
	if (err)
		error(err, err, "error: unable to initialize pthread_barrier");

	err = qman_global_init();
	if (err)
		error(err, err, "error: qman global init failed");

	/* Prepare and create compound fds */
	err = prepare_test_frames(&crypto_info);
	if (err)
		error(err, err, "error: preparing test frames failed");

	set_crypto_cbs(&crypto_cb);

	for (i = 0; i < ncpus; i++) {
		thread_data[i].test_param = (void *)(&crypto_info);
		thread_data[i].test_cb = (void *)(&crypto_cb);
	}

	/* Starting threads on all active cpus */
	err = start_threads(thread_data, ncpus, 1, worker_fn);
	if (err)
		error(err, err, "error: start_threads failure");

	/* Wait for all the threads to finish */
	wait_threads(thread_data, ncpus);

	validate_test(crypto_info.itr_num, crypto_info.buf_num,
		      crypto_info.buf_size);

	free_fd(crypto_info.buf_num);

	/*
	 * Unregister modules
	 */
	unregister_modules();

	of_finish();
	exit(EXIT_SUCCESS);
}

/**
 * @brief	Returns number of iterations for test
 * @param[in]	params - test parameters
 * @return	Number of iterations for test
 */
int get_num_of_iterations(void *params)
{
	return ((struct test_param *)params)->itr_num;
}

/**
 * @brief	Sets the number of executed iterations for a test
 * @param[in]	params - test parameters
 * @return	Number of iterations executed for the test
 */
void set_num_of_iterations(void *params, unsigned int itr_num)
{
	((struct test_param *)params)->itr_num = itr_num;
}

/**
 * @brief	Returns number of buffers for test
 * @param[in]	params - test parameters
 * @return	Number of buffers for test
 */
inline int get_num_of_buffers(void *params)
{
	return ((struct test_param *)params)->buf_num;
}

/**
 * @brief	Returns test mode - CIPHER/PERF
 * @param[in]	params - test parameters
 * @return	Test mode - CIPHER/PERF
 */
inline enum test_mode get_test_mode(void *params)
{
	return ((struct test_param *)params)->mode;
}

/**
 * @brief	Returns if test requires authentication
 * @param[in]	params - test parameters
 * @return	0 - doesn't require authentication/1 - requires authentication
 */
inline uint8_t requires_authentication(void *params)
{
	return ((struct test_param *)params)->authnct;
}


/**
 * @brief	Returns number of cpus for test
 * @param[in]	params - test parameters
 * @return	Number of cpus for test
 */
inline long get_num_of_cpus(void)
{
	return ncpus;
}

/**
 * @brief	Returns thread barrier for test
 * @param[in]	None
 * @return	Thread barrier
 */
inline pthread_barrier_t *get_thread_barrier(void)
{
	return &app_barrier;
}

/**
 * @brief	Set specific callbacks for test
 * @param[in]	crypto_cb - structure that holds reference to test callbacks
 * @return	None
 */
static void set_crypto_cbs(struct test_cb *crypto_cb)
{
	memset(crypto_cb, 0, sizeof(struct test_cb));

	crypto_cb->set_sec_descriptor = proto->setup_sec_descriptor;
	crypto_cb->is_enc_match = test_enc_match;
	crypto_cb->is_dec_match = test_dec_match;
	crypto_cb->set_enc_buf = set_enc_buf;
	crypto_cb->set_dec_buf = set_dec_buf;
	crypto_cb->get_num_of_iterations = get_num_of_iterations;
	crypto_cb->set_num_of_iterations = set_num_of_iterations;
	crypto_cb->get_num_of_buffers = get_num_of_buffers;
	crypto_cb->get_test_mode = get_test_mode;
	crypto_cb->get_num_of_cpus = get_num_of_cpus;
	crypto_cb->requires_authentication = requires_authentication;
	crypto_cb->get_thread_barrier = get_thread_barrier;
	crypto_cb->enc_done_cbk = proto->enc_done_cbk;
	crypto_cb->dec_done_cbk = proto->dec_done_cbk;
}

int register_modules()
{
	int argp_idx;
	struct protocol_info *cur_proto;
	char proto_help_str[40];

	/* Allocate space for children parsers and options.
	 * Note: this might not be completely filled */
	argp_children = calloc(ARRAY_SIZE(register_protocol),
			       sizeof(*argp_children));
	if (unlikely(!argp_children)) {
		pr_err("Failed to allocate argp_children");
		return -ENOMEM;
	}

	protocols = calloc(ARRAY_SIZE(register_protocol),
			   sizeof(*cur_proto));
	if (unlikely(!protocols)) {
		pr_err("Failed to allocate protocols structure");
		free(argp_children);
		return -ENOMEM;
	}

	for (proto_num = 0, argp_idx = 0;
	     proto_num < ARRAY_SIZE(register_protocol);
	     proto_num++) {
		cur_proto = register_protocol[proto_num]();
		if (unlikely(!cur_proto)) {
			pr_err("failed to register protocol");
			free(protocols);
			free(argp_children);
			return -EINVAL;
		}

		if (likely(cur_proto->argp_children)) {
			/* Copy the argp parser options for this protocol. */
			memcpy(&argp_children[argp_idx],
			       cur_proto->argp_children,
			       sizeof(*argp_children));

			/* Update the group so every protocol has its
			 * unique group
			 */
			argp_children[argp_idx].group = argp_idx;

			argp_idx++;
		}

		/*
		 * Update the "protocol" parameter help message to reflect the
		 * protocol being added.
		 */
		snprintf(proto_help_str, sizeof(proto_help_str),
			 " %d for %s\n", proto_num + 1, cur_proto->name);

		strncat(protocol_string, proto_help_str,
			sizeof(protocol_string));

		protocols[proto_num] = cur_proto;
	}

	argp.children = argp_children;

	return 0;
}

void unregister_modules()
{
	int i;

	for (i = 0; i < proto_num; i++)
		protocols[i]->unregister(protocols[i]);

	free(protocols);
	free(argp_children);
}

int check_fd_status(unsigned int buf_num) {
	int ind;
	extern struct qm_fd *fd;

	for (ind = 0; ind < buf_num; ind++)
		if (unlikely(fd[ind].status)) {
			int fail = 1;
			if (unlikely(proto->check_status))
				fail = proto->check_status(&fd[ind].status,
							   proto);

			if (likely(fail)) {
				fprintf(stderr, "error: Bad status return from SEC\n");
				print_frame_desc(&fd[ind]);
				return -1;
			}
		}
	return 0;
}

