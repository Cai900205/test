/* Copyright (c) 2011 - 2012 Freescale Semiconductor, Inc.
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

#include <usdpaa/compat.h>
#include <usdpaa/of.h>
#include <usdpaa/dma_mem.h>
#include <usdpaa/fsl_dma.h>
#include <usdpaa/fsl_srio.h>
#include <error.h>
#include <atb_clock.h>
#include <readline.h>
#include <assert.h>
#include <inttypes.h>
#include <unistd.h>

#define SRIO_SYS_ADDR		0x10000000	/* used for srio system addr */
#define SRIO_WIN_SIZE		0x200000
#define SRIO_INPUT_CMD_NUM	6
#define SRIO_CMD_MIN_NUM	3
#define SRIO_TEST_DATA_NUM	21
#define SRIO_POOL_PORT_SECT_NUM	4
#define SRIO_POOL_PORT_OFFSET\
	(SRIO_WIN_SIZE * SRIO_POOL_PORT_SECT_NUM)
#define SRIO_POOL_SECT_SIZE	SRIO_WIN_SIZE
#define SRIO_POOL_SIZE	0x1000000
#define TEST_MAX_TIMES		50
#define ATTR_CMD_MAX_NUM	7
#define ATTR_CMD_MIN_NUM	5
#define OP_CMD_NUM		8
#define TEST_CMD_NUM		7
#define DMA_TEST_CHAIN_NUM 6
#define TASK_MAX_NUM	2

struct srio_pool_org {
	uint8_t write_recv_data[SRIO_POOL_SECT_SIZE]; /* space mapped to
							 other port win */
	uint8_t read_recv_data[SRIO_POOL_SECT_SIZE]; /* port read data space */
	uint8_t write_data_prep[SRIO_POOL_SECT_SIZE]; /* port DMA write data
							 prepare space */
	uint8_t res[SRIO_POOL_SECT_SIZE];
};

struct srio_pool_org_phys {
	dma_addr_t write_recv_data;
	dma_addr_t read_recv_data;
	dma_addr_t write_data_prep;
	dma_addr_t res;
};
struct srio_port_data {
	struct srio_pool_org_phys phys;
	struct srio_pool_org *virt;
	struct srio_port_info port_info;
	void *range_virt;
};

enum srio_io_op {
	SRIO_DIR_WRITE,
	SRIO_DIR_READ,
	SRIO_DIR_SET_MEM,
	SRIO_DIR_PRI_MEM,
};

struct dma_pool {
	dma_addr_t dma_phys_base;
	void *dma_virt_base;
};

enum srio_attr_level2_cmd {
	DEVICE_ID = 0,
	TARGET_ID,
	SEG_NUM,
	SUBSEG_NUM,
	SUBSEG_TDID,
	ACCEPT_ALL,
	WIN_ATTR,
	SEG_ATTR,
	IRQ,
};

enum test_cmd {
	TEST_SRIO = 0,
	TEST_DMA_CHAIN,
	TEST_SHOW_PERF,
	TEST_SHOW_TASK,
	TEST_FREE_TASK,
};

enum test_task_type {
	DMA_TEST,
	CORE_TEST,
};

static const uint32_t srio_test_win_attrv[] = {3, 4, 5, 4, 0};
static const char * const srio_test_win_attrc[] = {"SWRITE", "NWRITE",
					"NWRITE_R", "NREAD", "DMA/CORE"};
static const char * const cmd_name[] = {"-attr", "-op", "-test"};
static const char * const attr_param[4][11] = { {"port1", "port2"},
			{"device_id", "target_id", "seg_num", "subseg_num",
			"subseg_tdid", "accept_all", "win_attr", "seg_attr",
			"irq","disabled","enabled"},
			{"flush", "swrite", "nwrite", "nwrite_r", "maintw"},
			{"io_read_home", "nread", "maintr", "atomic_inc",
			"atomic_dec", "atomic_set", "atomic_clr"} };
static const char * const op_param[][4] = { {"port1", "port2"},
					{"w", "r", "s", "p"} };
static const char * const test_param[][5] = {{"srio", "dma_chain", "show_perf",
				"show_task", "free_task"},
				{"port1", "port2"},
				{"dma", "core"},
				{"swrite", "nwrite", "nwrite_r", "nread"} };
static const char * const sector_name[] = {"map space", "read data space",
					"write preparing space", "reserved"};
static const char * const perf_str[] = {"length(byte):", "time(us):",
					"avg Gb/s:", "max Gb/s:"};

enum srio_cmd {
	SRIO_ATTR,
	SRIO_OP,
	SRIO_TEST
};

struct cmd_port_param {
	uint32_t attr_cmd3_id;
	uint32_t attr_cmd4;
	uint32_t attr_tdid;
	uint32_t attr_read;
	uint32_t attr_write;
	uint8_t op_type;
	uint8_t op_win_id;
	uint8_t op_seg_id;
	uint8_t op_subseg_id;
	size_t op_len;
};

struct cmd_param_type {
	struct cmd_port_param *port;
	uint8_t curr_cmd;
	uint8_t curr_port_id;
	uint8_t test_type;
	uint8_t test_bwc;
	uint8_t test_task_type;
	uint8_t test_srio_type;
	uint8_t test_srio_priority;
	uint8_t test_data;
	uint32_t test_num;
};

struct srio_task_record {
	const char *task_type;
	const char *trans_type;
	uint32_t payload_size;
	uint32_t port;
	uint32_t priority;
	uint32_t data;
};

struct srio_task_flag {
	uint64_t show_times;
	uint32_t show_perf;
	uint32_t free_task;
	uint32_t port;
};

struct task_arg_type {
	struct srio_dev *sriodev;
	struct dma_ch *dmadev;
	uint64_t src_phys;
	uint64_t dest_phys;
	void *src_virt;
	void *dest_virt;
	uint32_t size;
	uint32_t port;
};

enum srio_write_type {
	FLUSH,
	SWRITE,
	NWRITE,
	NWRITE_R,
	MAINTW,
};

enum srio_read_type {
	IO_READ_HOME,
	NREAD,
	MAINTR,
	ATOMIC_INC,
	ATOMIC_DEC,
	ATOMIC_SET,
	ATOMIC_CLR,
};

static struct cmd_param_type cmd_param;
static int port_num;
static struct srio_task_record srio_task[TASK_MAX_NUM];
static struct srio_task_flag task_flag[TASK_MAX_NUM];
static struct task_arg_type task_arg;
static struct dma_ch *dmadev1, *dmadev2;

