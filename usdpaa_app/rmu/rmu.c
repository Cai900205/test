/* Copyright (c) 2012 Freescale Semiconductor, Inc.
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
#include <usdpaa/fsl_rmu.h>
#include <error.h>
#include <atb_clock.h>
#include <readline.h>
#include <unistd.h>
#include <inttypes.h>

#define RMU_CMD_MIN_NUM	2
#define MSG_TEST_MAX_SIZE	12 /* power of 2, 4096 bytes (max message) */
#define RMU_POOL_SIZE	0x1000000
#define TEST_MAX_TIMES		3
#define ATTR_CMD_NUM		5
#define OP_CMD_NUM		10
#define TEST_CMD_NUM		3

static const char * const cmd_name[] = {"-attr", "-op", "-test"};
static const char * const attr_param[][3] = { {"msg0", "msg1", "dbell"},
					{"txdesc", "txbuff", "rxbuff"} };
static const char * const op_param[][5] = { {"msg0", "msg1", "dbell"},
						{"t", "r", "ar", "s", "p"} };
static const char * const test_param[][2] = {{"msg", "dbell"} };

enum rmu_cmd {
	RMU_ATTR,
	RMU_OP,
	RMU_TEST,
};

enum rmu_unit_name {
	RMU_MSG0,
	RMU_MSG1,
	RMU_DBELL
};

enum rmu_op {
	RMU_TX,
	RMU_RX,
	RMU_ARX,
	RMU_SET_MEM,
	RMU_PRI_MEM,
};

enum rmu_attr_level2_cmd {
	TXDESC,
	TXBUFF,
	RXBUFF,
};

enum test_cmd {
	TEST_MSG,
	TEST_DBELL,
};

struct cmd_param_type {
	uint32_t curr_cmd;
	uint32_t curr_unit;
	uint32_t attr_type;
	uint32_t attr_num;
	uint32_t op_type;
	uint32_t op_dest_id;
	uint8_t op_port_id;
	uint8_t op_dest_mbox;
	uint8_t op_priority;
	uint8_t op_rxthread;
	uint32_t op_data;
	size_t op_data_len;
	uint32_t test_type;
};

struct rmu_resource {
	struct rmu_unit *msg0;
	struct rmu_unit *msg1;
	struct rmu_unit *dbell;
	struct rmu_ring *msg0desc;
	struct rmu_ring *msg0txbuff;
	struct rmu_ring *msg0rxbuff;
	struct rmu_ring *msg1desc;
	struct rmu_ring *msg1txbuff;
	struct rmu_ring *msg1rxbuff;
	struct rmu_ring *dbellrxbuff;
	uint32_t msg0txbuff_slot; /* next should tx buff */
	uint32_t msg1txbuff_slot; /* next should tx buff */
	uint32_t msg0rxthread_en;
	uint32_t msg1rxthread_en;
	uint32_t dbellrxthread_en;
};

static struct cmd_param_type cmd_param;
static struct rmu_resource resource;

static int attr_param_trans(int32_t cmd_num, char **cmd_in)
{
	int32_t i;

	if (cmd_num != ATTR_CMD_NUM)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(attr_param[0]) && attr_param[0][i]; i++)
		if (!strcmp(cmd_in[2], attr_param[0][i]))
			break;

	if (i == ARRAY_SIZE(attr_param[0]) || !attr_param[0][i])
		return -EINVAL;

	cmd_param.curr_unit = i;

	for (i = 0; i < ARRAY_SIZE(attr_param[1]) && attr_param[1][i]; i++)
		if (!strcmp(cmd_in[3], attr_param[1][i]))
			break;

	if (i == ARRAY_SIZE(attr_param[1]) || !attr_param[1][i])
		return -EINVAL;

	cmd_param.attr_type = i;
	cmd_param.attr_num = strtoul(cmd_in[4], NULL, 0);

	return 0;
}

static int op_param_trans(int32_t cmd_num, char **cmd_in)
{
	int32_t i;

	if (cmd_num > OP_CMD_NUM)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(op_param[0]) && op_param[0][i]; i++)
		if (!strcmp(cmd_in[2], op_param[0][i]))
			break;

	if (i == ARRAY_SIZE(op_param[0]) || !op_param[0][i])
		return -EINVAL;

	cmd_param.curr_unit = i;

	for (i = 0; i < ARRAY_SIZE(op_param[1]) && op_param[1][i]; i++)
		if (!strcmp(cmd_in[3], op_param[1][i]))
			break;

	if (i == ARRAY_SIZE(op_param[1]) || !op_param[1][i])
		return -EINVAL;

	cmd_param.op_type = i;

	switch (i) {
	case RMU_TX:
		if (cmd_param.curr_unit == RMU_DBELL) {
			if (cmd_num != 8)
				return -EINVAL;
			cmd_param.op_port_id = strtoul(cmd_in[4], NULL, 0);
			cmd_param.op_dest_id = strtoul(cmd_in[5], NULL, 0);
			cmd_param.op_priority = strtoul(cmd_in[6], NULL, 0);
			cmd_param.op_data = strtoul(cmd_in[7], NULL, 0);
		} else {
			if (cmd_num != 9)
				return -EINVAL;
			cmd_param.op_port_id = strtoul(cmd_in[4], NULL, 0);
			cmd_param.op_dest_id = strtoul(cmd_in[5], NULL, 0);
			cmd_param.op_dest_mbox = strtoul(cmd_in[6], NULL, 0);
			cmd_param.op_priority = strtoul(cmd_in[7], NULL, 0);
			cmd_param.op_data_len = strtoul(cmd_in[8], NULL, 0);
		}
		break;
	case RMU_RX:
		if (cmd_num != 4)
			return -EINVAL;
		break;
	case RMU_ARX:
		if (cmd_num != 5)
			return -EINVAL;
		cmd_param.op_rxthread = strtoul(cmd_in[4], NULL, 0);
		break;
	case RMU_SET_MEM:
		if (cmd_num != 6)
			return -EINVAL;
		cmd_param.op_data = strtoul(cmd_in[4], NULL, 0);
		cmd_param.op_data_len = strtoul(cmd_in[5], NULL, 0);
		break;
	case RMU_PRI_MEM:
		if (cmd_num != 5)
			return -EINVAL;
		cmd_param.op_data_len = strtoul(cmd_in[4], NULL, 0);
		break;
	}

	return 0;
}

