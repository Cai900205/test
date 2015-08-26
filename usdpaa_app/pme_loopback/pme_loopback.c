/* Copyright (c) 2011-2012 Freescale Semiconductor, Inc.
 * All rights reserved.
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "pme_loopback.h"
#include <readline.h>
#include <usdpaa/of.h>
#include <usdpaa/fsl_usd.h>
#include <usdpaa/fsl_pme.h>
#include <usdpaa/dma_mem.h>
#include <sys/time.h>
#include <inttypes.h>

#undef DEBUG
#undef USE_MALLOC

#define MICROSECONDS_PER_SECOND 1000000
/* 50 Chars: 1234567890abcdefghijklmnopqrstuvwxyz!@#$%^&*()[]{}? */
static const char alphabet[] =
	"1234567890abcdefghijklmnopqrstuvwxyz!@#$%^&*()[]{}?";

/******************/
/* Worker threads */
/******************/
struct msg_prep_scan {
	int scan_size;
	int pattern_width;
	int low_inflight;
	int high_inflight;
	int use_comp_frame;
};

struct pattern_info {
	char *pattern_data;
	int length;
};

struct msg_prep_scan_2 {
	int scan_size;
	int low_inflight;
	int high_inflight;
	int use_comp_frame;
	struct pattern_info thepattern;
};

struct msg_create_ctx_flow_mode {
	int ren;
	int sessionid;
};

struct worker_msg {
	/* The CLI thread sets ::msg!=worker_msg_none then waits on the barrier.
	 * The worker thread checks for this in its polling loop, and if set it
	 * will perform the desired function, set ::msg=worker_msg_none, then go
	 * into the barrier (releasing itself and the CLI thread). */
	volatile enum worker_msg_type {
		worker_msg_none = 0,
		worker_msg_list,
		worker_msg_quit,
		worker_msg_create_ctx_flow_mode,
		worker_msg_create_ctx_direct_mode,
		worker_msg_delete_ctx,
		worker_msg_prep_scan,
		worker_msg_prep_scan_2,
		worker_msg_free_mem,
		worker_msg_start_scan,
		worker_msg_stop_scan,
		worker_msg_display_stats,
		worker_msg_clear_stats,
	} msg;
	pthread_barrier_t barr;
	union {
		struct msg_prep_scan prep_scan_msg;
		struct msg_create_ctx_flow_mode create_ctx_flow_mode_msg;
		struct msg_prep_scan_2 prep_scan_2_msg;
	};

} ____cacheline_aligned;

struct ctrl_op {
	struct pme_ctx_ctrl_token ctx_ctr;
	unsigned int cb_done;
	enum pme_status cmd_status;
	u8 res_flag;
};

struct stats {
	int in_flight;
	int rx_packets;
	int dropped;
	int total_notifs;
	int num_queue_empty;
	int num_erns;
	int num_truncates;
	int full_fifo;
	struct timeval start_tv;
	struct timeval end_tv;
	int total_start_sec;
	int total_start_usec;
	int total_end_sec;
	int total_end_usec;
	int total_sec;
	int total_usec;
};

struct worker;
struct worker_ref  {
	struct pme_ctx ctx;
	struct worker *worker;
};

struct worker {
	struct worker_ref *ref;
	struct worker_msg *msg;
	int cpu;
	pthread_t id;
	int result;
	struct list_head node;
	struct ctrl_op ctrl_token;
	void *scan_data;
	int scan_size;
	int pattern_width;
	int sessionid;
	int ren;
	struct qm_sg_entry	*sg_table;	/* S-G Table for @fd_in */
	struct qm_fd		fd_in;
	int use_comp_frame;
	int run_scan;
	int low_inflight;
	int high_inflight;
	int produce_suis;
	struct stats stats;
	struct list_head token_list;
} ____cacheline_aligned;

static void add_worker_time(struct worker *worker)
{
	if (worker->stats.end_tv.tv_usec < worker->stats.start_tv.tv_usec) {
		worker->stats.total_usec = MICROSECONDS_PER_SECOND +
			worker->stats.end_tv.tv_usec -
			worker->stats.start_tv.tv_usec;
		worker->stats.total_sec	 =
			worker->stats.end_tv.tv_sec -
			worker->stats.start_tv.tv_sec - 1;
	} else {
		worker->stats.total_usec = worker->stats.end_tv.tv_usec -
						worker->stats.start_tv.tv_usec;
		worker->stats.total_sec = worker->stats.end_tv.tv_sec -
						worker->stats.start_tv.tv_sec;
	}
}

/* -------------------------------- */
/* msg-processing within the worker */

static void scan_cb(struct pme_ctx *ctx, const struct qm_fd *fd,
		struct pme_ctx_token *token)
{
	struct worker* worker = ((struct worker_ref *)ctx)->worker;
	enum pme_status pm_status = pme_fd_res_status(fd);
	unsigned long fd_flags = pme_fd_res_flags(fd);

	if (!(--worker->stats.in_flight))
		++worker->stats.num_queue_empty;
	++worker->stats.total_notifs;
#ifdef USE_MALLOC
	free(token);
#else
	list_add(&token->node, &worker->token_list);
#endif
	if (unlikely(pm_status != pme_status_ok))
		fprintf(stderr, "PME error status 0x%x\n", pm_status);

	if (unlikely(fd_flags & PME_STATUS_TRUNCATED))
		++worker->stats.num_truncates;
}

static void scan_ern_cb(struct pme_ctx *ctx, const struct qm_mr_entry *mr,
		struct pme_ctx_token *token)
{
	struct worker* worker = (struct worker *) ctx;

	if (!(--worker->stats.in_flight))
		++worker->stats.num_queue_empty;
	++worker->stats.total_notifs;
	++worker->stats.num_erns;
#ifdef USE_MALLOC
	free(token);
#else
	list_add(&token->node, &worker->token_list);
#endif
}