/* Handle SRIO error interrupt */
static void *interrupt_handler(void *data)
{
	int s, srio_fd, nfds;
	fd_set readset;
	uint32_t junk;
	struct srio_dev *sriodev = data;

	srio_fd = fsl_srio_fd(sriodev);
	nfds = srio_fd + 1;

	while (1) {
		FD_ZERO(&readset);
		FD_SET(srio_fd, &readset);
		s = select(nfds, &readset, NULL, NULL, NULL);
		if (s < 0) {
			error(0, 0, "RMan&SRIO select error");
			break;
		}
		if (s) {
			read(srio_fd, &junk, sizeof(junk));
			fsl_srio_irq_handler(sriodev);
		}
	}

	pthread_exit(NULL);
}

void fsl_srio_err_handle_enable(struct srio_dev *sriodev)
{
	int ret;
	pthread_t interrupt_handler_id;

	ret = pthread_create(&interrupt_handler_id, NULL,
			     interrupt_handler, sriodev);
	if (ret)
		error(0, errno, "Create interrupt handler thread error");
}

static int par_to_srio_attr(uint8_t wr_attr_id, uint8_t rd_attr_id,
			    uint8_t *wr_attr, uint8_t *rd_attr)
{
	if ((!wr_attr) || (!rd_attr))
		return -EINVAL;

	switch (wr_attr_id) {
	case FLUSH:
		*wr_attr = SRIO_ATTR_FLUSH;
		break;
	case SWRITE:
		*wr_attr = SRIO_ATTR_SWRITE;
		break;
	case NWRITE:
		*wr_attr = SRIO_ATTR_NWRITE;
		break;
	case NWRITE_R:
		*wr_attr = SRIO_ATTR_NWRITE_R;
		break;
	case MAINTW:
		*wr_attr = SRIO_ATTR_MAINTW;
	}

	switch (rd_attr_id) {
	case IO_READ_HOME:
		*rd_attr = SRIO_ATTR_IO_READ_HOME;
		break;
	case NREAD:
		*rd_attr = SRIO_ATTR_NREAD;
		break;
	case MAINTR:
		*rd_attr = SRIO_ATTR_MAINTR;
		break;
	case ATOMIC_INC:
		*rd_attr = SRIO_ATTR_ATOMIC_INC;
		break;
	case ATOMIC_DEC:
		*rd_attr = SRIO_ATTR_ATOMIC_DEC;
		break;
	case ATOMIC_SET:
		*rd_attr = SRIO_ATTR_ATOMIC_SET;
		break;
	case ATOMIC_CLR:
		*rd_attr = SRIO_ATTR_ATOMIC_CLR;
	}

	return 0;
}

static int attr_param_trans(int32_t cmd_num, char **cmd_in)
{
	int i, j, k;
	uint8_t port = 0;
	uint8_t rd_attr_id = 0;
	uint8_t wr_attr_id = 0;
	uint8_t rd_attr = 0;
	uint8_t wr_attr = 0;

	if (cmd_num > ATTR_CMD_MAX_NUM || cmd_num < ATTR_CMD_MIN_NUM)
		return -EINVAL;

	for (j = 0, k = 2; j < ARRAY_SIZE(attr_param); j++, k++) {
		for (i = 0; i < ARRAY_SIZE(attr_param[j]) && attr_param[j][i];
		     i++)
			if (!strcmp(cmd_in[k], attr_param[j][i]))
				break;

		if (i == ARRAY_SIZE(attr_param[j]) || !attr_param[j][i])
			return -EINVAL;

		if (j == 0)
			port = cmd_param.curr_port_id = i;
		else if (j == 1) {
			cmd_param.port[port].attr_cmd3_id = i;
			cmd_param.port[port].attr_cmd4 =
				strtoul(cmd_in[4], NULL, 0);
			switch (i) {
			case DEVICE_ID:
			case TARGET_ID:
			case SEG_NUM:
			case SUBSEG_NUM:
			case ACCEPT_ALL:
			case IRQ:
				j = ARRAY_SIZE(attr_param);
				break;
			case SUBSEG_TDID:
				if (!cmd_in[5])
					return -EINVAL;
				cmd_param.port[port].attr_tdid =
					strtoul(cmd_in[5], NULL, 0);
				j = ARRAY_SIZE(attr_param);
				break;
			default:
				k++;
				break;
			}
		} else if (j == 2)
			wr_attr_id = i;
		else if (j == 3)
			rd_attr_id = i;
		else
			return -EINVAL;
	}

	par_to_srio_attr(wr_attr_id, rd_attr_id, &wr_attr, &rd_attr);
	cmd_param.port[port].attr_write = wr_attr;
	cmd_param.port[port].attr_read = rd_attr;

	return 0;
}

static int op_param_trans(int32_t cmd_num, char **cmd_in)
{
	int32_t i;
	uint8_t port_id;

	if (cmd_num != OP_CMD_NUM)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(op_param[0]) && op_param[0][i]; i++)
		if (!strcmp(cmd_in[2], op_param[0][i]))
			break;

	if (i == ARRAY_SIZE(op_param[0]) || !op_param[0][i])
		return -EINVAL;

	port_id = i;

	for (i = 0; i < ARRAY_SIZE(op_param[1]) && op_param[1][i]; i++)
		if (!strcmp(cmd_in[6], op_param[1][i]))
			break;

	if (i == ARRAY_SIZE(op_param[1]) || !op_param[1][i])
		return -EINVAL;

	cmd_param.curr_port_id = port_id;
	cmd_param.port[port_id].op_type = i;
	cmd_param.port[port_id].op_win_id = strtoul(cmd_in[3], NULL, 0);
	cmd_param.port[port_id].op_seg_id = strtoul(cmd_in[4], NULL, 0);
	cmd_param.port[port_id].op_subseg_id = strtoul(cmd_in[5], NULL, 0);
	cmd_param.port[port_id].op_len = strtoul(cmd_in[7], NULL, 0);
	if (cmd_param.port[port_id].op_len > SRIO_WIN_SIZE)
		return -EINVAL;

	return 0;
}

static int test_param_trans(int32_t cmd_num, char **cmd_in)

