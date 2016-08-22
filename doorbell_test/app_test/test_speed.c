/* Copyright (c) 2011-2012 Freescale Semiconductor, Inc.
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

#include <fra_common.h>
#include <sys/time.h>
#include <fra_cfg_parser.h>
#include <rman_interface.h>
#include <fra_fq_interface.h>
#include <test_speed.h>
#include <fra.h>
#include <usdpaa/fsl_dma.h>
#include <usdpaa/fsl_srio.h>

//#define DEBUG_SWRITE

#define test_speed_dist_tx_name		"dstr_to_peer"
#define test_speed_dist_rx_name		"dstr_from_peer"
#define test_speed_dist_cmd_tx_name	"dbell_to_peer"
#define test_speed_dist_cmd_rx_name	"dbell_from_peer"

//static struct distribution *test_speed_dist_tx;
//static struct distribution *test_speed_dist_rx;
static struct distribution *test_speed_dist_cmd_tx;
static struct distribution *test_speed_dist_cmd_rx;

#ifdef ENABLE_FRA_DEBUG
#define wait_cmd_handler_time 2000000
#define wait_msg_handler_time 200000
#else
#define wait_cmd_handler_time 200
#define wait_msg_handler_time 5
#endif

#define test_speed_cmd_offset 13
#define test_speed_cmd_mask 0x1fff

enum test_speed_mode {
	RECEIVE,
	SEND
};

enum test_speed_cmd {
	START,
	RESTART,
	END,
	PACKETS_SIZE,
	PACKETS_NUM
};

#define MHZ	1000000
//static float	cpu_clock;

struct test_speed test_speed;
uint64_t send_time[TEST_MAX_PACKTETS_NUM];
uint64_t receive_time[TEST_MAX_PACKTETS_NUM];

/*
static int get_clockinfo(void)
{
	FILE *file;
	char cpuinfo[8192];
	uint32_t bytes_read;
	char *match_char;

	file = fopen("/proc/cpuinfo", "r");
	if (file == NULL)
		return -ENOENT;
	bytes_read = fread(cpuinfo, 1, sizeof(cpuinfo), file);
	fclose(file);

	if (bytes_read == 0 || bytes_read == sizeof(cpuinfo))
		return -ENOENT;

	match_char = strstr(cpuinfo, "clock");

	if (match_char)
		sscanf(match_char, "clock : %f", &cpu_clock);

	return 0;
}
*/

static void test_speed_send_cmd(uint16_t data)
{
	struct msg_buf *msg;
    int ret=0;
    printf("**********send cmd *********\n");
    msg = msg_alloc(RIO_TYPE_DBELL);
    printf("tx msg addr:%lx\n",(unsigned long)msg);
    if (!msg) {
		error(0, 0, "msg alloc fails");
		return;
	}
	dbell_set_data(msg, data);
	ret=rman_send_msg(test_speed_dist_cmd_tx->rman_tx, 0, msg);
    printf("testspeed: send command:0x%x ret:%d\n", data,ret);
}

enum handler_status
test_speed_cmd_rx_handler(struct distribution *dist, struct hash_opt *opt,
			  const struct qm_fd *fd)
{
	struct msg_buf *msg;
	uint16_t data;
	msg = fd_to_msg((struct qm_fd *)fd);
	if (!msg) {
		fra_drop_frame(fd);
        return HANDLER_ERROR;
    }

    printf("rx msg addr:%lx\n",(unsigned long)msg);
	data = dbell_get_data(msg);
	printf("dbell_rx_handler get data 0x%x\n", data);
	fra_drop_frame(fd);
	
    return HANDLER_CONTINUE;
}


void test_speed_send_msg(void)
{
	int i;
    uint16_t data=test_speed.packets_size;
    printf("test %x times:%d\n",test_speed.packets_size,test_speed.packets_num);
    for(i=0;i<test_speed.packets_num;i++) {
        usleep(200); /* wait test speed cmd handler */
	    test_speed_send_cmd(data);
        data++;
    }
}

static int fra_cli_dbell_test(int argc, char *argv[])
{
	struct distribution *dist = NULL;
	struct dist_order  *dist_order;

	if (argc != 2 && argc != 4) {
		fprintf(stderr, "dbell_test correct format:\n"
			"\tdbell_test [send/receive] [packet length] "
			"[packet number]");
		return -EINVAL;
	}
	memset(&test_speed, 0, sizeof(struct test_speed));
	if (argc == 2 && !strcmp(argv[1], "receive"))
		test_speed.mode = RECEIVE;
	else if (argc == 4) {
		test_speed.packets_size = strtoul(argv[2], NULL, 0);
		test_speed.packets_num = strtoul(argv[3], NULL, 0);
		test_speed.mode = SEND;
	}
    test_speed.total_loop=1;
    if (!fra)
		return -EINVAL;

	list_for_each_entry(dist_order, &fra->dist_order_list, node) {
		dist = dist_order->dist;
		if (test_speed_dist_cmd_tx && test_speed_dist_cmd_rx)
			break;
		while (dist) {
			if (!strcmp(test_speed_dist_cmd_tx_name,
				    dist->cfg->name))
				test_speed_dist_cmd_tx = dist;
			if (!strcmp(test_speed_dist_cmd_rx_name,
				    dist->cfg->name))
				test_speed_dist_cmd_rx = dist;
			dist = dist->next;
		}
	}
	
    if (!test_speed_dist_cmd_tx) {
		error(0, 0, "can not find %s distribution",
			test_speed_dist_cmd_tx_name);
		return -EINVAL;
	}
	if (!test_speed_dist_cmd_rx) {
		error(0, 0, "can not find %s distribution",
			test_speed_dist_cmd_rx_name);
		return -EINVAL;
	}
    struct srio_dev *sriodev=rman_if_get_sriodev();
    int err;
    err = fsl_srio_set_targetid(sriodev,0,1,0x11);
    if(err!=0) {
            printf("sro set targetid  failed!\n");
            fflush(stdout);
     }
    err = fsl_srio_set_deviceid(sriodev,0,0x11);
    if(err!=0) {
            printf("sro set deviceid  failed!\n");
            fflush(stdout);
     }
    test_speed_dist_cmd_rx->handler = test_speed_cmd_rx_handler;
    test_speed_to_send();
    return 0;
}

cli_cmd(dbell_test, fra_cli_dbell_test);
