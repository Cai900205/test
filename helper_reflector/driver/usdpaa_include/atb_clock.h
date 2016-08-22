/* Copyright (c) 2011 Freescale Semiconductor, Inc.
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

#include <stdio.h>
#include <errno.h>

#define ATB_MHZ	1000000

struct atb_clock {
	uint64_t	start;
	uint64_t	total;
	uint64_t	max;
	uint64_t	min;
	uint64_t	count;
};

/* This function returns cpu frequency in HZ,
 * or a negative error code in the case of failure
 */
static inline int64_t atb_get_multiplier(void)
{
	FILE *file;
	char cpuinfo[8192];
	uint32_t bytes_read;
	char *match_char;
	float cpu_clock;

	file = fopen("/proc/cpuinfo", "r");
	if (file == NULL)
		return -errno;
	bytes_read = fread(cpuinfo, 1, sizeof(cpuinfo), file);
	fclose(file);

	if (bytes_read == 0 || bytes_read == sizeof(cpuinfo))
		return -ENOENT;

	match_char = strstr(cpuinfo, "clock");

	if (match_char)
		sscanf(match_char, "clock : %f", &cpu_clock);

	return cpu_clock * ATB_MHZ;
}

/* This function initializes the atb clock structure */
static inline void atb_clock_init(struct atb_clock *atb_clock)
{
	atb_clock->start =
	atb_clock->total =
	atb_clock->max =
	atb_clock->count = 0;

	atb_clock->min = -1;
}

/* This function should be called after atb clock work done
 *
 * Reserving this interface for future usage
 */
static inline void atb_clock_finish(struct atb_clock *atb_clock)
{
}

/* This function starts a timing sample within the clock object
 *
 * The timing sample will be completed once atb_clock_stop() or
 * atb_clock_capture() is called
 */
static inline void atb_clock_start(struct atb_clock *atb_clock)
{
	atb_clock->start = mfatb();
}

/* This function resets the total and count that are used in determining
 * average time
 */
static inline void atb_clock_reset(struct atb_clock *atb_clock)
{
	atb_clock->total = atb_clock->count = 0;
}

/* This function stops a timing sample as begun by atb_clock_start()
 *
 * Other state is updated accordingly too, such as count, max and min
 */
static inline void atb_clock_stop(struct atb_clock *atb_clock)
{
	uint64_t interv = mfatb() - atb_clock->start;
	atb_clock->total += interv;

	if (interv > atb_clock->max)
		atb_clock->max = interv;
	else if (interv < atb_clock->min)
		atb_clock->min = interv;

	atb_clock->count++;
}

/* This function captures a timing sample for a clock
 *
 * Unlike atb_clock_stop(), the "stop" point of this timing sample
 * implicitly becomes the "start" point for the next timing sample
 * As such, atb_clock_capture() can be called repeatedly in order
 * to take timing samples with no gaps
 */
static inline void atb_clock_capture(struct atb_clock *atb_clock)
{
	uint64_t snapshot = mfatb();
	uint64_t interv = snapshot - atb_clock->start;
	atb_clock->total += interv;
	atb_clock->start = snapshot;

	if (interv > atb_clock->max)
		atb_clock->max = interv;
	else if (interv < atb_clock->min)
		atb_clock->min = interv;

	atb_clock->count++;
}

/* This function returns atb total clock ticks */
static inline uint64_t atb_clock_total(const struct atb_clock *atb_clock)
{
	return atb_clock->total;
}

/* This function returns the max atb clock ticks */
static inline uint64_t atb_clock_max(const struct atb_clock *atb_clock)
{
	return atb_clock->max;
}

/* This function returns the min atb clock ticks */
static inline uint64_t atb_clock_min(const struct atb_clock *atb_clock)
{
	return atb_clock->min;
}

/* This function returns total measurement counts of atb clock */
static inline uint64_t atb_clock_count(const struct atb_clock *atb_clock)
{
	return atb_clock->count;
}

/* This function returns the average of all the timing samples, rounded
 * up or down to the nearest integer (If rounding is not desired, cast
 * atb_clock_total() to a floating-point type and divide it by
 * atb_clock_count() to obtain a more exact average)
 */
static inline uint64_t atb_clock_average(const struct atb_clock *atb_clock)
{
	return (atb_clock->total + (atb_clock->count >> 1)) / atb_clock->count;
}

/* This function converts the atb clock ticks to seconds */
static inline double atb_to_seconds(uint64_t atb, uint64_t multiplier)
{
	return (double)atb / multiplier;
}