{
	int32_t i;

	if (cmd_num > TEST_CMD_NUM)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(test_param[0]) && test_param[0][i]; i++)
		if (!strcmp(cmd_in[2], test_param[0][i]))
			break;

	if (i == ARRAY_SIZE(test_param[0]) || !test_param[0][i])
		return -EINVAL;

	cmd_param.test_type = i;

	switch (i) {
	case TEST_SRIO:
		if (cmd_num != 5 && cmd_num != 7)
			return -EINVAL;

		for (i = 0;
			i < ARRAY_SIZE(test_param[1]) && test_param[1][i]; i++)
			if (!strcmp(cmd_in[3], test_param[1][i]))
				break;

		if (i == ARRAY_SIZE(test_param[1]) || !test_param[1][i])
			return -EINVAL;
		cmd_param.curr_port_id = i;

		for (i = 0;
			i < ARRAY_SIZE(test_param[2]) && test_param[2][i]; i++)
			if (!strcmp(cmd_in[4], test_param[2][i]))
				break;

		if (i == ARRAY_SIZE(test_param[2]) || !test_param[2][i])
			return -EINVAL;
		cmd_param.test_task_type = i;
		cmd_param.test_num = 0;

		if (cmd_num == 7) {
			for (i = 0;
			     i < ARRAY_SIZE(test_param[3]) && test_param[3][i];
			     i++)
				if (!strcmp(cmd_in[5], test_param[3][i]))
					break;

			if (i == ARRAY_SIZE(test_param[3]) || !test_param[3][i])
				return -EINVAL;
			cmd_param.test_srio_type = i;

			cmd_param.test_num = strtoul(cmd_in[6], NULL, 0);
		}
		break;
	case TEST_DMA_CHAIN:
	case TEST_SHOW_TASK:
		if (cmd_num != 3)
			return -EINVAL;
		break;
	case TEST_SHOW_PERF:
		if (cmd_num != 5)
			return -EINVAL;
		for (i = 0;
			i < ARRAY_SIZE(test_param[1]) && test_param[1][i]; i++)
			if (!strcmp(cmd_in[3], test_param[1][i]))
				break;

		if (i == ARRAY_SIZE(test_param[1]) || !test_param[1][i])
			return -EINVAL;
		cmd_param.curr_port_id = i;

		cmd_param.test_num = strtoul(cmd_in[4], NULL, 0);
		break;
	case TEST_FREE_TASK:
		if (cmd_num != 4)
			return -EINVAL;
		for (i = 0;
			i < ARRAY_SIZE(test_param[1]) && test_param[1][i]; i++)
			if (!strcmp(cmd_in[3], test_param[1][i]))
				break;

		if (i == ARRAY_SIZE(test_param[1]) || !test_param[1][i])
			return -EINVAL;
		cmd_param.curr_port_id = i;
		break;
	}

	return 0;
}


static int cmd_translate(int32_t cmd_num, char **cmd_in)
{
	int i, err = 0;

	if (cmd_num < SRIO_CMD_MIN_NUM)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(cmd_name) && cmd_name[i]; i++)
		if (!strcmp(cmd_in[1], cmd_name[i]))
			break;

	if (i == ARRAY_SIZE(cmd_name) || !cmd_name[i])
		return -EINVAL;

	cmd_param.curr_cmd = i;

	switch (i) {
	case SRIO_ATTR:
		err = attr_param_trans(cmd_num, cmd_in);
		break;
	case SRIO_OP:
		err = op_param_trans(cmd_num, cmd_in);
		break;
	case SRIO_TEST:
		err = test_param_trans(cmd_num, cmd_in);
		break;
	}

	return err;
}

static int op_implement(struct srio_dev *sriodev, struct dma_ch *dmadev,
			struct srio_port_data  *port_data)
{
	uint8_t port_id = cmd_param.curr_port_id;
	int i, j, k, err;
	const char *pri;
	uint8_t nseg, nsseg;
	uint32_t seg_size, sseg_size, win_offset;

	nseg = fsl_srio_get_seg_num(sriodev, port_id, 1);
	if (nseg < 0)
		return nseg;

	nsseg = fsl_srio_get_subseg_num(sriodev, port_id, 1);
	if (nsseg < 0)
		return nsseg;

	if ((cmd_param.port[port_id].op_seg_id > nseg) ||
		(cmd_param.port[port_id].op_subseg_id > nsseg))
		return -EINVAL;
	seg_size = SRIO_WIN_SIZE / nseg;
	sseg_size = seg_size / nsseg;

	if ((!cmd_param.port[port_id].op_seg_id && nseg) ||
		(!cmd_param.port[port_id].op_subseg_id && nsseg))
		return -EINVAL;

	if ((nseg && (cmd_param.port[port_id].op_len > seg_size)) ||
		(nsseg && (cmd_param.port[port_id].op_len > seg_size)))
		return -EINVAL;

	if (!nseg)
		win_offset = 0;
	else
		if (nsseg)
			win_offset = seg_size *
				(cmd_param.port[port_id].op_seg_id - 1) +
				sseg_size *
				(cmd_param.port[port_id].op_subseg_id - 1);
		else
			win_offset = seg_size *
				(cmd_param.port[port_id].op_seg_id - 1);

	switch (cmd_param.port[port_id].op_type) {
	case SRIO_DIR_WRITE:
		fsl_dma_direct_start(dmadev,
				     port_data[port_id].phys.write_data_prep,
				     port_data[port_id].port_info.range_start +
				     win_offset,
				     cmd_param.port[port_id].op_len);
		break;
	case SRIO_DIR_READ:
		fsl_dma_direct_start(dmadev,
				     port_data[port_id].port_info.range_start +
				     win_offset,
				     port_data[port_id].phys.read_recv_data,
				     cmd_param.port[port_id].op_len);
		break;
	case SRIO_DIR_SET_MEM:
		for (i = 0; i < port_num; i++) {
			memset(&port_data[i].virt->write_recv_data,
			       i * SRIO_POOL_PORT_SECT_NUM,
			       SRIO_POOL_SECT_SIZE);
			memset(&port_data[i].virt->read_recv_data,
			       i * SRIO_POOL_PORT_SECT_NUM + 1,
			       SRIO_POOL_SECT_SIZE);
			memset(&port_data[i].virt->write_data_prep,
			       i * SRIO_POOL_PORT_SECT_NUM + 2,
			       SRIO_POOL_SECT_SIZE);
			memset(&port_data[i].virt->res,
			       i * SRIO_POOL_PORT_SECT_NUM + 3,
			       SRIO_POOL_SECT_SIZE);
		}
		break;
	case SRIO_DIR_PRI_MEM:
		for (j = 0; j < port_num; j++)
			for (k = 0; k < SRIO_POOL_PORT_SECT_NUM; k++) {
				printf("port%d %s\n", j + 1, sector_name[k]);
				pri = (typeof(pri))port_data[j].virt +
					SRIO_POOL_SECT_SIZE * k;
				for (i = 0; i < 20; i += 4)
					printf("%x\t%x\t%x\t%x\n",
					       *(pri + i), *(pri + i + 1),
					       *(pri + i + 2), *(pri + i + 3));
			}
	}

	if (cmd_param.port[port_id].op_type <= SRIO_DIR_READ) {
		err = fsl_dma_wait(dmadev);
		if (err < 0) {
			fsl_srio_clr_bus_err(sriodev);
			return err;
		}
	}
	return 0;
}

static int attr_implement(struct srio_dev *sriodev,
			  struct srio_port_data	 *port_data)
{
	int32_t err = 0;
	uint8_t port_id = cmd_param.curr_port_id;

