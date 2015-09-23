/*
 * Copyright (c) 2006 Mellanox Technologies Ltd.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * $Id$
 */
#include <linux/device.h>
#include <linux/in.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pci.h>
#include <linux/time.h>
#include <linux/workqueue.h>

#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>
#include <net/tcp_states.h>
#include <rdma/sdp_socket.h>
#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
#include <net/ipv6.h>
#include <net/transp_v6.h>
#endif
#include "sdp.h"

#define SDP_MAJV_MINV 0x22

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0))
	#define ipv6_addr_copy(a, b) (*(a) = *(b))
#endif

SDP_MODPARAM_INT(sdp_rx_size, 0x40, "HW rx queue size (max num of credits)."
		" Must be power of 2.");

SDP_MODPARAM_SINT(sdp_retry_count, 5, "IB layer retry count");

SDP_MODPARAM_SINT(sdp_link_layer_ib_only, 0, "Support only link layer of "
		"type Infiniband");

static void sdp_qp_event_handler(struct ib_event *event, void *data)
{
	if (event->event == IB_EVENT_PATH_MIG) {
		sdp_dbg(NULL, "Path migration event\n");
		return;
	}
	sdp_warn(NULL, "unexpected invocation: event: %d, data=%p\n",
			event->event, data);
}

static int sdp_get_max_dev_sge(struct ib_device *dev)
{
	struct ib_device_attr attr;
	static int max_sges = -1;
	int rc;

	if (max_sges > 0)
		goto out;

	rc = ib_query_device(dev, &attr);
	if (rc) {
		sdp_warn(NULL, "ib_query_device failed: %d\n", rc);
		goto out;
	}

	max_sges = attr.max_sge;

out:
	return max_sges;
}

static int sdp_init_qp(struct sock *sk, struct rdma_cm_id *id)
{
	struct ib_qp_init_attr qp_init_attr = {
		.event_handler = sdp_qp_event_handler,
		.cap.max_send_wr = SDP_TX_SIZE,
		.cap.max_recv_wr = sdp_rx_size,
		.cap.max_inline_data = sdp_inline_thresh,
        	.sq_sig_type = IB_SIGNAL_REQ_WR,
        	.qp_type = IB_QPT_RC,
	};
	struct ib_device *device = id->device;
	int rc;

	sdp_dbg(sk, "%s\n", __func__);

	sdp_sk(sk)->max_sge = sdp_get_max_dev_sge(device);
	sdp_dbg(sk, "Max sges: %d\n", sdp_sk(sk)->max_sge);

	qp_init_attr.cap.max_send_sge = MIN(sdp_sk(sk)->max_sge, SDP_MAX_SEND_SGES);
	sdp_dbg(sk, "Setting max send sge to: %d\n", qp_init_attr.cap.max_send_sge);

	qp_init_attr.cap.max_recv_sge = MIN(sdp_sk(sk)->max_sge, SDP_MAX_RECV_SGES);
	sdp_dbg(sk, "Setting max recv sge to: %d\n", qp_init_attr.cap.max_recv_sge);

	sdp_sk(sk)->sdp_dev = ib_get_client_data(device, &sdp_client);
	if (!sdp_sk(sk)->sdp_dev) {
		sdp_warn(sk, "SDP not available on device %s\n", device->name);
		rc = -ENODEV;
		goto err_rx;
	}

	rc = sdp_rx_ring_create(sdp_sk(sk), device);
	if (rc)
		goto err_rx;

	rc = sdp_tx_ring_create(sdp_sk(sk), device);
	if (rc)
		goto err_tx;

	qp_init_attr.recv_cq = sdp_sk(sk)->rx_ring.cq;
	qp_init_attr.send_cq = sdp_sk(sk)->tx_ring.cq;

	rc = rdma_create_qp(id, sdp_sk(sk)->sdp_dev->pd, &qp_init_attr);
	if (rc) {
		sdp_warn(sk, "Unable to create QP: %d.\n", rc);
		goto err_qp;
	}
	sdp_sk(sk)->qp = id->qp;
	sdp_sk(sk)->ib_device = device;
	sdp_sk(sk)->qp_active = 1;
	sdp_sk(sk)->context.device = device;
	sdp_sk(sk)->inline_thresh = qp_init_attr.cap.max_inline_data;

	sdp_dbg(sk, "%s done\n", __func__);
	return 0;

err_qp:
	sdp_tx_ring_destroy(sdp_sk(sk));
err_tx:
	sdp_rx_ring_destroy(sdp_sk(sk));
err_rx:
	return rc;
}

