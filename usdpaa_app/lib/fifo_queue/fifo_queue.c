/* Copyright (c) 2013 Freescale Semiconductor, Inc.
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
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * FIFO queue implementation
 */

#include <errno.h>
#include <error.h>
#include <string.h>
#include <stdlib.h>
#include "fifo_queue.h"


#define FIFO_ENTRY_VALID	0x1
#define FIFO_ENTRY_INVALID	0x0


int fifo_create(struct fifo_q *q, unsigned size)
{
	int err = 0;

	if (!q)
		return -EINVAL;

	memset(q, 0, sizeof(struct fifo_q));
	q->size = size;

	err = pthread_mutex_init(&q->get_lock, NULL);
	if (err)
		return err;

	err = pthread_mutex_init(&q->put_lock, NULL);
	if (err)
		return err;

	if (!size)
		return 0;

	q->q = (struct fifo_q_entry*) calloc(size,
					sizeof(struct fifo_q_entry));
	if (!q->q)
		return -ENOMEM;

	return err;
}

int fifo_set_alert(struct fifo_q *q,
		unsigned alert_thd,
		fifo_q_alert alert_func)
{
	if (!q)
		return -EINVAL;

	if (!alert_func)
		return -EINVAL;

	if (alert_thd > q->size) {
		error(0, EINVAL, "FIFO queue alert threshold (%d) exceeds queue size (%d).",
			alert_thd, q->size);
		return -EINVAL;
	}

	q->alert_thd	= alert_thd;
	q->alert_func	= alert_func;

	return 0;
}

int fifo_destroy(struct fifo_q *q)
{
	if (!q)
		return -EINVAL;

	if (!q->size)
		return 0;

	free(q->q);

	memset(q, 0, sizeof(struct fifo_q));

	return 0;
}

int fifo_add(struct fifo_q *q, void *p)
{
	int err = 0;
	unsigned use;

	if (!q)
		return -EINVAL;

	err = pthread_mutex_lock(&q->put_lock);
	if (err < 0)
		return err;

	if (q->q[q->in].flags & FIFO_ENTRY_VALID) {
		/* No more room */
		pthread_mutex_unlock(&q->put_lock);
		return -ENOSPC;
	}

	q->q[q->in++].p = p;
	q->q[q->in - 1].flags |= FIFO_ENTRY_VALID;

	/* Wrap around */
	if (q->in >= q->size)
		q->in = 0;

	err = pthread_mutex_unlock(&q->put_lock);

	/* Check for alert */
	if (q->alert_func) {
		if (q->in < q->out)
			use = q->size - q->out + q->in;
		else
			use = q->in - q->out;
		if (use >= q->alert_thd)
			q->alert_func(q);
	}

	return err;
}

void *fifo_try_get(struct fifo_q *q)
{
	int err;
	void *ret = NULL;

	if (!q)
		return NULL;

	err = pthread_mutex_lock(&q->get_lock);
	if (err < 0)
		return NULL;

	if (q->q[q->out].flags & FIFO_ENTRY_VALID) {
		ret = q->q[q->out++].p;
		q->q[q->out - 1].flags = FIFO_ENTRY_INVALID;

		/* Wrap around */
		if (q->out >= q->size)
			q->out = 0;
	}

	pthread_mutex_unlock(&q->get_lock);

	return ret;
}