	switch (cmd_param.port[port_id].attr_cmd3_id) {
	case DEVICE_ID:
		err = fsl_srio_set_deviceid(sriodev, port_id,
			cmd_param.port[port_id].attr_cmd4);
		break;
	case TARGET_ID:
		err = fsl_srio_set_targetid(sriodev, port_id, 1,
			cmd_param.port[port_id].attr_cmd4);
		break;
	case SEG_NUM:
		err = fsl_srio_set_seg_num(sriodev, port_id, 1,
			cmd_param.port[port_id].attr_cmd4);
		break;
	case SUBSEG_NUM:
		err = fsl_srio_set_subseg_num(sriodev, port_id, 1,
			cmd_param.port[port_id].attr_cmd4);
		break;
	case SUBSEG_TDID:
		err = fsl_srio_set_seg_sgtgtdid(sriodev, port_id, 1,
			cmd_param.port[port_id].attr_cmd4 - 1,
			cmd_param.port[port_id].attr_tdid);
		break;
	case ACCEPT_ALL:
		if (cmd_param.port[port_id].attr_cmd4)
			err = fsl_srio_enable_accept_all(sriodev, port_id);
		else
			err = fsl_srio_disable_accept_all(sriodev, port_id);
		break;
	case IRQ:
		if (cmd_param.port[port_id].attr_cmd4 == 1)
			err = fsl_srio_irq_enable(sriodev);
		else if (cmd_param.port[port_id].attr_cmd4 == 0)
			err = fsl_srio_irq_disable(sriodev);
		break;
	case WIN_ATTR:
		fsl_srio_set_obwin(sriodev, port_id, 1,
			port_data[port_id].port_info.range_start,
			SRIO_SYS_ADDR, LAWAR_SIZE_2M);
		fsl_srio_set_ibwin(sriodev, port_id, 1,
			port_data[port_id].phys.write_recv_data,
			SRIO_SYS_ADDR, LAWAR_SIZE_2M);
		err = fsl_srio_set_obwin_attr(sriodev, port_id, 1,
			cmd_param.port[port_id].attr_read,
			cmd_param.port[port_id].attr_write);
		break;
	case SEG_ATTR:
		fsl_srio_set_obwin(sriodev, port_id, 1,
			port_data[port_id].port_info.range_start,
			SRIO_SYS_ADDR, LAWAR_SIZE_2M);
		fsl_srio_set_ibwin(sriodev, port_id, 1,
			port_data[port_id].phys.write_recv_data,
			SRIO_SYS_ADDR, LAWAR_SIZE_2M);
		err = fsl_srio_set_seg_attr(sriodev, port_id, 1,
			cmd_param.port[port_id].attr_cmd4 - 1,
			cmd_param.port[port_id].attr_read,
			cmd_param.port[port_id].attr_write);
		break;
	}

	return err;
}

static int srio_perf_test(struct srio_dev *sriodev, struct dma_ch *dmadev,
			  struct srio_port_data	 *port_data)
{
	int i, j, k, h, err;
	uint64_t src_phys, dest_phys;
	void *src_virt;
	void *dest_virt;
	struct atb_clock *atb_clock;
	uint64_t atb_multiplier = 0;
	uint8_t port = cmd_param.curr_port_id;
	uint8_t test_type = cmd_param.test_task_type;

	if (srio_task[port].payload_size) {
		printf("A running task on port %d!\n", port + 1);
		printf("First free the runnig task and then create the new!\n");
		printf("Running task_type:%s; trans:%s; port:%d; size:%d\n",
		       srio_task[port].task_type, srio_task[port].trans_type,
		       srio_task[port].port + 1, srio_task[port].payload_size);

		return 0;
	}

	if (!(fsl_srio_port_connected(sriodev) & (0x1 << port))) {
		printf("port %d is error or disable!\n", port + 1);
		return 0;
	}

	atb_clock = malloc(sizeof(struct atb_clock));
	if (!atb_clock)
		return -errno;

	atb_multiplier = atb_get_multiplier();

	for (i = 0; i < port_num; i++)
		if (fsl_srio_port_connected(sriodev) & (0x1 << i)) {
			fsl_srio_set_obwin(sriodev, i, 1,
					   port_data[i].port_info.range_start,
					   SRIO_SYS_ADDR, LAWAR_SIZE_2M);
			fsl_srio_set_ibwin(sriodev, i, 1,
					   port_data[i].phys.write_recv_data,
					   SRIO_SYS_ADDR, LAWAR_SIZE_2M);
			fsl_srio_set_seg_num(sriodev, i, 1, 0);
			fsl_srio_set_subseg_num(sriodev, i, 1, 0);
		}

	if (test_type == CORE_TEST) {
		printf("===port %d core test===\n", port + 1);
		for (i = 0; i < ARRAY_SIZE(srio_test_win_attrv); i++) {
			if (i != 4)
				printf("\nSRIO %s Test for %d times\n",
				       srio_test_win_attrc[i], TEST_MAX_TIMES);
			else
				printf("\nSRIO CORE Test for %d times\n",
				       TEST_MAX_TIMES);

			if (i < ARRAY_SIZE(srio_test_win_attrv) - 2)
				fsl_srio_set_obwin_attr(sriodev, port, 1, 0,
							srio_test_win_attrv[i]);
			else if (i == 3)
				fsl_srio_set_obwin_attr(sriodev, port, 1,
							srio_test_win_attrv[i],
							0);

			if (i < ARRAY_SIZE(srio_test_win_attrv) - 2) {
				src_virt =
					&port_data[port].virt->write_data_prep;
				dest_virt = port_data[port].range_virt;
			} else if (i == ARRAY_SIZE(srio_test_win_attrv) - 2) {
				src_virt = port_data[port].range_virt;
				dest_virt =
					&port_data[port].virt->read_recv_data;
			} else {
				src_virt =
					&port_data[port].virt->write_data_prep;
				dest_virt =
					&port_data[port].virt->write_recv_data;
			}

			/* SRIO test data is from 4 bytes to 1M bytes*/
			for (j = 2; j < SRIO_TEST_DATA_NUM; j++) {
				atb_clock_init(atb_clock);
				atb_clock_reset(atb_clock);
				for (k = 0; k < TEST_MAX_TIMES; k++) {
					atb_clock_start(atb_clock);
					memcpy(dest_virt, src_virt, (1 << j));
					atb_clock_stop(atb_clock);
				}
				printf("%s %-15u %s %-15f %s %-15f %s %-15f\n",
				       perf_str[0],
				       (1 << j),
				       perf_str[1],
				       atb_to_seconds(
						atb_clock_total(atb_clock),
						atb_multiplier) / TEST_MAX_TIMES
				       * ATB_MHZ,
				       perf_str[2],
				       (1 << j) * 8 * TEST_MAX_TIMES /
				       (atb_to_seconds(
						atb_clock_total(atb_clock),
						atb_multiplier) * 1000000000.0),
				       perf_str[3],
				       (1 << j) * 8 /
				       (atb_to_seconds(atb_clock_min(atb_clock),
					atb_multiplier) * 1000000000.0));
				atb_clock_finish(atb_clock);
			}
		}
		return 0;
	}

