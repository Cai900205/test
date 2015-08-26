/* Copyright (c) 2011 Freescale Semiconductor, Inc.
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

#include "thread_priv.h"

static pthread_barrier_t barr;
static __thread struct thread_data *__my_thread_data;

static struct thread_data *g_ctxs;
static int g_num_ctxs;

struct thread_data *my_thread_data(void)
{
	return __my_thread_data;
}

static void *thread_wrapper(void *arg)
{
	struct thread_data *tdata = (struct thread_data *)arg;
	cpu_set_t cpuset;
	int s;

	__my_thread_data = tdata;
	/* Set this thread affine to cpu */
	CPU_ZERO(&cpuset);
	CPU_SET(tdata->cpu, &cpuset);
	s = pthread_setaffinity_np(tdata->id, sizeof(cpu_set_t), &cpuset);
	if (s != 0) {
		handle_error_en(s, "pthread_setaffinity_np");
		goto end;
	}
	s = qman_thread_init();
	if (s) {
		fprintf(stderr, "qman_thread_init(%d) failed, ret=%d\n",
			tdata->cpu, s);
		goto end;
	}
	/* First barrier allows global fqalloc initialisation to start */
	pthread_barrier_wait(&barr);
	/* Second barrier waits for fqalloc init to complete */
	pthread_barrier_wait(&barr);
	/* Invoke the application thread function */
	s = tdata->fn(tdata);
end:
	__my_thread_data = NULL;
	tdata->result = s;
	return NULL;
}

void sigint(int signo)
{
	int i;
	(void)signo;

	for (i = 0; i < g_num_ctxs; i++)
		pthread_cancel(g_ctxs[i].id);
}

int start_threads_custom(struct thread_data *ctxs, int num_ctxs)
{
	int i, err;
	struct thread_data *ctx;
	sigset_t sigset, oldset;
	struct sigaction s;

	/*
	 * Remove SIGINT handling from this thread: the worker threads will
	 * inherit the signal mask and it's not desirable to have the worker
	 * threads handle SIGINT themselves, so this will avoid them catching
	 * the signal instead of this thread.
	 */
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGINT);
	err = pthread_sigmask(SIG_BLOCK, &sigset, &oldset);
	if (err) {
		perror("clear SIGINT on main thread failed: ");
		return err;
	}

	/* Create the barrier used for qman_fqalloc_init() */
	i = pthread_barrier_init(&barr, NULL, num_ctxs + 1);
	/* Create the threads */
	for (i = 0, ctx = &ctxs[0]; i < num_ctxs; i++, ctx++) {
		/* Create+start the thread */
		err = pthread_create(&ctx->id, NULL, thread_wrapper, ctx);
		if (err != 0) {
			fprintf(stderr, "error starting thread %d, %d already "
				"started\n", i, i - 1);
			return err;
		}
	}

	g_num_ctxs = num_ctxs;
	g_ctxs = &ctxs[0];

	/*
	 * Install the signal handler for SIGINT.
	 */
	s.sa_handler = sigint;
	sigemptyset(&s.sa_mask);
	err = sigaction(SIGINT, &s, NULL);
	if (err) {
		perror("failed to install SIGINT handler: ");
		return err;
	}

	/*
	 *  Restore the signal mask for this thread, thus making sure it is the
	 *  only one receiving SIGINT and acting upon it.
	 */
	err = pthread_sigmask(SIG_SETMASK, &oldset, NULL);
	if (err) {
		perror("failed to restore signal mask for main thread: ");
		return err;
	}

	/* Wait till threads have initialised thread-local qman/bman */
	pthread_barrier_wait(&barr);
	/* Release threads to start processing again */
	pthread_barrier_wait(&barr);
	return 0;
}

int wait_threads(struct thread_data *ctxs, int num_ctxs)
{
	int i, err = 0;
	struct thread_data *ctx;
	/* Wait for them to join */
	for (i = 0, ctx = &ctxs[0]; i < num_ctxs; i++, ctx++) {
		int res = pthread_join(ctx->id, NULL);
		if (res != 0) {
			fprintf(stderr, "error joining thread %d\n", i);
			if (!err)
				err = res;
		}
	}
	return err;
}
