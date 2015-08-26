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

#include <internal/compat.h>
#include <usdpaa/dma_mem.h>
#include <usdpaa/fsl_dma.h>
#include <usdpaa/fsl_srio.h>
#include <error.h>
#include <srio.h>
/* Handle SRIO error interrupt */
void *srio_error_poll(void *data)
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
			     srio_error_poll, sriodev);
	if (ret)
		error(0, errno, "Create interrupt handler thread error");
}