static int sdp_get_max_send_frags(u32 buf_size)
{
	return MIN(
		/* +1 to conpensate on not aligned buffers */
		(PAGE_ALIGN(buf_size) >> PAGE_SHIFT) + 1,
		SDP_MAX_SEND_SGES - 1);
}

static int sdp_connect_handler(struct sock *sk, struct rdma_cm_id *id,
		       	struct rdma_cm_event *event)
{
	struct sockaddr_in *dst_addr;
	struct sock *child;
	const struct sdp_hh *h;
	int rc = 0;

	sdp_dbg(sk, "%s %p -> %p\n", __func__, sdp_sk(sk)->id, id);

	h = event->param.conn.private_data;
	SDP_DUMP_PACKET(sk, "RX", NULL, &h->bsdh);

	if (h->ipv_cap & HH_IPV_MASK & ~(HH_IPV4 | HH_IPV6)) {
		sdp_warn(sk, "Bad IPV field in SDP Hello header: 0x%x\n",
				h->ipv_cap & HH_IPV_MASK);
		return -EINVAL;
	}

	if (!h->max_adverts)
		return -EINVAL;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 3, 0))
	child = sk_clone(sk, GFP_KERNEL);
#else
	child = sk_clone_lock(sk, GFP_KERNEL);
#endif
	if (!child)
		return -ENOMEM;

	sdp_init_sock(child);

	dst_addr = (struct sockaddr_in *)&id->route.addr.dst_addr;
	sdp_inet_dport(child) = dst_addr->sin_port;

#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
	if (inet6_sk(sk)) {
		struct ipv6_pinfo *newnp;

		newnp = inet_sk(child)->pinet6 = sdp_inet6_sk_generic(child);

		memcpy(newnp, inet6_sk(sk), sizeof(struct ipv6_pinfo));

		if ((h->ipv_cap & HH_IPV_MASK) == HH_IPV4) {
			/* V6 mapped */
			sdp_inet_daddr(child) = dst_addr->sin_addr.s_addr;
			ipv6_addr_set(&newnp->daddr, 0, 0, htonl(0x0000FFFF),
					h->src_addr.ip4.addr);

			ipv6_addr_set(&newnp->saddr, 0, 0, htonl(0x0000FFFF),
					h->dst_addr.ip4.addr);

			ipv6_addr_copy(&newnp->rcv_saddr, &newnp->saddr);
		} else if ((h->ipv_cap & HH_IPV_MASK) == HH_IPV6) {
			struct sockaddr_in6 *dst_addr6 = (struct sockaddr_in6 *)dst_addr;
			struct sockaddr_in6 *src_addr6 =
				(struct sockaddr_in6 *)&id->route.addr.src_addr;

			ipv6_addr_copy(&newnp->daddr, &dst_addr6->sin6_addr);
			ipv6_addr_copy(&newnp->saddr, &src_addr6->sin6_addr);
			ipv6_addr_copy(&newnp->rcv_saddr, &src_addr6->sin6_addr);
		} else {
			sdp_warn(child, "Bad IPV field: 0x%x\n", h->ipv_cap & HH_IPV_MASK);
		}

		sdp_inet_daddr(child) = sdp_inet_saddr(child) =
			sdp_inet_rcv_saddr(child) = LOOPBACK4_IPV6;
	} else
#endif
	{
		sdp_inet_daddr(child) = dst_addr->sin_addr.s_addr;
	}

#ifdef SDP_SOCK_HISTORY
	sdp_ssk_hist_rename(sk);
