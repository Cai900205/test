/* Copyright (c) 2014 Freescale Semiconductor, Inc.
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
#include "ppam_if.h"
#include <ppac_interface.h>
#include <usdpaa/fsl_bman.h>
#include <usdpaa/fsl_qman.h>
#include "internal/compat.h"

#include <error.h>
#include <unistd.h>
#include "fmc.h"
#include "app_config.h"
#include "usdpaa/fsl_dpa_ipsec.h"
#include "usdpaa/fsl_dpa_stats.h"
#include "app_common.h"
#include "xfrm_km.h"
#include "ipsec_sizing.h"

/* Nax number of counters */
#define NUM_CNT 16

/* statistic counter storage area size*/
#define CNT_STORAGE_SIZE 1000

/* storage area offset for statistic counters */
#define STORAGE_OFFSET 0

#define STORAGE_OFFSET_ETH	0
#define STORAGE_OFFSET_IB_REASS	100

static int dpa_stats_id;
void *storage;

static int ib_reass_stats_cnt;

int create_dpa_stats_counters(int num_cnt)
{
	struct dpa_stats_params stats_params;
	int err;

	err = dpa_stats_lib_init();
	if (err < 0) {
		fprintf(stderr, "Failed to initialize the"
			" stats user space library (%d)\n", err);
		return err;
	}

	TRACE("Stats library initialized\n");
	stats_params.max_counters = num_cnt;
	stats_params.storage_area_len = CNT_STORAGE_SIZE;
	stats_params.storage_area = malloc(stats_params.storage_area_len);
	if (!stats_params.storage_area) {
		fprintf(stderr, "Failed to allocate storage area for "
			"statistics\n");
		return -ENOMEM;
	}

	err = dpa_stats_init(&stats_params, &dpa_stats_id);
	if (err < 0) {
		fprintf(stderr, "Failed to initialize  "
				"stats instance (%d)\n", err);
		return err;
	}

	TRACE("Created stats instance (%d)\n", dpa_stats_id);

	/* Save storage area pointer */
	storage = stats_params.storage_area;
	return 0;
}


int create_eth_stats_counter(struct fman_if *__if)
{
	struct dpa_stats_cnt_params cnt_params;
	struct ppac_interface *ppac_if;
	int err;

	cnt_params.type = DPA_STATS_CNT_ETH;
	if (__if->mac_type == fman_mac_10g) {
		TRACE("create stats counter for 10G interface %d\n",
		      __if->mac_idx);
		cnt_params.eth_params.src.eth_id = DPA_STATS_ETH_10G_PORT0 +
						__if->mac_idx;
	} else if (__if->mac_type == fman_mac_1g) {
		TRACE("create stats counter for 1G interface %d\n",
		     __if->mac_idx);
		cnt_params.eth_params.src.eth_id = __if->mac_idx;
	}

	cnt_params.eth_params.src.engine_id = app_conf.fm;
	cnt_params.eth_params.cnt_sel = (DPA_STATS_CNT_ETH_ALL - 1) &
		(~(DPA_STATS_CNT_ETH_IN_UNICAST_PKTS |
		DPA_STATS_CNT_ETH_OUT_UNICAST_PKTS));

	ppac_if = get_ppac_if(__if);

	err = dpa_stats_create_counter(dpa_stats_id,
			&cnt_params, &ppac_if->ppam_data.stats_cnt);
	if (err < 0)
		fprintf(stderr, "Failed to create stats counter"
				" - all statistics for port %d,%d (%d)\n",
				__if->mac_idx, __if->mac_type, err);
	return err;
}

int create_ib_reass_stats_counter(void)
{
	struct dpa_stats_cnt_params cnt_params;
	int err;

	cnt_params.type = DPA_STATS_CNT_REASS;
	cnt_params.reass_params.reass = ib_reass;
	cnt_params.reass_params.cnt_sel = DPA_STATS_CNT_REASS_IPv4_ALL;
	err = dpa_stats_create_counter(dpa_stats_id,
		&cnt_params, &ib_reass_stats_cnt);
	if (err < 0) {
		fprintf(stderr, "Failed to create stats "
				 "counter - IPv4 statistics "
				"for inbound reassembly(%d)\n", err);
	}
	return err;
}

int stats_init(void)
{
	int ret;
	ret = create_dpa_stats_counters(NUM_CNT);
	if (ret < 0) {
		fprintf(stderr, "Could not create stats "
			"counters (%d)\n", ret);
		goto out;
	}

	ret = create_eth_stats_counter(app_conf.ob_eth);
	if (ret < 0)
		goto out;

	ret = create_ib_reass_stats_counter();
out:
	return ret;
}

void stats_cleanup(void)
{
	free(storage);
	dpa_stats_free(dpa_stats_id);
	dpa_stats_lib_exit();
}

int show_sa_stats(int argc, char *argv[])
{
	struct dpa_ipsec_sa_stats sa_stats;
	int err, sa_id = 0;

	memset(&sa_stats, 0, sizeof(sa_stats));
	if (argc != 2)
		return -EINVAL;

	if (!isdigit(*argv[1])) {
		printf("\n.SA id must be of type int.\n");
		return -EINVAL;
	}

	sa_id = atoi(argv[1]);
	err = dpa_ipsec_sa_get_stats(sa_id, &sa_stats);
	if (err == 0) {
		printf("\nSA(%d) statistics:\n", sa_id);
		printf("    - Encrypted/Decrypted OK : %u packets (i.e. %u "
			"bytes)\n", sa_stats.packets_count,
			sa_stats.bytes_count);
		printf("    - Total input            : %u packets\n\n",
			sa_stats.input_packets);
	} else
		error (0, err, "Failed to acquire SA=%d statistics", sa_id);

	return 0;
}