static int test_param_trans(int32_t cmd_num, char **cmd_in)

{
	int32_t i;

	if (cmd_num != TEST_CMD_NUM)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(test_param[0]) && test_param[0][i]; i++)
		if (!strcmp(cmd_in[2], test_param[0][i]))
			break;

	if (i == ARRAY_SIZE(test_param[0]) || !test_param[0][i])
		return -EINVAL;

	cmd_param.test_type = i;

	if (cmd_param.test_type == TEST_MSG)
		cmd_param.curr_unit = RMU_MSG0;
	else
		cmd_param.curr_unit = RMU_DBELL;

	return 0;
}

static int cmd_translate(int32_t cmd_num, char **cmd_in)
{
	int i, err = 0;

	if (cmd_num < RMU_CMD_MIN_NUM)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(cmd_name) && cmd_name[i]; i++)
		if (!strcmp(cmd_in[1], cmd_name[i]))
			break;

	if (i == ARRAY_SIZE(cmd_name) || !cmd_name[i])
		return -EINVAL;

	cmd_param.curr_cmd = i;

	switch (i) {
	case RMU_ATTR:
		err = attr_param_trans(cmd_num, cmd_in);
		break;
	case RMU_OP:
		err = op_param_trans(cmd_num, cmd_in);
		break;
	case RMU_TEST:
		err = test_param_trans(cmd_num, cmd_in);
		break;
	}

	return err;
}

static int fsl_rmu_msg_send(struct rmu_unit *unit, struct rmu_ring *desc,
					struct rmu_ring *txbuff, uint32_t slot)
{
	int err = 0;
	struct msg_tx_info *info;

	info = malloc(sizeof(struct msg_tx_info));
	if (!info) {
		error(0, errno, "%s(): malloc", __func__);
		return -errno;
	}

	info->port = cmd_param.op_port_id;
	info->destid = cmd_param.op_dest_id;
	info->mbox = cmd_param.op_dest_mbox;
	info->priority = cmd_param.op_priority;
	info->virt_buffer = txbuff->virt + slot * txbuff->cell_size;
	info->phys_buffer = txbuff->phys + slot * txbuff->cell_size;
	info->len = cmd_param.op_data_len;

	err = fsl_add_outb_msg(unit, desc, info);

	free(info);

	return err;
}

static int fsl_rmu_msg_receive(struct rmu_unit *unit, struct rmu_ring *ring)
{
	int err = 0;
	uint32_t i;
	uint32_t *info;

	info = malloc(ring->cell_size);
	if (!info) {
		error(0, errno, "%s(): malloc", __func__);
		return -errno;
	}

	err = fsl_rmu_msg_inb_handler(unit, (void *)info, ring);
	if (err < 0)
		printf("\tmessage fetch failed!\n");
	else {
		printf("--processing message:\n");
		for (i = 0; i < ring->cell_size/4; i += 4)
			printf(" %08x %08x %08x %08x\n",
				*(info + i), *(info + i + 1),
				*(info + i + 2), *(info + i + 3));
	}
	free(info);

	return err;
}

static int fsl_rmu_dbell_receive(struct rmu_unit *unit,
				struct rmu_ring *ring)
{
	int err = 0;
	struct dbell_info *info;

	info = malloc(sizeof(struct dbell_info));
	if (!info) {
		error(0, errno, "%s(): malloc", __func__);
		return -errno;
	}

	err = fsl_rmu_dbell_inb_handler(unit, (void *)info, ring);
	if (err < 0)
		printf("\tdootbell fetch failed!\n");
	else
		printf("--processing doorbell,"
			" sid %2.2x tid %2.2x info %4.4x\n",
			info->sid,
			info->tid,
			info->info);

	free(info);

	return err;
}

static int fsl_rmu_msg_txbuff_set(struct rmu_ring *ring,
				uint32_t slot, uint8_t data, uint32_t len)
{
	uint32_t i;
	uint8_t *setbuff;

	printf("\ttxbuff %d; virt addr:%p; phy addr:%"PRIu64"\n", slot,
			(ring->virt + slot * ring->cell_size),
			(ring->phys + slot * ring->cell_size));
	setbuff = (uint8_t *)(ring->virt + slot * ring->cell_size);
	for (i = 0; i < len; i++)
		*(setbuff + i) = data;

	printf("\tSlot %d tx buff set down.\n ", slot);
	return 0;
}

