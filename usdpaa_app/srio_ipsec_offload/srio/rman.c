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
#include "../ppam_if.h"
#include <ppac_interface.h>
#include <usdpaa/fman.h>
#include "../app_config.h"

#include <rman_interface.h>
#include <rman_cfg.h>
#include <rman_bpool.h>
#include <usdpaa/fsl_qman.h>
#include <usdpaa/fsl_bman.h>

#include <rman_fq_interface.h>
#include <srio.h>
#include <usdpaa/dma_mem.h>

/* Should change it as static */
static struct rman_rx *rman_rx;
static struct rman_tx *rman_tx;

/* hard code for RMan */
struct rman_cfg rman_cfg = {
    .fq_bits[RIO_TYPE_MBOX] = 2,
    .fq_bits[RIO_TYPE_DSTR] = 2,
    .bpid[RIO_TYPE_DSTR] = 16,
    .bpid[RIO_TYPE_MBOX] = 16,
    .bpid[RIO_TYPE_DBELL] = 16,
    .md_create = 0,
    .osid = 0,
    .sgbpid = 16,
    .efq = 0,
};

struct rio_tran dt_tran = {
    .type = 9,
    .flowlvl = 1,
    .flowlvl_mask = 2,
    .dstr = {
        .streamid = 0,
        .streamid_mask = 31,
        .cos = 4,
        .cos_mask = 0,
    },
};

struct rman_rx_cfg rxcfg = {
    .rio_port = 1, /* port 1 is used for Rx */
    .port_mask = 1,
    .sid = 0,
    .sid_mask = 255,
    .fqid = RMAN_RX_FQID,
    .fq_mode = DIRECT,
    .wq = 4,
    .tran = &dt_tran,
};

struct rman_tx_cfg txcfg = {
	.rio_port = 0, /* port 0 is used for Tx */
	.fqs_num = RMAN_TX_FQID_NUM,
	.wq = 3,
	.did = 1,       /* Destination ID */
	.fqid = RMAN_TX_FQID,
	.tran = &dt_tran,
};

void rman_finish(void)
{
	rman_rx_finish(rman_rx);
	rman_tx_finish(rman_tx);
	rman_if_finish();
}

static void rman_interrupt_handler(void)
{
	int status;

	status = rman_interrupt_status();
	if (!status)
		return;

	if (status & RMAN_OTE_ERROR_MASK) {
		error(0, 0, "gets rman outbound transaction error");
		return;
	}
	if (status & RMAN_ITE_ERROR_MASK) {
		error(0, 0, "gets rman inbound transaction error");
		return;
	}

	/* A workaround to avoid PFDR low watermark error interrupt */
	if (status & RMAN_BAE_ERROR_MASK) {
		rman_if_rxs_disable();
		/* Waiting to release the buffer */
		sleep(1);
		rman_if_rxs_enable();
	}

#ifdef RMAN_ERROR_INTERRUPT_INFO
	if (status & RMAN_OFER_ERROR_MASK)
		error(0, 0, "Outbound frame queue enqueue rejection error");
	if (status & RMAN_IFER_ERROR_MASK)
		error(0, 0, "Inbound frame queue enqueue rejection error");
	if (status & RMAN_BAE_ERROR_MASK)
		error(0, 0, "RMan buffer allocation error");
	if (status & RMAN_T9IC_ERROR_MASK)
		error(0, 0, "Type9 interrupt coalescing drop threshold exceed");
	if (status & RMAN_T8IC_ERROR_MASK)
		error(0, 0, "Type8 interrupt coalescing drop threshold exceed");
	if (status & RMAN_MFE_ERROR_MASK)
		error(0, 0, "RMan message format error");
#endif

	rman_interrupt_clear();
	rman_interrupt_enable();
}

