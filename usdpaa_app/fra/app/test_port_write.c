/* Copyright 2014 Freescale Semiconductor, Inc.
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
#include <fra_fq_interface.h>
#include <fra.h>
#include <test_port_write.h>

#define test_pw_dist_tx_name	"pw_to_peer"
#define test_pw_dist_rx_name	"pw_from_peer"

static struct distribution *test_pw_dist_tx;
static struct distribution *test_pw_dist_rx;

struct test_pw {
	char data[64];
	int len;
};

static struct test_pw test_pw;

void test_pw_send_data(void)
{
	struct msg_buf *msg;
	__maybe_unused int i;

	if (test_pw.len < 4 || test_pw.len > 64) {
		error(0, 0,
			"data payload of port-write should be "
			"from 4 to 64 bytes");
		return;
	}
	msg = msg_alloc(RIO_TYPE_PW);
	if (!msg) {
		error(0, 0, "msg alloc fails");
		return;
	}

	msg->len = test_pw.len;
	memcpy(msg->data, test_pw.data, msg->len);

#ifdef ENABLE_FRA_DEBUG
	printf("Port Write: send data:\n");
	for (i = 0; i < test_pw.len; i++)
		printf("%c", test_pw.data[i]);
	printf("\n");
#endif

	rman_send_msg(test_pw_dist_tx->rman_tx, 0, msg);
}

enum handler_status
test_pw_rx_handler(struct distribution *dist, struct hash_opt *opt,
		   const struct qm_fd *fd)
{
	struct msg_buf *msg;
	int i;

	msg = fd_to_msg((struct qm_fd *)fd);
	if (!msg)
		fra_drop_frame(fd);

	printf("Port Write: get %d bytes data:", msg->len);
	for (i = 0; i < msg->len; i++)
		printf("%c", ((char *) msg->data)[i]);
	printf("\n");

	return HANDLER_CONTINUE;
}

static int fra_cli_test_pw(int argc, char *argv[])
{
	struct distribution *dist = NULL;
	struct dist_order  *dist_order;

	if (argc != 2) {
		fprintf(stderr, "pw correct format:\n \tpw [data]");
		return -EINVAL;
	}

	if (!fra)
		return -EINVAL;

	memset(&test_pw, 0, sizeof(test_pw));
	test_pw.len = strlen(argv[1]);
	if (test_pw.len > 64)
		test_pw.len = 64;
	if (test_pw.len < 4)
		test_pw.len = 4;

	strncpy(test_pw.data, argv[1], test_pw.len);

	list_for_each_entry(dist_order, &fra->dist_order_list, node) {
		dist = dist_order->dist;

		if (test_pw_dist_tx && test_pw_dist_rx)
			break;

		while (dist) {
			if (!strcmp(test_pw_dist_tx_name, dist->cfg->name))
				test_pw_dist_tx = dist;
			if (!strcmp(test_pw_dist_rx_name, dist->cfg->name))
				test_pw_dist_rx = dist;
			dist = dist->next;
		}
	}

	if (!test_pw_dist_tx) {
		error(0, 0, "can not find %s distribution",
			test_pw_dist_tx_name);
		return -EINVAL;
	}
	if (!test_pw_dist_rx) {
		error(0, 0, "can not find %s distribution",
			test_pw_dist_rx_name);
		return -EINVAL;
	}

	test_pw_dist_rx->handler = test_pw_rx_handler;

	test_pw_to_send();

	return 0;
}

cli_cmd(pw, fra_cli_test_pw);