#endif
	__sock_put(child, SOCK_REF_CLONE);

	down_read(&device_removal_lock);

	rc = sdp_init_qp(child, id);
	if (rc) {
		bh_unlock_sock(child);
		up_read(&device_removal_lock);
		sdp_sk(child)->destructed_already = 1;
#ifdef SDP_SOCK_HISTORY
		sdp_ssk_hist_close(child);
#endif
		sk_free(child);
		return rc;
	}

	sdp_sk(child)->max_bufs = ntohs(h->bsdh.bufs);
	atomic_set(&sdp_sk(child)->tx_ring.credits, sdp_sk(child)->max_bufs);

	sdp_sk(child)->min_bufs = tx_credits(sdp_sk(child)) / 4;
	sdp_sk(child)->xmit_size_goal = ntohl(h->localrcvsz) -
		sizeof(struct sdp_bsdh);

	sdp_sk(child)->send_frags = sdp_get_max_send_frags(sdp_sk(child)->xmit_size_goal);
	sdp_init_buffers(sdp_sk(child), rcvbuf_initial_size);

	id->context = child;
	sdp_sk(child)->id = id;

	list_add_tail(&sdp_sk(child)->backlog_queue,
			&sdp_sk(sk)->backlog_queue);
	sdp_sk(child)->parent = sk;

	bh_unlock_sock(child);
	sdp_add_sock(sdp_sk(child));
	up_read(&device_removal_lock);

	sdp_exch_state(child, TCPF_LISTEN | TCPF_CLOSE, TCP_SYN_RECV);

	/* child->sk_write_space(child); */
	/* child->sk_data_ready(child, 0); */
	sk->sk_data_ready(sk, 0);

	return 0;
}

static int sdp_response_handler(struct sock *sk, struct rdma_cm_id *id,
				struct rdma_cm_event *event)
{
	const struct sdp_hah *h;
	struct sockaddr_in *dst_addr;
	sdp_dbg(sk, "%s\n", __func__);

	sdp_exch_state(sk, TCPF_SYN_SENT, TCP_ESTABLISHED);
	sdp_set_default_moderation(sdp_sk(sk));

	if (sock_flag(sk, SOCK_KEEPOPEN))
		sdp_start_keepalive_timer(sk);

	if (sock_flag(sk, SOCK_DEAD))
		return 0;

	h = event->param.conn.private_data;
	SDP_DUMP_PACKET(sk, "RX", NULL, &h->bsdh);
	sdp_sk(sk)->max_bufs = ntohs(h->bsdh.bufs);
	atomic_set(&sdp_sk(sk)->tx_ring.credits, sdp_sk(sk)->max_bufs);
	sdp_sk(sk)->min_bufs = tx_credits(sdp_sk(sk)) / 4;
	sdp_sk(sk)->xmit_size_goal =
		ntohl(h->actrcvsz) - sizeof(struct sdp_bsdh);
	sdp_sk(sk)->send_frags = sdp_get_max_send_frags(sdp_sk(sk)->xmit_size_goal);
	sdp_sk(sk)->xmit_size_goal = MIN(sdp_sk(sk)->xmit_size_goal,
		sdp_sk(sk)->send_frags * PAGE_SIZE);

	sdp_sk(sk)->poll_cq = 1;

	sk->sk_state_change(sk);
	sk_wake_async(sk, 0, POLL_OUT);

	dst_addr = (struct sockaddr_in *)&id->route.addr.dst_addr;
	sdp_inet_dport(sk) = dst_addr->sin_port;
	sdp_inet_daddr(sk) = dst_addr->sin_addr.s_addr;

#ifdef SDP_SOCK_HISTORY
	sdp_ssk_hist_rename(sk);
#endif
	return 0;
}

