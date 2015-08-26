/*
 * Copyright (C) 2011 - 2012 Freescale Semiconductor, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <usdpaa/dma_mem.h>
#include <stdint.h>
#include <stdbool.h>
#include "app_common.h"
#include "ipsec/ipsec_sec.h"
#include "ipsec/ipsec_decap.h"
#include "mm/mem_cache.h"
#include "ip/ip_accept.h"
#include "ip/ip_protos.h"
#include "ip/ip_hooks.h"
#include "ethernet/eth.h"
#include "ip/ip_output.h"
#include "ip/ip_forward.h"

void ipsec_create_compound_fd(struct qm_fd *fd, const struct qm_fd *old_fd,
				 struct iphdr *ip_hdr, uint8_t mode)
{
	/* we are passing in the input frame & data */
	struct qm_sg_entry *sg;
	struct qm_sg_entry *next_sg;
	uint32_t size = 0;

#define MAX_PADDING_LEN 16
#define PAD_LEN		2
#define ESP_HDR_LEN	12
#define PRIV_DATA_SIZE	32

	if (mode == ENCRYPT)
		size = PRIV_DATA_SIZE + ETHER_HDR_LEN + ESP_HDR_LEN +
			sizeof(struct iphdr) + ip_hdr->tot_len + PAD_LEN +
			MAX_PADDING_LEN;
	else
		size = ip_hdr->tot_len;

	sg = __dma_mem_ptov(qm_fd_addr(old_fd));
	memset(sg, 0, 2*sizeof(struct qm_sg_entry));

	/* output buffer */
	qm_sg_entry_set64(sg, qm_fd_addr_get64(old_fd));
	sg->length = size;
	if (DECRYPT == mode) {
		if (qm_fd_contig == old_fd->format) {
			sg->offset = old_fd->offset + ETHER_HDR_LEN;
		} else if (qm_fd_sg == old_fd->format) {
			sg->offset = old_fd->offset;
			/* next_sg is same as input buffer */
			sg->extension = 1;
		}
	} else {
		sg->offset = PRIV_DATA_SIZE + ETHER_HDR_LEN;
	}
	sg->bpid = old_fd->bpid;

	/* input buffer */
	sg++;
	qm_sg_entry_set64(sg, qm_fd_addr_get64(old_fd));
	sg->length = ip_hdr->tot_len;
	if (qm_fd_contig == old_fd->format) {
		sg->offset = old_fd->offset + ETHER_HDR_LEN;
		sg->extension = 0;
	} else if (qm_fd_sg == old_fd->format) {
		sg->offset = old_fd->offset;
		next_sg = __dma_mem_ptov(qm_sg_addr(sg) + sg->offset);
		next_sg->length -= ETHER_HDR_LEN;
		next_sg->offset += ETHER_HDR_LEN;
		sg->extension = 1;
	}
	sg->bpid = old_fd->bpid;
	sg->final = 1;
	sg--;
	qm_fd_addr_set64(fd, __dma_mem_vtop(sg));
	fd->_format1 = qm_fd_compound;
	fd->cong_weight = 0;
	fd->cmd = 0;
}

void ipsec_create_simple_fd(struct qm_fd *simple_fd,
			    const struct qm_fd *compound_fd, uint8_t mode)
{
	struct qm_sg_entry *sg;

	sg = __dma_mem_ptov(qm_fd_addr(compound_fd));
	qm_fd_addr_set64(simple_fd, qm_sg_addr(sg));
	if (0 == sg->extension)
		simple_fd->format = qm_fd_contig;
	else
		simple_fd->format = qm_fd_sg;

	simple_fd->offset = (uint16_t)sg->offset;
	simple_fd->length20 = (uint16_t)sg->length;
	simple_fd->bpid = sg->bpid;
	simple_fd->cmd = 0;

	/* SEC40 updates the length field when it writes
	   output.
	 */
	pr_debug
	    ("Offset Post CAAM is %x sg->addr_lo is %x, bpid = %x",
	     sg->offset, sg->addr_lo, simple_fd->bpid);
}

void ipsec_build_outer_ip_hdr(struct iphdr *ip_hdr,
			      uint32_t *saddr, uint32_t *daddr)
{
	ip_hdr->version = IPVERSION;
	ip_hdr->ihl = sizeof(*ip_hdr) / sizeof(uint32_t);
	ip_hdr->tos = 0;
	ip_hdr->tot_len = sizeof(*ip_hdr);
	ip_hdr->id = 0;
	ip_hdr->frag_off = 0;
	ip_hdr->ttl = IPDEFTTL;
	ip_hdr->protocol = IPPROTO_ESP;
	/* we do not know what the length is going to be for
	   encapsulated packet. so for now initialize the
	   checksum field. compute checksum after we get the
	   packet back from SEC40.
	 */
	ip_hdr->check = 0;
	ip_hdr->saddr = *saddr;
	ip_hdr->daddr = *daddr;
	ip_hdr->check = ip_checksum(ip_hdr, sizeof(struct iphdr));
}