static int do_create_ctx_flow_mode(struct worker *worker,
					struct worker_msg *msg)
{
	int ret;
	struct pme_flow flow;

	worker->ref->ctx.cb = scan_cb;
	worker->ref->ctx.ern_cb = scan_ern_cb;

	worker->sessionid = msg->create_ctx_flow_mode_msg.sessionid;
	worker->ren = msg->create_ctx_flow_mode_msg.ren;

	ret = pme_ctx_init(&worker->ref->ctx,
			PME_CTX_FLAG_LOCAL,
			0, /* bpid*/
			4, /* qosin */
			4, /* qosout */
			0,/* ignored if not split mode*/
			NULL /* stashing */);
	if (ret) {
		fprintf(stderr, "pme_ctx_init failed %d\n", ret);
		return ret;
	}

	ret = pme_ctx_enable(&worker->ref->ctx);
	if (ret) {
		fprintf(stderr, "pme_ctx_enable failed %d\n", ret);
		return ret;
	}

	pme_sw_flow_init(&flow);
	flow.sessionid = worker->sessionid;
	flow.ren = worker->ren;;
	ret = pme_ctx_ctrl_update_flow(&worker->ref->ctx, PME_CMD_FCW_ALL,
			&flow, &worker->ctrl_token.ctx_ctr);
	if (ret) {
		fprintf(stderr, "pme_ctx_ctrl_update_flow failed %d\n", ret);
		return ret;
	}
	wait_for_completion(&worker->ctrl_token.cb_done);
#ifdef DEBUG
	printf("done do_create_ctx_flow_mode\n");
#endif
	return 0;
}

static void do_create_ctx_direct_mode(struct worker *worker)
{
	int ret;
	struct qm_fqd_stashing stashing;

	memset(&stashing, 0, sizeof(stashing));
	stashing.context_cl = 2;

	worker->ref->ctx.cb = scan_cb;
	worker->ref->ctx.ern_cb = scan_ern_cb;

	ret = pme_ctx_init(&worker->ref->ctx,
			PME_CTX_FLAG_LOCAL | PME_CTX_FLAG_DIRECT,
			0, /* bpid*/
			4, /* qosin */
			4, /* qosout */
			0,/* ignored if not split mode*/
			&stashing /* stashing */);
	if (ret) {
		fprintf(stderr, "pme_ctx_init failed %d\n", ret);
		return;
	}

	ret = pme_ctx_enable(&worker->ref->ctx);
	if (ret) {
		fprintf(stderr, "pme_ctx_enable failed %d\n", ret);
		return;
	}
}

static int do_delete_ctx(struct worker *worker)
{
	int ret;

	ret = pme_ctx_disable(&worker->ref->ctx, 0,
				&worker->ctrl_token.ctx_ctr);
	if (ret < 0)
		fprintf(stderr, "pme_ctx_disable failed %d\n", ret);
	if (ret > 0)
		/* need to wait */
		wait_for_completion(&worker->ctrl_token.cb_done);
	pme_ctx_finish(&worker->ref->ctx);
	return 0;
}

static int do_prep_scan(struct worker *worker, struct worker_msg *msg)
{
	int ret = 0, pos = 0, i = 0, j = 0, z;
	char pattern[51];

	worker->scan_size = msg->prep_scan_msg.scan_size;
	worker->pattern_width = msg->prep_scan_msg.pattern_width;
	worker->low_inflight = msg->prep_scan_msg.low_inflight;
	worker->high_inflight = msg->prep_scan_msg.high_inflight;
	worker->scan_data = __dma_mem_memalign(L1_CACHE_BYTES,
					       worker->scan_size);
	worker->use_comp_frame = msg->prep_scan_msg.use_comp_frame;

	if (!worker->scan_data) {
		fprintf(stderr, "Failed to alloc scan_data\n");
		return -1;
	}
	memset(worker->scan_data, '.', worker->scan_size);

	/* Set a specific pattern in the scan data that can be used to
	* trigger matches
	*/
	if (worker->pattern_width) {
		i = 1;
		pattern[worker->pattern_width] = 0;
		while ((pos + worker->pattern_width) <= worker->scan_size) {
			for (j = 1; j <= worker->pattern_width; j++) {
				if ((i % j) == 0)
					pattern[j-1] = alphabet[j-1];
				else
					pattern[j-1] = 'X';
			}
			memcpy(worker->scan_data + pos, pattern,
				worker->pattern_width);
#ifdef DEBUG
			if (i <= worker->pattern_width)
				printf("%d:%s\n", i, pattern);
#endif
			i++;
			pos += worker->pattern_width;
		}
		printf("SUI pattern is:\n");
		for (z = 0; z < worker->scan_size; z++)
			printf("%c", ((char *)worker->scan_data)[z]);
		printf("\n");
	}

	worker->sg_table = __dma_mem_memalign(L1_CACHE_BYTES,
					      sizeof(struct qm_sg_entry) * 2);
	/* Initialize each scan_data */
	memset(&worker->fd_in, 0, sizeof(worker->fd_in));
	memset(worker->sg_table, 0, sizeof(struct qm_sg_entry)*2);
	if (worker->use_comp_frame) {
		worker->fd_in.format	= qm_fd_compound;
		qm_fd_addr_set64(&worker->fd_in, pme_map(worker->sg_table));
		worker->sg_table[1].length = worker->scan_size;
		qm_sg_entry_set64(&worker->sg_table[1],
				pme_map(worker->scan_data));
		worker->sg_table[1].final = 1;
		worker->fd_in.cong_weight = 1; /* buffer_size; */
	} else {
		worker->fd_in.format = qm_fd_contig;
		qm_fd_addr_set64(&worker->fd_in, pme_map(worker->scan_data));
		worker->fd_in.length20 = worker->scan_size;
	}
#ifndef USE_MALLOC
	INIT_LIST_HEAD(&worker->token_list);
	for (i = 0; i < worker->high_inflight; i++) {
		struct pme_ctx_token *token =
			malloc(sizeof(struct pme_ctx_token));
		if (!token) {
			fprintf(stderr, "failed to alloc token\n");
			return -1;
		}
		list_add(&token->node, &worker->token_list);
	}
#endif
	return ret;
}