	printf("===port %d dma test===\n", port + 1);
	for (h = 0; h < DMA_BWC_NUM; h++) {
		if (h < DMA_BWC_NUM - 1)
			/* BWC is from 1 to 1024 */
			printf("\n----------BWC is %d byte-----------\n",
			       (1 << h));
		else {
			h = DMA_BWC_DIS;
			printf("\n----------BWC is Disabled----------\n");
		}

		fsl_dma_chan_bwc(dmadev, h);

		for (i = 0; i < ARRAY_SIZE(srio_test_win_attrv); i++) {
			if (i != 4)
				printf("\nSRIO %s Test for %d times\n",
				       srio_test_win_attrc[i], TEST_MAX_TIMES);
			else
				printf("\nSRIO DMA Test for %d times\n",
				       TEST_MAX_TIMES);

			if (i < ARRAY_SIZE(srio_test_win_attrv) - 2)
				fsl_srio_set_obwin_attr(sriodev, port, 1, 0,
							srio_test_win_attrv[i]);
			else if (i == 3)
				fsl_srio_set_obwin_attr(sriodev, port, 1,
							srio_test_win_attrv[i],
							0);

			if (i < ARRAY_SIZE(srio_test_win_attrv) - 2) {
				src_phys =
					port_data[port].phys.write_data_prep;
				dest_phys =
					port_data[port].port_info.range_start;
			} else if (i == ARRAY_SIZE(srio_test_win_attrv) - 2) {
				src_phys =
					port_data[port].port_info.range_start;
				dest_phys =
					port_data[port].phys.read_recv_data;
			} else {
				src_phys =
					port_data[port].phys.write_data_prep;
				dest_phys =
					port_data[port].phys.write_recv_data;
			}

			/* SRIO test data is from 4 bytes to 1M bytes*/
			for (j = 2; j < SRIO_TEST_DATA_NUM; j++) {
				atb_clock_init(atb_clock);
				atb_clock_reset(atb_clock);
				for (k = 0; k < TEST_MAX_TIMES; k++) {
					atb_clock_start(atb_clock);
					fsl_dma_direct_start(dmadev,
							src_phys, dest_phys,
							(1 << j));

					err = fsl_dma_wait(dmadev);
					if (err < 0) {
						error(0, -err,
						"SRIO transmission failed");
						fsl_srio_clr_bus_err(sriodev);
						atb_clock_finish(atb_clock);
						return err;
					}
					atb_clock_stop(atb_clock);
				}
				printf("%s %-15u %s %-15f %s %-15f %s %-15f\n",
				       perf_str[0],
				       (1 << j),
				       perf_str[1],
				       atb_to_seconds(
						atb_clock_total(atb_clock),
						atb_multiplier) / TEST_MAX_TIMES
				       * ATB_MHZ,
				       perf_str[2],
				       (1 << j) * 8 * TEST_MAX_TIMES /
				       (atb_to_seconds(
						atb_clock_total(atb_clock),
						atb_multiplier) * 1000000000.0),
				       perf_str[3],
				       (1 << j) * 8 /
				       (atb_to_seconds(atb_clock_min(atb_clock),
					atb_multiplier) * 1000000000.0));
				atb_clock_finish(atb_clock);
			}
		}
	}

	return 0;
}

static int dma_chain_mode_test(struct dma_ch *dmadev,
				struct srio_port_data  *port_data)
{
	struct dma_link_setup_data *link_data;
	struct dma_link_dsc *link_dsc;
	int i;

	link_data = (struct dma_link_setup_data *)
		malloc(sizeof(*link_data) * DMA_TEST_CHAIN_NUM);
	link_dsc = (struct dma_link_dsc *)port_data[1].virt->res;

	for (i = 0; i < DMA_TEST_CHAIN_NUM; i++) {
		link_data[i].byte_count = 256;
		link_data[i].src_addr = port_data[0].phys.write_recv_data +
					(i + 1) * SRIO_POOL_SECT_SIZE;
		link_data[i].dst_addr = port_data[0].phys.write_recv_data +
					i * SRIO_POOL_SECT_SIZE;
		link_data[i].dst_snoop_en = 1;
		link_data[i].src_snoop_en = 1;
		link_data[i].dst_nlwr = 0;
		link_data[i].dst_stride_en = 0;
		link_data[i].src_stride_en = 0;
		link_data[i].dst_stride_dist = 0;
		link_data[i].src_stride_dist = 0;
		link_data[i].dst_stride_size = 0;
		link_data[i].src_stride_size = 0;
		link_data[i].err_interrupt_en = 0;
		link_data[i].seg_interrupt_en = 0;
		link_data[i].link_interrupt_en = 0;
	}
	fsl_dma_chain_link_build(link_data, link_dsc,
				port_data[1].phys.res, DMA_TEST_CHAIN_NUM);
	fsl_dma_chain_basic_start(dmadev, link_data, port_data[1].phys.res);

	return 0;
}

