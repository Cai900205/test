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

#include "helper_common.h"

void fsl_free_bman_buffer(struct bman_pool *bp,
			  struct bm_buffer *buf, int count, int flags)
{
	int ret;
	if (unlikely(bp == NULL)) {
		TRACE("non-existed buffer pool\n");
		return;
	}

retry:
	ret = bman_release(bp, buf, count, flags);
	if (unlikely(ret == -EBUSY))
		goto retry;
	else if (ret)
		TRACE("buffer free failure, ret %d\n", ret);
}

void fsl_dump(const char *comments, u8 *pkt, u32 len)
{
	int j = 0;

	printf("%s, addr 0x%x, len 0x%x:\n", comments, (u32) pkt, len);

	len = (len > 0x400) ? 0x400 : len;
	for (j = 0; j < len; j++)
		printf("%02x%s", pkt[j], ((j & 0xf) == 0xf) ? "\n" : " ");

	if ((j & 0xf) > 0)
		printf("\n");
}

u16 fsl_ip_cksum(u8 *pkt, u8 len)
{
	u32 sum = 0;
	u16 *buff = (u16 *) pkt;

	for (; len > 0; len -= 2) {
		sum += *buff;
		buff++;
	}
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);

	return (u16) (~sum);
}

void fsl_iphdr_create(struct iphdr *iph, u32 sip, u32 dip, u8 proto)
{
	iph->version = 4;
	iph->ihl = 5;
	iph->tos = 0;
	iph->id = 0;
	iph->frag_off = 0;
	iph->ttl = 64;
	iph->tot_len = 20;
	iph->protocol = proto;
	iph->check = 0;

	/* we do not know what the length is going to be for
	   encapsulated packet. so for now initialize the
	   checksum field. compute checksum after we get the
	   packet back from SEC40.
	 */
	iph->saddr = sip;
	iph->daddr = dip;
	iph->check = fsl_ip_cksum((u8 *) iph, sizeof(struct iphdr));
}

void fsl_fq_clean(struct qman_fq *fq)
{
	u32 flags;
	int s = qman_retire_fq(fq, &flags);
	if (s == 1) {
		/* Retire is non-blocking, poll for completion */
		enum qman_fq_state state;
		do {
			qman_poll();
			qman_fq_state(fq, &state, &flags);
		} while (state != qman_fq_state_retired);
		if (flags & QMAN_FQ_STATE_NE) {
			/* FQ isn't empty, drain it */
			s = qman_volatile_dequeue(fq, 0,
						  QM_VDQCR_NUMFRAMES_TILLEMPTY);
			BUG_ON(s);
			/* Poll for completion */
			do {
				qman_poll();
				qman_fq_state(fq, &state, &flags);
			} while (flags & QMAN_FQ_STATE_VDQCR);
		}
	}
	s = qman_oos_fq(fq);
	BUG_ON(s);
	qman_destroy_fq(fq, 0);
}

void fsl_drop_frame(const struct qm_fd *fd)
{
	struct bm_buffer buf[8];
	int i, j;
	u8 bpid;

	if (fd->format == qm_fd_sg) {
		struct qm_sg_entry *sg = (struct qm_sg_entry *)
		    (ptov(qm_fd_addr(fd)) + fd->offset);

		i = 0;
		do {
			bpid = sg[i].bpid;
			j = 0;
			do {
				buf[j].hi = sg[i].addr_hi;
				buf[j].lo = sg[i].addr_lo;
				j++;
				i++;
			} while (j < ARRAY_SIZE(buf) && !sg[i - 1].final &&
				 sg[i - 1].bpid == sg[i].bpid);

			DEBUG("free sg buffer %d to bp %d\n", j, bpid);
			fsl_free_bman_buffer(get_bman_pool(bpid), buf, j, 0);
		} while (!sg[i - 1].final);
	}

	/* free the fd */
	DEBUG("free fd buffer to bp %d\n", fd->bpid);
	bm_buffer_set64(&buf[0], qm_fd_addr(fd));
	fsl_free_bman_buffer(get_bman_pool(fd->bpid), &buf[0], 1, 0);
}
