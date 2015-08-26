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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <net/if.h>
#include <linux/rtnetlink.h>

#include <unistd.h>
#include <arpa/inet.h>

#define BUFSIZE 8192

struct route_info {
	struct in_addr dst_addr;
	struct in_addr src_addr;
	struct in_addr gw_addr;
	unsigned char dst_len;
	char if_name[IF_NAMESIZE];
};


static int read_nl_sock(int sock_fd, char *buf_ptr, int seq_num, int pid)
{
	struct nlmsghdr *nl_hdr;
	int read_len = 0, msg_len = 0;

	do {
		read_len = recv(sock_fd, buf_ptr, BUFSIZE - msg_len, 0);
		if (read_len < 0) {
			fprintf(stderr,
				"cannot receive from route NL socket\n");
			return -1;
		}

		nl_hdr = (struct nlmsghdr *)buf_ptr;
		if ((NLMSG_OK(nl_hdr, read_len) == 0) ||
		    (nl_hdr->nlmsg_type == NLMSG_ERROR)) {
			fprintf(stderr, "route NL socket returned error\n");
			return -1;
		}

		if (nl_hdr->nlmsg_type == NLMSG_DONE) {
			break;
		} else {
			buf_ptr += read_len;
			msg_len += read_len;
		}

		if ((nl_hdr->nlmsg_flags & NLM_F_MULTI) == 0)
			break;
	} while ((nl_hdr->nlmsg_seq != seq_num) ||
		(nl_hdr->nlmsg_pid != pid));

	return msg_len;
}

static void parse_routes(struct nlmsghdr *nl_hdr, struct route_info *rt_info)
{
	struct rtmsg *rt_msg;
	struct rtattr *rt_attr;
	int rt_len;

	rt_msg = (struct rtmsg *)NLMSG_DATA(nl_hdr);

	if ((rt_msg->rtm_family != AF_INET) ||
	    (rt_msg->rtm_table != RT_TABLE_MAIN))
		return;

	rt_attr = (struct rtattr *)RTM_RTA(rt_msg);
	rt_len = RTM_PAYLOAD(nl_hdr);
	for (; RTA_OK(rt_attr, rt_len); rt_attr = RTA_NEXT(rt_attr, rt_len)) {
		switch (rt_attr->rta_type) {
		case RTA_OIF:
			if_indextoname(*(int *)RTA_DATA(rt_attr),
				       rt_info->if_name);
			break;

		case RTA_GATEWAY:
			memcpy(&rt_info->gw_addr, RTA_DATA(rt_attr),
			       sizeof(rt_info->gw_addr));
			break;

		case RTA_PREFSRC:
			memcpy(&rt_info->src_addr, RTA_DATA(rt_attr),
			       sizeof(rt_info->src_addr));
			break;

		case RTA_DST:
			memcpy(&rt_info->dst_addr, RTA_DATA(rt_attr),
			       sizeof(rt_info->dst_addr));
			break;
		}
	}
	rt_info->dst_len = rt_msg->rtm_dst_len;
}

int get_dst_addrs(struct in_addr *dst_addr, unsigned char *dst_len,
		  struct in_addr *gw_addr, unsigned int max_len)
{
	int ret, i = 0;
	struct nlmsghdr *nl_msg;
	struct route_info rt_info;
	char msg_buf[BUFSIZE];

	int sock, len, msg_seq = 0;
	sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
	if (sock < 0) {
		fprintf(stderr, "cannot open route NL socket\n");
		return -1;
	}

	memset(msg_buf, 0, BUFSIZE);
	nl_msg = (struct nlmsghdr *)msg_buf;
	nl_msg->nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	nl_msg->nlmsg_type = RTM_GETROUTE;
	nl_msg->nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST;
	nl_msg->nlmsg_seq = msg_seq++;
	nl_msg->nlmsg_pid = getpid();

	ret = send(sock, nl_msg, nl_msg->nlmsg_len, 0);
	if (ret < 0) {
		fprintf(stderr, "cannot send RTM_GETROUTE dump msg\n");
		close(sock);
		return ret;
	}

	len = read_nl_sock(sock, msg_buf, msg_seq, getpid());
	if (len < 0) {
		close(sock);
		return len;
	}

	for (; NLMSG_OK(nl_msg, len); nl_msg = NLMSG_NEXT(nl_msg, len)) {
		memset(&rt_info, 0, sizeof(struct route_info));
		parse_routes(nl_msg, &rt_info);

		if (!memcmp(&rt_info.gw_addr, gw_addr, sizeof(*gw_addr))) {
			dst_addr[i] = rt_info.dst_addr;
			dst_len[i] = rt_info.dst_len;
			if (i == max_len)
				break;
			i++;
		}
	}

	close(sock);

	return i;
}