static int do_prep_scan_2(struct worker *worker, struct worker_msg *msg)
{
	int ret = 0, i = 0, z;

	worker->scan_size = msg->prep_scan_2_msg.scan_size;
	worker->low_inflight = msg->prep_scan_2_msg.low_inflight;
	worker->high_inflight = msg->prep_scan_2_msg.high_inflight;
	worker->scan_data = __dma_mem_memalign(L1_CACHE_BYTES,
					       worker->scan_size);
	worker->use_comp_frame = msg->prep_scan_2_msg.use_comp_frame;

	if (!worker->scan_data) {
		fprintf(stderr, "Failed to alloc scan_data\n");
		return -1;
	}
	memset(worker->scan_data, '.', worker->scan_size);

	memcpy(worker->scan_data, msg->prep_scan_2_msg.thepattern.pattern_data,
		((msg->prep_scan_2_msg.thepattern.length > worker->scan_size) ?
			worker->scan_size :
			msg->prep_scan_2_msg.thepattern.length));

	printf("SUI pattern is:\n");
	for (z = 0; z < worker->scan_size; z++)
		printf("%c", ((char *)worker->scan_data)[z]);
	printf("\n");

	worker->sg_table = __dma_mem_memalign(L1_CACHE_BYTES,
					      sizeof(struct qm_sg_entry) * 2);
	/* Initialize each scan_data */
	memset(&worker->fd_in, 0, sizeof(worker->fd_in));
	memset(worker->sg_table, 0, sizeof(struct qm_sg_entry)*2);
	if (worker->use_comp_frame) {
		worker->fd_in.format = qm_fd_compound;
		qm_fd_addr_set64(&worker->fd_in, pme_map(worker->sg_table));
		worker->sg_table[1].length = worker->scan_size;
		qm_sg_entry_set64(&worker->sg_table[1],
				pme_map(worker->scan_data));
		worker->sg_table[1].final = 1;
		worker->fd_in.cong_weight = 1; /* buffer_size; */
	} else {
		worker->fd_in.format = qm_fd_contig;
		qm_fd_addr_set64(&worker->fd_in, pme_map(worker->scan_data));
		worker->fd_in.length20 = worker->scan_size;
	}
#ifndef USE_MALLOC
	INIT_LIST_HEAD(&worker->token_list);
	for (i = 0; i < worker->high_inflight; i++) {
		struct pme_ctx_token *token =
			malloc(sizeof(struct pme_ctx_token));
		if (!token) {
			fprintf(stderr, "failed to alloc token\n");
			return -1;
		}
		list_add(&token->node, &worker->token_list);
	}
#endif
	return ret;
}

static void do_start_scan(struct worker *worker)
{
	memset(&worker->stats, 0, sizeof(worker->stats));
	worker->run_scan = 1;
	worker->use_comp_frame = 0;
	worker->produce_suis = 0;
}

static void do_stop_scan(struct worker *worker)
{
	worker->run_scan = 0;
}

static int do_free_mem(struct worker *worker)
{
	if (worker->sg_table) {
		__dma_mem_free(worker->sg_table);
		worker->sg_table = NULL;
	}
	if (worker->scan_data) {
		__dma_mem_free(worker->scan_data);
		worker->scan_data = NULL;
	}
#ifndef USE_MALLOC
	while (list_empty(&worker->token_list)) {
		struct pme_ctx_token *token = container_of(
			worker->token_list.next, struct pme_ctx_token, node);
		list_del(&token->node);
		free(token);
	}
#endif
	return 0;
}

static void do_clear_stats(struct worker *worker)
{
	memset(&worker->stats, 0, sizeof(worker->stats));
}

static void do_display_stats(struct worker *worker)
{
	printf("Stats for worker on cpu %d\n", worker->cpu);
	printf(" in_flight = %d\n", worker->stats.in_flight);
	printf(" rx_packets = %d\n", worker->stats.rx_packets);
	printf(" dropped_packets = %d\n", worker->stats.dropped);
	printf(" total_notifs = %d\n", worker->stats.total_notifs);
	printf(" num_queue_empty = %d\n", worker->stats.num_queue_empty);
	printf(" num_erns = %d\n", worker->stats.num_erns);
	printf(" num_truncates = %d\n", worker->stats.num_truncates);
	printf(" full_fifo = %d\n", worker->stats.full_fifo);
	printf(" Time: %d.%06d secs\n", worker->stats.total_sec,
					worker->stats.total_usec);
}

