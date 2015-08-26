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

#define test_speed_dist_tx_name		"dstr_to_peer"
#define test_speed_dist_rx_name		"dstr_from_peer"
#define test_speed_dist_cmd_tx_name	"dbell_to_peer"
#define test_speed_dist_cmd_rx_name	"dbell_from_peer"

static struct distribution *test_speed_dist_tx;
static struct distribution *test_speed_dist_rx;
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
static float	cpu_clock;

struct test_speed test_speed;
uint64_t send_time[TEST_MAX_PACKTETS_NUM];
uint64_t receive_time[TEST_MAX_PACKTETS_NUM];

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

void test_speed_info(void)
{
	if (!test_speed.loop) {
		fprintf(stderr, "Testspeed: successful loop times is 0\n");
		return;
	}

	get_clockinfo();

	if (test_speed.mode == RECEIVE)
		fprintf(stderr, "Testspeed information:\n"
			"\tone loop:%d packets with length of %d bytes\n"
			"\tsuccessful loop number:%d\n"
			"\teach loop(rx) take time:%fus\n"
			"\teach packet(rx) take time:%fus\n"
			"\tsrio speed:%fGbps\n",
			test_speed.packets_num, test_speed.packets_size,
			test_speed.loop,
			test_speed.total_interval / test_speed.loop / cpu_clock,
			test_speed.total_interval / test_speed.loop /
				test_speed.packets_num / cpu_clock,
			test_speed.packets_size * 8 /
				(test_speed.total_interval / test_speed.loop /
				test_speed.packets_num / cpu_clock) /
				(1024 * 1024 * 1024 / MHZ));
	else
		fprintf(stderr, "Testspeed information:\n"
			"\tone loop:%d packets with length of %d bytes\n"
			"\ttest loop number:%d successful loop number:%d\n"
			"\teach loop(tx&rx) take time:%fus\n"
			"\teach packet(tx&rx) take time:%fus\n"
			"\tsrio speed:%fGbps\n",
			test_speed.packets_num, test_speed.packets_size,
			test_speed.total_loop, test_speed.loop,
			test_speed.total_interval / test_speed.loop / cpu_clock,
			test_speed.total_interval / test_speed.loop /
				test_speed.packets_num / cpu_clock,
			test_speed.packets_size * 8 * 2 /
				(test_speed.total_interval / test_speed.loop /
				test_speed.packets_num / cpu_clock) /
				(1024 * 1024 * 1024 / MHZ));
	return;
}

static void test_speed_send_cmd(uint16_t data)
{
	struct msg_buf *msg;

	msg = msg_alloc(RIO_TYPE_DBELL);
	if (!msg) {
		error(0, 0, "msg alloc fails");
		return;
	}

	dbell_set_data(msg, data);
	FRA_DBG("testspeed: send command:0x%x", data);
	printf("testspeed: send command:0x%x", data);
	rman_send_msg(test_speed_dist_cmd_tx->rman_tx, 0, msg);
}

enum handler_status
test_speed_cmd_rx_handler(struct distribution *dist, struct hash_opt *opt,
			  const struct qm_fd *fd)
{
	struct msg_buf *msg;
	uint16_t data;
	int i, error_flag;
	uint64_t max, min;
	msg = fd_to_msg((struct qm_fd *)fd);
	if (!msg)
		fra_drop_frame(fd);

	data = dbell_get_data(msg);
	FRA_DBG("test_speed_cmd_rx_handler get data 0x%x cmd %d", data,
		data >> test_speed_cmd_offset);
	printf("test_speed_cmd_rx_handler get data 0x%x cmd %d", data,
		data >> test_speed_cmd_offset);
        fflush(stdout);

/*	switch (data >> test_speed_cmd_offset) {
	case START:
		memset(&test_speed, 0, sizeof(struct test_speed));
		memset(receive_time, 0, sizeof(receive_time));
		break;
	case RESTART:
		max = 0;
		min = -1;
		error_flag = 0;
		for (i = 0; i < test_speed.packets_num; i++) {
			if (receive_time[i] == 0) {
				error_flag = 1;
				break;
			}
			if (receive_time[i] > max)
				max = receive_time[i];
			if (receive_time[i] < min)
				min = receive_time[i];
		}

		if (error_flag)
			break;
		test_speed.total_interval += max - min;
		test_speed.loop++;
		memset(receive_time, 0, sizeof(receive_time));
		break;
	case END:
		test_speed_info();
		break;
	case PACKETS_NUM:
		test_speed.packets_num = data & test_speed_cmd_mask;
		break;
	case PACKETS_SIZE:
		test_speed.packets_size = data & test_speed_cmd_mask;
		break;
	default:
		error(0, 0,
			"testspeed receive an unkown command 0x%x", data);
		break;
	}*/
	return HANDLER_CONTINUE;
}

enum handler_status
test_speed_rx_handler(struct distribution *dist, struct hash_opt *opt,
		      const struct qm_fd *fd)
{
/*	struct msg_buf *msg;
	int i;
	msg = fd_to_msg((struct qm_fd *)fd);
	if (!msg) {
		fra_drop_frame(fd);
		return HANDLER_DONE;
	}

	i = *(int *)msg->data;
	printf("########################\n");
	if (i < 0 || i >= test_speed.packets_num) {
		fra_drop_frame(fd);
		return HANDLER_DONE;
	}

	receive_time[i] = mfatb();
	FRA_DBG("Get %d packet start at cpu clock: %llu", i+1,
		receive_time[i]);*/
	return HANDLER_CONTINUE;
}