static int fsl_rmu_msg_txbuff_print(struct rmu_ring *ring,
				uint32_t slot, uint32_t len)
{
	uint32_t i;
	uint32_t *data;

	printf("\ttxbuff %d; virt addr:%p; phy addr:%x\n", slot,
			(ring->virt + slot * ring->cell_size),
			(uint32_t)(ring->phys + slot * ring->cell_size));
	data = (uint32_t *)(ring->virt + slot * ring->cell_size);
	for (i = 0; i < len/4; i += 4) {
		printf("\t%p:", (data + i));
		printf(" %08x %08x %08x %08x\n",
			*(data + i), *(data + i + 1),
			*(data + i + 2), *(data + i + 3));
	}

	return 0;
}

static void *t_dbell_rx_handler(void *arg)
{
	int err = 0;
	uint32_t i = 0;
	struct dbell_info *info;

	info = malloc(sizeof(struct dbell_info));
	if (!info) {
		error(0, errno, "%s(): malloc", __func__);
		pthread_exit(NULL);
	}

	while (1) {
		err = fsl_rmu_dbell_inb_handler(resource.dbell,
			(void *)info, resource.dbellrxbuff);
		if (!err) {
			i++;
			printf("--doorbell%d:"
				" sid %2.2x tid %2.2x info %4.4x\n",
				i,
				info->sid,
				info->tid,
				info->info);
		}
		if (!resource.dbellrxthread_en)
			break;
	}
	free(info);

	pthread_exit(NULL);
}

static int fsl_rmu_dbell_receive_thread(void)
{
	int ret = 0;
	pthread_t dbell_rx_id;

	if (cmd_param.op_rxthread) {
		if (resource.dbellrxthread_en)
			printf("already exits!\n");
		else {
			ret = pthread_create(&dbell_rx_id, NULL,
				t_dbell_rx_handler, NULL);
			if (ret) {
				error(0, errno, "create failed!\n");
				return -errno;
			} else {
				printf("create success!\n");
				resource.dbellrxthread_en = 1;
			}
		}
	} else {
		resource.dbellrxthread_en = 0;
		printf("released\n");
	}

	return 0;
}

static void *t_msg_rx_handler(void *arg)
{
	int err = 0;
	uint32_t i;
	uint32_t j = 0;
	uint32_t *info, *rxthread_en;
	struct rmu_unit *unit;
	struct rmu_ring *ring;

	if (cmd_param.curr_unit) {
		unit = resource.msg1;
		ring = resource.msg1rxbuff;
		rxthread_en = &resource.msg1rxthread_en;
	} else {
		unit = resource.msg0;
		ring = resource.msg0rxbuff;
		rxthread_en = &resource.msg0rxthread_en;
	}

	info = malloc(ring->cell_size);
	if (!info) {
		error(0, errno, "%s(): malloc", __func__);
		pthread_exit(NULL);
	}

	while (1) {
		err = fsl_rmu_msg_inb_handler(unit, (void *)info, ring);
		if (!err) {
			j++;
			printf("--message%d:\n", j);
			for (i = 0; i < ring->cell_size/4; i += 4)
				printf(" %08x %08x %08x %08x\n",
					*(info + i), *(info + i + 1),
					*(info + i + 2), *(info + i + 3));
		}
		if (!*rxthread_en)
			break;
	}
	free(info);

	pthread_exit(NULL);
}

static int fsl_rmu_msg_receive_thread(uint32_t *rxthread_en)
{
	int ret = 0;
	pthread_t msg_rx_id;

	if (cmd_param.op_rxthread) {
		if (*rxthread_en)
			printf("already exits!\n");
		else {
			ret = pthread_create(&msg_rx_id, NULL,
				t_msg_rx_handler, NULL);
			if (ret) {
				error(0, errno, "create failed!\n");
				return -errno;
			} else {
				printf("create success!\n");
				*rxthread_en = 1;
			}
		}
	} else {
		*rxthread_en = 0;
		printf("released\n");
	}

	return 0;
}