static void *t_srio_dma_test(void *arg)
{
	struct task_arg_type *args = arg;
	struct srio_dev *sriodev = args->sriodev;
	struct dma_ch *dmadev = args->dmadev;
	uint64_t src_phys = args->src_phys;
	uint64_t dest_phys = args->dest_phys;
	uint32_t size = args->size;
	uint32_t port = args->port;
	int err = 0;
	struct atb_clock *atb_clock = NULL;
	uint64_t atb_multiplier = 0;
	uint64_t perf_times = ~0;

	while (1) {
		if (task_flag[port].show_perf) {
			task_flag[port].show_perf = 0;
			perf_times = task_flag[port].show_times;
			atb_clock = malloc(sizeof(struct atb_clock));
			if (!atb_clock) {
				printf("port %d: show performance error!\n",
				       port + 1);
				task_flag[port].show_times = 0;
				perf_times = ~0;
				break;
			}
			atb_multiplier = atb_get_multiplier();
			atb_clock_init(atb_clock);
			atb_clock_reset(atb_clock);
		}

		if (perf_times && task_flag[port].show_times &&
		    !task_flag[port].show_perf)
			atb_clock_start(atb_clock);
		fsl_dma_direct_start(dmadev, src_phys, dest_phys, size);

		err = fsl_dma_wait(dmadev);
		if (perf_times && task_flag[port].show_times &&
		    !task_flag[port].show_perf)
			atb_clock_stop(atb_clock);
		if (err < 0) {
			printf("port %d: dma task error!\n", port + 1);
			fsl_srio_clr_bus_err(sriodev);
			task_flag[port].show_times = 0;
			perf_times = ~0;
			break;
		}
		if (!perf_times && task_flag[port].show_times) {
			printf("%s %-15u %s %-15f %s %-15f %s %-15f\n",
			       perf_str[0], size, perf_str[1],
				atb_to_seconds(atb_clock_total(atb_clock),
					       atb_multiplier) /
				task_flag[port].show_times
				* ATB_MHZ,
				perf_str[2],
				size * 8 * task_flag[port].show_times /
				(atb_to_seconds(atb_clock_total(atb_clock),
					atb_multiplier) * 1000000000.0),
				perf_str[3],
				size * 8 /
				(atb_to_seconds(atb_clock_min(atb_clock),
					atb_multiplier) * 1000000000.0));
			atb_clock_finish(atb_clock);
			task_flag[port].show_times = 0;
			perf_times = ~0;
		} else if (task_flag[port].show_times) {
			perf_times--;
		}

		if (task_flag[port].free_task) {
			srio_task[port].payload_size = 0;
			if (perf_times)
				atb_clock_finish(atb_clock);
			task_flag[port].free_task = 0;
			printf("down!\n");
			break;
		}
	}
	pthread_exit(NULL);
}

static void *t_srio_core_test(void *arg)
{
	struct task_arg_type *args = arg;
	void *src_virt = args->src_virt;
	void *dest_virt = args->dest_virt;
	uint32_t size = args->size;
	uint32_t port = args->port;
	struct atb_clock *atb_clock = NULL;
	uint64_t atb_multiplier = 0;
	uint64_t perf_times = ~0;

	while (1) {
		if (task_flag[port].show_perf) {
			task_flag[port].show_perf = 0;
			perf_times = task_flag[port].show_times;
			atb_clock = malloc(sizeof(struct atb_clock));
			if (!atb_clock) {
				printf("port %d: show performance error!\n",
				       port + 1);
				task_flag[port].show_times = 0;
				perf_times = ~0;
				break;
			}
			atb_multiplier = atb_get_multiplier();
			atb_clock_init(atb_clock);
			atb_clock_reset(atb_clock);
		}

		if (perf_times && task_flag[port].show_times &&
		    !task_flag[port].show_perf)
			atb_clock_start(atb_clock);
		memcpy(dest_virt, src_virt, size);
		if (perf_times && task_flag[port].show_times &&
		    !task_flag[port].show_perf)
			atb_clock_stop(atb_clock);
		if (!perf_times && task_flag[port].show_times) {
			printf("%s %-15u %s %-15f %s %-15f %s %-15f\n",
			       perf_str[0], size, perf_str[1],
				atb_to_seconds(atb_clock_total(atb_clock),
					       atb_multiplier) /
				task_flag[port].show_times
				* ATB_MHZ,
				perf_str[2],
				size * 8 * task_flag[port].show_times /
				(atb_to_seconds(atb_clock_total(atb_clock),
					atb_multiplier) * 1000000000.0),
				perf_str[3],
				size * 8 /
				(atb_to_seconds(atb_clock_min(atb_clock),
					atb_multiplier) * 1000000000.0));
			atb_clock_finish(atb_clock);
			task_flag[port].show_times = 0;
			perf_times = ~0;
		} else if (task_flag[port].show_times) {
			perf_times--;
		}

		if (task_flag[port].free_task) {
			srio_task[port].payload_size = 0;
			if (perf_times)
				atb_clock_finish(atb_clock);
			task_flag[port].free_task = 0;
			printf("down!\n");
			break;
		}
	}
	pthread_exit(NULL);
}

static int srio_perf_task(struct srio_dev *sriodev,
			struct srio_port_data *port_data)
{
	int err;
	uint64_t src_phys, dest_phys;
	void *src_virt, *dest_virt;
	uint8_t port = cmd_param.curr_port_id;
	uint8_t test_type = cmd_param.test_task_type;
	uint32_t attr_read, attr_write;
	uint32_t size = cmd_param.test_num;
	pthread_t srio_task_id;

	if (srio_task[port].payload_size) {
		printf("A running task on port %d!\n", port + 1);
		printf("First free the runnig task and then create the new!\n");
		printf("Running task_type:%s; trans:%s; port:%d; size:%d\n",
		       srio_task[port].task_type, srio_task[port].trans_type,
		       srio_task[port].port + 1, srio_task[port].payload_size);

		return 0;
	}

	attr_read = srio_test_win_attrv[3];
	attr_write = srio_test_win_attrv[cmd_param.test_srio_type];

	if (size > SRIO_WIN_SIZE) {
		printf("Max block size is %d\n", SRIO_WIN_SIZE);
		return -errno;
	}

	if (fsl_srio_port_connected(sriodev) & (0x1 << port)) {
		fsl_srio_set_obwin(sriodev, port, 1,
				   port_data[port].port_info.range_start,
				   SRIO_SYS_ADDR, LAWAR_SIZE_2M);
		fsl_srio_set_obwin_attr(sriodev, port, 1,
					attr_read, attr_write);
	} else {
		printf("SRIO port %d error!\n", port + 1);
		return -errno;
	}

        fsl_srio_set_targetid(sriodev,0,1,0x11);	
        fsl_srio_set_targetid(sriodev,1,1,0x14);
	
	if (task_flag[port].port == port)
		task_flag[port].free_task = 0;

	if (test_type == DMA_TEST) {
		if (port) {
			fsl_dma_chan_basic_direct_init(dmadev2);
			fsl_dma_chan_bwc(dmadev2, DMA_BWC_512);
			task_arg.dmadev = dmadev2;
		} else {
			fsl_dma_chan_basic_direct_init(dmadev1);
			fsl_dma_chan_bwc(dmadev1, DMA_BWC_512);
		task_arg.dmadev = dmadev1;
		}

		if (cmd_param.test_srio_type != 3) {
			src_phys = port_data[port].phys.write_data_prep;
			dest_phys = port_data[port].port_info.range_start;
		} else {
			src_phys = port_data[port].port_info.range_start;
			dest_phys = port_data[port].phys.read_recv_data;
		}
		memset((void *)&port_data[port].virt->write_data_prep,
		       0x5a, SRIO_WIN_SIZE);

		task_arg.sriodev = sriodev;
		task_arg.src_phys = src_phys;
		task_arg.dest_phys = dest_phys;
		task_arg.size = size;
		task_arg.port = port;

		err = pthread_create(&srio_task_id, NULL,
			t_srio_dma_test, &task_arg);
		if (err) {
			printf("Port %d : dma task creating failed!\n",
			       port + 1);
			return -errno;
		} else {
			printf("Port %d : dma task creating successed!\n",
			       port + 1);
			printf(" -- srio_type:%s; payload_size:%d; data:0x5a\n",
			       srio_test_win_attrc[cmd_param.test_srio_type],
			       size);

		srio_task[port].task_type = "dma_task";
		srio_task[port].trans_type =
			srio_test_win_attrc[cmd_param.test_srio_type];
		srio_task[port].payload_size = size;
		srio_task[port].port = port;
		}
		return 0;
	}

