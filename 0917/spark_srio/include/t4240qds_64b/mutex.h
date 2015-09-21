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


#ifndef MUTEX_H_
#define MUTEX_H_

/* Mutex stuff */
#define mutex_t		pthread_mutex_t
#define mutex_init(x) \
	do { \
		__maybe_unused int __foo;	\
		pthread_mutexattr_t __foo_attr;	\
		__foo = pthread_mutexattr_init(&__foo_attr);	\
		BUG_ON(__foo);	\
		__foo = pthread_mutexattr_settype(&__foo_attr,	\
						  PTHREAD_MUTEX_ADAPTIVE_NP); \
		BUG_ON(__foo);	\
		__foo = pthread_mutex_init(x, &__foo_attr); \
		BUG_ON(__foo); \
	} while (0)
#define mutex_lock(x) \
	do { \
		__maybe_unused int __foo = pthread_mutex_lock(x); \
		BUG_ON(__foo); \
	} while (0)
#define mutex_unlock(x) \
	do { \
		__maybe_unused int __foo = pthread_mutex_unlock(x); \
		BUG_ON(__foo); \
	} while (0)

#endif /* MUTEX_H_ */