static int op_implement(void)
{
	int err = 0;
	uint32_t unit_id = cmd_param.curr_unit;

	switch (cmd_param.op_type) {
	case RMU_TX:
		if (unit_id == RMU_DBELL) {
			err = fsl_rmu_dbell_send(resource.dbell,
					cmd_param.op_port_id,
					cmd_param.op_dest_id,
					cmd_param.op_priority,
					cmd_param.op_data);
		} else if (unit_id == RMU_MSG0) {
			err = fsl_rmu_msg_send(resource.msg0, resource.msg0desc,
						resource.msg0txbuff,
						resource.msg0txbuff_slot);
			/* move tx buffer slot to next buff */
			resource.msg0txbuff_slot++;
			if (resource.msg0txbuff_slot ==
				resource.msg0txbuff->entries)
				resource.msg0txbuff_slot = 0;
		} else {
			err = fsl_rmu_msg_send(resource.msg1, resource.msg1desc,
						resource.msg1txbuff,
						resource.msg1txbuff_slot);
			/* move tx buffer slot to next buff */
			resource.msg1txbuff_slot++;
			if (resource.msg1txbuff_slot ==
				resource.msg1txbuff->entries)
				resource.msg1txbuff_slot = 0;
		}
		break;
	case RMU_RX:
		if (unit_id == RMU_DBELL) {
			printf("OP: next available rx doorbell:\n");
			err =  fsl_rmu_dbell_receive(resource.dbell,
						resource.dbellrxbuff);
		} else if (unit_id == RMU_MSG0) {
			printf("OP: next available rx message for msg0:\n");
			err = fsl_rmu_msg_receive(resource.msg0,
					resource.msg0rxbuff);
		} else {
			printf("OP: next available rx message for msg1:\n");
			err = fsl_rmu_msg_receive(resource.msg1,
					resource.msg1rxbuff);
		}
		break;
	case RMU_ARX:
		if (unit_id == RMU_DBELL) {
			printf("OP: doorbell rx thread  ...");
			err =  fsl_rmu_dbell_receive_thread();
		} else if (unit_id == RMU_MSG0) {
			printf("OP: msg0 rx thread ...");
			err = fsl_rmu_msg_receive_thread(
				&resource.msg0rxthread_en);
		} else {
			printf("OP: msg1 rx thread ...");
			err = fsl_rmu_msg_receive_thread(
				&resource.msg1rxthread_en);
		}
		break;
	case RMU_SET_MEM:
		if (unit_id == RMU_DBELL) {
			printf("OP: Doorbell has no Tx buff can be set!\n");
		} else if (unit_id == RMU_MSG0) {
			printf("OP: msg0 NEXT tx buffer SET, slot:%d\n",
						resource.msg0txbuff_slot);
			err = fsl_rmu_msg_txbuff_set(resource.msg0txbuff,
						resource.msg0txbuff_slot,
						cmd_param.op_data,
						cmd_param.op_data_len);
		} else {
			printf("OP: msg1 NEXT tx buffer SET, slot:%d\n",
						resource.msg1txbuff_slot);
			err = fsl_rmu_msg_txbuff_set(resource.msg1txbuff,
						resource.msg1txbuff_slot,
						cmd_param.op_data,
						cmd_param.op_data_len);
		}
		break;
	case RMU_PRI_MEM:
		if (unit_id == RMU_DBELL) {
			printf("OP: Doorbell has no Tx buff can be printed!\n");
		} else if (unit_id == RMU_MSG0) {
			printf("OP: msg0 NEXT tx buffer PRINT, slot:%d\n",
						resource.msg0txbuff_slot);
			err = fsl_rmu_msg_txbuff_print(resource.msg0txbuff,
						resource.msg0txbuff_slot,
						cmd_param.op_data_len);
		} else {
			printf("OP: msg1 NEXT tx buffer PRINT, slot:%d\n",
						resource.msg1txbuff_slot);
			err = fsl_rmu_msg_txbuff_print(resource.msg1txbuff,
						resource.msg1txbuff_slot,
						cmd_param.op_data_len);
		}
		break;
	}

	return err;
}

static int fsl_rmu_txdesc_print(uint32_t unit)
{
	uint32_t slot;
	struct rmu_ring *desc;
	uint32_t i, j;
	uint32_t *data;

	slot = cmd_param.attr_num;

	if (unit == RMU_DBELL) {
		printf("ATTR: Doorbell has no Tx Desc can be printed!\n");
		return -EINVAL;
	} else if (unit == RMU_MSG0) {
		desc = resource.msg0desc;
	} else {
		desc = resource.msg1desc;
	}
	if (desc == NULL) {
		printf("ATTR: Msg%d has no availableTx Desc!\n", unit);
		return -EINVAL;
	} else {
		printf("ATTR: Msg%d Tx Desc ......\n", unit);
		printf("\tcell size:%dbytes; entries:%d\n", desc->cell_size,
					desc->entries);
		printf("\tbase virt addr:%p; base phy addr:%"PRIu64"\n",
					desc->virt,
					desc->phys);
		printf("\n");
		if (slot >= desc->entries) {
			printf("\tall the desc:\n");
			for (j = 0; j < desc->entries; j++) {
				printf("\tdesc %d; virt addr:%p; phy addr:%"PRIu64"\n",
					j,
					(desc->virt + j * desc->cell_size),
					(desc->phys + j * desc->cell_size));
				data = (uint32_t *)(desc->virt
					+ j * desc->cell_size);
				for (i = 0; i < desc->cell_size/4; i += 4) {
					printf("\t%p:", (data + i));
					printf(" %08x %08x %08x %08x\n",
						*(data + i), *(data + i + 1),
						*(data + i + 2),
						*(data + i + 3));
				}
			}
		} else {
			printf("\tdesc %d; virt addr:%p; phy addr:%"PRIu64"\n",
			       slot, (desc->virt + slot * desc->cell_size),
			       (desc->phys + slot * desc->cell_size));
			data = (uint32_t *)(desc->virt
				+ slot * desc->cell_size);
			for (i = 0; i < desc->cell_size/4; i += 4) {
				printf("\t%p:", (data + i));
				printf(" %08x %08x %08x %08x\n",
					*(data + i), *(data + i + 1),
					*(data + i + 2), *(data + i + 3));
			}
		}
	}

	return 0;
}