	/* core srio task */
	if (cmd_param.test_task_type != 3) {
		src_virt = (void *)&(port_data[port].virt->write_data_prep);
		dest_virt = (void *)port_data[port].range_virt;
	} else {
		src_virt = (void *)port_data[port].range_virt;
		dest_virt = (void *)&(port_data[port].virt->read_recv_data);
	}

	memset((void *)&port_data[port].virt->write_data_prep,
	       0x5a, SRIO_WIN_SIZE);

	task_arg.src_virt = src_virt;
	task_arg.dest_virt = dest_virt;
	task_arg.size = size;
	task_arg.port = port;

	err = pthread_create(&srio_task_id, NULL,
		t_srio_core_test, &task_arg);
	if (err) {
		printf("Port %d : core task creating failed!\n",
		       port + 1);
		return -errno;
	} else {
		printf("Port %d : core task creating successed!\n",
		       port + 1);
		printf(" -- srio_type:%s; size:%d; data:0x5a\n",
		       srio_test_win_attrc[cmd_param.test_srio_type], size);

		srio_task[port].task_type = "core_task";
		srio_task[port].trans_type =
			srio_test_win_attrc[cmd_param.test_srio_type];
		srio_task[port].payload_size = size;
		srio_task[port].port = port;
	}
	return 0;
}

static int srio_task_perf_show(void)
{
	int port = cmd_param.curr_port_id;

	if (!srio_task[port].payload_size) {
		printf("No running tasks on port %d!\n", port + 1);
		return 0;
	}
	if (!cmd_param.test_num)
		return 0;

	if (task_flag[port].show_times) {
		printf("Port %d is working on %"PRIu64" times performance\n",
		       port + 1, task_flag[port].show_times);
		return 0;
	}

	task_flag[port].show_times = cmd_param.test_num;
	task_flag[port].show_perf = 1;
	task_flag[port].port = port;
	printf("Task_type:%s; trans:%s; port:%d; size:%d\n",
	       srio_task[port].task_type, srio_task[port].trans_type,
	       srio_task[port].port + 1, srio_task[port].payload_size);
	printf("Show the performance of the %d transmissions ...\n",
	       cmd_param.test_num);
	return 0;
}

static int srio_task_show(void)
{
	int i;

	printf("===================== srio task info ===================\n");

	for (i = 0; i < TASK_MAX_NUM; i++) {
		if (!srio_task[i].payload_size)
			continue;
		printf("== type:%s; trans:%s; port:%d; size:%d\n",
		       srio_task[i].task_type, srio_task[i].trans_type,
		       srio_task[i].port + 1, srio_task[i].payload_size);
	}

	return 0;
}

static int srio_task_free(void)
{
	int port = cmd_param.curr_port_id;

	if (!srio_task[port].payload_size) {
		printf("No running tasks on port %d!\n", port + 1);
		return 0;
	}
	task_flag[port].free_task = 1;
	task_flag[port].port = port;
	printf("Free running tasks on port %d ...\n", port + 1);
	return 0;
}

static int test_implement(struct srio_dev *sriodev, struct dma_ch *dmadev,
			struct srio_port_data  *port_data)
{
	switch (cmd_param.test_type) {
	case TEST_SRIO:
		if (cmd_param.test_num)
			srio_perf_task(sriodev, port_data);
		else
			srio_perf_test(sriodev, dmadev, port_data);
		break;
	case TEST_DMA_CHAIN:
		dma_chain_mode_test(dmadev, port_data);
		break;
	case TEST_SHOW_PERF:
		srio_task_perf_show();
		break;
	case TEST_SHOW_TASK:
		srio_task_show();
		break;
	case TEST_FREE_TASK:
		srio_task_free();
		break;
	}
	return 0;
}

static int cmd_implement(struct srio_dev *sriodev, struct dma_ch *dmadev,
			 struct srio_port_data	*port_data)
{
	int err = 0;

	switch (cmd_param.curr_cmd) {
	case SRIO_ATTR:
		err = attr_implement(sriodev, port_data);
		break;
	case SRIO_OP:
		err = op_implement(sriodev, dmadev, port_data);
		break;
	case SRIO_TEST:
		err = test_implement(sriodev, dmadev, port_data);
	}

	return err;
}

/* Init DMA pool */
static int dma_usmem_init(struct dma_pool *pool)
{
	int err;

	dma_mem_generic = dma_mem_create(DMA_MAP_FLAG_ALLOC,
					NULL, SRIO_POOL_SIZE);
	if (!dma_mem_generic) {
		err = -EINVAL;
		error(0, -err, "%s(): dma_mem_create()", __func__);
		return err;
	}

	pool->dma_virt_base = __dma_mem_memalign(64, SRIO_POOL_PORT_OFFSET);
	if (!pool->dma_virt_base) {
		err = -EINVAL;
		error(0, -err, "%s(): __dma_mem_memalign()", __func__);
		return err;
	}
	pool->dma_phys_base = __dma_mem_vtop(pool->dma_virt_base);

	return 0;
}

static int dma_pool_init(struct dma_pool **pool)
{
	struct dma_pool *dma_pool;
	int err;

	dma_pool = malloc(sizeof(*dma_pool));
	if (!dma_pool) {
		error(0, errno, "%s(): DMA pool", __func__);
		return -errno;
	}
	memset(dma_pool, 0, sizeof(*dma_pool));
	*pool = dma_pool;

	err = dma_usmem_init(dma_pool);
	if (err < 0) {
		error(0, -err, "%s(): DMA pool", __func__);
		free(dma_pool);
		return err;
	}

	return 0;
}

static void dma_pool_finish(struct dma_pool *pool)
{
	free(pool);
}

