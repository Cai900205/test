/* Copyright (c) 2010-2011 Freescale Semiconductor, Inc.
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

/* Barrier used by tests running across all threads */
static pthread_barrier_t barr;

void sync_all(void)
{
	pthread_barrier_wait(&barr);
}

static LIST_HEAD(workers);
static unsigned long ncpus;

/* ensure no stale notifications are in the portals - not that they'd be
 * incorrectly handled later on, but we don't want them impacting subsequent
 * benchmarks. */
static void calm_down(void)
{
	qman_poll_dqrr(16);
	qman_poll_slow();
	bman_poll();
	/* For kicks, sync the cpus prior to starting the next test */
	sync_all();
}

static void *worker_fn(void *__worker)
{
	cpu_set_t cpuset;
	struct worker *worker = __worker;
	const struct qman_portal_config *qconfig;
	const struct bman_portal_config *bconfig;
	int err;

	/* Set cpu affinity */
	CPU_ZERO(&cpuset);
	CPU_SET(worker->cpu, &cpuset);
	err = pthread_setaffinity_np(worker->id, sizeof(cpu_set_t), &cpuset);
	if (err != 0) {
		fprintf(stderr, "pthread_setaffinity_np(%d) failed, ret=%d\n",
			worker->cpu, err);
		exit(EXIT_FAILURE);
	}

	if (worker->do_global_init) {
		/* Set up the bpid allocator */
		err = bman_global_init();
		if (err) {
			fprintf(stderr, "bman_global_init() failed, ret=%d\n",
				err);
			exit(EXIT_FAILURE);
		}
		/* Set up the fqid allocator */
		err = qman_global_init();
		if (err) {
			fprintf(stderr, "qman_global_init() failed, ret=%d\n",
				err);
			exit(EXIT_FAILURE);
		}
		/* Map the DMA-able memory */
		dma_mem_generic = dma_mem_create(DMA_MAP_FLAG_ALLOC, NULL,
						 0x400000);
		if (!dma_mem_generic) {
			fprintf(stderr, "dma_mem_create() failed\n");
			exit(EXIT_FAILURE);
		}
		/* The main thread is waiting on this */
		pthread_barrier_wait(&worker->global_init_barrier);
	}

	/* Initialise bman/qman portals */
	err = bman_thread_init();
	if (err) {
		fprintf(stderr, "bman_thread_init(%d) failed, ret=%d\n",
			worker->cpu, err);
		exit(EXIT_FAILURE);
	}
	err = qman_thread_init();
	if (err) {
		fprintf(stderr, "qman_thread_init(%d) failed, ret=%d\n",
			worker->cpu, err);
		exit(EXIT_FAILURE);
	}

	qconfig = qman_get_portal_config();
	bconfig = bman_get_portal_config();
	printf("Worker %d, qman={cpu=%d,irq=%d,ch=%d,pools=0x%08x}\n"
		"          bman={cpu=%d,irq=%d,mask=0x%08x_%08x}\n",
		worker->cpu,
		qconfig->cpu, qconfig->irq, qconfig->channel, qconfig->pools,
		bconfig->cpu, bconfig->irq,
		bconfig->mask.__state[0], bconfig->mask.__state[1]);

#if 0
	qman_test_high(worker);
	calm_down();
	bman_test_high(worker);
	calm_down();
#endif
	speed(worker);
	calm_down();
	blastman(worker);
	calm_down();

	printf("Worker %d exiting\n", worker->cpu);
	return 0;
}

static struct worker *worker_new(int cpu, int do_global_init,
				int idx, int total)
{
	struct worker *ret;
	int err = posix_memalign((void **)&ret, L1_CACHE_BYTES, sizeof(*ret));
	ret->cpu = cpu;
	ret->do_global_init = do_global_init;
	ret->idx = idx;
	ret->total_cpus = total;
	if (do_global_init)
		pthread_barrier_init(&ret->global_init_barrier, NULL, 2);
	err = pthread_create(&ret->id, NULL, worker_fn, ret);
	if (err) {
		free(ret);
		goto out;
	}
	if (do_global_init)
		pthread_barrier_wait(&ret->global_init_barrier);
	return ret;
out:
	fprintf(stderr, "error: failed to create thread for %d\n", cpu);
	return NULL;
}

/* Keep "workers" ordered by cpu on insert */
static void worker_add(struct worker *worker)
{
	struct worker *i;
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
	int err;
	list_del(&worker->node);
	err = pthread_join(worker->id, NULL);
	if (err) {
		/* Leak, but warn */
		fprintf(stderr, "Failed to join thread %d\n", worker->cpu);
		return;
	}
	free(worker);
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

static void usage(void)
{
	fprintf(stderr, "usage: qbman [cpu-range]\n");
	fprintf(stderr, "where [cpu-range] is 'n' or 'm..n'\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	struct worker *worker, *tmpw;
	int ret, first, last, loop;

	ret = of_init();
	if (ret) {
		pr_err("of_init() failed\n");
		exit(EXIT_FAILURE);
	}

	ncpus = (unsigned long)sysconf(_SC_NPROCESSORS_ONLN);

	/* Parse the args */
	if (ncpus == 1)
		first = last = 0;
	else {
		first = 1;
		last = ncpus - 1;
	}
	if (argc == 2) {
		ret = parse_cpus(argv[1], &first, &last);
		if (ret)
			usage();
	} else if (argc != 1)
		usage();

	/* Create the barrier used by sync_all() */
	ret = pthread_barrier_init(&barr, NULL, last - first + 1);
	if (ret != 0) {
		fprintf(stderr, "Failed to init barrier\n");
		exit(EXIT_FAILURE);
	}

	/* Create the threads */
	for (loop = first; loop <= last; loop++) {
		worker = worker_new(loop, (loop == first), loop - first,
					last - first + 1);
		if (!worker)
			panic("worker_new() failed");
		worker_add(worker);
	}

	/* Catch their exit */
	list_for_each_entry_safe(worker, tmpw, &workers, node)
		worker_free(worker);
	of_finish();
	return 0;
}