static int fsl_rmu_txbuff_print(uint32_t unit)
{
	uint32_t slot;
	struct rmu_ring *txbuff;
	uint32_t i, j;
	uint32_t *data;

	slot = cmd_param.attr_num;

	if (unit == RMU_DBELL) {
		printf("ATTR: Doorbell has no Tx buffer can be printed!\n");
		return -EINVAL;
	} else if (unit == RMU_MSG0) {
		txbuff = resource.msg0txbuff;
		printf("ATTR: Msg0 NEXT Tx buffer slot:%d\n",
					resource.msg0txbuff_slot);
	} else {
		txbuff = resource.msg1txbuff;
		printf("ATTR: Msg1 NEXT Tx buffer slot:%d\n",
					resource.msg1txbuff_slot);
	}
	if (txbuff == NULL) {
		printf("ATTR: Msg%d has no availableTx buffer!\n", unit);
		return -EINVAL;
	} else {
		printf("ATTR: Msg%d Tx buffer ......\n", unit);
		printf("\tcell size:%dbytes; entries:%d\n", txbuff->cell_size,
					txbuff->entries);
		printf("\tbase virt addr:%p; base phy addr:%"PRIu64"\n",
			txbuff->virt, txbuff->phys);
		printf("\n");
		if (slot >= txbuff->entries) {
			printf("\tall the Tx buffer:\n");
			for (j = 0; j < txbuff->entries; j++) {
				printf("\ttxbuff %d; virt addr:%p;"
				       "phy addr:%"PRIu64"\n", j,
					(txbuff->virt + j * txbuff->cell_size),
					(txbuff->phys + j * txbuff->cell_size));
				data = (uint32_t *)(txbuff->virt
					+ j * txbuff->cell_size);
				for (i = 0; i < txbuff->cell_size/4; i += 4) {
					printf("\t%p:", (data + i));
					printf(" %08x %08x %08x %08x\n",
						*(data + i), *(data + i + 1),
						*(data + i + 2),
						*(data + i + 3));
				}
			}
		} else {
			printf("\ttxbuff %d; virt addr:%p; phy addr:%"PRIu64"\n", slot,
					(txbuff->virt + slot * txbuff->cell_size),
					(txbuff->phys + slot * txbuff->cell_size));
			data = (uint32_t *)(txbuff->virt
				+ slot * txbuff->cell_size);
			for (i = 0; i < txbuff->cell_size/4; i += 4) {
				printf("\t%p:", (data + i));
				printf(" %08x %08x %08x %08x\n",
					*(data + i), *(data + i + 1),
					*(data + i + 2), *(data + i + 3));
			}
		}
	}

	return 0;
}

static int fsl_rmu_rxbuff_print(uint32_t unit)
{
	uint32_t slot;
	struct rmu_ring *rxbuff;
	uint32_t i, j;
	uint32_t *data;

	slot = cmd_param.attr_num;

	if (unit == RMU_DBELL) {
		rxbuff = resource.dbellrxbuff;
		if (rxbuff == NULL) {
			printf("ATTR: DBELL has no available Rx buffer!\n");
			return -EINVAL;
		} else
			printf("ATTR: DBELL Rx buffer ......\n");
	} else if (unit == RMU_MSG0) {
		rxbuff = resource.msg0rxbuff;
		if (rxbuff == NULL) {
			printf("ATTR: Msg0 has no available Rx buffer!\n");
			return -EINVAL;
		} else
			printf("ATTR: Msg0 Rx buffer ......\n");
	} else {
		rxbuff = resource.msg1rxbuff;
		if (rxbuff == NULL) {
			printf("ATTR: Msg1 has no available Rx buffer!\n");
			return -EINVAL;
		} else
			printf("ATTR: Msg1 Rx buffer ......\n");
	}
	printf("\tcell size:%dbytes; entries:%d\n",
				rxbuff->cell_size, rxbuff->entries);
	printf("\tbase virt addr:%p; base phy addr:%"PRIu64"\n",
				rxbuff->virt, rxbuff->phys);
	printf("\n");
	if (slot >= rxbuff->entries) {
		printf("\tall the Rx buffer:\n");
		for (j = 0; j < rxbuff->entries; j++) {
			printf("\trxbuff %d; virt addr:%p; phy addr:%"PRIu64"\n", j,
					(rxbuff->virt + j * rxbuff->cell_size),
					(rxbuff->phys + j * rxbuff->cell_size));
			data = (uint32_t *)(rxbuff->virt
				+ j * rxbuff->cell_size);
			for (i = 0; i < rxbuff->cell_size/4; i += 4) {
				printf("\t%p:", (data + i));
				printf(" %08x %08x %08x %08x\n",
					*(data + i), *(data + i + 1),
					*(data + i + 2), *(data + i + 3));
			}
		}
	} else {
		printf("\trxbuff %d; virt addr:%p; phy addr:%"PRIu64"\n", slot,
			(rxbuff->virt + slot * rxbuff->cell_size),
			(rxbuff->phys + slot * rxbuff->cell_size));
		data = (uint32_t *)(rxbuff->virt + slot * rxbuff->cell_size);
		for (i = 0; i < rxbuff->cell_size/4; i += 4) {
			printf("\t%p:", (data + i));
			printf(" %08x %08x %08x %08x\n",
				*(data + i), *(data + i + 1),
				*(data + i + 2), *(data + i + 3));
		}
	}

	return 0;
}

static int attr_implement(void)
{
	int32_t err = 0;
	uint32_t unit_id = cmd_param.curr_unit;

	switch (cmd_param.attr_type) {
	case TXDESC:
		err = fsl_rmu_txdesc_print(unit_id);
		break;
	case TXBUFF:
		err = fsl_rmu_txbuff_print(unit_id);
		break;
	case RXBUFF:
		err = fsl_rmu_rxbuff_print(unit_id);
		break;
	default:
		err = -EINVAL;
	}

	return err;
}

