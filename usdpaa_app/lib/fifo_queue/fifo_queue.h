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
 * FIFO queue interface
 */

#ifndef __FIFO_QUEUE_H
#define __FIFO_QUEUE_H


#include <pthread.h>


struct fifo_q;

typedef void (*fifo_q_alert)(const struct fifo_q *);


struct fifo_q_entry {
	int	flags;
	void	*p;
};

struct fifo_q {
	struct fifo_q_entry	*q;
	unsigned		size;
	unsigned		in;
	unsigned		out;
	unsigned		alert_thd;
	fifo_q_alert		alert_func;
	pthread_mutex_t		get_lock;
	pthread_mutex_t		put_lock;
};


/* Creates a FIFO queue starting from a provided FIFO queue data structure. */
int fifo_create(struct fifo_q *q, unsigned size);

/* Sets an alert callback to be called when a certain threshold is reached. */
int fifo_set_alert(struct fifo_q *q,
		unsigned threshold,
		fifo_q_alert alert_func);

/* Destroys a specified FIFO queue */
int fifo_destroy(struct fifo_q *q);

/*
 * Adds a new item to the FIFO queue. Called by providers. Returns zero on
 * success or an error code if the queue is full.
 */
int fifo_add(struct fifo_q *q, void *p);

/*
 * Tries to get an item from the FIFO queue. Called by consumers. Returns
 * NULL if the queue is empty.
 */
void *fifo_try_get(struct fifo_q *q);


#endif /* __FIFO_QUEUE_H */