static void cmd_format_print(void)
{
	printf("-----------------SRIO APP CMD FORMAT-----------------\n");
	printf("Set window attribute\n");
	printf("sra -attr [port_id] [fun] [fun_id]");
	printf("([write_attr] [read_attr])\n");
	printf("sra -attr port1/2 device_id [id]\n");
	printf("sra -attr port1/2 target_id [id]\n");
	printf("sra -attr port1/2 seg_num [num]\n");
	printf("sra -attr port1/2 subseg_num [num]\n");
	printf("sra -attr port1/2 subseg_tdid [seg_id] [tdid]\n");
	printf("sra -attr port1/2 accept_all [id]\n");
	printf("sra -attr port1/2 irq [id]\n");
	printf("sra -attr port1/2 win_attr [id] [write_attr] [read_attr]\n");
	printf("sra -attr port1/2 seg_attr [id] [write_attr] [read_attr]\n");
	printf("\nNotes:\n");
	printf("\t[id] for command accept_all: 0 - disable; 1 - enable\n");
	printf("\t[id] for irq: 0 - disable; 1 - enable\n");
	printf("\t");
	printf("\t[write_attr]	: swrite/nwrite/nwrite_r\n");
	printf("\t[read_attr]	:\n"
	       " nread/atomic_inc/atomic_dec/atomic_set/atomic_clr\n");
	printf("\nDo sra operation\n");
	printf("sra -op [port_id] [win_id] [seg_id] [subseg_id] ");
	printf("[operation] [data_len]\n");
	printf("sra -op port1/2 [win_id] [seg_id] [subseg_id] ");
	printf("w/r/s/p [data_len]\n");
	printf("\t[data_len]	: should be less than the ");
	printf("window/segment/subsegment's size, max size is 2M\n");
	printf("\t                data_len should be 1/2/4 for ");
	printf("ATOMIC operation\n");
	printf("\nDo SRIO test and print performance result\n");
	printf("sra -test [case] [port] [task] [srio] [number]\n");
	printf(" sra -test srio port1/2 dma/core [srio_type] payload_size\n");
	printf("\t[srio_type] should be swrite/nwrite/nwrite_r/nread\n");
	printf("\tpayload_size should be less than 2M bytes\n");
	printf(" sra -test dma_chain\n");
	printf(" sra -test show_perf port1/2 times\n");
	printf(" sra -test show_task\n");
	printf(" sra -test free_task port1/2\n");
	printf("-----------------------------------------------------\n");
}

const char sra_prompt[] = "sra> ";
/* dma link data input */


int main(int argc, char *argv[])
{
	struct srio_dev *sriodev;
	struct dma_ch *dmadev;
	struct dma_pool *dmapool = NULL;
	int i, err;
	struct srio_port_data *port_data;
	int cli_argc;
	char *cli, **cli_argv;

	of_init();
	err = fsl_srio_uio_init(&sriodev);
	if (err < 0)
		error(EXIT_FAILURE, -err, "%s(): srio_uio_init()", __func__);

	port_num = fsl_srio_get_port_num(sriodev);

	memset(&cmd_param, 0, sizeof(cmd_param));
	cmd_param.port = malloc(sizeof(struct cmd_port_param) * port_num);
	if (!cmd_param.port) {
		error(0, errno, "%s(): command port", __func__);
		goto err_cmd_malloc;
	}
	memset(cmd_param.port, 0, sizeof(struct cmd_port_param));

	port_data = malloc(sizeof(struct srio_port_data) * port_num);
	if (!port_data) {
		error(0, errno, "%s(): port_data", __func__);
		goto err_cmd_malloc;
	}

	for (i = 0; i < port_num; i++) {
		fsl_srio_connection(sriodev, i);
		fsl_srio_get_port_info(sriodev, i + 1, &port_data[i].port_info,
				       &port_data[i].range_virt);
	}

	err = fsl_srio_port_connected(sriodev);
	if (err <= 0) {
		error(0, -err, "%s(): fsl_srio_port_connected", __func__);
		goto err_srio_connected;
	}

	err = dma_pool_init(&dmapool);

	for (i = 0; i < port_num; i++) {
		dma_addr_t port_phys_base =
			dmapool->dma_phys_base + SRIO_POOL_PORT_OFFSET * i;
		port_data[i].phys.write_recv_data = port_phys_base;
		port_data[i].phys.read_recv_data =
			port_phys_base + SRIO_POOL_SECT_SIZE;
		port_data[i].phys.write_data_prep =
			port_phys_base + SRIO_POOL_SECT_SIZE * 2;
		port_data[i].phys.res =
			port_phys_base + SRIO_POOL_SECT_SIZE * 3;

		port_data[i].virt = (typeof(port_data[i].virt))
			(dmapool->dma_virt_base + i * SRIO_POOL_PORT_OFFSET);
		fsl_srio_set_ibwin(sriodev, i, 1,
				   port_data[i].phys.write_recv_data,
				   SRIO_SYS_ADDR, LAWAR_SIZE_2M);
	}

	err = fsl_dma_chan_init(&dmadev, 0, 0);
	if (err < 0) {
		error(0, -err, "%s(): fsl_dma_chan_init()", __func__);
		goto err_srio_connected;
	}

	err = fsl_dma_chan_init(&dmadev1, 0, 1);
	if (err < 0) {
		error(0, -err, "%s(): fsl_dma_chan_init()", __func__);
		goto err_srio_connected;
	}

	err = fsl_dma_chan_init(&dmadev2, 1, 1);
	if (err < 0) {
		error(0, -err, "%s(): fsl_dma_chan_init()", __func__);
		goto err_srio_connected;
	}

	fsl_srio_err_handle_enable(sriodev);

	/* Run the CLI loop */
	while (1) {
		/* Get CLI input */
		cli = readline(sra_prompt);
		if (unlikely((cli == NULL) || strncmp(cli, "q", 1) == 0))
			break;
		if (cli[0] == 0) {
			free(cli);
			continue;
		}

		cli_argv = history_tokenize(cli);
		if (unlikely(cli_argv == NULL)) {
			error(EXIT_SUCCESS, 0,
			      "Out of memory while parsing: %s", cli);
			free(cli);
			continue;
		}
		for (cli_argc = 0; cli_argv[cli_argc] != NULL; cli_argc++)
			;

		add_history(cli);
		err = cmd_translate(cli_argc, cli_argv);
		if (err < 0)
			cmd_format_print();

		for (cli_argc = 0; cli_argv[cli_argc] != NULL; cli_argc++)
			free(cli_argv[cli_argc]);
		free(cli_argv);
		free(cli);

		if (err < 0)
			continue;
		fsl_dma_chan_basic_direct_init(dmadev);

		cmd_implement(sriodev, dmadev, port_data);
	}

	free(port_data);
	free(cmd_param.port);
	fsl_dma_chan_finish(dmadev);
	fsl_dma_chan_finish(dmadev1);
	fsl_dma_chan_finish(dmadev2);
	dma_pool_finish(dmapool);
	fsl_srio_uio_finish(sriodev);

	of_finish();
	return EXIT_SUCCESS;

err_srio_connected:
	free(port_data);
	free(cmd_param.port);
err_cmd_malloc:
	fsl_srio_uio_finish(sriodev);
	of_finish();

	return err;
}