static int rmu_msg_test(void)
{
	struct atb_clock *atb_clock;
	uint64_t atb_multiplier = 0;
	int err = 0;
	uint32_t j, k;
	struct msg_tx_info *info;

	info = malloc(sizeof(struct msg_tx_info));
	if (!info)
		return -errno;

	atb_clock = malloc(sizeof(struct atb_clock));
	if (!atb_clock) {
		free(info);
		return -errno;
	}

	atb_multiplier = atb_get_multiplier();

	printf("TEST: msg0 performance test ......\n");

	fsl_rmu_msg_txbuff_set(resource.msg0txbuff, 0,
					90, 4096);
	info->port = 0;
	info->destid = 0;
	info->mbox = 0;
	info->priority = 0;
	info->virt_buffer = resource.msg0txbuff->virt;
	info->phys_buffer = resource.msg0txbuff->phys;

	/* msg test data is from 8 bytes to 4K bytes*/
	for (j = 3; j <= MSG_TEST_MAX_SIZE; j++) {
		atb_clock_init(atb_clock);
		atb_clock_reset(atb_clock);
		info->len = 1 << j;
		for (k = 0; k < TEST_MAX_TIMES; k++) {
			fsl_msg_send_err_clean(resource.msg0);
			atb_clock_start(atb_clock);
			fsl_add_outb_msg(resource.msg0,
				resource.msg0desc, info);

			err = fsl_msg_send_wait(resource.msg0);
			if (err < 0) {
				error(0, -err,
					  "msg0 transmission failed");
				atb_clock_finish(atb_clock);
				free(atb_clock);
				free(info);
				return err;
			}
			atb_clock_stop(atb_clock);
		}
		printf("length(byte): %-15u time(us): %-15f"
			   "avg Gb/s: %-15f max Gb/s: %-15f\n", (1 << j),
			   atb_to_seconds(atb_clock_total(atb_clock),
					  atb_multiplier) / TEST_MAX_TIMES
			   * ATB_MHZ,
			   (1 << j) * 8 * TEST_MAX_TIMES /
			   (atb_to_seconds(atb_clock_total(atb_clock),
					   atb_multiplier) * 1000000000.0),
			   (1 << j) * 8 /
			   (atb_to_seconds(atb_clock_min(atb_clock),
					   atb_multiplier) * 1000000000.0));
		atb_clock_finish(atb_clock);
	}

	free(info);
	free(atb_clock);

	return 0;
}

static int rmu_dbell_test(void)
{
	struct atb_clock *atb_clock;
	uint64_t atb_multiplier = 0;
	int err = 0;
	uint32_t k;

	atb_clock = malloc(sizeof(struct atb_clock));
	if (!atb_clock)
		return -errno;

	atb_multiplier = atb_get_multiplier();

	printf("TEST: dbell performance test ......\n");

	atb_clock_init(atb_clock);
	atb_clock_reset(atb_clock);
	for (k = 0; k < TEST_MAX_TIMES; k++) {
		fsl_dbell_send_err_clean(resource.dbell);
		atb_clock_start(atb_clock);
		fsl_rmu_dbell_send(resource.dbell, 0, 0, 0, 0x5a5a);

		err = fsl_dbell_send_wait(resource.dbell);
		if (err < 0) {
			error(0, -err,
				  "dbell transmission failed");
			atb_clock_finish(atb_clock);
			free(atb_clock);
			return err;
		}
		atb_clock_stop(atb_clock);
	}
	printf("length(byte): %-15u time(us): %-15f"
		   "avg Gb/s: %-15f max Gb/s: %-15f\n", 2,
		   atb_to_seconds(atb_clock_total(atb_clock),
				  atb_multiplier) / TEST_MAX_TIMES
		   * ATB_MHZ,
		   2 * 8 * TEST_MAX_TIMES /
		   (atb_to_seconds(atb_clock_total(atb_clock),
				   atb_multiplier) * 1000000000.0),
		   2 * 8 /
		   (atb_to_seconds(atb_clock_min(atb_clock),
				   atb_multiplier) * 1000000000.0));
	atb_clock_finish(atb_clock);

	free(atb_clock);

	return 0;
}

static int test_implement(void)
{
	if (cmd_param.test_type == TEST_MSG)
		rmu_msg_test();
	else if (cmd_param.test_type == TEST_DBELL)
		rmu_dbell_test();

	return 0;
}

static int cmd_implement(void)
{
	int err = 0;

	switch (cmd_param.curr_cmd) {
	case RMU_ATTR:
		err = attr_implement();
		break;
	case RMU_OP:
		err = op_implement();
		break;
	case RMU_TEST:
		err = test_implement();
	}

	return err;
}

/* Init DMA pool */
static int dma_usmem_init(struct rmu_ring **pool, size_t align, size_t size)
{
	struct rmu_ring *dma_pool;
	int err;

	dma_pool = malloc(sizeof(*dma_pool));
	if (!dma_pool) {
		error(0, errno, "%s(): DMA pool", __func__);
		return -errno;
	}
	memset(dma_pool, 0, sizeof(*dma_pool));

	dma_pool->virt = __dma_mem_memalign(align, size);
	if (!dma_pool->virt) {
		err = -EINVAL;
		error(0, -err, "%s(): __dma_mem_memalign()", __func__);
		free(dma_pool);
		return err;
	}
	dma_pool->phys = __dma_mem_vtop(dma_pool->virt);
	memset(dma_pool->virt, 0, size);

	*pool = dma_pool;

	return 0;
}

static int dma_pool_init(void)
{
	int err;

	dma_mem_generic = dma_mem_create(DMA_MAP_FLAG_ALLOC,
					NULL, RMU_POOL_SIZE);
	if (!dma_mem_generic) {
		err = -EINVAL;
		error(0, -err, "%s(): dma_mem_create()", __func__);
		return err;
	}

	return 0;
}

static void dma_pool_finish(struct rmu_ring *pool)
{
	free(pool);
}

