/* Copyright 2012 Freescale Semiconductor, Inc.
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

/* System headers */
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <readline.h>

/* USDPAA APIs */
#include <usdpaa/dma_mem.h>

static const char prompt[] = "raw_test> ";

static const char STR_create[] = "--create";
static const char STR_no_create[] = "--no-create";
static const char STR_lazy[] = "--lazy";
static const char STR_no_lazy[] = "--no-lazy";
static const char STR_share[] = "--share=";
static const char STR_size[] = "--size=";
static const char STR_align[] = "--align=";

static int bad_parse;

static unsigned long get_ularg(const char *s, const char *pref)
{
	char *endptr;
	unsigned long ularg = strtoul(s, &endptr, 0);
	bad_parse = 0;
	if ((endptr == s) || (*endptr != '\0')) {
		fprintf(stderr, "Invalid %s%s\n", pref, s);
		bad_parse = 1;
	} else if (ularg == ULONG_MAX) {
		fprintf(stderr, "Out of range %s%s\n", pref, s);
		bad_parse = 1;
	}
	return ularg;
}

#define NEXT_ARG() (argv++, --argc)

/*
 * Usage:
 *     rma_test [options]
 * Options:
 *     --share=<name>  names the sharable map, otherwise use private map
 *     --create        for --share, create the sharable/named map (default)
 *     --no-create     for --share, the sharable/named map must already exist
 *     --lazy          for --create, this will allows maps to already exist
 *     --no-lazy       for --create, maps must not already exist (default)
 *     --size=<num>    required if not mapping an existing sharable/named map
 */
int main(int argc, char *argv[])
{
	int do_create = 1, do_lazy = 0;
	const char *share_name = NULL;
	size_t len = 0;
	uint32_t flags = 0;
	void *raw_ptr;
	while (NEXT_ARG()) {
		if (!strcmp(*argv, STR_create))
			do_create = 1;
		else if (!strcmp(*argv, STR_no_create))
			do_create = 0;
		else if (!strcmp(*argv, STR_lazy))
			do_lazy = 1;
		else if (!strcmp(*argv, STR_no_lazy))
			do_lazy = 0;
		else if (!strncmp(*argv, STR_share, strlen(STR_share)))
			share_name = &(*argv)[strlen(STR_share)];
		else if (!strncmp(*argv, STR_size, strlen(STR_size)))
			len = get_ularg(&(*argv)[strlen(STR_size)],
					STR_size);
		else {
			fprintf(stderr, "Unrecognised argument '%s'\n", *argv);
			exit(EXIT_FAILURE);
		}
		if (bad_parse)
			exit(EXIT_FAILURE);
	}
	if (share_name) {
		flags |= DMA_MAP_FLAG_SHARED;
		if (do_create) {
			flags |= DMA_MAP_FLAG_NEW;
			if (do_lazy)
				flags |= DMA_MAP_FLAG_LAZY;
		}
	}
	dma_mem_generic = dma_mem_create(flags, share_name, len);
	if (!dma_mem_generic) {
		fprintf(stderr, "Mapping %s:0x%x:0x%zx failed\n",
			share_name ? share_name : "<private>", flags, len);
		exit(EXIT_FAILURE);
	}
	dma_mem_params(dma_mem_generic, &flags, &share_name, &len);
	raw_ptr = dma_mem_raw(dma_mem_generic, &len);
	printf("Mapped \"%s\":0x%x:0x%zx to %p\n", share_name, flags, len,
	       raw_ptr);
	/* Go in to CLI loop */
	while (1) {
		char *cli = readline(prompt);
		if (!cli)
			exit(EXIT_SUCCESS);
		/* Pre-process the command */
		if (cli[0] == '\0') {
			free(cli);
			continue;
		}
		argv = history_tokenize(cli);
		if (!argv) {
			fprintf(stderr, "Out of memory while parsing: %s\n",
				cli);
			free(cli);
			continue;
		}
		for (argc = 0; argv[argc]; argc++)
			;
		/* Process the command */
		if (argv[0][0] == 'q')
			exit(EXIT_SUCCESS);
		else
			fprintf(stderr, "Error: unrecognised command '%s'\n",
				*argv);
		/* Post-process the command */
		for (argc = 0; argv[argc]; argc++)
			free(argv[argc]);
		free(argv);
		free(cli);
	}
}