static int sdp_connected_handler(struct sock *sk)
{
	struct sock *parent;
	sdp_dbg(sk, "%s\n", __func__);

	parent = sdp_sk(sk)->parent;
	BUG_ON(!parent);

	sdp_exch_state(sk, TCPF_SYN_RECV, TCP_ESTABLISHED);

#ifdef SDP_SOCK_HISTORY
	sdp_ssk_hist_rename(sk);
#endif
	sdp_set_default_moderation(sdp_sk(sk));

	if (sock_flag(sk, SOCK_KEEPOPEN))
		sdp_start_keepalive_timer(sk);

	if (sock_flag(sk, SOCK_DEAD))
		return 0;

	lock_sock(parent);
	if (!sdp_sk(parent)->id) { /* TODO: look at SOCK_DEAD? */
		sdp_dbg(sk, "parent is going away.\n");
		goto done;
	}

	sk_acceptq_added(parent);
	sdp_dbg(parent, "%s child connection established\n", __func__);
	list_del_init(&sdp_sk(sk)->backlog_queue);
	list_add_tail(&sdp_sk(sk)->accept_queue,
			&sdp_sk(parent)->accept_queue);

	parent->sk_state_change(parent);
	sk_wake_async(parent, 0, POLL_OUT);
done:
	release_sock(parent);

	return 0;
}

static int sdp_disconnected_handler(struct sock *sk)
{
	struct sdp_sock *ssk = sdp_sk(sk);

	sdp_dbg(sk, "%s\n", __func__);

	if (ssk->tx_ring.cq)
		if (sdp_xmit_poll(ssk, 1))
			sdp_post_sends(ssk, 0);

	if (sk->sk_state == TCP_SYN_RECV) {
		sdp_connected_handler(sk);

		if (rcv_nxt(ssk))
			return 0;
	}

	return -ECONNRESET;
}

