/* Copyright (c) 2010 - 2011 Freescale Semiconductor, Inc.
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

#ifndef BIGATOMIC_H
#define BIGATOMIC_H

#include <usdpaa/compat.h>

/* 64-bit atomics */
struct bigatomic {
	atomic_t upper;
	atomic_t lower;
};

static inline void bigatomic_set(struct bigatomic *b, u64 i)
{
	atomic_set(&b->upper, i >> 32);
	atomic_set(&b->lower, i & 0xffffffff);
}
static inline u64 bigatomic_read(const struct bigatomic *b)
{
	u32 upper, lower;
	do {
		upper = atomic_read(&b->upper);
		lower = atomic_read(&b->lower);
	} while (upper != (u32)atomic_read(&b->upper));
	return ((u64)upper << 32) | (u64)lower;
}
static inline void bigatomic_inc(struct bigatomic *b)
{
	if (atomic_inc_and_test(&b->lower))
		atomic_inc(&b->upper);
}

#endif	/* BIGATOMIC_H */
