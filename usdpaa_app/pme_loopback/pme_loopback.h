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

#ifndef _PME_LOOPBACK_H
#define _PME_LOOPBACK_H

#warning "Fix below (should be #include <usdpaa/compat.h>"
#include <internal/compat.h>
#include <argp.h>

#define init_completion(i) { *i = 0; }

#ifdef PME_LOOPBACK_TRACE
#define TRACE           printf
#else
#define TRACE(x...)     do { ; } while (0)
#endif

struct pme_loopback_arguments {
	int first, last;
};

typedef int (*cli_handle_t)(int argc, char *argv[]);
struct cli_table_entry {
	const char *cmd;
	const cli_handle_t handle;
};
#define cli_cmd(cmd, handle)                                    \
	const struct cli_table_entry cli_table_entry_##cmd      \
	__attribute__((used, section(".data.cli_table")))       \
	= {__stringify(cmd), handle}

extern const struct cli_table_entry cli_table_start[], cli_table_end[];

#define foreach_cli_table_entry(cli_cmd)        \
	for (cli_cmd = cli_table_start; cli_cmd < cli_table_end; cli_cmd++)

#endif
