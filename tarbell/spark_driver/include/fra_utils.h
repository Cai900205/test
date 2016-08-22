/*
 * Copyright 2012 Freescale Semiconductor, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor "AS IS" AND ANY
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

#ifndef _FRA_UTILS_H
#define _FRA_UTILS_H

#include <fra_cfg.h>
#include <usdpaa/compat.h>
#include <error.h>

#ifdef ENABLE_FRA_DEBUG
extern int debug_off;
#define FRA_DBG(fmt, args...) \
	do { \
		if (!debug_off) \
			fprintf(stderr, fmt"\n", ##args); \
	} while (0)
#else
#define FRA_DBG(fmt, args...)
#endif

/* Fra can implement new CLI functions using the cli_cmd() macro. */
typedef int (*cli_handle_t)(int argc, char *argv[]);
struct cli_table_entry {
	const char *cmd;
	const cli_handle_t handle;
};
#define cli_cmd(cmd, handle)					\
	const struct cli_table_entry cli_table_entry_##cmd	\
	__attribute__((used, section(".rodata.cli_table")))	\
	= {__stringify(cmd), handle}

#endif /* _FRA_UTILS_H */