static void cmd_format_print(void)
{
	printf("-----------------RMU APP CMD FORMAT-----------------\n");
	printf("--Set Rmu Attribute--\n");
	printf("rmu -attr [unit_name] [fun] [slot]\n");
	printf("rmu -attr msg0/msg1/dbell  txdesc [slot]\n");
	printf("rmu -attr msg0/msg1/dbell  txbuff [slot]\n");
	printf("rmu -attr msg0/msg1/dbell  rxbuff [slot]\n");
	printf("\t-[unit_name]: msg0/msg1/dbell\n");
	printf("\t-[slot]: the slot of the buffer in the ring\n");
	printf("\n--Do Rmu Operation--\n");
	printf("rmu -op [unit_name] [operation] [port_id] [dest_id] ");
	printf("[dest_mbox] [priority] [data] [data_len]\n");
	printf("rmu -op msg0/msg1/dbell t/r/ar/s/p [port_id] [dest_id] ");
	printf("[dest_mbox] [priority] [data] [data_len]\n");
	printf("\t-[operation]: t---tx a message or doorbell\n");
	printf("\t	r---rx a message or doorbell and print\n");
	printf("\t	ar---create a rx thread for the unit and print\n");
	printf("\t	s---set next tx message buffer\n");
	printf("\t	p---print next tx local buffer\n ");
	printf("\t-[data]: for message should be u8 size\n");
	printf("\t	for dbell should be u16 size\n");
	printf("\t-[data_len]: for message should be 8~4096 bytes\n");
	printf("\t	for dbell should be equal 2 bytes\n");
	printf("\n--Do Rmu Test and Print Performance Result--\n");
	printf("rmu -test [case_name]\n");
	printf("\t-[case_name]: msg/dbell\n");
	printf("-----------------------------------------------------\n");
}

const char rmu_prompt[] = "rmu> ";