void test_speed_send_msg(void)
{
	int i;
	struct msg_buf *msg;

	if (test_speed.end_flag) {
		test_speed_send_cmd(RESTART << test_speed_cmd_offset);
		usleep(wait_cmd_handler_time); /* wait test speed cmd handler */
		test_speed_send_cmd(END << test_speed_cmd_offset);
		return;
	}
	if (!test_speed.loop) { /* the first loop */
	//	test_speed_send_cmd(START << test_speed_cmd_offset);
		test_speed_send_cmd(PACKETS_SIZE << test_speed_cmd_offset |
				    test_speed.packets_size);
		test_speed_send_cmd(PACKETS_NUM << test_speed_cmd_offset |
				    test_speed.packets_num);
	} else
		test_speed_send_cmd(RESTART << test_speed_cmd_offset);
		
        test_speed_send_cmd(PACKETS_SIZE << test_speed_cmd_offset |
				    test_speed.packets_size);

//	usleep(wait_cmd_handler_time); /* wait test speed cmd handler */
        sleep(1);
/*	for (i = 0; i < test_speed.packets_num; i++) {
		msg = msg_alloc(RIO_TYPE_DSTR);
		if (!msg) {
			error(0, 0, "msg_alloc failed");
			return;
		}
		msg->len = test_speed.packets_size;
		*(int *)msg->data = i;
		send_time[i] = mfatb();
		rman_send_msg(test_speed_dist_tx->rman_tx, i, msg);
	}*/
}

void test_speed_wait_receive(void)
{
	struct timeval start, end;
	uint64_t max;
	int i, error_flag;

//	usleep(wait_cmd_handler_time); /* wait test speed cmd handler */

	gettimeofday(&start, NULL);
	while (1) {
		gettimeofday(&end, NULL);
		if (1000000*(end.tv_sec - start.tv_sec) +
		    (end.tv_usec - start.tv_usec) >
		    test_speed.packets_num * wait_msg_handler_time)
			break;
	}

	max = 0;
	error_flag = 0;
/*	for (i = 0; i < test_speed.packets_num; i++) {
		if (receive_time[i] == 0 || send_time[i] == 0) {
			error_flag = 1;
			break;
		}
		if (receive_time[i] > max)
			max = receive_time[i];
	}
	if (error_flag)
		return;
*/
	test_speed.total_interval += max - send_time[0];
	test_speed.loop++;
}

static int fra_cli_test_speed(int argc, char *argv[])
{
	struct distribution *dist = NULL;
	struct dist_order  *dist_order;

	if (argc != 2 && argc != 5) {
		fprintf(stderr, "testspeed correct format:\n"
			"\ttestspeed [send/receive] [packet length] "
			"[packet number] [loop number]");
		return -EINVAL;
	}

	memset(&test_speed, 0, sizeof(struct test_speed));

	if (argc == 2 && !strcmp(argv[1], "receive"))
		test_speed.mode = RECEIVE;
	else if (argc == 5) {
		test_speed.packets_size = strtoul(argv[2], NULL, 10);
		test_speed.packets_num = strtoul(argv[3], NULL, 10);
		test_speed.total_loop = strtoul(argv[4], NULL, 10);
		if (test_speed.packets_num > TEST_MAX_PACKTETS_NUM) {
			fprintf(stderr, " MAX packets number is %d\n",
				TEST_MAX_PACKTETS_NUM);
			return -EINVAL;
		}
		if (test_speed.packets_size >
		    (msg_max_size(RIO_TYPE_DSTR) - RM_DATA_OFFSET)) {
			fprintf(stderr, " MAX packet size is %d",
				msg_max_size(RIO_TYPE_DSTR) - RM_DATA_OFFSET);
			return -EINVAL;
		}
		test_speed.mode = SEND;
	}

	if (!fra)
		return -EINVAL;

	list_for_each_entry(dist_order, &fra->dist_order_list, node) {
		dist = dist_order->dist;
		if (test_speed_dist_tx && test_speed_dist_rx &&
		    test_speed_dist_cmd_tx && test_speed_dist_cmd_rx)
			break;
		while (dist) {
			if (!strcmp(test_speed_dist_tx_name, dist->cfg->name))
				test_speed_dist_tx = dist;
			if (!strcmp(test_speed_dist_rx_name, dist->cfg->name))
				test_speed_dist_rx = dist;

			if (!strcmp(test_speed_dist_cmd_tx_name,
				    dist->cfg->name))
				test_speed_dist_cmd_tx = dist;
			if (!strcmp(test_speed_dist_cmd_rx_name,
				    dist->cfg->name))
				test_speed_dist_cmd_rx = dist;
			dist = dist->next;
		}
	}

	if (!test_speed_dist_tx) {
		error(0, 0, "can not find %s distribution",
			test_speed_dist_tx_name);
		return -EINVAL;
	}
	if (!test_speed_dist_rx) {
		error(0, 0, "can not find %s distribution",
			test_speed_dist_rx_name);
		return -EINVAL;
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
	test_speed_dist_rx->handler = test_speed_rx_handler;

	if (test_speed.mode == RECEIVE)
		test_speed_dist_cmd_rx->handler = test_speed_cmd_rx_handler;
	else
		test_speed_to_send();
	return 0;
}

cli_cmd(testspeed, fra_cli_test_speed);