static void *rman_srio_fd_status_poll(void *data)
{
	int s, rman_fd, srio_fd, nfds;
	fd_set readset;
	uint32_t junk;

	rman_fd = rman_global_fd();
	srio_fd = fsl_srio_fd(rman_if_get_sriodev());

	if (rman_fd > srio_fd)
		nfds = rman_fd + 1;
	else
		nfds = srio_fd + 1;

	rman_interrupt_clear();
	rman_interrupt_enable();
	fsl_srio_clr_bus_err(rman_if_get_sriodev());
	fsl_srio_irq_enable(rman_if_get_sriodev());

	while (1) {
		FD_ZERO(&readset);
		FD_SET(rman_fd, &readset);
		FD_SET(srio_fd, &readset);
		s = select(nfds, &readset, NULL, NULL, NULL);
		if (s < 0) {
			error(0, 0, "RMan&SRIO select error");
			break;
		}
		if (s) {
			if (FD_ISSET(rman_fd, &readset)) {
				read(rman_fd, &junk, sizeof(junk));
				rman_interrupt_handler();
			}
			if (FD_ISSET(srio_fd, &readset)) {
				read(srio_fd, &junk, sizeof(junk));
				fsl_srio_irq_handler(rman_if_get_sriodev());
			}
		}
	}

	pthread_exit(NULL);
}

void rman_interrupt_handler_start(void)
{
	int ret;
	pthread_t interrupt_handler_id;
        RMAN_DBG("Initialze rman_interrupt_handler_start");
	ret = pthread_create(&interrupt_handler_id, NULL,
			     rman_srio_fd_status_poll, NULL);
	if (ret)
		error(0, errno, "Create interrupt handler thread error");
}

int rman_init(void)
{
    int i, j, err;
	struct dma_pool *dmapool = NULL;
    int port_num;
    uint32_t count, desc_size;
    struct dma_link_dsc *desc_virt;
    dma_addr_t src_phys, dst_phys;
    dma_addr_t desc_phys;

    /* sRIO init */
	err = fsl_srio_uio_init(&sriodev);
	if (err < 0)
		error(EXIT_FAILURE, -err, "%s(): srio_uio_init()", __func__);

	port_num = fsl_srio_get_port_num(sriodev);

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

    fsl_srio_err_handle_enable(sriodev);
    /* RMan init */
    if (rman_if_init(sriodev, &rman_cfg)) {
        error(0, 0, "failed to initialize rman if");
        err = -EINVAL;
        goto _err;
    }
    RMAN_DBG("Initialze rman rx port");
    if (rman_if_port_connect(rxcfg.rio_port)) {
        error(0, 0, "SRIO port%d is not connected",
                rxcfg.rio_port);
        return 0;
    }
    rman_rx = rman_rx_init(rxcfg.fqid, rxcfg.fq_mode,
            rxcfg.tran);

    if (!rman_rx)
        return 0;

    rman_rx_listen(rman_rx, rxcfg.rio_port, rxcfg.port_mask,
            rxcfg.sid, rxcfg.sid_mask);

    /*tx*/
    RMAN_DBG("Initialze rman tx port");
    if (rman_if_port_connect(txcfg.rio_port)) {
        error(0, 0, "SRIO port%d is not connected",
                txcfg.rio_port);
        return 0;
    }

    rman_tx = rman_tx_init(txcfg.rio_port, txcfg.fqid,
            txcfg.fqs_num, txcfg.wq,
            txcfg.tran);

    if (!rman_tx)
        return 0;

#ifdef RMAN_TX_CONFIRM
    /* hard code for dist field */
    rman_tx_status_listen(rman_tx, 0, 1);
#endif
#ifdef RMAN_MBOX_MULTICAST
    rman_tx_enable_multicast(rman_tx,
            txcfg.did >> RM_MBOX_MG_SHIFT,
            txcfg.did & RM_MBOX_ML_MASK);
#endif
    rman_tx_connect(rman_tx, txcfg.did);

    rman_interrupt_handler_start();

    rman_rx_enable(rman_rx);
    return 0;
_err:
    rman_finish();
err_srio_connected:
	free(port_data);
err_cmd_malloc:
	fsl_srio_uio_finish(sriodev);

    return err;
}