static int process_msg(struct worker *worker, struct worker_msg *msg)
{
	int ret = 1;

	/* List */
	if (msg->msg == worker_msg_list)
		printf("Thread alive on cpu %d\n", worker->cpu);

	/* Quit */
	else if (msg->msg == worker_msg_quit)
		ret = 0;

	/* create pme_ctx, flow mode */
	else if (msg->msg == worker_msg_create_ctx_flow_mode)
		do_create_ctx_flow_mode(worker, msg);

	/* create pme_ctx, direct mode */
	else if (msg->msg == worker_msg_create_ctx_direct_mode)
		do_create_ctx_direct_mode(worker);

	/* delete pme_ctx */
	else if (msg->msg == worker_msg_delete_ctx)
		do_delete_ctx(worker);

	else if (msg->msg == worker_msg_prep_scan)
		do_prep_scan(worker, msg);

	else if (msg->msg == worker_msg_prep_scan_2)
		do_prep_scan_2(worker, msg);

	else if (msg->msg == worker_msg_start_scan)
		do_start_scan(worker);

	else if (msg->msg == worker_msg_stop_scan)
		do_stop_scan(worker);

	else if (msg->msg == worker_msg_free_mem)
		do_free_mem(worker);

	else if (msg->msg == worker_msg_display_stats)
		do_display_stats(worker);

	else if (msg->msg == worker_msg_clear_stats)
		do_clear_stats(worker);

	/* What did you want? */
	else
		panic("bad message type");

	/* Release ourselves and the CLI thread from this message */
	msg->msg = worker_msg_none;
	pthread_barrier_wait(&msg->barr);
	return ret;
}

static void ctrl_cb(struct pme_ctx *ctx, const struct qm_fd *fd,
		struct pme_ctx_ctrl_token *token)
{
	struct ctrl_op *ctrl = (struct ctrl_op *)token;

	ctrl->cmd_status = pme_fd_res_status(fd);
	ctrl->res_flag = pme_fd_res_flags(fd);
	complete(&ctrl->cb_done);
}

/* the worker's polling loop calls this function to drive the message pump */
static inline int check_msg(struct worker *worker)
{
	struct worker_msg *msg = worker->msg;
	if (likely(msg->msg == worker_msg_none))
		return 1;
	return process_msg(worker, msg);
}

/* ---------------------- */
/* worker thread function */

static void *worker_fn(void *__worker)
{
	struct worker *worker = __worker;
	cpu_set_t cpuset;
	int s;

	TRACE("This is the thread on cpu %d\n", worker->cpu);

	/* Set this cpu-affinity */
	CPU_ZERO(&cpuset);
	CPU_SET(worker->cpu, &cpuset);
	s = pthread_setaffinity_np(worker->id, sizeof(cpu_set_t), &cpuset);
	if (s != 0) {
		fprintf(stderr, "pthread_setaffinity_np(%d) failed, ret=%d\n",
			worker->cpu, s);
		goto end;
	}

	/* Initialise bman/qman portals */
	s = bman_thread_init();
	if (s) {
		fprintf(stderr, "bman_thread_init(%d) failed, ret=%d\n",
			worker->cpu, s);
		goto end;
	}
	s = qman_thread_init();
	if (s) {
		fprintf(stderr, "qman_thread_init(%d) failed, ret=%d\n",
			worker->cpu, s);
		goto end;
	}

	init_completion(&worker->ctrl_token.cb_done);
	worker->ctrl_token.ctx_ctr.cb = ctrl_cb;

	/* Run! */
	TRACE("Starting poll loop on cpu %d\n", worker->cpu);

	/* check_msg returns 1 when no message,
	* otherwise returns value from process_msg: 0 : quit
	*/
	while (check_msg(worker)) {
		/* I'm in in run_scan state */
		if (worker->run_scan)
			gettimeofday(&worker->stats.start_tv, NULL);
		while (worker->run_scan) {
			struct pme_ctx_token *token;

			if (!worker->produce_suis && worker->stats.in_flight <
					worker->low_inflight) {
				worker->produce_suis = 1;
			}

			if (worker->produce_suis) {
#ifdef USE_MALLOC
				token = malloc(sizeof(*token));
				if (!token) {
					fprintf(stderr, "malloc failed\n");
					break;
				}
#else
				token = container_of(worker->token_list.next,
					struct pme_ctx_token, node);
				list_del(&token->node);
#endif
				s = pme_ctx_scan(&worker->ref->ctx,
						0,
						&worker->fd_in,
						PME_SCAN_ARGS(0, 0, 0xffff),
						token);
				if (s) {
#ifdef USE_MALLOC
					free(token);
#else
					list_add(&token->node,
						&worker->token_list);
#endif
					if (s == -EBUSY) {
						/* enqueue command ring full */
						++worker->stats.full_fifo;
					} else {
						++worker->stats.dropped;
						break;
					}
				} else {
					++worker->stats.rx_packets;
					worker->stats.in_flight++;
					if (worker->stats.in_flight >=
							worker->high_inflight) {
						worker->produce_suis = 0;
					}
				}
			}
			if (!worker->produce_suis) {
				do {
					check_msg(worker);
					qman_poll_dqrr(16);
				} while (worker->stats.in_flight >=
						worker->low_inflight);
			}
			check_msg(worker);
			if (!worker->run_scan && worker->stats.in_flight) {
				while (worker->stats.in_flight)
					qman_poll_dqrr(16);
			}
			gettimeofday(&worker->stats.end_tv, NULL);
			add_worker_time(worker);
		} /* produce suis */
	}

end:
	qman_thread_finish();
	bman_thread_finish();
	TRACE("Leaving thread on cpu %d\n", worker->cpu);
	/* TODO: tear down the portal! */
	pthread_exit(NULL);
}

/* ------------------------------ */
/* msg-processing from main()/CLI */

static void msg_list(struct worker *worker)
{
	struct worker_msg *msg = worker->msg;
	msg->msg = worker_msg_list;
	pthread_barrier_wait(&msg->barr);
}

static void msg_quit(struct worker *worker)
{
	struct worker_msg *msg = worker->msg;
	msg->msg = worker_msg_quit;
	pthread_barrier_wait(&msg->barr);
}

static void msg_create_ctx_flow_mode(struct worker *worker, int sessionid,
					int ren)
{
	struct worker_msg *msg = worker->msg;
	msg->create_ctx_flow_mode_msg.sessionid = sessionid;
	msg->create_ctx_flow_mode_msg.ren = ren;
	msg->msg = worker_msg_create_ctx_flow_mode;
	pthread_barrier_wait(&msg->barr);
}

