/* Copyright (c) 2010-2011 Freescale Semiconductor, Inc.
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

#include <ppac.h>
#include <usdpaa/compat.h>
#include <usdpaa/usdpaa_netcfg.h>

#include <stdio.h>

static void usage(void)
{
	fprintf(stderr, "usage: test_netcfg <fmc_pcd_file> "
					"<fmc_cfgdata_file>\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	int rcode;
	struct usdpaa_netcfg_info *uscfg_info;

	printf("---------------START------------------\n");

	if (argc != 3)
		usage();

	rcode = of_init();
	if (rcode) {
		pr_err("of_init() failed\n");
		exit(EXIT_FAILURE);
	}
/*	PCD file = /usr/etc/usdpaa_policy_hash_ipv4.xml
 *	CFGDATA file = /usr/etc/usdpaa_config_p4_serdes_0xe.xml
 */

	uscfg_info = usdpaa_netcfg_acquire(argv[1], argv[2]);
	if (uscfg_info == NULL) {
		fprintf(stderr, "error: NO Config information available\n");
		return -ENXIO;
	}

	dump_usdpaa_netcfg(uscfg_info);

	usdpaa_netcfg_release(uscfg_info);
	of_finish();
	printf("---------------END------------------\n");
	return EXIT_SUCCESS;
}