int main(int argc, char *argv[])
{
	int err;
	int cli_argc;
	char *cli, **cli_argv;

	of_init();
	dma_pool_init();

	/* msg0 uio init */
	err = fsl_rmu_unit_uio_init(&resource.msg0, RMU_UNIT_MSG0);
	if (err < 0) {
		error(EXIT_FAILURE, -err,
			"%s(): rmu_unit_uio_init()", __func__);
		resource.msg0 = NULL;
	} else {
		printf("RMU: msg0 uio initialized.\n");
		/* init msg0desc for tx */
		err = dma_usmem_init(&resource.msg0desc,
				MSG_TX_DESC_RING_ENTRY * MSG_DESC_SIZE,
				MSG_TX_DESC_RING_ENTRY * MSG_DESC_SIZE);
		if (err < 0) {
			printf("\tmsg0desc ring init failed!\n");
			resource.msg0desc = NULL;
		} else {
			resource.msg0desc->cell_size = MSG_DESC_SIZE;
			resource.msg0desc->entries = MSG_TX_DESC_RING_ENTRY;
			fsl_rmu_msg_outb_init(resource.msg0, resource.msg0desc);
			printf("     -msg0desc ...\n");
			printf("\tcell size:%dbytes; entries:%d\n",
						resource.msg0desc->cell_size,
						resource.msg0desc->entries);
			printf("\tbase virt addr:%p; base phy addr:%"PRIu64"\n",
					resource.msg0desc->virt,
					resource.msg0desc->phys);
		}

		/* init msg0txbuff for tx */
		err = dma_usmem_init(&resource.msg0txbuff,
				MSG_TX_BUFF_RING_ENTRY * MSG_FRAME_SIZE,
				MSG_TX_BUFF_RING_ENTRY * MSG_FRAME_SIZE);
		if (err < 0) {
			printf("\tmsg0txbuff ring init failed!\n");
			resource.msg0txbuff = NULL;
		} else {
			resource.msg0txbuff->cell_size = MSG_FRAME_SIZE;
			resource.msg0txbuff->entries = MSG_TX_BUFF_RING_ENTRY;
			printf("     -msg0txbuff ...\n");
			printf("\tcell size:%dbytes; entries:%d\n",
						resource.msg0txbuff->cell_size,
						resource.msg0txbuff->entries);
			printf("\tbase virt addr:%p; base phy addr:%"PRIu64"\n",
					resource.msg0txbuff->virt,
					resource.msg0txbuff->phys);
		}

		/* init msg0rxbuff for rx */
		err = dma_usmem_init(&resource.msg0rxbuff,
				MSG_RX_BUFF_RING_ENTRY * MSG_FRAME_SIZE,
				MSG_RX_BUFF_RING_ENTRY * MSG_FRAME_SIZE);
		if (err < 0) {
			printf("\tmsg0rxbuff ring init failed!\n");
			resource.msg0rxbuff = NULL;
		} else {
			resource.msg0rxbuff->cell_size = MSG_FRAME_SIZE;
			resource.msg0rxbuff->entries = MSG_RX_BUFF_RING_ENTRY;
			fsl_rmu_msg_inb_init(resource.msg0,
				resource.msg0rxbuff);
			printf("     -msg0rxbuff ...\n");
			printf("\tcell size:%dbytes; entries:%d\n",
						resource.msg0rxbuff->cell_size,
						resource.msg0rxbuff->entries);
			printf("\tbase virt addr:%p; base phy addr:%"PRIu64"\n",
					resource.msg0rxbuff->virt,
					resource.msg0rxbuff->phys);
		}
	}

	/* msg1 uio init */
	err = fsl_rmu_unit_uio_init(&resource.msg1, RMU_UNIT_MSG1);
	if (err < 0) {
		error(EXIT_FAILURE, -err,
			"%s(): rmu_unit_uio_init()", __func__);
		resource.msg1 = NULL;
	} else {
		printf("RMU: msg1 uio initialized.\n");
		/* init msg1desc for tx */
		err = dma_usmem_init(&resource.msg1desc,
				MSG_TX_DESC_RING_ENTRY * MSG_DESC_SIZE,
				MSG_TX_DESC_RING_ENTRY * MSG_DESC_SIZE);
		if (err < 0) {
			printf("\tmsg1desc ring init failed!\n");
			resource.msg1desc = NULL;
		} else {
			resource.msg1desc->cell_size = MSG_DESC_SIZE;
			resource.msg1desc->entries = MSG_TX_DESC_RING_ENTRY;
			fsl_rmu_msg_outb_init(resource.msg1, resource.msg1desc);
			printf("     -msg1desc ...\n");
			printf("\tcell size:%dbytes; entries:%d\n",
						resource.msg1desc->cell_size,
						resource.msg1desc->entries);
			printf("\tbase virt addr:%p; base phy addr:%"PRIu64"\n",
					resource.msg1desc->virt,
					resource.msg1desc->phys);
		}

		/* init msg1txbuff for tx */
		err = dma_usmem_init(&resource.msg1txbuff,
				MSG_TX_BUFF_RING_ENTRY * MSG_FRAME_SIZE,
				MSG_TX_BUFF_RING_ENTRY * MSG_FRAME_SIZE);
		if (err < 0) {
			printf("\tmsg1txbuff ring init failed!\n");
			resource.msg1txbuff = NULL;
		} else {
			resource.msg1txbuff->cell_size = MSG_FRAME_SIZE;
			resource.msg1txbuff->entries = MSG_TX_BUFF_RING_ENTRY;
			printf("     -msg1txbuff ...\n");
			printf("\tcell size:%dbytes; entries:%d\n",
						resource.msg1txbuff->cell_size,
						resource.msg1txbuff->entries);
			printf("\tbase virt addr:%p; base phy addr:%"PRIu64"\n",
					resource.msg1txbuff->virt,
					resource.msg1txbuff->phys);
		}

		/* init msg1rxbuff for rx */
		err = dma_usmem_init(&resource.msg1rxbuff,
				MSG_RX_BUFF_RING_ENTRY * MSG_FRAME_SIZE,
				MSG_RX_BUFF_RING_ENTRY * MSG_FRAME_SIZE);
		if (err < 0) {
			printf("\tmsg1rxbuff ring init failed!\n");
			resource.msg1rxbuff = NULL;
		} else {
			resource.msg1rxbuff->cell_size = MSG_FRAME_SIZE;
			resource.msg1rxbuff->entries = MSG_RX_BUFF_RING_ENTRY;
			fsl_rmu_msg_inb_init(resource.msg1,
					resource.msg1rxbuff);
			printf("     -msg1rxbuff ...\n");
			printf("\tcell size:%dbytes; entries:%d\n",
						resource.msg1rxbuff->cell_size,
						resource.msg1rxbuff->entries);
			printf("\tbase virt addr:%p; base phy addr:%"PRIu64"\n",
					resource.msg1rxbuff->virt,
					resource.msg1rxbuff->phys);
		}
	}

	/* dbell uio init */
	err = fsl_rmu_unit_uio_init(&resource.dbell, RMU_UNIT_DBELL);
	if (err < 0) {
		error(EXIT_FAILURE, -err,
			"%s(): rmu_unit_uio_init()", __func__);
		resource.dbell = NULL;
	} else {
		printf("RMU: doorbell uio initialized.\n");
		/* init dbellrxbuff for rx */
		err = dma_usmem_init(&resource.dbellrxbuff,
				DBELL_RX_BUFF_RING_ENTRY * DBELL_FRAME_SIZE,
				DBELL_RX_BUFF_RING_ENTRY * DBELL_FRAME_SIZE);
		if (err < 0) {
			printf("\tdbellrxbuff ring init failed!\n");
			resource.dbellrxbuff = NULL;
		} else {
			resource.dbellrxbuff->cell_size =
				DBELL_FRAME_SIZE;
			resource.dbellrxbuff->entries =
				DBELL_RX_BUFF_RING_ENTRY;
			fsl_rmu_dbell_inb_init(resource.dbell,
				resource.dbellrxbuff);
			printf("     -dbellrxbuff ...\n");
			printf("\tcell size:%dbytes; entries:%d\n",
					resource.dbellrxbuff->cell_size,
					resource.dbellrxbuff->entries);
			printf("\tbase virt addr:%p; base phy addr:%"PRIu64"\n",
					resource.dbellrxbuff->virt,
					resource.dbellrxbuff->phys);
		}
	}

	/* Run the CLI loop */
	while (1) {
		/* Get CLI input */
		cli = readline(rmu_prompt);
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

		cmd_implement();
	}

	dma_pool_finish(resource.msg0desc);
	dma_pool_finish(resource.msg0txbuff);
	dma_pool_finish(resource.msg0rxbuff);
	dma_pool_finish(resource.msg1desc);
	dma_pool_finish(resource.msg1txbuff);
	dma_pool_finish(resource.msg1rxbuff);
	dma_pool_finish(resource.dbellrxbuff);
	fsl_rmu_unit_uio_finish(resource.msg0);
	fsl_rmu_unit_uio_finish(resource.msg1);
	fsl_rmu_unit_uio_finish(resource.dbell);

	of_finish();
	return EXIT_SUCCESS;
}
