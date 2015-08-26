/* Copyright (c) 2009-2011 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
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

#include "private.h"

/*************/
/* constants */
/*************/

#define NUM_IGNORE	18
#define NUM_CPUS	2
#undef TEST_FD
#define ENQUEUE_BACKOFF	200

/*************************************/
/* Predeclarations (eg. for fq_base) */
/*************************************/

static enum qman_cb_dqrr_result cb_dqrr(struct qman_portal *,
					struct qman_fq *,
					const struct qm_dqrr_entry *);
static void cb_ern(struct qman_portal *, struct qman_fq *,
			const struct qm_mr_entry *);
static const struct qman_fq fq_base = {
	.cb = {
		.dqrr = cb_dqrr,
		.ern = cb_ern
	}
};

/***************/
/* global vars */
/***************/

static __PERCPU struct qm_fd fd;
static __PERCPU struct qm_fd fd_dq;

static __PERCPU u64 eq_capture[2] ____cacheline_aligned;
static __PERCPU u64 dq_capture[2] ____cacheline_aligned;
static __PERCPU u32 test_frames;
static __PERCPU u32 test_start;
static __PERCPU u32 dq_count;
static __PERCPU u32 eq_jam;
static __PERCPU int sdqcr_complete;

/* This is our test array (only used by index 0) */
struct test {
	int num_cpus;
	u32 num_enqueues;
};
static const struct test tests[] = {
	{ 1, 8192},
	{ 2, 8192},
	{ 4, 8192},
	{ 7, 8192},
	{ 0, 0}
};

/* Helpers for initialising and "incrementing" a frame descriptor */
static void fd_init(struct qm_fd *__fd)
{
	qm_fd_addr_set64(__fd, 0xabdeadbeefLLU);
	__fd->format = qm_fd_contig_big;
	__fd->length29 = 7;
	__fd->cmd = 0xfeedf00d;
}

#ifdef TEST_FD
static void fd_inc(struct qm_fd *__fd)
{
	__fd->addr_lo++;
	__fd->addr_hi--;
	__fd->cmd++;
}

/* The only part of the 'fd' we can't memcmp() is the ppid */
static int fd_cmp(const struct qm_fd *a, const struct qm_fd *b)
{
	int r = a->addr_hi - b->addr_hi;
	if (!r)
		r = a->addr_lo - b->addr_lo;
	if (!r)
		r = a->format - b->format;
	if (!r)
		r = a->opaque - b->opaque;
	if (!r)
		r = a->cmd - b->cmd;
	return r;
}
#endif

static int elapse(u64 *a)
{
	return (int)(a[1] - a[0]);
}

static int avg(u64 *a)
{
	int e = elapse(a);
	int d = test_start;
	return (e + (d / 2)) / d;
}

/********/
/* test */
/********/

static void do_enqueues(struct qman_fq *fq)
{
	unsigned int loop = test_frames;
	dcbt_rw(eq_capture);
	while (loop) {
		int err;
		if (loop == test_start)
			eq_capture[0] = mfatb();
retry:
		err = qman_enqueue(fq, &fd, 0);
		if (err) {
			eq_jam++;
			cpu_spin(ENQUEUE_BACKOFF);
			goto retry;
		}
#ifdef TEST_FD
		fd_inc(&fd);
#endif
		loop--;
	}
	eq_capture[1] = mfatb();
}

static enum qman_cb_dqrr_result cb_dqrr(struct qman_portal *p __always_unused,
					struct qman_fq *fq __always_unused,
				const struct qm_dqrr_entry *dq __maybe_unused)
{
#ifdef TEST_FD
	if (fd_cmp(&fd_dq, &dq->fd)) {
		pr_err("BADNESS: dequeued frame doesn't match;\n");
		pr_err("Got:\n");
		hexdump(&dq->fd, sizeof(dq->fd));
		pr_err("Expected:\n");
		hexdump(&fd_dq, sizeof(fd_dq));
		BUG();
	}
	fd_inc(&fd_dq);
#endif
	if (dq_count-- == test_start)
		dq_capture[0] = mfatb();
	else if (!dq_count) {
		dq_capture[1] = mfatb();
		BUG_ON(!(dq->stat & QM_DQRR_STAT_FQ_EMPTY));
		sdqcr_complete = 1;
	}
	return qman_cb_dqrr_consume;
}

static void cb_ern(struct qman_portal *p __always_unused,
			struct qman_fq *fq __always_unused,
			const struct qm_mr_entry *msg __always_unused)
{
	panic("cb_ern() unimplemented");
}

void speed(struct worker *worker)
{
	struct qman_fq *fq;
	const struct test *test = &tests[0];

	pr_info("SPEED: --- starting high-level test (cpu %d) ---\n",
		worker->cpu);
	fq = __dma_mem_memalign(L1_CACHE_BYTES, sizeof(*fq));
	BUG_ON(!fq);
	memcpy(fq, &fq_base, sizeof(fq_base));
	sync_all();

	fd_init(&fd);
	fd_init(&fd_dq);

	while (test->num_cpus) {
		int doIrun = (worker->total_cpus < test->num_cpus) ? 0 :
			((worker->idx < test->num_cpus) ? 1 : 0);

		test_frames = test->num_enqueues;
		test_start = test->num_enqueues - NUM_IGNORE;
		dq_count = test->num_enqueues;
		eq_jam = 0;

		if (doIrun) {
			struct qm_mcc_initfq initfq;
			/* Initialise (parked) FQ */
			if (qman_create_fq(0, QMAN_FQ_FLAG_DYNAMIC_FQID, fq))
				panic("qman_create_fq() failed\n");
			memset(&initfq, 0, sizeof(initfq));
			initfq.we_mask = QM_INITFQ_WE_FQCTRL |
					QM_INITFQ_WE_CONTEXTA;
			initfq.fqd.fq_ctrl = QM_FQCTRL_CTXASTASHING;
			initfq.fqd.context_a.stashing.context_cl =
				(sizeof(*fq) + L1_CACHE_BYTES - 1) / L1_CACHE_BYTES;

			if (qman_init_fq(fq, QMAN_INITFQ_FLAG_LOCAL, &initfq))
				panic("qman_init_fq() failed\n");
		}

		sync_all();

		if (doIrun) {
			do_enqueues(fq);
			dcbt_rw(&dq_count);
			dcbt_rw(dq_capture);
			if (qman_schedule_fq(fq))
				panic("qman_schedule_fq() failed\n");
			wait_event(nothing, sdqcr_complete);
			sdqcr_complete = 0;
		}

		sync_all();

		if (doIrun) {
			u32 flags;
			if (qman_retire_fq(fq, &flags))
				panic("qman_retire_fq() failed\n");
			BUG_ON(flags & (QMAN_FQ_STATE_CHANGING |
				QMAN_FQ_STATE_NE | QMAN_FQ_STATE_ORL));
			if (qman_oos_fq(fq))
				panic("qman_oos_fq() failed\n");
			qman_destroy_fq(fq, 0);
			pr_info("test {%d,%d} eq %d, dq %d, eq_jam %d\n",
				test->num_cpus, test->num_enqueues,
				avg(eq_capture), avg(dq_capture), eq_jam);
		}
		test++;
	}
	sync_all();
	pr_info("SPEED: --- finished high-level test (cpu %d) ---\n",
		worker->cpu);
	__dma_mem_free(fq);
}