static void msg_create_ctx_direct_mode(struct worker *worker)
{
	struct worker_msg *msg = worker->msg;
	msg->msg = worker_msg_create_ctx_direct_mode;
	pthread_barrier_wait(&msg->barr);
}

static void msg_delete_ctx(struct worker *worker)
{
	struct worker_msg *msg = worker->msg;
	msg->msg = worker_msg_delete_ctx;
	pthread_barrier_wait(&msg->barr);
}

static void msg_prep_scan(struct worker *worker, int scan_size,
				int pattern_width, int low_inflight,
				int high_inflight, int use_comp_frame)
{
	struct worker_msg *msg = worker->msg;
	msg->prep_scan_msg.scan_size = scan_size;
	msg->prep_scan_msg.pattern_width = pattern_width;
	msg->prep_scan_msg.low_inflight = low_inflight;
	msg->prep_scan_msg.high_inflight = high_inflight;
	msg->prep_scan_msg.use_comp_frame = use_comp_frame;
	msg->msg = worker_msg_prep_scan;
	pthread_barrier_wait(&msg->barr);
}

static void msg_prep_scan_2(struct worker *worker, int scan_size,
				char *pattern_data, int pattern_length,
				int low_inflight, int high_inflight,
				int use_comp_frame)
{
	struct worker_msg *msg = worker->msg;
	msg->prep_scan_2_msg.scan_size = scan_size;
	msg->prep_scan_2_msg.thepattern.pattern_data = pattern_data;
	msg->prep_scan_2_msg.thepattern.length = pattern_length;
	msg->prep_scan_2_msg.low_inflight = low_inflight;
	msg->prep_scan_2_msg.high_inflight = high_inflight;
	msg->prep_scan_2_msg.use_comp_frame = use_comp_frame;
	msg->msg = worker_msg_prep_scan_2;
	pthread_barrier_wait(&msg->barr);
}


static void msg_start_scan(struct worker *worker)
{
	struct worker_msg *msg = worker->msg;
	msg->msg = worker_msg_start_scan;
	pthread_barrier_wait(&msg->barr);
}
static void msg_stop_scan(struct worker *worker)
{
	struct worker_msg *msg = worker->msg;
	msg->msg = worker_msg_stop_scan;
	pthread_barrier_wait(&msg->barr);
}
static void msg_free_mem(struct worker *worker)
{
	struct worker_msg *msg = worker->msg;
	msg->msg = worker_msg_free_mem;
	pthread_barrier_wait(&msg->barr);
}
static void msg_display_stats(struct worker *worker)
{
	struct worker_msg *msg = worker->msg;
	msg->msg = worker_msg_display_stats;
	pthread_barrier_wait(&msg->barr);
}
static void msg_clear_stats(struct worker *worker)
{
	struct worker_msg *msg = worker->msg;
	msg->msg = worker_msg_clear_stats;
	pthread_barrier_wait(&msg->barr);
}
/* ---------------------------- */
/* worker setup from main()/CLI */

static struct worker *worker_new(int cpu)
{
	struct worker *ret;
	int err = posix_memalign((void **)&ret, L1_CACHE_BYTES, sizeof(*ret));
	if (err)
		goto out;
	memset(ret, 0, sizeof(*ret));
	err = posix_memalign((void **)&ret->msg, L1_CACHE_BYTES,
			sizeof(*ret->msg));
	if (err) {
		free(ret);
		goto out;
	}

	ret->ref = __dma_mem_memalign(L1_CACHE_BYTES,sizeof(*ret->ref));
	if (!ret->ref) {
		free(ret->msg);
		free(ret);
		goto out;
	}

	ret->cpu = cpu;
	ret->ref->worker = ret;
	ret->msg->msg = worker_msg_none;
	pthread_barrier_init(&ret->msg->barr, NULL, 2);
	err = pthread_create(&ret->id, NULL, worker_fn, ret);
	if (err) {
		free(ret->msg);
		free(ret);
		goto out;
	}
	/* Block until the worker is in its polling loop (by sending a "list"
	 * command and waiting for it to get processed). This ensures any
	 * start-up logging is produced before the CLI prints another prompt. */
	msg_list(ret);
	return ret;
out:
	fprintf(stderr, "error: failed to create thread for %d\n", cpu);
	return NULL;
}

static void __worker_free(struct worker *worker)
{
	int err, cpu = worker->cpu;
	msg_quit(worker);
	err = pthread_join(worker->id, NULL);
	if (err) {
		/* Leak, but warn */
		fprintf(stderr, "Failed to join thread %d\n", worker->cpu);
		return;
	}
	free(worker->msg);
	__dma_mem_free(worker->ref);
	free(worker);
	printf("Thread killed on cpu %d\n", cpu);
}

/********************/
/* main()/CLI logic */
/********************/

static LIST_HEAD(workers);
static unsigned long ncpus;

static void worker_add(struct worker *worker)
{
	struct worker *i;
	/* Keep workers ordered by cpu */
	list_for_each_entry(i, &workers, node) {
		if (i->cpu >= worker->cpu) {
			list_add_tail(&worker->node, &i->node);
			return;
		}
	}
	list_add_tail(&worker->node, &workers);
}

static void worker_free(struct worker *worker)
{
	list_del(&worker->node);
	__worker_free(worker);
}

static void worker_reap(struct worker *worker)
{
	if (!pthread_tryjoin_np(worker->id, NULL)) {
		list_del(&worker->node);
		__worker_free(worker);
		printf("Caught dead thread, cpu %d\n", worker->cpu);
		free(worker->msg);
		free(worker);
	}
}

