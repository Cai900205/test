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

#define NUM_IGNORE		18
#define NUM_BUFS_PER_RELEASE	1
#undef DO_CHECKSUMS
#define RELEASE_BACKOFF		200

/***************/
/* global vars */
/***************/

#ifdef DO_CHECKSUMS
/* When releasing, this is our checksum */
static __PERCPU u32 rsum;
/* When acquiring, this is our checksum */
static __PERCPU u32 asum;
#endif

static __PERCPU u64 rel_capture[2] ____cacheline_aligned;
static __PERCPU u64 acq_capture[2] ____cacheline_aligned;
static __PERCPU u32 test_buffers;
static __PERCPU u32 test_start;
static __PERCPU u32 rel_jam, acq_jam;

/* This is our test array (only used by index 0) */
struct test {
	int num_cpus;
	int stockpiling;
	u32 num_buffers;
};
static const struct test tests[] = {
	{ 1, 0, 8192},
	{ 2, 0, 8192},
	{ 4, 0, 8192},
	{ 7, 0, 8192},
	{ 1, 1, 8192},
	{ 2, 1, 8192},
	{ 4, 1, 8192},
	{ 7, 1, 8192},
	{ 0, 0, 0}
};

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

#ifdef DO_CHECKSUMS
#define fill_in(b) \
do { \
	rsum += base; \
	bm_buffer_set64(&b, base++); \
} while (0)
#define read_in(b) \
do { \
	asum += bm_buffer_get64(&b);
} while (0)
#else
#define fill_in(b) \
do { \
	bm_buffer_set64(&b, base++); \
} while (0)
#define read_in(b)
#endif

static void do_releases(struct bman_pool *pool)
{
	struct bm_buffer bufs[NUM_BUFS_PER_RELEASE];
	unsigned int loop = test_buffers;
	u32 base = 0xdeadbeef;
#ifdef DO_CHECKSUMS
	rsum = 0;
#endif
	dcbt_rw(rel_capture);
	while (loop) {
		int err;
		if (loop == test_start)
			rel_capture[0] = mfatb();
		/* Loops (and gcc) seem to generate more cycles than the
		 * interface we're trying to benchmark! Take matters into one's
		 * own hands... */
#if NUM_BUFS_PER_RELEASE > 0
		fill_in(bufs[0]);
#endif
#if NUM_BUFS_PER_RELEASE > 1
		fill_in(bufs[1]);
#endif
#if NUM_BUFS_PER_RELEASE > 2
		fill_in(bufs[2]);
#endif
#if NUM_BUFS_PER_RELEASE > 3
		fill_in(bufs[3]);
#endif
#if NUM_BUFS_PER_RELEASE > 4
		fill_in(bufs[4]);
#endif
#if NUM_BUFS_PER_RELEASE > 5
		fill_in(bufs[5]);
#endif
#if NUM_BUFS_PER_RELEASE > 6
		fill_in(bufs[6]);
#endif
#if NUM_BUFS_PER_RELEASE > 7
		fill_in(bufs[7]);
#endif
retry:
		err = bman_release(pool, &bufs[0], NUM_BUFS_PER_RELEASE, 0);
		if (unlikely(err)) {
			rel_jam++;
			cpu_spin(RELEASE_BACKOFF);
			goto retry;
		}
		loop--;
	}
	rel_capture[1] = mfatb();
}

static void do_acquires(struct bman_pool *pool)
{
	struct bm_buffer bufs[NUM_BUFS_PER_RELEASE];
	unsigned int loop = test_buffers;
#ifdef DO_CHECKSUMS
	asum = 0;
#endif
	dcbt_rw(acq_capture);
	while (loop) {
		int err;
		if (loop == test_start)
			acq_capture[0] = mfatb();
retry:
		err = bman_acquire(pool, &bufs[0], NUM_BUFS_PER_RELEASE, 0);
		if (unlikely(err != NUM_BUFS_PER_RELEASE)) {
			BUG_ON(err >= 0);
			acq_jam++;
			barrier();
			goto retry;
		}
#if NUM_BUFS_PER_RELEASE > 0
		read_in(bufs[0]);
#endif
#if NUM_BUFS_PER_RELEASE > 1
		read_in(bufs[1]);
#endif
#if NUM_BUFS_PER_RELEASE > 2
		read_in(bufs[2]);
#endif
#if NUM_BUFS_PER_RELEASE > 3
		read_in(bufs[3]);
#endif
#if NUM_BUFS_PER_RELEASE > 4
		read_in(bufs[4]);
#endif
#if NUM_BUFS_PER_RELEASE > 5
		read_in(bufs[5]);
#endif
#if NUM_BUFS_PER_RELEASE > 6
		read_in(bufs[6]);
#endif
#if NUM_BUFS_PER_RELEASE > 7
		read_in(bufs[7]);
#endif
		loop--;
	}
	acq_capture[1] = mfatb();
}

void blastman(struct worker *worker)
{
	struct bman_pool_params params;
	struct bman_pool *pool = NULL; /* unnecessary, but gcc doesn't see */
	const struct test *test = &tests[0];

	pr_info("BLAST: --- starting high-level test (cpu %d) ---\n",
		worker->cpu);
	sync_all();

	while (test->num_cpus) {
		int doIrun = (worker->total_cpus < test->num_cpus) ? 0 :
			((worker->idx < test->num_cpus) ? 1 : 0);

		test_buffers = test->num_buffers;
		test_start = test->num_buffers - NUM_IGNORE;
		rel_jam = acq_jam = 0;

		if (doIrun) {
			params.flags = BMAN_POOL_FLAG_DYNAMIC_BPID;
			if (test->stockpiling)
				params.flags |= BMAN_POOL_FLAG_STOCKPILE;
			pool = bman_new_pool(&params);
			if (!pool)
				panic("bman_new_pool() failed\n");
		}

		sync_all();

		if (doIrun) {
			do_releases(pool);
			do_acquires(pool);
		}

		sync_all();

		if (doIrun) {
			bman_free_pool(pool);
			pr_info("test {%d,%c,%d} rel %d, acq %d, jam (%d:%d)\n",
				test->num_cpus, test->stockpiling ? 'Y' : 'x',
				test->num_buffers, avg(rel_capture),
				avg(acq_capture), rel_jam, acq_jam);
		}
		test++;
	}
	sync_all();
	pr_info("BLAST: --- finished high-level test (cpu %d) ---\n",
		worker->cpu);
}

