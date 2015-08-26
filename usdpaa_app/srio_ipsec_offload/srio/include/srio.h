/* Copyright (c) 2014 Freescale Semiconductor, Inc.
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

#include <internal/compat.h>
#include <usdpaa/dma_mem.h>
#include <usdpaa/fsl_dma.h>
#include <usdpaa/fsl_srio.h>
#include <error.h>
#include <atb_clock.h>
#define SRIO_SYS_ADDR		0x10000000	/* used for srio system addr */
#define SRIO_WIN_SIZE		0x200000
#define SRIO_INPUT_CMD_NUM	6
#define SRIO_CMD_MIN_NUM	2
#define SRIO_TEST_DATA_NUM	21
#define SRIO_POOL_PORT_SECT_NUM	4
#define SRIO_POOL_PORT_OFFSET\
	(SRIO_WIN_SIZE * SRIO_POOL_PORT_SECT_NUM)
#define SRIO_POOL_SECT_SIZE	SRIO_WIN_SIZE
#define SRIO_POOL_SIZE	0x1000000
#define TEST_MAX_TIMES		1000000
#define ATTR_CMD_NUM		7
#define OP_CMD_NUM		8
#define TEST_CMD_NUM		3
#define DMA_TEST_CHAIN_NUM 1

#define DATA_SIZE SRIO_WIN_SIZE
#define CHANNEL_NUM 8
struct dma_ch *dma[2][4];
struct srio_dev *sriodev;
struct srio_port_data *port_data;
dma_addr_t dma_desc_rx_phys;
dma_addr_t dma_desc_tx_phys;
void *dma_desc_rx;
void *dma_desc_tx;

/* dma link description structure */
struct dma_link_dsc {
	uint32_t src_attr;
	uint32_t src_addr;
	uint32_t dst_attr;
	uint32_t dst_addr;
	uint32_t nld_eaddr;
	uint32_t nld_addr;
	uint32_t byte_count;
	uint32_t rev;
};

struct dma_ch_regs {
	uint32_t	mr;
	uint32_t	sr;
	uint32_t	eclndar;
	uint32_t	clndar;
	uint32_t	satr;
	uint32_t	sar;
	uint32_t	datr;
	uint32_t	dar;
	uint32_t	bcr;
	uint32_t	enlndar;
	uint32_t	nlndar;
	uint32_t	res;
	uint32_t	eclsdar;
	uint32_t	clsdar;
	uint32_t	enlsdar;
	uint32_t	nlsdar;
	uint32_t	ssr;
	uint32_t	dsr;
	uint32_t	res1[14];
};

struct dma_ch {
	struct dma_ch_regs *regs;	/* channel register map */
	int32_t fd;			/* dma channel uio device fd */
};


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

struct dma_pool {
	dma_addr_t dma_phys_base;
	void *dma_virt_base;
};

struct perfstat_param {
    pthread_t id;
    int cpu;
    uint32_t msg_size;
};

struct perftest_param {
    pthread_t id;
    int cpu;
    struct srio_dev *sriodev;
    struct srio_port_data *port_data;
    uint32_t test_mode;
    uint32_t msg_size;
};

void fsl_srio_err_handle_enable(struct srio_dev *sriodev);
void pstop(void);
void *perfstat(void *param);
void *srio_loop_test(void *param);
int dma_chain_mode_test(struct dma_ch *dmadev,
				struct srio_port_data  *port_data, uint32_t size);
int dma_usmem_init(struct dma_pool *pool);
int dma_pool_init(struct dma_pool **pool);
void dma_pool_finish(struct dma_pool *pool);