/* Parse a cpu id. On entry legit/len contain acceptable "next char" values, on
 * exit *legit points to the "next char" we found. Return -1 for bad * parse. */
static int parse_cpu(const char *str, const char **legit, int legitlen)
{
	char *endptr;
	int ret = -EINVAL;
	/* Extract a ulong */
	unsigned long tmp = strtoul(str, &endptr, 0);
	if ((tmp == ULONG_MAX) || (endptr == str))
		goto out;
	/* Check next char */
	while (legitlen--) {
		if (**legit == *endptr) {
			/* validate range */
			if (tmp >= ncpus) {
				ret = -ERANGE;
				goto out;
			}
			*legit = endptr;
			return (int)tmp;
		}
		(*legit)++;
	}
out:
	fprintf(stderr, "error: invalid cpu '%s'\n", str);
	return ret;
}

/* Parse a cpu range (eg. "3"=="3..3"). Return 0 for valid parse. */
static int parse_cpus(const char *str, int *start, int *end)
{
	/* NB: arrays of chars, not strings. Also sizeof(), not strlen()! */
	static const char PARSE_STR1[] = { ' ', '.', '\0' };
	static const char PARSE_STR2[] = { ' ', '\0' };
	const char *p = &PARSE_STR1[0];
	int ret;
	ret = parse_cpu(str, &p, sizeof(PARSE_STR1));
	if (ret < 0)
		return ret;
	*start = ret;
	if ((p[0] == '.') && (p[1] == '.')) {
		const char *p2 = &PARSE_STR2[0];
		ret = parse_cpu(p + 2, &p2, sizeof(PARSE_STR2));
		if (ret < 0)
			return ret;
	}
	*end = ret;
	return 0;
}

static struct worker *worker_find(int cpu, int want)
{
	struct worker *worker;
	list_for_each_entry(worker, &workers, node) {
		if (worker->cpu == cpu) {
			if (!want)
				fprintf(stderr, "skipping cpu %d, in use.\n",
					cpu);
			return worker;
		}
	}
	if (want)
		fprintf(stderr, "skipping cpu %d, not in use.\n", cpu);
	return NULL;
}


#define call_for_each_worker(str, fn) \
	do { \
		int fstart, fend, fret = parse_cpus(str, &fstart, &fend); \
		if (!fret) { \
			while (fstart <= fend) { \
				struct worker *fw = worker_find(fstart, 1); \
				if (fw) \
					fn(fw); \
				fstart++; \
			} \
		} \
	} while (0)

static const char argp_doc[] = "\n\
USDPAA PME_LOOPBACK-based application\
";
static const char _pme_loopback_args[] = "[cpu-range]";

static const struct argp_option argp_opts[] = {
	{
		"cpu-range", 0,	0, OPTION_DOC, "'index' or 'first'..'last'"
	},
	{}
};

static const struct argp_option prep_scan_argp_opts[] = {
	{"buffer_size", 's', "BUFFER SIZE", OPTION_DOC,
		"\n\r\t\t Scan Buffer size in bytes\n"},
	{"cpu-range", 0, 0, OPTION_DOC, "'index' or 'first'..'last'"},
	{}
};