int show_ipsec_stats(int argc, char *argv[])
{
	struct dpa_ipsec_stats ipsec_stats;
	int err, instance_id;

        if (argc != 2)
                return -EINVAL;

        if (!isdigit(*argv[1])) {
                printf("\n.Instance ID must be of type int.\n");
                return -EINVAL;
        }

        instance_id = atoi(argv[1]);

	err = dpa_ipsec_get_stats(instance_id, &ipsec_stats);
	if (err == 0) {
		printf("\nIPSec global statistics:\n");
		printf("    - SA Lookup Miss on INBOUND : %u packets (i.e. %u "
			"bytes)\n", ipsec_stats.inbound_miss_pkts,
			ipsec_stats.inbound_miss_bytes);
		printf("    - Policy Miss on OUTBOUND   : %u packets (i.e. %u "
			"bytes)\n\n", ipsec_stats.outbound_miss_pkts,
			ipsec_stats.outbound_miss_bytes);
	} else
		error(0, err, "Failed to acquire IPSec global statistics");

	return 0;
}

static void print_dpa_stats_cnts(unsigned int storage_area_offset,
				 unsigned int cnts_written,
				 int bytes_written)
{
	uint32_t *stg = (uint32_t *)(storage + storage_area_offset);
	uint32_t i = 0;
	switch (storage_area_offset) {
	case STORAGE_OFFSET_ETH:
	printf("RX_DROP_PKTS %d\n", *(stg + i++));
	printf("RX_BYTES %d\n", *(stg + i++));
	printf("RX_PKTS %d\n", *(stg + i++));
	printf("BC_PKTS %d\n", *(stg + i++));
	printf("MC_PKTS %d\n", *(stg + i++));
	printf("CRC_ALIGN_ERR %d\n", *(stg + i++));
	printf("UNDERSIZE_PKTS %d\n", *(stg + i++));
	printf("OVERSIZE_PKTS %d\n", *(stg + i++));
	printf("FRAGMENTS %d\n", *(stg + i++));
	printf("JABBERS %d\n", *(stg + i++));
	printf("64BYTE_PKTS %d\n", *(stg + i++));
	printf("65_127BYTE_PKTS %d\n", *(stg + i++));
	printf("128_255BYTE_PKTS %d\n", *(stg + i++));
	printf("256_511BYTE_PKTS %d\n", *(stg + i++));
	printf("512_1023BYTE_PKTS %d\n", *(stg + i++));
	printf("1024_1518BYTE_PKTS %d\n", *(stg + i++));
	printf("TX_PKTS %d\n", *(stg + i++));
	printf("TX_DROP_PKTS %d\n\n", *(stg + i++));
	break;
	case STORAGE_OFFSET_IB_REASS:
	printf("IPv4_FRAMES %d\n", *(stg + i++));
	printf("IPv4_FRAGS_VALID %d\n", *(stg + i++));
	printf("IPv4_FRAGS_TOTAL %d\n", *(stg + i++));
	printf("IPv4_FRAGS_MALFORMED %d\n",
	       *(stg + i++));
	printf("IPv4_FRAGS_DISCARDED %d\n",
	       *(stg + i++));
	printf("IPv4_AUTOLEARN_BUSY %d\n",
	       *(stg + i++));
	printf("IPv4_EXCEED_16FRAGS %d\n\n",
	       *(stg + i++));
	break;
	}
}
void request_done_cb(int _dpa_stats_id,
		unsigned int storage_area_offset, unsigned int cnts_written,
		int bytes_written)
{
	print_dpa_stats_cnts(storage_area_offset, cnts_written, bytes_written);
}

int show_eth_stats(int argc, char *argv[])
{
	struct dpa_stats_cnt_request_params req_params;
	struct ppac_interface *ppac_if;
	struct fman_if *__if;
	int port_idx, port_type, cnts_len, err;

	if (argc != 3)
		return -EINVAL;

	if (!isdigit(*argv[1])) {
		printf("\n.Port index must be of type int.\n");
		return -EINVAL;
	}
	port_idx = atoi(argv[1]);

	if (!isdigit(*argv[2])) {
		printf("\n.Port type must be of type int.\n");
		return -EINVAL;
	}
	port_type = atoi(argv[2]);

	__if = get_fif(app_conf.fm, port_idx, port_type);

	if (!__if) {
		printf("\nInvalid port id or type.\n");
		return -EINVAL;
	}

	ppac_if = get_ppac_if(__if);
	if (!ppac_if) {
		printf("\n.ppac interface could not be found.\n");
		return -EINVAL;
	}

	req_params.cnts_ids = &ppac_if->ppam_data.stats_cnt;
	req_params.cnts_ids_len = 1;
	req_params.reset_cnts = false;
	req_params.storage_area_offset = STORAGE_OFFSET_ETH;
	err = dpa_stats_get_counters(req_params,
		&cnts_len, &request_done_cb);

	if (err < 0) {
		fprintf(stderr, "Failed to create stats request"
			"for eth port idx %d (%d)\n", port_idx, err);
	}
	return err;
}

int show_ib_reass_stats(int argc, char *argv[])
{
	struct dpa_stats_cnt_request_params req_params;
	int cnts_len, err;

	if (argc != 1)
		return -EINVAL;

	req_params.cnts_ids = &ib_reass_stats_cnt;
	req_params.cnts_ids_len = 1;
	req_params.reset_cnts = false;
	req_params.storage_area_offset = STORAGE_OFFSET_IB_REASS;
	err = dpa_stats_get_counters(req_params,
		&cnts_len, &request_done_cb);

	if (err < 0) {
		fprintf(stderr, "Failed to create stats request"
			"for inbound reassembly (%d)\n", err);
	}
	return 0;
}