int sdp_cma_handler(struct rdma_cm_id *id, struct rdma_cm_event *event)
{
	struct rdma_conn_param conn_param;
	struct sock *parent = NULL;
	struct sock *child = NULL;
	struct sock *sk;
	struct sdp_hah hah;
	struct sdp_hh hh;

	int rc = 0, rc2;

	sk = id->context;
	if (!sk) {
		sdp_dbg(NULL, "cm_id is being torn down, event %s\n",
		       	rdma_cm_event_str(event->event));
		return event->event == RDMA_CM_EVENT_CONNECT_REQUEST ?
			-EINVAL : 0;
	}

	sdp_add_to_history(sk, rdma_cm_event_str(event->event));

	lock_sock_nested(sk, SINGLE_DEPTH_NESTING);
	sdp_dbg(sk, "event: %s\n", rdma_cm_event_str(event->event));
	if (!sdp_sk(sk)->id) {
		sdp_dbg(sk, "socket is being torn down\n");
		rc = event->event == RDMA_CM_EVENT_CONNECT_REQUEST ?
			-EINVAL : 0;
		release_sock(sk);
		return rc;
	}

	switch (event->event) {
	case RDMA_CM_EVENT_ADDR_RESOLVED:
		if (sdp_link_layer_ib_only &&
			rdma_node_get_transport(id->device->node_type) ==
				RDMA_TRANSPORT_IB &&
			rdma_port_get_link_layer(id->device, id->port_num) !=
				IB_LINK_LAYER_INFINIBAND) {
			sdp_dbg(sk, "Link layer is: %d. Only IB link layer "
				"is allowed\n",
				rdma_port_get_link_layer(id->device,
					id->port_num));
			rc = -ENETUNREACH;
			break;
		}

		rc = rdma_resolve_route(id, SDP_ROUTE_TIMEOUT);
		break;
	case RDMA_CM_EVENT_ADDR_ERROR:
		rc = -ENETUNREACH;
		break;
	case RDMA_CM_EVENT_ROUTE_RESOLVED:
		rc = sdp_init_qp(sk, id);
		if (rc)
			break;
		memset(&hh, 0, sizeof hh);
		hh.bsdh.mid = SDP_MID_HELLO;
		hh.bsdh.len = htonl(sizeof(struct sdp_hh));
		hh.max_adverts = 1;

		hh.majv_minv = SDP_MAJV_MINV;
		sdp_init_buffers(sdp_sk(sk), rcvbuf_initial_size);
		hh.bsdh.bufs = htons(rx_ring_posted(sdp_sk(sk)));
		atomic_set(&sdp_sk(sk)->remote_credits,
				rx_ring_posted(sdp_sk(sk)));
		hh.localrcvsz = hh.desremrcvsz = htonl(sdp_sk(sk)->recv_frags *
				PAGE_SIZE + sizeof(struct sdp_bsdh));
#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
		if (inet6_sk(sk)) {
			struct sockaddr *src_addr = (struct sockaddr *)&id->route.addr.src_addr;
			struct sockaddr_in *addr4 = (struct sockaddr_in *)src_addr;
			struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)src_addr;

			if (src_addr->sa_family == AF_INET) {
				/* IPv4 over IPv6 */
				ipv6_addr_set(&inet6_sk(sk)->rcv_saddr, 0, 0, htonl(0xFFFF),
						addr4->sin_addr.s_addr);
			} else {
				inet6_sk(sk)->rcv_saddr = addr6->sin6_addr;
			}
			inet6_sk(sk)->saddr = inet6_sk(sk)->rcv_saddr;
		}
			else
#endif
		{
			sdp_inet_saddr(sk) = sdp_inet_rcv_saddr(sk) =
				((struct sockaddr_in *)&id->route.addr.src_addr)->sin_addr.s_addr;
		}
		memset(&conn_param, 0, sizeof conn_param);
		conn_param.private_data_len = sizeof hh;
		conn_param.private_data = &hh;
		conn_param.responder_resources = 4 /* TODO */;
		conn_param.initiator_depth = 4 /* TODO */;
		conn_param.retry_count = sdp_retry_count;
		SDP_DUMP_PACKET(sk, "TX", NULL, &hh.bsdh);

		if (sdp_apm_enable) {
			rc = rdma_enable_apm(id, RDMA_ALT_PATH_BEST);
			if (rc)
				sdp_warn(sk, "APM couldn't be enabled for active side: %d\n", rc);
		}

		rc = rdma_connect(id, &conn_param);
		break;

	case RDMA_CM_EVENT_ALT_ROUTE_RESOLVED:
		sdp_dbg(sk, "alt route was resolved slid=%d, dlid=%d\n",
				id->route.path_rec[1].slid, id->route.path_rec[1].dlid);
		break;

	case RDMA_CM_EVENT_ALT_PATH_LOADED:
		sdp_dbg(sk, "alt route path loaded\n");
		break;

	case RDMA_CM_EVENT_ALT_ROUTE_ERROR:
		sdp_warn(sk, "alt route resolve error\n");
		break;

	case RDMA_CM_EVENT_ROUTE_ERROR:
		rc = -ETIMEDOUT;
		break;
	case RDMA_CM_EVENT_CONNECT_REQUEST:
		rc = sdp_connect_handler(sk, id, event);
		if (rc) {
			sdp_dbg(sk, "Destroying qp\n");
			rdma_reject(id, NULL, 0);
			break;
		}
		child = id->context;
		atomic_set(&sdp_sk(child)->remote_credits,
				rx_ring_posted(sdp_sk(child)));
		memset(&hah, 0, sizeof hah);
		hah.bsdh.mid = SDP_MID_HELLO_ACK;
		hah.bsdh.bufs = htons(rx_ring_posted(sdp_sk(child)));
		hah.bsdh.len = htonl(sizeof(struct sdp_hah));
		hah.majv_minv = SDP_MAJV_MINV;
		hah.ext_max_adverts = 1; /* Doesn't seem to be mandated by spec,
					    but just in case */
		hah.actrcvsz = htonl(sdp_sk(child)->recv_frags * PAGE_SIZE +
			sizeof(struct sdp_bsdh));
		memset(&conn_param, 0, sizeof conn_param);
		conn_param.private_data_len = sizeof hah;
		conn_param.private_data = &hah;
		conn_param.responder_resources = 4 /* TODO */;
		conn_param.initiator_depth = 4 /* TODO */;
		conn_param.retry_count = sdp_retry_count;
		SDP_DUMP_PACKET(sk, "TX", NULL, &hah.bsdh);
		rc = rdma_accept(id, &conn_param);
		if (rc) {
			sdp_sk(child)->id = NULL;
			id->qp = NULL;
			id->context = NULL;
			parent = sdp_sk(child)->parent; /* TODO: hold ? */
		} else if (sdp_apm_enable) {
				rc2 = rdma_enable_apm(id, RDMA_ALT_PATH_BEST);
				if (rc2)
					sdp_warn(sk, "APM couldn't be enabled for passive side: %d\n", rc2);
		}
		break;
	case RDMA_CM_EVENT_CONNECT_RESPONSE:
		rc = sdp_response_handler(sk, id, event);
		if (rc) {
			sdp_dbg(sk, "Destroying qp\n");
			rdma_reject(id, NULL, 0);
		} else {
			rc = rdma_accept(id, NULL);
			if (!rc && sdp_apm_enable) {
				rc2 = rdma_enable_apm(id, RDMA_ALT_PATH_BEST);
				if (rc2)
					sdp_warn(sk, "APM couldn't be enabled for passive side:%d \n", rc2);
			}
		}
		break;
	case RDMA_CM_EVENT_CONNECT_ERROR:
		rc = -ETIMEDOUT;
		break;
	case RDMA_CM_EVENT_UNREACHABLE:
		rc = -ENETUNREACH;
		break;
	case RDMA_CM_EVENT_REJECTED:
		rc = -ECONNREFUSED;
		break;
	case RDMA_CM_EVENT_ESTABLISHED:
		sdp_inet_saddr(sk) = sdp_inet_rcv_saddr(sk) =
			((struct sockaddr_in *)&id->route.addr.src_addr)->sin_addr.s_addr;
		rc = sdp_connected_handler(sk);
		break;
	case RDMA_CM_EVENT_DISCONNECTED: /* This means DREQ/DREP received */
		if (sk->sk_state == TCP_LAST_ACK) {
			sdp_cancel_dreq_wait_timeout(sdp_sk(sk));

			sdp_exch_state(sk, TCPF_LAST_ACK, TCP_TIME_WAIT);

			sdp_dbg(sk, "%s: waiting for Infiniband tear down\n",
				__func__);
		}

		sdp_sk(sk)->qp_active = 0;
		rdma_disconnect(id);

		if (sk->sk_state != TCP_TIME_WAIT) {
			if (sk->sk_state == TCP_CLOSE_WAIT) {
				sdp_dbg(sk, "IB teardown while in "
					"TCP_CLOSE_WAIT taking reference to "
					"let close() finish the work\n");
				sock_hold(sk, SOCK_REF_CMA);
				sdp_start_cma_timewait_timeout(sdp_sk(sk),
						SDP_CMA_TIMEWAIT_TIMEOUT);

			}
			sdp_set_error(sk, -EPIPE);
			rc = sdp_disconnected_handler(sk);
		}
		break;
	case RDMA_CM_EVENT_TIMEWAIT_EXIT:
		rc = sdp_disconnected_handler(sk);
		break;
	case RDMA_CM_EVENT_DEVICE_REMOVAL:
		rc = -ENETRESET;
		break;

	case RDMA_CM_EVENT_ADDR_CHANGE:
		sdp_dbg(sk, "Got Address change event\n");
		rc = 0;
		break;
	default:
		printk(KERN_ERR "SDP: Unexpected CMA event: %d\n",
		       event->event);
		rc = -ECONNABORTED;
		break;
	}

	sdp_dbg(sk, "event: %s handled\n", rdma_cm_event_str(event->event));

	if (rc && sdp_sk(sk)->id == id) {
		child = sk;
		sdp_sk(sk)->id = NULL;
		id->qp = NULL;
		id->context = NULL;
		parent = sdp_sk(sk)->parent;
		sdp_reset_sk(sk, rc);
	}

	release_sock(sk);

	sdp_dbg(sk, "event: %s done. status %d\n",
			rdma_cm_event_str(event->event), rc);

	if (parent) {
		lock_sock(parent);
		if (!sdp_sk(parent)->id) { /* TODO: look at SOCK_DEAD? */
			sdp_dbg(sk, "parent is going away.\n");
			child = NULL;
			goto done;
		}
		if (!list_empty(&sdp_sk(child)->backlog_queue))
			list_del_init(&sdp_sk(child)->backlog_queue);
		else
			child = NULL;
done:
		release_sock(parent);
		if (child)
			sdp_common_release(child);
	}
	return rc;
}