static error_t pme_loopback_parse(int key, char *arg, struct argp_state *state)
{
	int _errno;
	struct pme_loopback_arguments *args;

	args = (typeof(args))state->input;
	switch (key) {
	case ARGP_KEY_ARGS:
		if (state->argc - state->next != 1)
			argp_usage(state);
		_errno = parse_cpus(state->argv[state->next], &args->first,
					&args->last);
		if (unlikely(_errno < 0))
			argp_usage(state);
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static const struct argp pme_loopback_argp = {
	argp_opts, pme_loopback_parse, _pme_loopback_args, argp_doc
};

struct pme_loopback_arguments pme_loopback_args;

static int pme_loopback_cli_help(int argc, char *argv[])
{
	const struct cli_table_entry *cli_cmd;

	puts("Available commands:");
	foreach_cli_table_entry(cli_cmd) {
		printf("%s ", cli_cmd->cmd);
	}
	puts("");

	return argc != 1 ? -EINVAL : 0;
}

static int pme_loopback_cli_add(int argc, char *argv[])
{
	struct worker *worker;
	int first, last, loop;

	if (argc != 2)
		return -EINVAL;

	if (parse_cpus(argv[1], &first, &last) == 0)
		for (loop = first; loop <= last; loop++) {
			worker = worker_find(loop, 0);
			if (worker)
				continue;
			worker = worker_new(loop);
			if (worker)
				worker_add(worker);
		}
	return 0;
}

static int pme_loopback_cli_list(int argc, char *argv[])
{
	struct worker *worker;

	if (argc > 2)
		return -EINVAL;
	/* cpu-range is an optional argument */
	if (argc > 1)
		call_for_each_worker(argv[1], msg_list);
	else
		list_for_each_entry(worker, &workers, node)
			msg_list(worker);
	return 0;
}

static int pme_loopback_cli_rm(int argc, char *argv[])
{
	struct worker *worker;
	int first, last, loop;

	if (argc != 2)
		return -EINVAL;

	if (parse_cpus(argv[1], &first, &last) == 0) {
		for (loop = first; loop <= last; loop++) {
			worker = worker_find(loop, 1);
			if (!worker)
				continue;
			worker_free(worker);
				continue;
		}
	}
	return 0;
}

static int pme_loopback_cli_create_ctx_flow_mode(int argc, char *argv[])
{
	struct worker *worker;
	int sessionid, ren;

	if ((argc > 4) || (argc < 3))
		return -EINVAL;

	sessionid = atoi(argv[1]);
	ren = atoi(argv[2]);
	/* cpu-range is an optional argument */
	if (argc > 3) {
		do {
			int fstart, fend, fret = parse_cpus(argv[3],
							&fstart, &fend);
			if (!fret) {
				while (fstart <= fend) {
					struct worker *fw =
						worker_find(fstart, 1);
					if (fw)
						msg_create_ctx_flow_mode(fw,
							sessionid,
							ren);
					fstart++;
				}
			}
		} while (0);
	} else {
		list_for_each_entry(worker, &workers, node)
			msg_create_ctx_flow_mode(worker, sessionid, ren);
	}
	return 0;
}

static int pme_loopback_cli_create_ctx_direct_mode(int argc, char *argv[])
{
	struct worker *worker;

	if (argc > 2)
		return -EINVAL;
	/* cpu-range is an optional argument */
	if (argc > 1)
		call_for_each_worker(argv[1], msg_create_ctx_direct_mode);
	else
		list_for_each_entry(worker, &workers, node)
			msg_create_ctx_direct_mode(worker);
	return 0;
}

static int pme_loopback_cli_delete_ctx(int argc, char *argv[])
{
	struct worker *worker;

	if (argc > 2)
		return -EINVAL;
	/* cpu-range is an optional argument */
	if (argc > 1)
		call_for_each_worker(argv[1], msg_delete_ctx);
	else
		list_for_each_entry(worker, &workers, node)
			msg_delete_ctx(worker);
	return 0;
}

static int pme_loopback_cli_prep_scan(int argc, char *argv[])
{
	struct worker *worker;
	int scan_size;
	int pattern_width;
	int low_inflight, high_inflight, use_comp_frame;

	if (argc > 7 || argc < 6)
		return -EINVAL;
	scan_size = atoi(argv[1]);
	pattern_width = atoi(argv[2]);
	low_inflight = atoi(argv[3]);
	high_inflight = atoi(argv[4]);
	use_comp_frame = atoi(argv[5]);

	if (pattern_width > sizeof(alphabet))
		return -EINVAL;

	/* cpu-range is an optional argument */
	if (argc > 6) {
		do {
			int fstart, fend, fret = parse_cpus(argv[6],
							&fstart, &fend);
			if (!fret) {
				while (fstart <= fend) {
					struct worker *fw =
						worker_find(fstart, 1);
					if (fw)
						msg_prep_scan(fw, scan_size,
							pattern_width,
							low_inflight,
							high_inflight,
							use_comp_frame);
					fstart++;
				}
			}
		} while (0);
	} else {
		list_for_each_entry(worker, &workers, node)
			msg_prep_scan(worker, scan_size, pattern_width,
				low_inflight, high_inflight, use_comp_frame);
	}
	return 0;

}

static int pme_loopback_cli_prep_scan_2(int argc, char *argv[])
{
	struct worker *worker;
	int scan_size;
	int low_inflight, high_inflight, use_comp_frame;
	char *pattern_data;
	int pattern_length;

	if (argc > 7 || argc < 6)
		return -EINVAL;
	scan_size = atoi(argv[1]);
	pattern_data = argv[2];
	pattern_length = strlen(pattern_data);
	low_inflight = atoi(argv[3]);
	high_inflight = atoi(argv[4]);
	use_comp_frame = atoi(argv[5]);

	/* cpu-range is an optional argument */
	if (argc > 6) {
		do {
			int fstart, fend, fret = parse_cpus(argv[6],
							&fstart, &fend);
			if (!fret) {
				while (fstart <= fend) {
					struct worker *fw =
						worker_find(fstart, 1);
					if (fw)
						msg_prep_scan_2(fw, scan_size,
							pattern_data,
							pattern_length,
							low_inflight,
							high_inflight,
							use_comp_frame);
					fstart++;
				}
			}
		} while (0);
	} else {
		list_for_each_entry(worker, &workers, node)
			msg_prep_scan_2(worker, scan_size, pattern_data,
			pattern_length, low_inflight, high_inflight,
			use_comp_frame);
	}
	return 0;

}

static int pme_loopback_cli_start_scan(int argc, char *argv[])
{
	struct worker *worker;

	if (argc > 2)
		return -EINVAL;
		/* cpu-range is an optional argument */
	if (argc > 1)
		call_for_each_worker(argv[1], msg_start_scan);
	else
		list_for_each_entry(worker, &workers, node)
			msg_start_scan(worker);
	return 0;
}

static int pme_loopback_cli_stop_scan(int argc, char *argv[])
{
	struct worker *worker;
	uint64_t total_units = 0;
	uint64_t total_units_dropped = 0;
	unsigned total_time_sec = 0;
	unsigned total_time_usec = 0;
	unsigned num_workers = 0;
	unsigned total_packet_size = 0;
	uint64_t pps = 0;
	uint64_t bw = 0;

	if (argc > 2)
		return -EINVAL;
	/* cpu-range is an optional argument */
	if (argc > 1) {
		call_for_each_worker(argv[1], msg_stop_scan);
		call_for_each_worker(argv[1], msg_stop_scan);
	} else {
		list_for_each_entry(worker, &workers, node)
			msg_stop_scan(worker);
		list_for_each_entry(worker, &workers, node)
			msg_stop_scan(worker);
	}

	list_for_each_entry(worker, &workers, node) {
		num_workers++;
		total_units += worker->stats.rx_packets;
		total_time_sec += worker->stats.total_sec;
		total_time_usec += worker->stats.total_usec;
		total_packet_size += worker->scan_size;
		total_units_dropped += worker->stats.dropped;
	}
	if (total_units)
		pps = total_units/((total_time_sec+total_time_usec*1e-6)
			/num_workers);
	bw = total_packet_size / num_workers;
	bw *= 8*pps;
	bw /= (1000*1000);
	printf("Total units scanned: %"PRIu64"\n", total_units);
	printf("Total units dropped: %"PRIu64"\n", total_units_dropped);
	printf("Total time: %f sec\n",
		(total_time_sec+total_time_usec*1e-6)/num_workers);
	printf("Scan Units per second: %"PRIu64"\n", pps);
	printf("Bandwidth: %"PRIu64" Mbps\n", bw);
	return 0;
}

static int pme_loopback_cli_free_mem(int argc, char *argv[])
{
	struct worker *worker;

	if (argc > 2)
		return -EINVAL;
	/* cpu-range is an optional argument */
	if (argc > 1)
		call_for_each_worker(argv[1], msg_free_mem);
	else
		list_for_each_entry(worker, &workers, node)
			msg_free_mem(worker);
	return 0;
}

static int pme_loopback_cli_display_stats(int argc, char *argv[])
{
	struct worker *worker;

	if (argc > 2)
		return -EINVAL;
	/* cpu-range is an optional argument */
	if (argc > 1)
		call_for_each_worker(argv[1], msg_display_stats);
	else
		list_for_each_entry(worker, &workers, node)
			msg_display_stats(worker);
	return 0;
}

static int pme_loopback_cli_clear_stats(int argc, char *argv[])
{
	struct worker *worker;

	if (argc > 2)
		return -EINVAL;
	/* cpu-range is an optional argument */
	if (argc > 1)
		call_for_each_worker(argv[1], msg_clear_stats);
	else
		list_for_each_entry(worker, &workers, node)
			msg_clear_stats(worker);
	return 0;
}

cli_cmd(help, pme_loopback_cli_help);
cli_cmd(add, pme_loopback_cli_add);
cli_cmd(list, pme_loopback_cli_list);
cli_cmd(rm, pme_loopback_cli_rm);
cli_cmd(create_ctx_flow_mode, pme_loopback_cli_create_ctx_flow_mode);
cli_cmd(create_ctx_direct_mode, pme_loopback_cli_create_ctx_direct_mode);
cli_cmd(delete_ctx, pme_loopback_cli_delete_ctx);
cli_cmd(prep_scan, pme_loopback_cli_prep_scan);
cli_cmd(prep_scan_2, pme_loopback_cli_prep_scan_2);
cli_cmd(start_scan, pme_loopback_cli_start_scan);
cli_cmd(stop_scan, pme_loopback_cli_stop_scan);
cli_cmd(free_mem, pme_loopback_cli_free_mem);
cli_cmd(display_stats, pme_loopback_cli_display_stats);
cli_cmd(clear_stats, pme_loopback_cli_clear_stats);

const char pme_loopback_prompt[] __attribute__((weak)) = "> ";

int main(int argc, char *argv[])
{
	struct worker *worker, *tmpworker;
	int rcode, loop, cli_argc;
	char *cli, **cli_argv;
	const struct cli_table_entry *cli_cmd;


	rcode = of_init();
	if (rcode) {
		pr_err("of_init() failed\n");
		exit(EXIT_FAILURE);
	}

	ncpus = (unsigned long)sysconf(_SC_NPROCESSORS_ONLN);

	if (ncpus > 1) {
		pme_loopback_args.first = 0;
		pme_loopback_args.last = 0;
	}

	rcode = argp_parse(&pme_loopback_argp, argc, argv, 0, NULL,
			&pme_loopback_args);
	if (unlikely(rcode != 0))
		return -rcode;

	/* - initialise DPAA */
	rcode = qman_global_init();
	if (rcode)
		fprintf(stderr, "error: qman global init, continuing\n");
	rcode = bman_global_init();
	if (rcode)
		fprintf(stderr, "error: bman global init, continuing\n");
	/* - map shmem */
	TRACE("Initialising dma_mem\n");
	dma_mem_generic = dma_mem_create(DMA_MAP_FLAG_ALLOC, NULL, 0x1000000);
	if (!dma_mem_generic)
		fprintf(stderr, "error: shmem init, continuing\n");

	/* Create the threads */
	TRACE("Starting %d threads for cpu-range '%d..%d'\n",
	      0, 0);
	for (loop = pme_loopback_args.first; loop <= pme_loopback_args.last;
			loop++) {
		worker = worker_new(loop);
		if (!worker) {
			rcode = -1;
			goto leave;
		}
		worker_add(worker);
	}

	while (1) {
		/* Reap any dead threads */
		list_for_each_entry_safe(worker, tmpworker, &workers, node)
			worker_reap(worker);

		/* Get command */
		cli = readline(pme_loopback_prompt);
		if (unlikely((cli == NULL) || strncmp(cli, "q", 1) == 0))
			break;
		if (cli[0] == 0) {
			free(cli);
			continue;
		}

		cli_argv = history_tokenize(cli);
		if (unlikely(cli_argv == NULL)) {
			fprintf(stderr, "Out of memory while parsing: %s\n",
				cli);
			free(cli);
			continue;
		}
		for (cli_argc = 0; cli_argv[cli_argc] != NULL; cli_argc++);

		foreach_cli_table_entry(cli_cmd) {
			if (strcmp(cli_argv[0], cli_cmd->cmd) == 0) {
				rcode = cli_cmd->handle(cli_argc, cli_argv);
				if (unlikely(rcode < 0)) {
					fprintf(stderr, "%s: %s\n",
						cli_cmd->cmd, strerror(-rcode));
				}
				add_history(cli);
				break;
			}
		}

		if (cli_cmd == cli_table_end)
			fprintf(stderr, "Unknown command: %s\n", cli);

		for (cli_argc = 0; cli_argv[cli_argc] != NULL; cli_argc++)
			free(cli_argv[cli_argc]);
		free(cli_argv);
		free(cli);
	}
	/* success */
	rcode = 0;

leave:
	list_for_each_entry_safe(worker, tmpworker, &workers, node) {
		worker_free(worker);
	}
	of_finish();
	return rcode;
}
